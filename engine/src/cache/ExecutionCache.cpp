#include "ExecutionCache.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <iostream>
#include <random>  // for cross-platform unique temp dir naming

namespace praxis {
namespace cache {

ExecutionCache::ExecutionCache(const std::string& cache_root)
    : enabled_(true) {
    
    // Determine cache root directory
    if (!cache_root.empty()) {
        cache_root_ = cache_root;
    } else {
        // Check environment variable
        const char* env_cache = std::getenv("PRAXIS_CACHE_DIR");
        if (env_cache && env_cache[0] != '\0') {
            cache_root_ = env_cache;
        } else {
            // Default: ./cache relative to current working directory
            cache_root_ = std::filesystem::current_path() / "cache";
        }
    }
    
    // Ensure cache root exists
    try {
        std::filesystem::create_directories(cache_root_);
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Warning: Failed to create cache directory: " << e.what() << "\n";
        enabled_ = false;
    }
}

std::filesystem::path ExecutionCache::get_entry_dir(const std::string& plan_hash) const {
    return cache_root_ / plan_hash;
}

bool ExecutionCache::try_restore(
    const recipe::ResolvedPlan& plan,
    const std::string& output_dir,
    recipe::RecipeExecutionResult& result
) {
    // Initialize cache miss state
    result.cache.hit = false;
    result.cache.plan_hash = plan.plan_hash;
    result.cache.reason = "";
    result.cache.reused_artifacts.clear();
    result.cache.cache_entry_age_seconds = 0;
    
    // Cache disabled if hash invalid
    if (!plan.hash_valid || plan.plan_hash.empty()) {
        result.cache.reason = "Plan hash invalid or empty";
        return false;
    }
    
    if (!enabled_) {
        result.cache.reason = "Cache disabled";
        return false;
    }
    
    auto entry_dir = get_entry_dir(plan.plan_hash);
    
    // Check if entry exists and is valid
    if (!validate_entry(entry_dir, plan)) {
        result.cache.reason = "Cache entry missing or invalid";
        return false;
    }
    
    // Read meta.json
    CacheEntryMeta meta;
    if (!read_meta(entry_dir, meta)) {
        result.cache.reason = "Failed to read cache metadata";
        return false;
    }
    
    // Restore artifacts to output_dir
    std::filesystem::create_directories(output_dir);
    
    std::vector<std::string> restored_artifacts;
    for (const auto& [type, rel_path] : meta.artifacts) {
        auto cached_file = entry_dir / rel_path;
        auto dest_file = std::filesystem::path(output_dir) / std::filesystem::path(rel_path).filename();
        
        if (!restore_artifact(cached_file, dest_file)) {
            std::cerr << "Warning: Failed to restore artifact: " << rel_path << "\n";
            return false;  // Partial restore is failure
        }
        
        restored_artifacts.push_back(dest_file.string());
        
        // Populate result artifact paths
        if (type == "step") {
            result.step_file_path = dest_file.string();
        } else if (type == "pkm") {
            result.pkm_file_path = dest_file.string();
        } else if (type == "bindings") {
            result.bindings_file_path = dest_file.string();
        }
    }
    
    // Load cached report if it exists to get kernel_ops and metrics
    auto cached_report = entry_dir / "report.json";
    if (std::filesystem::exists(cached_report)) {
        // TODO: Parse cached report and restore kernel_ops/metrics if needed
        // For Phase 4, we mark kernel_ops as empty (nothing executed)
    }
    
    // Mark result as cache hit (no kernel ops executed)
    result.success = true;
    result.report_version = "1.1";
    result.kernel_ops.clear();  // Critical: no kernel execution happened
    result.warnings.clear();
    
    // Populate structured cache info
    result.cache.hit = true;
    result.cache.plan_hash = plan.plan_hash;
    result.cache.reason = "";  // Empty = successful hit
    result.cache.reused_artifacts = restored_artifacts;
    
    // Calculate cache entry age using stored unix timestamp (portable)
    if (meta.created_unix_seconds > 0) {
        auto now = std::chrono::system_clock::now();
        auto now_unix = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()
        ).count();
        result.cache.cache_entry_age_seconds = now_unix - meta.created_unix_seconds;
    } else {
        result.cache.cache_entry_age_seconds = 0;  // Fallback for old cache entries
    }
    
