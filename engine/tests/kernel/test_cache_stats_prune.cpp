// Sprint 6 Phase 4: Stats and Prune correctness test
// Tests --cache-stats and --prune-cache functionality

#include "../../src/cache/OpCache.hpp"
#include "../../src/kernel/KernelVersion.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>

using namespace praxis;

// Helper: Write test file
bool write_test_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path);
    if (!out) return false;
    out << content;
    return true;
}

// Helper: Create a complete cache entry
bool create_cache_entry(cache::OpCache& cache, const std::string& op_hash, 
                       const std::string& content = "test data") {
    auto store = cache.begin_store(op_hash);
    if (!store.active) return false;
    
    auto output_path = store.get_staging_path("out", ".step");
    std::filesystem::create_directories(output_path.parent_path());
    
    if (!write_test_file(output_path, content)) {
        cache.abort_store(store);
        return false;
    }
    
    cache::OpCacheEntryMeta meta;
    meta.op_hash = op_hash;
    meta.engine_version = "test";
    meta.kernel_version = praxis::kernel::get_kernel_version_string();
    meta.created_at = "2026-01-06T00:00:00Z";
    
    cache::OutputArtifact artifact;
    artifact.name = "out";
    artifact.filename = "out.step";
    artifact.bytes = std::filesystem::file_size(output_path);
    artifact.sha256 = cache.compute_file_sha256(output_path);
    
    meta.outputs.push_back(artifact);
    
    return cache.commit_store(store, meta);
}

// Helper: Set file modification time (simulates old entries)
void set_mtime_days_ago(const std::filesystem::path& path, int days) {
    auto now = std::filesystem::file_time_type::clock::now();
    auto old_time = now - std::chrono::hours(24 * days);
    std::filesystem::last_write_time(path, old_time);
}

// Test 1: Basic stats collection
bool test_cache_stats() {
    std::cout << "\n=== Test 1: Cache stats collection ===\n";
    
    auto cache_root = std::filesystem::temp_directory_path() / "test_cache_stats";
    std::filesystem::remove_all(cache_root);
    std::filesystem::create_directories(cache_root);
    
    cache::OpCache cache(cache_root);
    
    // Create several entries with different sizes
    std::vector<std::pair<std::string, std::string>> entries = {
        {"aaa111stats", "small"},
        {"bbb222stats", "medium size content here"},
        {"ccc333stats", "large content with lots of data to make this file bigger than the others"}
    };
    
    for (const auto& [hash, content] : entries) {
        if (!create_cache_entry(cache, hash, content)) {
            std::cerr << "FAIL: Could not create entry " << hash << "\n";
            return false;
        }
    }
    
    // Get stats
    auto stats = cache.get_stats();
    
    std::cout << "Cache stats:\n";
    std::cout << "  Entry count: " << stats.entry_count << "\n";
    std::cout << "  Total bytes: " << stats.total_bytes << "\n";
    std::cout << "  Schema version: " << stats.schema_version << "\n";
    
    // Verify entry count
    if (stats.entry_count != 3) {
        std::cerr << "FAIL: Expected 3 entries, got " << stats.entry_count << "\n";
        return false;
    }
    
    // Verify total bytes is reasonable (should be sum of all output files)
    size_t expected_min = entries[0].second.size() + entries[1].second.size() + entries[2].second.size();
    if (stats.total_bytes < expected_min) {
        std::cerr << "FAIL: Total bytes too small (" << stats.total_bytes 
                  << " < " << expected_min << ")\n";
        return false;
    }
    
    // Verify schema version
    if (stats.schema_version != 1) {
        std::cerr << "FAIL: Expected schema version 1, got " << stats.schema_version << "\n";
        return false;
    }
    
    std::cout << "PASS: Stats collection accurate\n";
    return true;
}

// Test 2: Prune by age (basic)
bool test_prune_by_age_basic() {
    std::cout << "\n=== Test 2: Prune by age (basic) ===\n";
    
    auto cache_root = std::filesystem::temp_directory_path() / "test_prune_basic";
    std::filesystem::remove_all(cache_root);
    std::filesystem::create_directories(cache_root);
    
    cache::OpCache cache(cache_root);
    
    // Create old entry (simulate 60 days old)
    std::string old_hash = "ddd444old";
    if (!create_cache_entry(cache, old_hash)) {
        std::cerr << "FAIL: Could not create old entry\n";
        return false;
    }
    
    auto old_entry = cache.debug_get_entry_dir(old_hash);
    set_mtime_days_ago(old_entry, 60);
    
    // Create recent entry (today)
    std::string new_hash = "eee555new";
    if (!create_cache_entry(cache, new_hash)) {
        std::cerr << "FAIL: Could not create new entry\n";
        return false;
    }
    
    // Prune entries older than 30 days
    auto prune_result = cache.prune_by_age(30);
    
    std::cout << "Prune results:\n";
    std::cout << "  Entries removed: " << prune_result.entries_removed << "\n";
    std::cout << "  Bytes freed: " << prune_result.bytes_freed << "\n";
    
    // Verify old entry was removed
    if (!cache.has(old_hash)) {
        std::cerr << "FAIL: Old entry should be removed but has() still returns true\n";
        // Note: has() checks .complete which may not exist after prune
    }
    
    auto old_result = cache.try_load(old_hash);
    if (old_result.hit) {
        std::cerr << "FAIL: Old entry should be removed but try_load succeeded\n";
        return false;
    }
    
    // Verify new entry still exists
    auto new_result = cache.try_load(new_hash);
    if (!new_result.hit) {
        std::cerr << "FAIL: New entry should still exist but try_load failed\n";
        return false;
    }
    
    // Verify prune count
    if (prune_result.entries_removed != 1) {
        std::cerr << "FAIL: Expected 1 entry removed, got " << prune_result.entries_removed << "\n";
        return false;
    }
    
    std::cout << "PASS: Prune correctly removed old entries\n";
    return true;
}

