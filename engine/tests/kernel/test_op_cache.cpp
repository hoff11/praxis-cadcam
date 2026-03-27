// Test op-level cache (Sprint 6 Phase 3)
#include "../../src/cache/OpCache.hpp"
#include "../../src/cache/HashContext.hpp"
#include "../../src/kernel/KernelVersion.hpp"
#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>

using namespace praxis::cache;
using namespace praxis::kernel;

// Helper: Create test cache directory
static std::filesystem::path create_test_cache_dir() {
    auto temp_dir = std::filesystem::temp_directory_path() / "praxis_test_opcache";
    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);
    return temp_dir;
}

// Helper: Create dummy output file
static void create_dummy_output(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream ofs(path);
    ofs << content;
}

int main() {
    std::cout << "========================================\\n";
    std::cout << "Op Cache Tests (Sprint 6 Phase 3)\\n";
    std::cout << "========================================\\n\\n";
    
    // Test 1: Store and load
    std::cout << "Test 1: Store and load\\n";
    std::cout << "--------------------------------------\\n";
    {
        auto cache_dir = create_test_cache_dir();
        OpCache cache(cache_dir);
        
        std::string test_hash = "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890";
        
        // Initially should not exist
        assert(!cache.has(test_hash));
        
        auto hit = cache.try_load(test_hash);
        assert(!hit.hit);
        std::cout << "✓ Cache miss on first load\\n";
        
        // Store an entry
        auto store = cache.begin_store(test_hash);
        assert(store.active);
        
        // Create dummy output
        auto output_path = store.get_staging_path("out", ".step");
        create_dummy_output(output_path, "STEP content");
        
        // Create metadata
        OpCacheEntryMeta meta;
        meta.op_hash = test_hash;
        meta.engine_version = "0.6.0";
        meta.kernel_version = get_kernel_version_string();
        meta.created_at = "2026-01-05T12:00:00Z";
        
        OutputArtifact artifact;
        artifact.name = "out";
        artifact.filename = "out.step";
        artifact.bytes = std::filesystem::file_size(output_path);
        meta.outputs.push_back(artifact);
        
        // Commit
        bool committed = cache.commit_store(store, meta);
        assert(committed);
        assert(!store.active);
        std::cout << "✓ Store committed successfully\\n";
        
        // Now should exist
        assert(cache.has(test_hash));
        
        hit = cache.try_load(test_hash);
        assert(hit.hit);
        assert(hit.op_hash == test_hash);
        assert(hit.output_paths.count("out") > 0);
        assert(std::filesystem::exists(hit.output_paths["out"]));
        std::cout << "✓ Cache hit on second load\\n";
        std::cout << "  Entry dir: " << hit.entry_dir << "\\n";
        std::cout << "  Output: " << hit.output_paths["out"] << "\\n\\n";
    }
    
    // Test 2: Sharding (first 2 chars)
    std::cout << "Test 2: Directory sharding\\n";
    std::cout << "--------------------------------------\\n";
    {
        auto cache_dir = create_test_cache_dir();
        OpCache cache(cache_dir);
        
        std::string hash_ab = "ab1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcd";
        std::string hash_cd = "cd1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcd";
        
        // Store two entries with different shards
        for (const auto& hash : {hash_ab, hash_cd}) {
            auto store = cache.begin_store(hash);
            auto output_path = store.get_staging_path("out", ".step");
            create_dummy_output(output_path, "content");
            
            OpCacheEntryMeta meta;
            meta.op_hash = hash;
            meta.engine_version = "0.6.0";
            meta.kernel_version = get_kernel_version_string();
            
            OutputArtifact artifact;
            artifact.name = "out";
            artifact.filename = "out.step";
            artifact.bytes = std::filesystem::file_size(output_path);
            meta.outputs.push_back(artifact);
            
            cache.commit_store(store, meta);
        }
        
        // Verify sharding structure
        auto v1_dir = cache_dir / "v1" / "sha256";
        assert(std::filesystem::exists(v1_dir / "ab" / hash_ab));
        assert(std::filesystem::exists(v1_dir / "cd" / hash_cd));
        std::cout << "✓ Entries stored in separate shard directories\\n";
        std::cout << "  ab/ shard: " << (v1_dir / "ab").string() << "\\n";
        std::cout << "  cd/ shard: " << (v1_dir / "cd").string() << "\\n\\n";
    }
    
    // Test 3: Atomic write (abort)
    std::cout << "Test 3: Atomic write abort\\n";
    std::cout << "--------------------------------------\\n";
    {
        auto cache_dir = create_test_cache_dir();
        OpCache cache(cache_dir);
        
        std::string test_hash = "fedcba0987654321fedcba0987654321fedcba0987654321fedcba0987654321";
        
        // Begin store
        auto store = cache.begin_store(test_hash);
        auto staging_path = store.staging_dir;
        assert(std::filesystem::exists(staging_path));
        std::cout << "✓ Staging directory created: " << staging_path << "\\n";
        
        // Abort without committing
        cache.abort_store(store);
        assert(!store.active);
        assert(!std::filesystem::exists(staging_path));
        std::cout << "✓ Staging directory cleaned up after abort\\n";
        
        // Entry should not exist
        assert(!cache.has(test_hash));
        std::cout << "✓ Entry not visible after abort\\n\\n";
    }
    
    // Test 4: Meta JSON serialization
    std::cout << "Test 4: Meta JSON serialization\\n";
    std::cout << "--------------------------------------\\n";
    {
        OpCacheEntryMeta meta;
        meta.schema_version = 1;
        meta.op_hash = "abc123";
        meta.engine_version = "0.6.0";
        meta.kernel_version = "6.9.1";
        meta.created_at = "2026-01-05T12:00:00Z";
        meta.producer = "praxis-engine";
        
        OutputArtifact out1;
        out1.name = "out";
        out1.filename = "out.step";
        out1.bytes = 12345;
        out1.sha256 = "deadbeef";
        meta.outputs.push_back(out1);
        
        std::string json = meta.to_json();
        assert(json.find("abc123") != std::string::npos);
        assert(json.find("0.6.0") != std::string::npos);
        assert(json.find("out.step") != std::string::npos);
        assert(json.find("12345") != std::string::npos);
        std::cout << "✓ Meta JSON contains all fields\\n";
        std::cout << "  Sample:\\n" << json.substr(0, 200) << "...\\n\\n";
    }
    
    std::cout << "========================================\\n";
    std::cout << "ALL OP CACHE TESTS PASSED!\\n";
    std::cout << "========================================\\n";
    
    return 0;
}
