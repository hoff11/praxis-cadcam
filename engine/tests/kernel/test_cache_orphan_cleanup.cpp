// Sprint 6 Phase 4: Orphan cleanup test
// Tests cleanup_temp_dirs() removes stale .tmp_* and .lock directories

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

// Helper: Set directory modification time
void set_mtime_days_ago(const std::filesystem::path& path, int days) {
    auto now = std::filesystem::file_time_type::clock::now();
    auto old_time = now - std::chrono::hours(24 * days);
    
    // Touch all files in directory to update mtime
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
        if (std::filesystem::is_regular_file(entry)) {
            std::filesystem::last_write_time(entry.path(), old_time);
        }
    }
    
    // Update directory itself
    std::filesystem::last_write_time(path, old_time);
}

// Test 1: Cleanup old temp directories
bool test_cleanup_old_temp_dirs() {
    std::cout << "\n=== Test 1: Cleanup old temp directories ===\n";
    
    auto cache_root = std::filesystem::temp_directory_path() / "test_cleanup_temp";
    std::filesystem::remove_all(cache_root);
    std::filesystem::create_directories(cache_root);
    
    // Create cache structure
    auto shard_dir = cache_root / "v1" / "sha256" / "ab";
    std::filesystem::create_directories(shard_dir);
    
    // Create old temp directory (simulating crashed/abandoned store)
    auto old_temp = shard_dir / "abc123hash.tmp_12345";
    std::filesystem::create_directories(old_temp / "outputs");
    write_test_file(old_temp / "outputs" / "out.step", "abandoned data");
    set_mtime_days_ago(old_temp, 2);  // 2 days old
    
    // Create recent temp directory (active store)
    auto recent_temp = shard_dir / "def456hash.tmp_67890";
    std::filesystem::create_directories(recent_temp / "outputs");
    write_test_file(recent_temp / "outputs" / "out.step", "active store");
    // Don't set old mtime - should be current
    
    std::cout << "Created temp directories:\n";
    std::cout << "  Old (2 days): " << old_temp.filename() << "\n";
    std::cout << "  Recent: " << recent_temp.filename() << "\n";
    
    // Run cleanup
    cache::OpCache cache(cache_root);
    auto cleanup_result = cache.cleanup_temp_dirs();
    
    std::cout << "\nCleanup results:\n";
    std::cout << "  Temp dirs removed: " << cleanup_result.temp_dirs_removed << "\n";
    std::cout << "  Lock dirs removed: " << cleanup_result.lock_dirs_removed << "\n";
    
    // Verify old temp was removed
    if (std::filesystem::exists(old_temp)) {
        std::cerr << "FAIL: Old temp directory still exists\n";
        return false;
    }
    
    // Verify recent temp still exists
    if (!std::filesystem::exists(recent_temp)) {
        std::cerr << "FAIL: Recent temp directory was removed (should be kept)\n";
        return false;
    }
    
    // Verify cleanup count
    if (cleanup_result.temp_dirs_removed != 1) {
        std::cerr << "FAIL: Expected 1 temp dir removed, got " << cleanup_result.temp_dirs_removed << "\n";
        return false;
    }
    
    std::cout << "PASS: Old temp removed, recent temp preserved\n";
    return true;
}

// Test 2: Cleanup old lock directories
bool test_cleanup_old_lock_dirs() {
    std::cout << "\n=== Test 2: Cleanup old lock directories ===\n";
    
    auto cache_root = std::filesystem::temp_directory_path() / "test_cleanup_lock";
    std::filesystem::remove_all(cache_root);
    std::filesystem::create_directories(cache_root);
    
    auto shard_dir = cache_root / "v1" / "sha256" / "cd";
    std::filesystem::create_directories(shard_dir);
    
    // Create old lock directory (stale lock from crashed process)
    auto old_lock = shard_dir / "cde789hash.lock";
    std::filesystem::create_directories(old_lock);
    set_mtime_days_ago(old_lock, 1);  // 1 day old (way past 10s timeout)
    
    // Create recent lock directory (active lock)
    auto recent_lock = shard_dir / "fgh012hash.lock";
    std::filesystem::create_directories(recent_lock);
    // Current time - should be kept
    
    std::cout << "Created lock directories:\n";
    std::cout << "  Old (1 day): " << old_lock.filename() << "\n";
    std::cout << "  Recent: " << recent_lock.filename() << "\n";
    
    // Run cleanup
    cache::OpCache cache(cache_root);
    auto cleanup_result = cache.cleanup_temp_dirs();
    
    std::cout << "\nCleanup results:\n";
    std::cout << "  Lock dirs removed: " << cleanup_result.lock_dirs_removed << "\n";
    
    // Verify old lock was removed
    if (std::filesystem::exists(old_lock)) {
        std::cerr << "FAIL: Old lock directory still exists\n";
        return false;
    }
    
    // Verify recent lock still exists
    if (!std::filesystem::exists(recent_lock)) {
        std::cerr << "FAIL: Recent lock directory was removed (should be kept)\n";
        return false;
    }
    
    // Verify cleanup count
    if (cleanup_result.lock_dirs_removed != 1) {
        std::cerr << "FAIL: Expected 1 lock dir removed, got " << cleanup_result.lock_dirs_removed << "\n";
        return false;
    }
    
    std::cout << "PASS: Old lock removed, recent lock preserved\n";
    return true;
}

