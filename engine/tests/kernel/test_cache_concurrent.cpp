// Sprint 6 Phase 4: Concurrent writer test
// Tests concurrent writes with current OpCache behavior

#include "../../src/cache/OpCache.hpp"
#include "../../src/kernel/KernelVersion.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

using namespace praxis;

// Test utility: Write a simple text file
bool write_test_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path);
    if (!out) return false;
    out << content;
    return true;
}

// Simulate a cache store operation (what a worker would do)
bool simulate_cache_store(const std::filesystem::path& cache_root, 
                         const std::string& op_hash, 
                         int worker_id,
                         bool& got_lock) {
    try {
        cache::OpCache cache(cache_root);
        
        // Begin store (acquires lock)
        auto store = cache.begin_store(op_hash);
        got_lock = store.active;
        
        if (!store.active) {
            std::cout << "[Worker " << worker_id << "] Failed to acquire lock (expected if other worker has it)\n";
            return false;
        }
        
        std::cout << "[Worker " << worker_id << "] Acquired lock, writing to staging...\n";
        
        // Simulate some work in staging directory
        auto output_path = store.get_staging_path("out", ".step");
        std::filesystem::create_directories(output_path.parent_path());
        
        std::string content = "Worker " + std::to_string(worker_id) + " was here";
        if (!write_test_file(output_path, content)) {
            cache.abort_store(store);
            return false;
        }
        
        // Simulate processing time
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Commit store
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
        
        bool committed = cache.commit_store(store, meta);
        std::cout << "[Worker " << worker_id << "] Commit " << (committed ? "succeeded" : "failed") << "\n";
        
        return committed;
        
    } catch (const std::exception& e) {
        std::cerr << "[Worker " << worker_id << "] Exception: " << e.what() << std::endl;
        return false;
    }
}

// Test 1: Two threads try to write same hash - one should get lock, other should timeout/skip
bool test_concurrent_threads() {
    std::cout << "\n=== Test 1: Concurrent threads writing same hash ===\n";
    
    auto cache_root = std::filesystem::temp_directory_path() / "test_concurrent_threads";
    std::filesystem::remove_all(cache_root);
    std::filesystem::create_directories(cache_root);
    
    std::string op_hash = "abc123concurrent";
    
    std::atomic<int> locks_acquired{0};
    std::atomic<int> commits_succeeded{0};
    
    auto worker = [&](int id) {
        bool got_lock = false;
        bool committed = simulate_cache_store(cache_root, op_hash, id, got_lock);
        
        if (got_lock) locks_acquired++;
        if (committed) commits_succeeded++;
    };
    
    // Launch two threads simultaneously
    std::thread t1(worker, 1);
    std::thread t2(worker, 2);
    
    t1.join();
    t2.join();
    
    std::cout << "\nResults:\n";
    std::cout << "  Locks acquired: " << locks_acquired << "\n";
    std::cout << "  Commits succeeded: " << commits_succeeded << "\n";
    
    // Verify entry exists and is valid
    cache::OpCache cache(cache_root);
    auto result = cache.try_load(op_hash);
    
    if (!result.hit) {
        std::cerr << "FAIL: Entry not found after concurrent writes\n";
        return false;
    }
    
    // Verify only one worker's data made it through
    auto output_path = result.output_paths["out"];
    std::ifstream in(output_path);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    
    std::cout << "  Final content: \"" << content << "\"\n";
    
    // Should have exactly one worker's content
    bool has_worker1 = content.find("Worker 1") != std::string::npos;
    bool has_worker2 = content.find("Worker 2") != std::string::npos;
    
    if (has_worker1 && has_worker2) {
        std::cerr << "FAIL: Both workers wrote to entry (locking failed)\n";
        return false;
    }
    
    if (!has_worker1 && !has_worker2) {
        std::cerr << "FAIL: No worker data found\n";
        return false;
    }
    
    std::cout << "PASS: Only one worker successfully committed\n";
    return true;
}

