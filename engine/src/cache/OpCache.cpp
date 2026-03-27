#include "OpCache.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <unistd.h>
#include <openssl/sha.h>
#include <chrono>

namespace praxis {
namespace cache {

// JSON serialization for OpCacheEntryMeta
std::string OpCacheEntryMeta::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"schema_version\": " << schema_version << ",\n";
    ss << "  \"op_hash\": \"" << op_hash << "\",\n";
    ss << "  \"engine_version\": \"" << engine_version << "\",\n";
    ss << "  \"kernel_version\": \"" << kernel_version << "\",\n";
    ss << "  \"created_at\": \"" << created_at << "\",\n";
    ss << "  \"producer\": \"" << producer << "\",\n";
    ss << "  \"outputs\": [\n";
    
    for (size_t i = 0; i < outputs.size(); ++i) {
        const auto& out = outputs[i];
        ss << "    {\n";
        ss << "      \"name\": \"" << out.name << "\",\n";
        ss << "      \"filename\": \"" << out.filename << "\",\n";
        ss << "      \"bytes\": " << out.bytes;
        if (!out.sha256.empty()) {
            ss << ",\n";
            ss << "      \"sha256\": \"" << out.sha256 << "\"";
        }
        ss << ",\n";
        ss << "      \"type\": \"" << out.type << "\",\n";
        ss << "      \"previewable\": " << (out.previewable ? "true" : "false") << ",\n";
        ss << "      \"semantic_type\": \"" << out.semantic_type << "\"";
        ss << "\n";
        ss << "    }";
        if (i < outputs.size() - 1) ss << ",";
        ss << "\n";
    }
    
    ss << "  ]\n";
    ss << "}";
    
