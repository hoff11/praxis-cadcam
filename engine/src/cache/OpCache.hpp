#pragma once

#include <string>
#include <vector>
#include <map>
#include <filesystem>

namespace praxis {
namespace cache {

// Cache statistics (Sprint 6 Phase 4)
struct CacheStats {
    std::filesystem::path cache_root;
    int schema_version = 1;
    size_t entry_count = 0;
    size_t total_bytes = 0;
};

// Prune statistics (Sprint 6 Phase 4)
struct PruneStats {
    size_t entries_removed = 0;
    size_t bytes_freed = 0;
};

// Cleanup statistics (Sprint 6 Phase 4)
struct CleanupStats {
    size_t temp_dirs_removed = 0;
    size_t lock_dirs_removed = 0;
};

// Output artifact metadata (Sprint 6 Phase 3)
struct OutputArtifact {
    std::string name;           // "out"
    std::string filename;       // "out.step"
    size_t bytes = 0;          // File size
    std::string sha256;        // Optional: integrity check
    
    // Sprint 12 fields for preview discovery
    std::string type;          // "step", "stl", "brep", etc.
    bool previewable = false;  // Can this be previewed?
    std::string semantic_type; // "Body", "Face", "Edge", etc.
};

// Op-level cache entry metadata (Sprint 6 Phase 3)
struct OpCacheEntryMeta {
    int schema_version = 1;
    std::string op_hash;
    std::string engine_version;
    std::string kernel_version;
    std::string created_at;
    std::string producer = "praxis-engine";
    std::vector<OutputArtifact> outputs;
    
    // Serialize to JSON string
    std::string to_json() const;
    
    // Deserialize from JSON string
    static OpCacheEntryMeta from_json(const std::string& json);
};

// Op cache hit result (Sprint 6 Phase 3)
struct OpCacheHit {
    bool hit = false;
    std::string op_hash;
    std::filesystem::path entry_dir;
    std::map<std::string, std::filesystem::path> output_paths;  // "out" -> path/to/out.step
};

// Op cache store handle for atomic writes (Sprint 6 Phase 3)
struct OpCacheStore {
    std::string op_hash;
    std::filesystem::path staging_dir;  // .tmp_<pid> directory
    std::filesystem::path final_dir;    // target directory after commit
    bool active = false;
    
    // Get staging path for output artifact
    std::filesystem::path get_staging_path(const std::string& output_name, const std::string& extension) const;
};

// Op-level cache utilities (Sprint 6 Phase 3)
class OpCache {
public:
    // Initialize op cache with root directory
    // Default: <cache_root>/v1/sha256
    explicit OpCache(const std::filesystem::path& cache_root);
    
    // Check if op hash exists in cache
    bool has(const std::string& op_hash) const;
    
    // Try to load cached op result
    OpCacheHit try_load(const std::string& op_hash);
    
    // Begin atomic store (creates staging directory)
    OpCacheStore begin_store(const std::string& op_hash);
    
    // Commit store (atomic rename from staging to final)
    bool commit_store(OpCacheStore& store, const OpCacheEntryMeta& meta);
    
    // Abort store (cleanup staging directory)
    void abort_store(OpCacheStore& store);
    
    // Get cache root
    std::filesystem::path get_cache_root() const { return cache_root_; }
    
    // Get cache statistics (Sprint 6 Phase 4)
    CacheStats get_stats() const;
    
    // Prune cache entries older than max_age_days (Sprint 6 Phase 4)
    PruneStats prune_by_age(int max_age_days);
    
    // Cleanup abandoned temp and lock directories (Sprint 6 Phase 4)
    // Default: remove temp/lock dirs older than 24 hours
    CleanupStats cleanup_temp_dirs(int max_age_hours = 24);
    
    // Compute SHA256 hash of file (Sprint 6 Phase 4)
    static std::string compute_file_sha256(const std::filesystem::path& file_path);
    
    // Debug helper: Get entry directory path (for testing)
    std::filesystem::path debug_get_entry_dir(const std::string& op_hash) const {
        return get_entry_dir(op_hash);
    }
    
private:
    std::filesystem::path cache_root_;  // e.g., <root>/v1/sha256
    
    // Get entry directory path for op hash (sharded by first 2 chars)
    std::filesystem::path get_entry_dir(const std::string& op_hash) const;
    
    // Get staging directory path for op hash
    std::filesystem::path get_staging_dir(const std::string& op_hash) const;
    
    // Validate cache entry (meta.json + outputs exist)
    bool validate_entry(const std::filesystem::path& entry_dir, const OpCacheEntryMeta& meta) const;
    
    // Read meta.json from entry directory
    bool read_meta(const std::filesystem::path& entry_dir, OpCacheEntryMeta& meta) const;
    
    // Write meta.json to directory
    bool write_meta(const std::filesystem::path& dir, const OpCacheEntryMeta& meta) const;
};

} // namespace cache
} // namespace praxis