// Test 3: Cleanup preserves valid cache entries
bool test_cleanup_preserves_entries() {
    std::cout << "\n=== Test 3: Cleanup preserves valid cache entries ===\n";
    
    auto cache_root = std::filesystem::temp_directory_path() / "test_cleanup_preserve";
    std::filesystem::remove_all(cache_root);
    std::filesystem::create_directories(cache_root);
    
    cache::OpCache cache(cache_root);
    
    // Create a valid cache entry
    std::string op_hash = "ijk345valid";
    auto store = cache.begin_store(op_hash);
    if (!store.active) {
        std::cerr << "FAIL: Could not begin store\n";
        return false;
    }
    
    auto output_path = store.get_staging_path("out", ".step");
    std::filesystem::create_directories(output_path.parent_path());
    write_test_file(output_path, "valid entry data");
    
    cache::OpCacheEntryMeta meta;
    meta.op_hash = op_hash;
    meta.engine_version = "test";
    meta.kernel_version = kernel::get_kernel_version_string();
    meta.created_at = "2026-01-06T00:00:00Z";
    
    cache::OutputArtifact artifact;
    artifact.name = "out";
    artifact.filename = "out.step";
    artifact.bytes = std::filesystem::file_size(output_path);
    artifact.sha256 = cache.compute_file_sha256(output_path);
    
    meta.outputs.push_back(artifact);
    
    if (!cache.commit_store(store, meta)) {
        std::cerr << "FAIL: Could not commit store\n";
        return false;
    }
    
    // Make entry old
    auto entry_dir = cache.debug_get_entry_dir(op_hash);
    set_mtime_days_ago(entry_dir, 10);
    
    // Create old temp/lock dirs nearby
    auto shard = op_hash.substr(0, 2);
    auto temp_dir = cache_root / "v1" / "sha256" / shard / "orphan123.tmp_99999";
    auto lock_dir = cache_root / "v1" / "sha256" / shard / "orphan456.lock";
    
    std::filesystem::create_directories(temp_dir / "outputs");
    std::filesystem::create_directories(lock_dir);
    set_mtime_days_ago(temp_dir, 2);
    set_mtime_days_ago(lock_dir, 2);
    
    // Run cleanup
    auto cleanup_result = cache.cleanup_temp_dirs();
    
    std::cout << "Cleanup removed: " << cleanup_result.temp_dirs_removed 
              << " temp dirs, " << cleanup_result.lock_dirs_removed << " lock dirs\n";
    
    // Verify valid entry still exists
    auto load_result = cache.try_load(op_hash);
    if (!load_result.hit) {
        std::cerr << "FAIL: Valid entry was affected by cleanup\n";
        return false;
    }
    
    // Verify orphans were removed
    if (std::filesystem::exists(temp_dir)) {
        std::cerr << "FAIL: Orphan temp dir still exists\n";
        return false;
    }
    
    if (std::filesystem::exists(lock_dir)) {
        std::cerr << "FAIL: Orphan lock dir still exists\n";
        return false;
    }
    
    std::cout << "PASS: Valid entries preserved, orphans removed\n";
    return true;
}