    return ss.str();
}

// Simple JSON parsing for OpCacheEntryMeta (minimal implementation)
OpCacheEntryMeta OpCacheEntryMeta::from_json(const std::string& json) {
    // TODO: Implement proper JSON parsing if needed
    // For now, this is a placeholder for basic validation
    OpCacheEntryMeta meta;
    meta.schema_version = 1;
    return meta;
}

// OpCacheStore helper
std::filesystem::path OpCacheStore::get_staging_path(const std::string& output_name, 
                                                     const std::string& extension) const {
    return staging_dir / "outputs" / (output_name + extension);
}

// OpCache implementation
OpCache::OpCache(const std::filesystem::path& cache_root) 
    : cache_root_(cache_root / "v1" / "sha256") {
    // Ensure cache root exists
    std::filesystem::create_directories(cache_root_);
}

std::filesystem::path OpCache::get_entry_dir(const std::string& op_hash) const {
    // Shard by first 2 characters: v1/sha256/ab/abcdef.../
    if (op_hash.length() < 2) {
        throw std::runtime_error("Invalid op_hash: too short");
    }
    
    std::string shard = op_hash.substr(0, 2);
    return cache_root_ / shard / op_hash;
}

std::filesystem::path OpCache::get_staging_dir(const std::string& op_hash) const {
    std::string shard = op_hash.substr(0, 2);
    std::ostringstream ss;
    ss << op_hash << ".tmp_" << getpid();
    return cache_root_ / shard / ss.str();
}

bool OpCache::has(const std::string& op_hash) const {
    auto entry_dir = get_entry_dir(op_hash);
    auto meta_file = entry_dir / "meta.json";
    
    // Quick check: does meta.json exist?
    return std::filesystem::exists(meta_file);
}

OpCacheHit OpCache::try_load(const std::string& op_hash) {
    OpCacheHit result;
    result.op_hash = op_hash;
    
    auto entry_dir = get_entry_dir(op_hash);
    auto meta_file = entry_dir / "meta.json";
    
    // Check if entry exists
    if (!std::filesystem::exists(meta_file)) {
        result.hit = false;
        return result;
    }
    
    // Read and validate meta
    OpCacheEntryMeta meta;
    if (!read_meta(entry_dir, meta)) {
        result.hit = false;
        return result;
    }
    
    // Validate entry
    if (!validate_entry(entry_dir, meta)) {
        result.hit = false;
        return result;
    }
    
    // Build output paths
    result.entry_dir = entry_dir;
    for (const auto& output : meta.outputs) {
        auto output_path = entry_dir / "outputs" / output.filename;
        result.output_paths[output.name] = output_path;
    }
    
    result.hit = true;
    return result;
}

OpCacheStore OpCache::begin_store(const std::string& op_hash) {
    OpCacheStore store;
    store.op_hash = op_hash;
    store.staging_dir = get_staging_dir(op_hash);
    store.final_dir = get_entry_dir(op_hash);
    store.active = true;
    
    // Create staging directory structure
    std::filesystem::create_directories(store.staging_dir / "outputs");
    
    return store;
}

bool OpCache::commit_store(OpCacheStore& store, const OpCacheEntryMeta& meta) {
    if (!store.active) {
        return false;
    }
    
    try {
        // Write meta.json to staging
        if (!write_meta(store.staging_dir, meta)) {
            return false;
        }

        // Write completion marker expected by integrity checks.
        {
            std::ofstream complete_file(store.staging_dir / ".complete");
            if (!complete_file) {
                return false;
            }
            complete_file << "ok\n";
        }
        
        // Ensure final directory parent exists
        std::filesystem::create_directories(store.final_dir.parent_path());
        
        // Atomic rename
        std::filesystem::rename(store.staging_dir, store.final_dir);
        
        store.active = false;
        return true;
        
    } catch (const std::exception& e) {
        // Cleanup on error
        abort_store(store);
        return false;
    }
}

void OpCache::abort_store(OpCacheStore& store) {
    if (!store.active) {
        return;
    }
    
    try {
        if (std::filesystem::exists(store.staging_dir)) {
            std::filesystem::remove_all(store.staging_dir);
        }
    } catch (...) {
        // Best effort cleanup
    }
    
    store.active = false;
}

bool OpCache::validate_entry(const std::filesystem::path& entry_dir, 
                             const OpCacheEntryMeta& meta) const {
    // Require completion marker to guard against partial/in-progress entries.
    if (!std::filesystem::exists(entry_dir / ".complete")) {
        return false;
    }

    // Check all output files exist
    for (const auto& output : meta.outputs) {
        auto output_path = entry_dir / "outputs" / output.filename;
        if (!std::filesystem::exists(output_path)) {
            return false;
        }
        
        // Optionally validate size
        if (output.bytes > 0) {
            auto actual_size = std::filesystem::file_size(output_path);
            if (actual_size != output.bytes) {
                return false;
            }
        }
    }
    
    return true;
}

bool OpCache::read_meta(const std::filesystem::path& entry_dir, 
                       OpCacheEntryMeta& meta) const {
    auto meta_file = entry_dir / "meta.json";
    
    if (!std::filesystem::exists(meta_file)) {
        return false;
    }
    
    try {
        std::ifstream ifs(meta_file);
        if (!ifs) {
            return false;
        }
        
        std::string json((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
        
        // Simple JSON parsing (minimal implementation for Phase 3)
        // Extract key fields using string search
        auto find_string_value = [&json](const std::string& key) -> std::string {
            std::string search = "\"" + key + "\": \"";
            auto pos = json.find(search);
            if (pos == std::string::npos) return "";
            pos += search.length();
            auto end = json.find("\"", pos);
            if (end == std::string::npos) return "";
            return json.substr(pos, end - pos);
        };
        
        auto find_int_value = [&json](const std::string& key) -> int {
            std::string search = "\"" + key + "\": ";
            auto pos = json.find(search);
            if (pos == std::string::npos) return 0;
            pos += search.length();
            return std::stoi(json.substr(pos));
        };
        
        meta.schema_version = find_int_value("schema_version");
        meta.op_hash = find_string_value("op_hash");
        meta.engine_version = find_string_value("engine_version");
        meta.kernel_version = find_string_value("kernel_version");
        meta.created_at = find_string_value("created_at");
        meta.producer = find_string_value("producer");
        
        // Parse outputs array (simple approach: look for filename fields)
        size_t outputs_pos = json.find("\"outputs\":");
        if (outputs_pos != std::string::npos) {
            size_t bracket_pos = json.find("[", outputs_pos);
            size_t end_bracket = json.find("]", bracket_pos);
            std::string outputs_section = json.substr(bracket_pos, end_bracket - bracket_pos);
            
            // Find each output object
            size_t search_pos = 0;
            while (true) {
                size_t name_pos = outputs_section.find("\"name\": \"", search_pos);
                if (name_pos == std::string::npos) break;
                
                OutputArtifact artifact;
                
                // Extract name
                name_pos += 9;  // length of "\"name\": \""
                size_t name_end = outputs_section.find("\"", name_pos);
                artifact.name = outputs_section.substr(name_pos, name_end - name_pos);
                
                // Extract filename
                size_t fname_pos = outputs_section.find("\"filename\": \"", name_end);
                if (fname_pos != std::string::npos) {
                    fname_pos += 13;
                    size_t fname_end = outputs_section.find("\"", fname_pos);
                    artifact.filename = outputs_section.substr(fname_pos, fname_end - fname_pos);
                }
                
                // Extract bytes
                size_t bytes_pos = outputs_section.find("\"bytes\": ", fname_pos);
                if (bytes_pos != std::string::npos) {
                    bytes_pos += 9;
                    size_t bytes_end = outputs_section.find_first_of(",\n}", bytes_pos);
                    std::string bytes_str = outputs_section.substr(bytes_pos, bytes_end - bytes_pos);
                    artifact.bytes = std::stoull(bytes_str);
                }
                
                meta.outputs.push_back(artifact);
                search_pos = name_end;
            }
        }

        // Reject clearly invalid/corrupt metadata.
        if (meta.schema_version <= 0 ||
            meta.op_hash.empty() ||
            meta.engine_version.empty() ||
            meta.kernel_version.empty() ||
            meta.outputs.empty()) {
            return false;
        }
        
        return true;
        
    } catch (...) {
        return false;
    }
}

bool OpCache::write_meta(const std::filesystem::path& dir, 
                        const OpCacheEntryMeta& meta) const {
    auto meta_file = dir / "meta.json";
    
    try {
        std::ofstream ofs(meta_file);
        if (!ofs) {
            return false;
        }
        
        ofs << meta.to_json();
        return true;
        
    } catch (...) {
        return false;
    }
}

// Get cache statistics (Sprint 6 Phase 4)
CacheStats OpCache::get_stats() const {
    CacheStats stats;
    stats.cache_root = cache_root_;
    stats.schema_version = 1;
    stats.entry_count = 0;
    stats.total_bytes = 0;
    
    if (!std::filesystem::exists(cache_root_)) {
        return stats;
    }
    
    try {
        // Iterate through shard directories (ab/, cd/, etc.)
        for (const auto& shard_entry : std::filesystem::directory_iterator(cache_root_)) {
            if (!shard_entry.is_directory()) continue;
            
            // Iterate through entries in each shard
            for (const auto& entry : std::filesystem::directory_iterator(shard_entry.path())) {
                if (!entry.is_directory()) continue;
                
                // Skip temp and lock directories
                std::string dirname = entry.path().filename().string();
                if (dirname.find(".tmp_") != std::string::npos ||
                    dirname.find(".lock") != std::string::npos) {
                    continue;
                }
                
                // Check if it's a valid cache entry (has meta.json)
                auto meta_file = entry.path() / "meta.json";
                if (!std::filesystem::exists(meta_file)) {
                    continue;
                }
                
                stats.entry_count++;
                
                // Sum up all file sizes in the entry
                for (const auto& file : std::filesystem::recursive_directory_iterator(entry.path())) {
                    if (file.is_regular_file()) {
                        stats.total_bytes += std::filesystem::file_size(file.path());
                    }
                }
            }
        }
    } catch (...) {
        // If iteration fails, return partial stats
    }
    
    return stats;
}

// Prune cache entries older than max_age_days (Sprint 6 Phase 4)
PruneStats OpCache::prune_by_age(int max_age_days) {
    PruneStats prune_stats;
    
    if (!std::filesystem::exists(cache_root_)) {
        return prune_stats;
    }
    
    auto now = std::filesystem::file_time_type::clock::now();
    auto cutoff_time = now - std::chrono::hours(24 * max_age_days);
    
    try {
        // Iterate through shard directories
        for (const auto& shard_entry : std::filesystem::directory_iterator(cache_root_)) {
            if (!shard_entry.is_directory()) continue;
            
            // Iterate through entries in each shard
            std::vector<std::filesystem::path> to_remove;
            
            for (const auto& entry : std::filesystem::directory_iterator(shard_entry.path())) {
                if (!entry.is_directory()) continue;
                
                // Skip temp and lock directories (handled by cleanup_temp_dirs)
                std::string dirname = entry.path().filename().string();
                if (dirname.find(".tmp_") != std::string::npos ||
                    dirname.find(".lock") != std::string::npos) {
                    continue;
                }
                
                // Check entry age based on mtime
                try {
                    auto mtime = std::filesystem::last_write_time(entry.path());
                    if (mtime < cutoff_time) {
                        to_remove.push_back(entry.path());
                    }
                } catch (...) {
                    // If we can't read mtime, skip this entry
                }
            }
            
            // Remove old entries
            for (const auto& path : to_remove) {
                try {
                    // Calculate size before removal
                    size_t entry_size = 0;
                    for (const auto& file : std::filesystem::recursive_directory_iterator(path)) {
                        if (file.is_regular_file()) {
                            entry_size += std::filesystem::file_size(file.path());
                        }
                    }
                    
                    // Remove entry
                    std::filesystem::remove_all(path);
                    
                    prune_stats.entries_removed++;
                    prune_stats.bytes_freed += entry_size;
                } catch (...) {
                    // If removal fails, continue with other entries
                }
            }
        }
    } catch (...) {
        // If iteration fails, return partial stats
    }
    
    return prune_stats;
}

// Cleanup abandoned temp and lock directories (Sprint 6 Phase 4)
CleanupStats OpCache::cleanup_temp_dirs(int max_age_hours) {
    CleanupStats cleanup_stats;
    
    if (!std::filesystem::exists(cache_root_)) {
        return cleanup_stats;
    }
    
    auto now = std::filesystem::file_time_type::clock::now();
    auto cutoff_time = now - std::chrono::hours(max_age_hours);
    
    try {
        // Iterate through shard directories
        for (const auto& shard_entry : std::filesystem::directory_iterator(cache_root_)) {
            if (!shard_entry.is_directory()) continue;
            
            std::vector<std::filesystem::path> to_remove;
            
            for (const auto& entry : std::filesystem::directory_iterator(shard_entry.path())) {
                if (!entry.is_directory()) continue;
                
                std::string dirname = entry.path().filename().string();
                bool is_temp = dirname.find(".tmp_") != std::string::npos;
                bool is_lock = dirname.find(".lock") != std::string::npos;
                
                if (!is_temp && !is_lock) {
                    continue;
                }
                
                // Check age
                try {
                    auto mtime = std::filesystem::last_write_time(entry.path());
                    if (mtime < cutoff_time) {
                        to_remove.push_back(entry.path());
                        
                        if (is_temp) {
                            cleanup_stats.temp_dirs_removed++;
                        } else {
                            cleanup_stats.lock_dirs_removed++;
                        }
                    }
                } catch (...) {
                    // If we can't read mtime, skip this entry
                }
            }
            
            // Remove old temp/lock directories
            for (const auto& path : to_remove) {
                try {
                    std::filesystem::remove_all(path);
                } catch (...) {
                    // If removal fails, continue with others
                }
            }
        }
    } catch (...) {
        // If iteration fails, return partial stats
    }
    
    return cleanup_stats;
}

// Compute SHA256 hash of file (Sprint 6 Phase 4)
std::string OpCache::compute_file_sha256(const std::filesystem::path& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        return "";
    }
    
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    
    char buffer[8192];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        SHA256_Update(&sha256, buffer, file.gcount());
    }
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256);
    
    // Convert to hex string
    std::ostringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    
    return ss.str();
}

} // namespace cache
} // namespace praxis
