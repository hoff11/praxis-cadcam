// Sprint 6 Phase 4: Cache integrity validation tests
// Tests integrity checks with current OpCache hit/miss contract

#include "../../src/cache/OpCache.hpp"
#include "../../src/kernel/KernelVersion.hpp"
#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;
using namespace praxis::cache;

// Helper: Create test cache directory
fs::path create_test_cache_dir() {
    static int counter = 0;
    fs::path cache_dir = fs::current_path() / ("test_cache_integrity_" + std::to_string(++counter));
    fs::remove_all(cache_dir);
    fs::create_directories(cache_dir);
    return cache_dir;
}

// Helper: Create dummy output file
void create_dummy_output(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream ofs(path);
    ofs << content;
}

// Test 1: SHA256 validation catches tampered file
void test_sha256_tampered_file() {
    std::cout << "\nTest 1: SHA256 detects tampered output file\n";
    std::cout << "--------------------------------------------\n";
    
    auto cache_dir = create_test_cache_dir();
    OpCache cache(cache_dir);
    
    std::string test_hash = "abc1234567890123456789012345678901234567890123456789012345678901";
    
    // Store an entry with SHA256
    auto store = cache.begin_store(test_hash);
    assert(store.active);
    
    auto output_path = store.get_staging_path("out", ".step");
    std::string original_content = "ISO-10303-21;\nHEADER;\nENDSEC;\nDATA;\nENDSEC;\nEND-ISO-10303-21;\n";
    create_dummy_output(output_path, original_content);
    
    OpCacheEntryMeta meta;
    meta.op_hash = test_hash;
    meta.engine_version = "0.6.0";
    meta.kernel_version = praxis::kernel::get_kernel_version_string();
    
    OutputArtifact artifact;
    artifact.name = "out";
    artifact.filename = "out.step";
    artifact.bytes = fs::file_size(output_path);
    artifact.sha256 = cache.compute_file_sha256(output_path);  // Compute real SHA256
    
    meta.outputs.push_back(artifact);
    
    bool committed = cache.commit_store(store, meta);
    assert(committed);
    std::cout << "✓ Entry stored with SHA256: " << artifact.sha256.substr(0, 16) << "...\n";
    
    // Verify initial load succeeds
    auto hit1 = cache.try_load(test_hash);
    assert(hit1.hit);
    std::cout << "✓ Initial load: HIT\n";
    
    // Tamper with the output file
    auto entry_dir = cache_dir / "v1" / "sha256" / test_hash.substr(0, 2) / test_hash;
    auto final_output = entry_dir / "outputs" / "out.step";
    {
        std::ofstream tamper(final_output, std::ios::trunc);
        tamper << "TAMPERED CONTENT\n";
    }
    std::cout << "✓ Output file tampered\n";
    
    // Now try_load should MISS due to SHA mismatch
    auto hit2 = cache.try_load(test_hash);
    assert(!hit2.hit);
    std::cout << "✓ After tampering: MISS\n";
    
    std::cout << "✅ Test 1 PASSED: SHA256 validation catches tampering\n";
}

// Test 2: Missing .complete marker causes miss
void test_missing_complete_marker() {
    std::cout << "\nTest 2: Missing .complete marker causes miss\n";
    std::cout << "---------------------------------------------\n";
    
    auto cache_dir = create_test_cache_dir();
    OpCache cache(cache_dir);
    
    std::string test_hash = "def2345678901234567890123456789012345678901234567890123456789012";
    
    // Store normally
    auto store = cache.begin_store(test_hash);
    assert(store.active);
    
    auto output_path = store.get_staging_path("out", ".step");
    create_dummy_output(output_path, "STEP content");
    
    OpCacheEntryMeta meta;
    meta.op_hash = test_hash;
    meta.engine_version = "0.6.0";
    meta.kernel_version = praxis::kernel::get_kernel_version_string();
    
    OutputArtifact artifact;
    artifact.name = "out";
    artifact.filename = "out.step";
    artifact.bytes = fs::file_size(output_path);
    artifact.sha256 = cache.compute_file_sha256(output_path);
    meta.outputs.push_back(artifact);
    
    cache.commit_store(store, meta);
    
    // Verify hit
    auto hit1 = cache.try_load(test_hash);
    assert(hit1.hit);
    std::cout << "✓ Initial load: HIT\n";
    
    // Remove .complete marker
    auto entry_dir = cache_dir / "v1" / "sha256" / test_hash.substr(0, 2) / test_hash;
    fs::remove(entry_dir / ".complete");
    std::cout << "✓ .complete marker removed\n";
    
    // Should now MISS
    auto hit2 = cache.try_load(test_hash);
    assert(!hit2.hit);
    std::cout << "✓ After removing .complete: MISS\n";
    
    std::cout << "✅ Test 2 PASSED: Missing .complete marker prevents hit\n";
}

