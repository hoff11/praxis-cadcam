#pragma once

#include "../recipe/RecipeTypes.hpp"
#include "../recipe/RecipeExecutor.hpp"
#include <string>
#include <filesystem>

namespace praxis {
namespace cache {

// Execution cache entry metadata (Sprint 5 Phase 4)
struct CacheEntryMeta {
    std::string plan_hash;
    std::string engine_version;
    std::string kernel_version;
    std::string created_utc;
    int64_t created_unix_seconds = 0;  // Unix timestamp for portable age calculation
    std::map<std::string, std::string> artifacts;  // artifact type -> relative path
    std::string resolved_plan_path;  // Path to resolved_plan.json
};

// Execution cache for plan_hash-keyed artifact reuse (Sprint 5 Phase 4)
class ExecutionCache {
public:
    // Initialize cache with root directory
    // Default: <workspace>/cache or env PRAXIS_CACHE_DIR
    explicit ExecutionCache(const std::string& cache_root = "");
    
    // Try to restore execution result from cache
    // Returns true if cache hit, populates result and copies artifacts to output_dir
    // Returns false if cache miss or disabled
    bool try_restore(
        const recipe::ResolvedPlan& plan,
        const std::string& output_dir,
        recipe::RecipeExecutionResult& result
    );
    
    // Store execution result in cache (atomic operation)
    // Only called on cache miss after successful execution
    // Returns true if stored successfully, false on error (non-fatal)
    bool store(
        const recipe::ResolvedPlan& plan,
        const recipe::RecipeExecutionResult& result,
        const std::string& output_dir
    );
    
    // Get cache root directory
    std::filesystem::path get_cache_root() const { return cache_root_; }
    
    // Check if cache is enabled
    bool is_enabled() const { return enabled_; }
    
private:
    std::filesystem::path cache_root_;
    bool enabled_;
    
    // Get entry directory for a plan hash
    std::filesystem::path get_entry_dir(const std::string& plan_hash) const;
    
    // Validate cache entry (check meta.json and artifacts exist)
    bool validate_entry(const std::filesystem::path& entry_dir, const recipe::ResolvedPlan& plan) const;
    
    // Copy/hardlink artifact from cache to output directory
    // Tries hardlink first, falls back to copy
    bool restore_artifact(const std::filesystem::path& cached_file, const std::filesystem::path& dest_file);
    
    // Write meta.json for cache entry
    bool write_meta(const std::filesystem::path& entry_dir, const CacheEntryMeta& meta);
    
    // Read meta.json from cache entry
    bool read_meta(const std::filesystem::path& entry_dir, CacheEntryMeta& meta) const;
    
    // Get current UTC timestamp as ISO 8601 string
    static std::string get_utc_timestamp();
};

} // namespace cache
} // namespace praxis