    // Legacy metrics for backward compatibility
    result.metrics["cache.hit"] = "true";
    result.metrics["plan_hash"] = plan.plan_hash;
    
    std::cout << "✓ Cache hit: " << plan.plan_hash << "\n";
    std::cout << "  Reused " << restored_artifacts.size() << " artifacts\n";
    
    return true;
}

bool ExecutionCache::store(
    const recipe::ResolvedPlan& plan,
    const recipe::RecipeExecutionResult& result,
    const std::string& output_dir
) {
    // Don't cache if hash invalid or execution failed
    if (!plan.hash_valid || plan.plan_hash.empty() || !result.success) {
        return false;
    }
    
    if (!enabled_) {
        return false;
    }
    
    auto entry_dir = get_entry_dir(plan.plan_hash);
    
    // If entry already exists, skip (shouldn't happen, but safe)
    if (std::filesystem::exists(entry_dir)) {
        return true;
    }
    
    // Create temporary directory for atomic write (cross-platform)
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::random_device rd;
    auto temp_dir = cache_root_ / (plan.plan_hash + ".tmp." + std::to_string(now) + "." + std::to_string(rd()));
    
    try {
        std::filesystem::create_directories(temp_dir);
        std::filesystem::create_directories(temp_dir / "artifacts");
        
        // Copy artifacts to cache
        CacheEntryMeta meta;
        meta.plan_hash = plan.plan_hash;
        meta.engine_version = plan.engine_version;
        meta.kernel_version = plan.kernel_version;
        meta.created_utc = get_utc_timestamp();
        
        // Store unix timestamp for portable age calculation
        auto now_sys = std::chrono::system_clock::now();
        meta.created_unix_seconds = std::chrono::duration_cast<std::chrono::seconds>(
            now_sys.time_since_epoch()
        ).count();
        
        // Copy STEP file if it exists
        if (!result.step_file_path.empty() && std::filesystem::exists(result.step_file_path)) {
            auto dest = temp_dir / "artifacts" / std::filesystem::path(result.step_file_path).filename();
            std::filesystem::copy_file(result.step_file_path, dest, 
                std::filesystem::copy_options::overwrite_existing);
            meta.artifacts["step"] = "artifacts/" + dest.filename().string();
        }
        
        // Copy PKM file if it exists
        if (!result.pkm_file_path.empty() && std::filesystem::exists(result.pkm_file_path)) {
            auto dest = temp_dir / "artifacts" / std::filesystem::path(result.pkm_file_path).filename();
            std::filesystem::copy_file(result.pkm_file_path, dest,
                std::filesystem::copy_options::overwrite_existing);
            meta.artifacts["pkm"] = "artifacts/" + dest.filename().string();
        }
        
        // Copy bindings file if it exists
        if (!result.bindings_file_path.empty() && std::filesystem::exists(result.bindings_file_path)) {
            auto dest = temp_dir / "artifacts" / std::filesystem::path(result.bindings_file_path).filename();
            std::filesystem::copy_file(result.bindings_file_path, dest,
                std::filesystem::copy_options::overwrite_existing);
            meta.artifacts["bindings"] = "artifacts/" + dest.filename().string();
        }
        
        // Write resolved_plan.json
        auto resolved_plan_file = temp_dir / "resolved_plan.json";
        std::ofstream plan_out(resolved_plan_file);
        if (plan_out) {
            plan_out << plan.to_canonical_json();
            plan_out.close();
            meta.resolved_plan_path = "resolved_plan.json";
        }
        
        // Write meta.json
        if (!write_meta(temp_dir, meta)) {
            std::filesystem::remove_all(temp_dir);
            return false;
        }
        
        // Validate: at least STEP artifact must exist before committing
        if (meta.artifacts.find("step") == meta.artifacts.end() || 
            !std::filesystem::exists(temp_dir / meta.artifacts.at("step"))) {
            std::cerr << "[ExecutionCache] Warning: No valid STEP artifact, not storing cache entry\n";
            std::filesystem::remove_all(temp_dir);
            return false;
        }
        
        // Atomic rename temp → final
        std::filesystem::rename(temp_dir, entry_dir);
        
        std::cout << "✓ Cached: " << plan.plan_hash << "\n";
        return true;
        
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Warning: Failed to store cache entry: " << e.what() << "\n";
        // Clean up temp directory
        try {
            std::filesystem::remove_all(temp_dir);
        } catch (...) {}
        return false;
    }
}

bool ExecutionCache::validate_entry(
    const std::filesystem::path& entry_dir,
    const recipe::ResolvedPlan& plan
) const {
    // Entry directory must exist
    if (!std::filesystem::exists(entry_dir)) {
        return false;
    }
    
    // meta.json must exist
    auto meta_file = entry_dir / "meta.json";
    if (!std::filesystem::exists(meta_file)) {
        return false;
    }
    
    // Read and validate meta
    CacheEntryMeta meta;
    if (!read_meta(entry_dir, meta)) {
        return false;
    }
    
    // plan_hash must match exactly (contract)
    if (meta.plan_hash != plan.plan_hash) {
        return false;
    }
    
    // Version mismatch is warning-only (hash is authoritative)
    if (meta.engine_version != plan.engine_version) {
        std::cout << "⚠ Cache entry engine version mismatch: "
                  << meta.engine_version << " vs " << plan.engine_version << "\n";
    }
    
    if (meta.kernel_version != plan.kernel_version) {
        std::cout << "⚠ Cache entry kernel version mismatch: "
                  << meta.kernel_version << " vs " << plan.kernel_version << "\n";
    }
    
    // STEP artifact must exist (consistency with store validation)
    auto step_it = meta.artifacts.find("step");
    if (step_it == meta.artifacts.end()) {
        return false;  // No STEP artifact recorded
    }
    
    if (!std::filesystem::exists(entry_dir / step_it->second)) {
        return false;  // STEP file missing
    }
    
    return true;
}

bool ExecutionCache::restore_artifact(
    const std::filesystem::path& cached_file,
    const std::filesystem::path& dest_file
) {
    if (!std::filesystem::exists(cached_file)) {
        return false;
    }
    
    // Ensure destination directory exists
    std::filesystem::create_directories(dest_file.parent_path());
    
    // Try hardlink first (fast, no disk usage)
    try {
        std::filesystem::create_hard_link(cached_file, dest_file);
        return true;
    } catch (const std::filesystem::filesystem_error&) {
        // Hardlink failed (cross-filesystem or permissions), fall back to copy
    }
    
    // Fallback: copy
    try {
        std::filesystem::copy_file(cached_file, dest_file,
            std::filesystem::copy_options::overwrite_existing);
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Failed to copy artifact: " << e.what() << "\n";
        return false;
    }
}

bool ExecutionCache::write_meta(
    const std::filesystem::path& entry_dir,
    const CacheEntryMeta& meta
) {
    auto meta_file = entry_dir / "meta.json";
    std::ofstream file(meta_file);
    
    if (!file) {
        return false;
    }
    
    file << "{\n";
    file << "  \"plan_hash\": \"" << meta.plan_hash << "\",\n";
    file << "  \"engine_version\": \"" << meta.engine_version << "\",\n";
    file << "  \"kernel_version\": \"" << meta.kernel_version << "\",\n";
    file << "  \"created_utc\": \"" << meta.created_utc << "\",\n";
    file << "  \"created_unix_seconds\": " << meta.created_unix_seconds << ",\n";
    file << "  \"resolved_plan\": \"" << meta.resolved_plan_path << "\",\n";
    file << "  \"artifacts\": {\n";
    
    bool first = true;
    for (const auto& [type, path] : meta.artifacts) {
        if (!first) file << ",\n";
        file << "    \"" << type << "\": \"" << path << "\"";
        first = false;
    }
    
    file << "\n  }\n";
    file << "}\n";
    
    file.close();
    return true;
}

bool ExecutionCache::read_meta(
    const std::filesystem::path& entry_dir,
    CacheEntryMeta& meta
) const {
    auto meta_file = entry_dir / "meta.json";
    std::ifstream file(meta_file);
    
    if (!file) {
        return false;
    }
    
    // Simple JSON parsing (for Phase 4, just extract key fields)
    // In production, use proper JSON library
    std::string line;
    while (std::getline(file, line)) {
        // Extract plan_hash
        auto hash_pos = line.find("\"plan_hash\":");
        if (hash_pos != std::string::npos) {
            auto start = line.find("\"", hash_pos + 12);
            auto end = line.find("\"", start + 1);
            if (start != std::string::npos && end != std::string::npos) {
                meta.plan_hash = line.substr(start + 1, end - start - 1);
            }
        }
        
        // Extract engine_version
        auto engine_pos = line.find("\"engine_version\":");
        if (engine_pos != std::string::npos) {
            auto start = line.find("\"", engine_pos + 17);
            auto end = line.find("\"", start + 1);
            if (start != std::string::npos && end != std::string::npos) {
                meta.engine_version = line.substr(start + 1, end - start - 1);
            }
        }
        
        // Extract kernel_version
        auto kernel_pos = line.find("\"kernel_version\":");
        if (kernel_pos != std::string::npos) {
            auto start = line.find("\"", kernel_pos + 17);
            auto end = line.find("\"", start + 1);
            if (start != std::string::npos && end != std::string::npos) {
                meta.kernel_version = line.substr(start + 1, end - start - 1);
            }
        }
        
        // Extract created_unix_seconds
        auto unix_pos = line.find("\"created_unix_seconds\":");
        if (unix_pos != std::string::npos) {
            auto start = line.find_first_of("0123456789-", unix_pos + 23);
            if (start != std::string::npos) {
                auto end = line.find_first_not_of("0123456789", start);
                if (end != std::string::npos) {
                    meta.created_unix_seconds = std::stoll(line.substr(start, end - start));
                } else {
                    meta.created_unix_seconds = std::stoll(line.substr(start));
                }
            }
        }
        
        // Extract artifact paths (simplified)
        auto step_pos = line.find("\"step\":");
        if (step_pos != std::string::npos) {
            auto start = line.find("\"", step_pos + 7);
            auto end = line.find("\"", start + 1);
            if (start != std::string::npos && end != std::string::npos) {
                meta.artifacts["step"] = line.substr(start + 1, end - start - 1);
            }
        }
        
        auto pkm_pos = line.find("\"pkm\":");
        if (pkm_pos != std::string::npos) {
            auto start = line.find("\"", pkm_pos + 6);
            auto end = line.find("\"", start + 1);
            if (start != std::string::npos && end != std::string::npos) {
                meta.artifacts["pkm"] = line.substr(start + 1, end - start - 1);
            }
        }
        
        auto bindings_pos = line.find("\"bindings\":");
        if (bindings_pos != std::string::npos) {
            auto start = line.find("\"", bindings_pos + 11);
            auto end = line.find("\"", start + 1);
            if (start != std::string::npos && end != std::string::npos) {
                meta.artifacts["bindings"] = line.substr(start + 1, end - start - 1);
            }
        }
    }
    
    file.close();
    return !meta.plan_hash.empty();
}

std::string ExecutionCache::get_utc_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace cache
} // namespace praxis