// Test 4: Cleanup handles empty directories
bool test_cleanup_empty_dirs() {
    std::cout << "\n=== Test 4: Cleanup handles empty orphan directories ===\n";
    
    auto cache_root = std::filesystem::temp_directory_path() / "test_cleanup_empty";
    std::filesystem::remove_all(cache_root);
    std::filesystem::create_directories(cache_root);
    
    auto shard_dir = cache_root / "v1" / "sha256" / "ef";
    std::filesystem::create_directories(shard_dir);
    
    // Create empty old temp directory
    auto empty_temp = shard_dir / "lmn678empty.tmp_11111";
    std::filesystem::create_directories(empty_temp);
    set_mtime_days_ago(empty_temp, 5);
    
    // Create empty old lock directory
    auto empty_lock = shard_dir / "opq901empty.lock";
    std::filesystem::create_directories(empty_lock);
    set_mtime_days_ago(empty_lock, 5);
    
    std::cout << "Created empty orphan directories\n";
    
    // Run cleanup
    cache::OpCache cache(cache_root);
    auto cleanup_result = cache.cleanup_temp_dirs();
    
    std::cout << "Cleanup removed: " << cleanup_result.temp_dirs_removed 
              << " temp dirs, " << cleanup_result.lock_dirs_removed << " lock dirs\n";
    
    // Verify both were removed
    if (std::filesystem::exists(empty_temp)) {
        std::cerr << "FAIL: Empty temp dir still exists\n";
        return false;
    }
    
    if (std::filesystem::exists(empty_lock)) {
        std::cerr << "FAIL: Empty lock dir still exists\n";
        return false;
    }
    
    if (cleanup_result.temp_dirs_removed != 1 || cleanup_result.lock_dirs_removed != 1) {
        std::cerr << "FAIL: Expected 1 temp and 1 lock removed\n";
        return false;
    }
    
    std::cout << "PASS: Empty orphans removed correctly\n";
    return true;
}

// Test 5: Cleanup age threshold
bool test_cleanup_age_threshold() {
    std::cout << "\n=== Test 5: Cleanup respects age threshold (24 hours) ===\n";
    
    auto cache_root = std::filesystem::temp_directory_path() / "test_cleanup_age";
    std::filesystem::remove_all(cache_root);
    std::filesystem::create_directories(cache_root);
    
    auto shard_dir = cache_root / "v1" / "sha256" / "gh";
    std::filesystem::create_directories(shard_dir);
    
    // Create temp dir that's 23 hours old (just under threshold)
    auto recent_ish_temp = shard_dir / "rst234recent.tmp_22222";
    std::filesystem::create_directories(recent_ish_temp);
    
    auto now = std::filesystem::file_time_type::clock::now();
    auto almost_24h = now - std::chrono::hours(23);
    std::filesystem::last_write_time(recent_ish_temp, almost_24h);
    
    // Create temp dir that's 25 hours old (over threshold)
    auto old_temp = shard_dir / "uvw567old.tmp_33333";
    std::filesystem::create_directories(old_temp);
    auto over_24h = now - std::chrono::hours(25);
    std::filesystem::last_write_time(old_temp, over_24h);
    
    std::cout << "Created temp dirs: 23h old and 25h old\n";
    
    // Run cleanup
    cache::OpCache cache(cache_root);
    auto cleanup_result = cache.cleanup_temp_dirs();
    
    std::cout << "Cleanup removed: " << cleanup_result.temp_dirs_removed << " temp dirs\n";
    
    // 23h temp should still exist
    if (!std::filesystem::exists(recent_ish_temp)) {
        std::cerr << "FAIL: 23h old temp was removed (should be kept)\n";
        return false;
    }
    
    // 25h temp should be removed
    if (std::filesystem::exists(old_temp)) {
        std::cerr << "FAIL: 25h old temp still exists (should be removed)\n";
        return false;
    }
    
    if (cleanup_result.temp_dirs_removed != 1) {
        std::cerr << "FAIL: Expected exactly 1 temp dir removed\n";
        return false;
    }
    
    std::cout << "PASS: Cleanup respects 24-hour age threshold\n";
    return true;
}

int main() {
    std::cout << "Sprint 6 Phase 4: Orphan Cleanup Tests\n";
    std::cout << "=======================================\n";
    
    bool all_passed = true;
    
    all_passed &= test_cleanup_old_temp_dirs();
    all_passed &= test_cleanup_old_lock_dirs();
    all_passed &= test_cleanup_preserves_entries();
    all_passed &= test_cleanup_empty_dirs();
    all_passed &= test_cleanup_age_threshold();
    
    std::cout << "\n=======================================\n";
    if (all_passed) {
        std::cout << "All orphan cleanup tests PASSED\n";
        return 0;
    } else {
        std::cout << "Some orphan cleanup tests FAILED\n";
        return 1;
    }
}
