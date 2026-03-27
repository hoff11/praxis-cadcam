// Sprint 11 Task 11.6: Preview Artifact Discoverability Tests
// Tests that previewable outputs can be discovered and classified post-execution

#include "../../src/cache/OpCache.hpp"
#include "../../src/recipe/RecipeTypes.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cassert>

namespace fs = std::filesystem;
using namespace praxis::cache;
using namespace praxis::recipe;

// Helper: Parse meta.json to extract preview metadata
struct PreviewMetadata {
    std::string type;
    bool previewable = false;
    std::string semantic_type;
    std::string filename;
};

static std::vector<PreviewMetadata> parse_meta_json(const fs::path& meta_path) {
    std::vector<PreviewMetadata> results;
    
    std::ifstream file(meta_path);
    if (!file) return results;
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    // Simple JSON parsing for outputs array
    size_t outputs_pos = content.find("\"outputs\":");
    if (outputs_pos == std::string::npos) return results;
    
    size_t array_start = content.find("[", outputs_pos);
    size_t array_end = content.find("]", array_start);
    if (array_start == std::string::npos || array_end == std::string::npos) return results;
    
    std::string outputs_section = content.substr(array_start + 1, array_end - array_start - 1);
    
    // Extract each output object (simplified parser)
    size_t pos = 0;
    while ((pos = outputs_section.find("{", pos)) != std::string::npos) {
        size_t end = outputs_section.find("}", pos);
        if (end == std::string::npos) break;
        
        std::string obj = outputs_section.substr(pos, end - pos + 1);
        
        PreviewMetadata meta;
        
        // Extract type
        size_t type_pos = obj.find("\"type\":");
        if (type_pos != std::string::npos) {
            size_t quote1 = obj.find("\"", type_pos + 7);
            size_t quote2 = obj.find("\"", quote1 + 1);
            meta.type = obj.substr(quote1 + 1, quote2 - quote1 - 1);
        }
        
        // Extract previewable
        size_t prev_pos = obj.find("\"previewable\":");
        if (prev_pos != std::string::npos) {
            meta.previewable = (obj.find("true", prev_pos) != std::string::npos);
        }
        
        // Extract semantic_type
        size_t sem_pos = obj.find("\"semantic_type\":");
        if (sem_pos != std::string::npos) {
            size_t quote1 = obj.find("\"", sem_pos + 16);
            size_t quote2 = obj.find("\"", quote1 + 1);
            meta.semantic_type = obj.substr(quote1 + 1, quote2 - quote1 - 1);
        }
        
        // Extract filename
        size_t file_pos = obj.find("\"filename\":");
        if (file_pos != std::string::npos) {
            size_t quote1 = obj.find("\"", file_pos + 11);
            size_t quote2 = obj.find("\"", quote1 + 1);
            meta.filename = obj.substr(quote1 + 1, quote2 - quote1 - 1);
        }
        
        results.push_back(meta);
        pos = end + 1;
    }
    
    return results;
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Sprint 11 Preview Discovery Tests\n";
    std::cout << "========================================\n\n";
    
    int failures = 0;
    
    // Test 1: Metadata Sufficiency Test
    std::cout << "Test 1: Metadata Sufficiency for Preview Discovery\n";
    {
        fs::path test_dir = fs::temp_directory_path() / "praxis_sprint11_preview";
        fs::path cache_dir = test_dir / "cache";
        
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
        fs::create_directories(cache_dir);
        
        OpCache op_cache(cache_dir);
        
        // Create test metadata with preview fields
        OpCacheEntryMeta meta;
        meta.op_hash = "test_preview_001";
        meta.engine_version = "0.1.0";
        meta.kernel_version = "1.0.0";
        meta.created_at = "2026-01-08T00:00:00Z";
        
        // Add previewable output
        OutputArtifact artifact1;
        artifact1.name = "body1";
        artifact1.filename = "body1.step";
        artifact1.bytes = 2048;
        artifact1.sha256 = "abc123";
        artifact1.type = "step";
        artifact1.previewable = true;
        artifact1.semantic_type = "Body";
        meta.outputs.push_back(artifact1);
        
        // Add non-previewable output
        OutputArtifact artifact2;
        artifact2.name = "metadata";
        artifact2.filename = "metadata.json";
        artifact2.bytes = 512;
        artifact2.sha256 = "def456";
        artifact2.type = "json";
        artifact2.previewable = false;
        artifact2.semantic_type = "";
        meta.outputs.push_back(artifact2);
        
        // Serialize to JSON
        std::string json = meta.to_json();
        
        // Write to file
        fs::path meta_path = cache_dir / "meta.json";
        std::ofstream out(meta_path);
        out << json;
        out.close();
        
        // Now parse it back WITHOUT execution context
        auto parsed = parse_meta_json(meta_path);
        
        if (parsed.empty()) {
            std::cerr << "  FAIL: Failed to parse meta.json\n";
            failures++;
        } else if (parsed.size() != 2) {
            std::cerr << "  FAIL: Expected 2 outputs, got " << parsed.size() << "\n";
            failures++;
        } else {
            // Verify first artifact is previewable
            if (!parsed[0].previewable) {
                std::cerr << "  FAIL: First artifact should be previewable\n";
                failures++;
            } else if (parsed[0].semantic_type != "Body") {
                std::cerr << "  FAIL: First artifact should have semantic_type 'Body', got '" << parsed[0].semantic_type << "'\n";
                failures++;
            } else if (parsed[0].type != "step") {
                std::cerr << "  FAIL: First artifact should have type 'step', got '" << parsed[0].type << "'\n";
                failures++;
            }
            
            // Verify second artifact is NOT previewable
            else if (parsed[1].previewable) {
                std::cerr << "  FAIL: Second artifact should not be previewable\n";
                failures++;
            } else if (parsed[1].type != "json") {
                std::cerr << "  FAIL: Second artifact should have type 'json', got '" << parsed[1].type << "'\n";
                failures++;
            } else {
                std::cout << "  PASS: Metadata sufficient for preview discovery\n";
            }
        }
        
        fs::remove_all(test_dir);
    }
    
    // Test 2: Classification Stability
    std::cout << "\nTest 2: Classification Stability Across Runs\n";
    {
        fs::path test_dir = fs::temp_directory_path() / "praxis_sprint11_classification";
        fs::path cache_dir = test_dir / "cache";
        
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
        fs::create_directories(cache_dir);
        
        // Create identical metadata twice
        auto create_meta = []() {
            OpCacheEntryMeta meta;
            meta.op_hash = "test_stable";
            meta.engine_version = "0.1.0";
            meta.kernel_version = "1.0.0";
            meta.created_at = "2026-01-08T00:00:00Z";
            
            OutputArtifact artifact;
            artifact.name = "part";
            artifact.filename = "part.step";
            artifact.bytes = 1024;
            artifact.sha256 = "hash123";
            artifact.type = "step";
            artifact.previewable = true;
            artifact.semantic_type = "Body";
            meta.outputs.push_back(artifact);
            
            return meta;
        };
        
        OpCacheEntryMeta meta1 = create_meta();
        OpCacheEntryMeta meta2 = create_meta();
        
        std::string json1 = meta1.to_json();
        std::string json2 = meta2.to_json();
        
        // Verify byte-identical serialization
        if (json1 != json2) {
            std::cerr << "  FAIL: Metadata serialization not stable\n";
            std::cerr << "  JSON 1 length: " << json1.length() << "\n";
            std::cerr << "  JSON 2 length: " << json2.length() << "\n";
            failures++;
        } else {
            // Write and parse both
            fs::path meta1_path = cache_dir / "meta1.json";
            fs::path meta2_path = cache_dir / "meta2.json";
            
            std::ofstream(meta1_path) << json1;
            std::ofstream(meta2_path) << json2;
            
            auto parsed1 = parse_meta_json(meta1_path);
            auto parsed2 = parse_meta_json(meta2_path);
            
            if (parsed1.empty() || parsed2.empty()) {
                std::cerr << "  FAIL: Failed to parse metadata\n";
                failures++;
            } else if (parsed1[0].previewable != parsed2[0].previewable) {
                std::cerr << "  FAIL: Preview classification differs between runs\n";
                failures++;
            } else if (parsed1[0].semantic_type != parsed2[0].semantic_type) {
                std::cerr << "  FAIL: Semantic type differs between runs\n";
                failures++;
            } else {
                std::cout << "  PASS: Classification stable across runs\n";
            }
        }
        
        fs::remove_all(test_dir);
    }
    
    // Test 3: UX Contract Compliance
    std::cout << "\nTest 3: UX Contract Compliance\n";
    {
        fs::path test_dir = fs::temp_directory_path() / "praxis_sprint11_ux_contract";
        fs::path cache_dir = test_dir / "cache";
        
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
        fs::create_directories(cache_dir);
        
        OpCache op_cache(cache_dir);
        
        OpCacheEntryMeta meta;
        meta.op_hash = "test_ux";
        meta.engine_version = "0.1.0";
        meta.kernel_version = "1.0.0";
        meta.created_at = "2026-01-08T00:00:00Z";
        
        OutputArtifact artifact;
        artifact.name = "preview_test";
        artifact.filename = "preview_test.step";
        artifact.bytes = 4096;
        artifact.sha256 = "ux_hash";
        artifact.type = "step";
        artifact.previewable = true;
        artifact.semantic_type = "Body";
        meta.outputs.push_back(artifact);
        
        std::string json = meta.to_json();
        fs::path meta_path = cache_dir / "meta.json";
        std::ofstream(meta_path) << json;
        
        // Verify UX contract fields are present
        bool has_failures = false;
        
        if (json.find("\"type\"") == std::string::npos) {
            std::cerr << "  FAIL: Missing 'type' field (UX contract requirement)\n";
            has_failures = true;
            failures++;
        }
        if (json.find("\"previewable\"") == std::string::npos) {
            std::cerr << "  FAIL: Missing 'previewable' field (UX contract requirement)\n";
            has_failures = true;
            failures++;
        }
        if (json.find("\"semantic_type\"") == std::string::npos) {
            std::cerr << "  FAIL: Missing 'semantic_type' field (UX contract requirement)\n";
            has_failures = true;
            failures++;
        }
        if (json.find("\"filename\"") == std::string::npos) {
            std::cerr << "  FAIL: Missing 'filename' field (UX contract requirement)\n";
            has_failures = true;
            failures++;
        }
        
        // Verify values are non-empty
        auto parsed = parse_meta_json(meta_path);
        if (parsed.empty()) {
            std::cerr << "  FAIL: Failed to parse metadata\n";
            failures++;
        } else {
            if (parsed[0].type.empty()) {
                std::cerr << "  FAIL: 'type' field is empty\n";
                has_failures = true;
                failures++;
            }
            if (parsed[0].semantic_type.empty()) {
                std::cerr << "  FAIL: 'semantic_type' field is empty for previewable artifact\n";
                has_failures = true;
                failures++;
            }
            if (parsed[0].filename.empty()) {
                std::cerr << "  FAIL: 'filename' field is empty\n";
                has_failures = true;
                failures++;
            }
        }
        
        if (!has_failures) {
            std::cout << "  PASS: Metadata complies with UX contract\n";
        }
        
        fs::remove_all(test_dir);
    }
    
    std::cout << "\n========================================\n";
    if (failures == 0) {
        std::cout << "All tests PASSED\n";
    } else {
        std::cout << failures << " test(s) FAILED\n";
    }
    std::cout << "========================================\n";
    
    return failures;
}