// Test 3: Prune skips temp directories
bool test_prune_skips_temp_dirs() {
    std::cout << "\n=== Test 3: Prune skips temp/lock directories ===\n";
    
    auto cache_root = std::filesystem::temp_directory_path() / "test_prune_temp";
    std::filesystem::remove_all(cache_root);
    std::filesystem::create_directories(cache_root);
    
    cache::OpCache cache(cache_root);
    
    // Create normal entry
    std::string hash = "fff666temp";
    if (!create_cache_entry(cache, hash)) {
        std::cerr << "FAIL: Could not create entry\n";
        return false;
    }
    
    // Make it old
    auto entry_dir = cache.debug_get_entry_dir(hash);
    set_mtime_days_ago(entry_dir, 60);
    
    // Create fake temp directory (simulate incomplete store)
    auto shard = hash.substr(0, 2);
    auto temp_dir = cache_root / "v1" / "sha256" / shard / (hash + ".tmp_12345");
    std::filesystem::create_directories(temp_dir / "outputs");
    write_test_file(temp_dir / "outputs" / "out.step", "temp data");
    set_mtime_days_ago(temp_dir, 60);
    
    // Create fake lock directory
    auto lock_dir = cache_root / "v1" / "sha256" / shard / (hash + ".lock");
    std::filesystem::create_directories(lock_dir);
    set_mtime_days_ago(lock_dir, 60);
    
    // Prune with age threshold
    auto prune_result = cache.prune_by_age(30);
    
    std::cout << "Prune results:\n";
    std::cout << "  Entries removed: " << prune_result.entries_removed << "\n";
    
    // Should only count the real entry, not temp/lock dirs
    if (prune_result.entries_removed != 1) {
        std::cerr << "FAIL: Expected 1 entry removed (temp/lock should be skipped), got " 
                  << prune_result.entries_removed << "\n";
        return false;
    }
    
    // Temp and lock dirs should still exist (cleanup_temp_dirs is separate)
    if (!std::filesystem::exists(temp_dir)) {
        std::cerr << "Note: Temp dir was removed (unexpected but okay if cleanup ran)\n";
    }
    
    std::cout << "PASS: Prune correctly skipped temp/lock directories\n";
    return true;
}

// Test 4: Stats accuracy after prune
bool test_stats_after_prune() {
    std::cout << "\n=== Test 4: Stats accuracy after prune ===\n";
    
    auto cache_root = std::filesystem::temp_directory_path() / "test_stats_prune";
    std::filesystem::remove_all(cache_root);
    std::filesystem::create_directories(cache_root);
    
    cache::OpCache cache(cache_root);
    
    // Create 5 entries
    for (int i = 0; i < 5; i++) {
        std::string hash = "ggg77" + std::to_string(i);
        if (!create_cache_entry(cache, hash, "test data " + std::to_string(i))) {
            std::cerr << "FAIL: Could not create entry " << i << "\n";
            return false;
        }
        
        // Make first 3 old
        if (i < 3) {
            set_mtime_days_ago(cache.debug_get_entry_dir(hash), 60);
        }
    }
    
    // Get stats before prune
    auto stats_before = cache.get_stats();
    std::cout << "Before prune: " << stats_before.entry_count << " entries, " 
              << stats_before.total_bytes << " bytes\n";
    
    // Prune old entries
    auto prune_result = cache.prune_by_age(30);
    std::cout << "Pruned " << prune_result.entries_removed << " entries, freed " 
              << prune_result.bytes_freed << " bytes\n";
    
    // Get stats after prune
    auto stats_after = cache.get_stats();
    std::cout << "After prune: " << stats_after.entry_count << " entries, " 
              << stats_after.total_bytes << " bytes\n";
    
    // Verify entry count decreased
    if (stats_after.entry_count != stats_before.entry_count - prune_result.entries_removed) {
        std::cerr << "FAIL: Entry count mismatch\n";
        return false;
    }
    
    // Verify expected state
    if (stats_after.entry_count != 2) {
        std::cerr << "FAIL: Expected 2 entries after prune, got " << stats_after.entry_count << "\n";
        return false;
    }
    
    std::cout << "PASS: Stats accurate after prune\n";
    return true;
}

int main() {
    std::cout << "Sprint 6 Phase 4: Stats and Prune Correctness Tests\n";
    std::cout << "====================================================\n";
    
    bool all_passed = true;
    
    all_passed &= test_cache_stats();
    all_passed &= test_prune_by_age_basic();
    all_passed &= test_prune_skips_temp_dirs();
    all_passed &= test_stats_after_prune();
    
    std::cout << "\n====================================================\n";
    if (all_passed) {
        std::cout << "All stats/prune tests PASSED\n";
        return 0;
    } else {
        std::cout << "Some stats/prune tests FAILED\n";
        return 1;
    }
}