// Test 3: Missing output file causes miss
void test_missing_output_file() {
    std::cout << "\nTest 3: Missing output file causes miss\n";
    std::cout << "----------------------------------------\n";
    
    auto cache_dir = create_test_cache_dir();
    OpCache cache(cache_dir);
    
    std::string test_hash = "ghi3456789012345678901234567890123456789012345678901234567890123";
    
    // Store normally
    auto store = cache.begin_store(test_hash);
    assert(store.active);
    
    auto output_path = store.get_staging_path("out", ".step");
    create_dummy_output(output_path, "STEP content");
    
    OpCacheEntryMeta meta;
    meta.op_hash = test_hash;
    meta.engine_version = "0.6.0";
    meta.kernel_version = praxis::kernel::get_kernel_version_string();
    
    OutputArtifact artifact;
    artifact.name = "out";
    artifact.filename = "out.step";
    artifact.bytes = fs::file_size(output_path);
    artifact.sha256 = cache.compute_file_sha256(output_path);
    meta.outputs.push_back(artifact);
    
    cache.commit_store(store, meta);
    
    // Verify hit
    auto hit1 = cache.try_load(test_hash);
    assert(hit1.hit);
    std::cout << "✓ Initial load: HIT\n";
    
    // Delete output file
    auto entry_dir = cache_dir / "v1" / "sha256" / test_hash.substr(0, 2) / test_hash;
    fs::remove(entry_dir / "outputs" / "out.step");
    std::cout << "✓ Output file deleted\n";
    
    // Should now MISS
    auto hit2 = cache.try_load(test_hash);
    assert(!hit2.hit);
    std::cout << "✓ After deleting output: MISS\n";
    
    std::cout << "✅ Test 3 PASSED: Missing output file prevents hit\n";
}

// Test 4: Corrupt meta.json causes parse error
void test_corrupt_meta_json() {
    std::cout << "\nTest 4: Corrupt meta.json causes parse error\n";
    std::cout << "---------------------------------------------\n";
    
    auto cache_dir = create_test_cache_dir();
    OpCache cache(cache_dir);
    
    std::string test_hash = "jkl4567890123456789012345678901234567890123456789012345678901234";
    
    // Store normally
    auto store = cache.begin_store(test_hash);
    assert(store.active);
    
    auto output_path = store.get_staging_path("out", ".step");
    create_dummy_output(output_path, "STEP content");
    
    OpCacheEntryMeta meta;
    meta.op_hash = test_hash;
    meta.engine_version = "0.6.0";
    meta.kernel_version = praxis::kernel::get_kernel_version_string();
    
    OutputArtifact artifact;
    artifact.name = "out";
    artifact.filename = "out.step";
    artifact.bytes = fs::file_size(output_path);
    artifact.sha256 = cache.compute_file_sha256(output_path);
    meta.outputs.push_back(artifact);
    
    cache.commit_store(store, meta);
    
    // Verify hit
    auto hit1 = cache.try_load(test_hash);
    assert(hit1.hit);
    std::cout << "✓ Initial load: HIT\n";
    
    // Corrupt meta.json
    auto entry_dir = cache_dir / "v1" / "sha256" / test_hash.substr(0, 2) / test_hash;
    {
        std::ofstream corrupt(entry_dir / "meta.json", std::ios::trunc);
        corrupt << "{ corrupt json ][{";
    }
    std::cout << "✓ meta.json corrupted\n";
    
    // Should now MISS
    auto hit2 = cache.try_load(test_hash);
    assert(!hit2.hit);
    std::cout << "✓ After corruption: MISS\n";
    
    std::cout << "✅ Test 4 PASSED: Corrupt meta.json causes parse error\n";
}

