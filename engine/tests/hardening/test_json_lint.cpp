/**
 * test_json_lint.cpp
 * Static analysis test to prevent regression of raw JSON string emission
 * 
 * This test scans source files for dangerous patterns where strings are inserted
 * into JSON without escape_json() wrapping, which can create invalid JSON.
 * 
 * CONTRACT:
 * - Any pattern like `<< "..." << variable << "..."` in JSON context must use escape_json()
 * - Numeric fields (floats/ints) must use canonical::format_float() or direct insertion
 * - All string fields in JSON must be wrapped with canonical::escape_json()
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <filesystem>

namespace fs = std::filesystem;

struct LintViolation {
    std::string file;
    int line_number;
    std::string line_content;
    std::string reason;
};

bool is_json_emission_file(const std::string& path) {
    // Files that emit JSON (commands, reports, interaction, cache)
    return path.find("/commands/") != std::string::npos ||
           path.find("/Report.cpp") != std::string::npos ||
           path.find("/Interaction") != std::string::npos ||
           path.find("/OpCache.cpp") != std::string::npos ||
           path.find("/KinematicLoader.cpp") != std::string::npos;
}

std::vector<LintViolation> scan_file_for_violations(const std::string& filepath) {
    std::vector<LintViolation> violations;
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return violations;
    }
    
    std::string line;
    int line_num = 0;
    
    // Patterns that indicate dangerous raw string insertion into JSON
    // Pattern: `<< "field": "` followed by stream insertion of a variable
    std::regex dangerous_pattern1(R"(<<\s*\"[^\"]*\":\s*\\\"\s*<<\s*[a-zA-Z_][a-zA-Z0-9_\.]*\s*<<)");
    
    // Pattern: `<< "{\"type\":\"" << variable` without escape_json
    std::regex dangerous_pattern2(R"(<<\s*\"[^\"]*\\\"\s*<<\s*[a-zA-Z_][a-zA-Z0-9_\.]*(?!\s*\)))");
    
    while (std::getline(file, line)) {
        line_num++;
        
        // Guard: Only treat as string value context if we see a JSON field followed by an opening quote for a string value, e.g., "name": "
        std::regex string_value_context(R"(\"[^\"]+\"\s*:\s*\")");
        
        // Skip lines that already use escape_json
        if (line.find("escape_json") != std::string::npos) {
            continue;
        }
        
        // Skip lines that are comments
        std::string trimmed = line;
        size_t comment_pos = trimmed.find("//");
        if (comment_pos != std::string::npos) {
            trimmed = trimmed.substr(0, comment_pos);
        }
        
        // Skip if not JSON emission context
        if (trimmed.find("<<") == std::string::npos) {
            continue;
        }
        if (trimmed.find("\"") == std::string::npos) {
            continue;
        }
        
        // Check for pattern: `<< "field_name": "` followed by variable insertion
        // This catches: `oss << "name": "` << my_var << ...`
        std::regex field_pattern(R"(<<\s*\"[a-zA-Z_][a-zA-Z0-9_]*\":\s*\"\s*<<\s*[a-zA-Z_])");
        if (std::regex_search(trimmed, field_pattern)) {
            // Check if this line uses format_float (numeric fields are OK)
            if (trimmed.find("format_float") == std::string::npos) {
                LintViolation v;
                v.file = filepath;
                v.line_number = line_num;
                v.line_content = trimmed;
                v.reason = "Raw string insertion in JSON field without escape_json()";
                violations.push_back(v);
            }
        }
        
        // Check for pattern: inserting into quoted JSON value context
        // Pattern: `<< "..." << variable_name <<` where variable is likely a string
        if (std::regex_search(trimmed, string_value_context)) {
            std::regex value_pattern(R"(<<\s*\"[^\"]*\"\s*<<\s*([a-zA-Z_][a-zA-Z0-9_\.]*)\s*<<\s*\")");
            std::smatch match;
            if (std::regex_search(trimmed, match, value_pattern)) {
                std::string var_name = match[1].str();
                // Skip if it's a known numeric field or uses format_float
                if (trimmed.find("format_float") == std::string::npos &&
                    var_name.find("count") == std::string::npos &&
                    var_name.find("size") == std::string::npos &&
                    var_name.find("index") == std::string::npos &&
                    var_name.find("bytes") == std::string::npos) {
                    LintViolation v;
                    v.file = filepath;
                    v.line_number = line_num;
                    v.line_content = trimmed;
                    v.reason = "Potential raw string '" + var_name + "' insertion without escape_json()";
                    violations.push_back(v);
                }
            }
        }
    }
    
    return violations;
}

int main() {
    std::cout << "=== JSON Lint Test ===\n";
    std::cout << "Scanning for raw JSON string insertions without escape_json()...\n\n";
    
    std::vector<LintViolation> all_violations;
    
    // Scan all C++ source files in src/
    // Prefer compile-time SRC_ROOT from CMake; fallback to relative path
    std::string src_dir = "../src";
    #ifdef SRC_ROOT
    src_dir = std::string(SRC_ROOT) + "/src";
    #endif
    
    std::cout << "Scanning directory: " << src_dir << "\n\n";
    
    try {
        for (const auto& entry : fs::recursive_directory_iterator(src_dir)) {
            if (entry.path().extension() == ".cpp") {
                std::string path = entry.path().string();
                
                // Only scan JSON-emitting files
                if (!is_json_emission_file(path)) {
                    continue;
                }
                
                auto violations = scan_file_for_violations(path);
                all_violations.insert(all_violations.end(), violations.begin(), violations.end());
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error scanning files: " << e.what() << "\n";
        return 1;
    }
    
    // Report violations
    if (all_violations.empty()) {
        std::cout << "✅ NO VIOLATIONS FOUND\n";
        std::cout << "All JSON string emissions appear to use canonical::escape_json()\n";
        return 0;
    }
    
    std::cout << "❌ FOUND " << all_violations.size() << " POTENTIAL VIOLATIONS:\n\n";
    
    for (const auto& v : all_violations) {
        std::cout << v.file << ":" << v.line_number << "\n";
        std::cout << "  Reason: " << v.reason << "\n";
        std::cout << "  Line: " << v.line_content << "\n\n";
    }
    
    std::cout << "LINT POLICY:\n";
    std::cout << "  - All string fields in JSON MUST use canonical::escape_json()\n";
    std::cout << "  - Numeric fields MUST use canonical::format_float() or direct insertion\n";
    std::cout << "  - Pattern `<< \\\"field\\\": \\\"\" << var` is FORBIDDEN without escape_json()\n";
    std::cout << "\n";
    std::cout << "To fix: Wrap all string variables with canonical::escape_json()\n";
    std::cout << "Example: oss << \\\"name\\\": \\\"\" << canonical::escape_json(name) << \"\\\"\n";
    
    return 1;  // Fail if violations found
}