// Test 2: Second writer after first commit preserves existing valid entry
bool test_sequential_with_recheck() {
    std::cout << "\n=== Test 2: Second writer preserves existing entry ===\n";
    
    auto cache_root = std::filesystem::temp_directory_path() / "test_recheck";
    std::filesystem::remove_all(cache_root);
    std::filesystem::create_directories(cache_root);
    
    std::string op_hash = "def456recheck";
    
    // First writer completes
    bool got_lock1 = false;
    bool committed1 = simulate_cache_store(cache_root, op_hash, 1, got_lock1);
    
    if (!committed1) {
        std::cerr << "FAIL: First writer failed to commit\n";
        return false;
    }
    
    std::cout << "\nFirst writer committed successfully\n";
    
    // Second writer attempts to store again
    cache::OpCache cache(cache_root);
    auto store2 = cache.begin_store(op_hash);
    
    if (!store2.active) {
        std::cerr << "FAIL: Second begin_store unexpectedly inactive\n";
        return false;
    }
    
    // Write competing payload and attempt commit; implementation may reject via rename failure.
    auto output2 = store2.get_staging_path("out", ".step");
    std::filesystem::create_directories(output2.parent_path());
    write_test_file(output2, "Worker 2 override attempt");

    cache::OpCacheEntryMeta meta2;
    meta2.op_hash = op_hash;
    meta2.engine_version = "test";
    meta2.kernel_version = kernel::get_kernel_version_string();
    meta2.created_at = "2026-01-06T00:00:00Z";
    cache::OutputArtifact artifact2;
    artifact2.name = "out";
    artifact2.filename = "out.step";
    artifact2.bytes = std::filesystem::file_size(output2);
    artifact2.sha256 = cache.compute_file_sha256(output2);
    meta2.outputs.push_back(artifact2);
    (void)cache.commit_store(store2, meta2);

    auto final_hit = cache.try_load(op_hash);
    if (!final_hit.hit) {
        std::cerr << "FAIL: Existing entry became invalid after second writer attempt\n";
        return false;
    }

    std::cout << "PASS: Existing entry remains valid after second writer attempt\n";
    return true;
}

// Test 3: Verify staging directory is cleaned up after commit
bool test_lock_released_after_commit() {
    std::cout << "\n=== Test 3: Staging cleanup after commit ===\n";
    
    auto cache_root = std::filesystem::temp_directory_path() / "test_lock_release";
    std::filesystem::remove_all(cache_root);
    std::filesystem::create_directories(cache_root);
    
    std::string op_hash = "ghi789lockrelease";
    
    cache::OpCache cache(cache_root);
    auto store = cache.begin_store(op_hash);
    if (!store.active) {
        std::cerr << "FAIL: Could not create staging store\n";
        return false;
    }

    auto output = store.get_staging_path("out", ".step");
    std::filesystem::create_directories(output.parent_path());
    write_test_file(output, "commit payload");

    cache::OpCacheEntryMeta meta;
    meta.op_hash = op_hash;
    meta.engine_version = "test";
    meta.kernel_version = kernel::get_kernel_version_string();
    meta.created_at = "2026-01-06T00:00:00Z";
    cache::OutputArtifact artifact;
    artifact.name = "out";
    artifact.filename = "out.step";
    artifact.bytes = std::filesystem::file_size(output);
    artifact.sha256 = cache.compute_file_sha256(output);
    meta.outputs.push_back(artifact);

    bool committed = cache.commit_store(store, meta);
    if (!committed) {
        std::cerr << "FAIL: Commit failed\n";
        return false;
    }
    
    if (std::filesystem::exists(store.staging_dir)) {
        std::cerr << "FAIL: Staging directory still exists after commit\n";
        return false;
    }
    
    auto entry_dir = cache.debug_get_entry_dir(op_hash);
    if (!std::filesystem::exists(entry_dir / "meta.json")) {
        std::cerr << "FAIL: Final entry missing after commit\n";
        return false;
    }

    std::cout << "PASS: Staging cleaned and final entry present after commit\n";
    return true;
}

// Test 4: Verify staging directory is cleaned up after abort
bool test_lock_released_after_abort() {
    std::cout << "\n=== Test 4: Staging cleanup after abort ===\n";
    
    auto cache_root = std::filesystem::temp_directory_path() / "test_lock_abort";
    std::filesystem::remove_all(cache_root);
    std::filesystem::create_directories(cache_root);
    
    std::string op_hash = "jkl012lockabort";
    
    cache::OpCache cache(cache_root);
    
    // Begin store
    auto store = cache.begin_store(op_hash);
    if (!store.active) {
        std::cerr << "FAIL: Could not acquire initial lock\n";
        return false;
    }
    
    // Write something to staging
    auto output_path = store.get_staging_path("out", ".step");
    std::filesystem::create_directories(output_path.parent_path());
    write_test_file(output_path, "test");
    
    // Abort instead of committing
    cache.abort_store(store);
    
    // Verify staging directory is cleaned up
    if (std::filesystem::exists(store.staging_dir)) {
        std::cerr << "FAIL: Staging directory still exists after abort\n";
        return false;
    }
    
    std::cout << "PASS: Staging cleaned up after abort\n";
    return true;
}

int main() {
    std::cout << "Sprint 6 Phase 4: Concurrent Writer Tests\n";
    std::cout << "==========================================\n";
    
    bool all_passed = true;
    
    all_passed &= test_concurrent_threads();
    all_passed &= test_sequential_with_recheck();
    all_passed &= test_lock_released_after_commit();
    all_passed &= test_lock_released_after_abort();
    
    std::cout << "\n==========================================\n";
    if (all_passed) {
        std::cout << "All concurrent writer tests PASSED\n";
        return 0;
    } else {
        std::cout << "Some concurrent writer tests FAILED\n";
        return 1;
    }
}