// Test 5: Tampered SHA256 in meta causes validation failure
void test_tampered_meta_sha256() {
    std::cout << "\nTest 5: Tampered SHA256 in meta causes validation failure\n";
    std::cout << "---------------------------------------------------------\n";
    
    auto cache_dir = create_test_cache_dir();
    OpCache cache(cache_dir);
    
    std::string test_hash = "mno5678901234567890123456789012345678901234567890123456789012345";
    
    // Store normally
    auto store = cache.begin_store(test_hash);
    assert(store.active);
    
    auto output_path = store.get_staging_path("out", ".step");
    create_dummy_output(output_path, "STEP content");
    
    OpCacheEntryMeta meta;
    meta.op_hash = test_hash;
    meta.engine_version = "0.6.0";
    meta.kernel_version = praxis::kernel::get_kernel_version_string();
    
    OutputArtifact artifact;
    artifact.name = "out";
    artifact.filename = "out.step";
    artifact.bytes = fs::file_size(output_path);
    artifact.sha256 = cache.compute_file_sha256(output_path);
    meta.outputs.push_back(artifact);
    
    cache.commit_store(store, meta);
    
    // Verify hit
    auto hit1 = cache.try_load(test_hash);
    assert(hit1.hit);
    std::cout << "✓ Initial load: HIT\n";
    
    // Manually rewrite meta.json with wrong SHA256
    auto entry_dir = cache_dir / "v1" / "sha256" / test_hash.substr(0, 2) / test_hash;
    {
        std::ofstream tamper_meta(entry_dir / "meta.json", std::ios::trunc);
        tamper_meta << "{\n";
        tamper_meta << "  \"cache_meta_schema\": 1,\n";
        tamper_meta << "  \"op_hash\": \"" << test_hash << "\",\n";
        tamper_meta << "  \"engine_version\": \"0.6.0\",\n";
        tamper_meta << "  \"kernel_version\": \"" << praxis::kernel::get_kernel_version_string() << "\",\n";
        tamper_meta << "  \"created_at\": \"2026-01-05T00:00:00Z\",\n";
        tamper_meta << "  \"producer\": \"praxis-engine\",\n";
        tamper_meta << "  \"outputs\": [\n";
        tamper_meta << "    {\n";
        tamper_meta << "      \"name\": \"out\",\n";
        tamper_meta << "      \"filename\": \"out.step\",\n";
        tamper_meta << "      \"bytes\": " << artifact.bytes << ",\n";
        tamper_meta << "      \"sha256\": \"0000000000000000000000000000000000000000000000000000000000000000\"\n";
        tamper_meta << "    }\n";
        tamper_meta << "  ]\n";
        tamper_meta << "}\n";
    }
    std::cout << "✓ meta.json SHA256 tampered to all zeros\n";
    
    // Should now MISS due to SHA mismatch
    auto hit2 = cache.try_load(test_hash);
    assert(!hit2.hit);
    std::cout << "✓ After tampering SHA in meta: MISS\n";
    
    std::cout << "✅ Test 5 PASSED: Tampered meta SHA256 causes validation failure\n";
}

int main() {
    std::cout << "=================================================\n";
    std::cout << "Sprint 6 Phase 4: Cache Integrity Validation Tests\n";
    std::cout << "=================================================\n";
    
    try {
        test_sha256_tampered_file();
        test_missing_complete_marker();
        test_missing_output_file();
        test_corrupt_meta_json();
        test_tampered_meta_sha256();
        
        std::cout << "\n=================================================\n";
        std::cout << "✅ ALL INTEGRITY TESTS PASSED\n";
        std::cout << "=================================================\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << "\n";
        return 1;
    }
}
