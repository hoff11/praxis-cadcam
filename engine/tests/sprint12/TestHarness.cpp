/**
 * TestHarness.cpp
 * Sprint 12 - Hard gate testing implementation
 */

#include "TestHarness.hpp"
#include "../../include/OCCTInspector.hpp"
#include "../../src/kernel/StepIO.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <cassert>
#include <cmath>
#include <array>
#include <cstdlib>
#include <openssl/sha.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace praxis::testing {

using namespace praxis::inspection;
using namespace praxis::kernel;

// ============================================================================
// JSON Serialization
// ============================================================================

json ShapeSignature::to_json() const {
    json j;
    j["body_count"] = body_count;
    j["volumes"] = volumes;
    j["bboxes"] = bbox_strings;
    return j;
}

std::string ShapeSignature::to_sha256() const {
    json j = to_json();
    std::string canonical = j.dump();
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(canonical.c_str()), 
           canonical.size(), hash);
    
    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return oss.str();
}

bool ShapeSignature::operator==(const ShapeSignature& other) const {
    if (body_count != other.body_count) return false;
    if (volumes.size() != other.volumes.size()) return false;
    
    constexpr double tol = 1e-6;
    for (size_t i = 0; i < volumes.size(); i++) {
        if (std::abs(volumes[i] - other.volumes[i]) > tol) return false;
    }
    
    return bbox_strings == other.bbox_strings;
}

// ============================================================================
// Path Resolution
// ============================================================================

fs::path TestHarness::get_binary_path() {
    // Tests run from build/ with WORKING_DIRECTORY set to build/
    // Binary is at build/praxis-cad
    fs::path candidate1 = "./praxis-cad";
    fs::path candidate2 = "./praxis-cad.exe";
    fs::path candidate3 = "praxis-cad";
    fs::path candidate4 = "praxis-cad.exe";
    
    if (fs::exists(candidate1)) return candidate1;
    if (fs::exists(candidate2)) return candidate2;
    if (fs::exists(candidate3)) return candidate3;
    if (fs::exists(candidate4)) return candidate4;
    
    std::cerr << "FATAL: praxis-cad binary not found\n";
    std::cerr << "Current dir: " << fs::current_path() << "\n";
    std::cerr << "Expected: ./praxis-cad or praxis-cad\n";
    assert(false && "Binary not found");
    return "";
}

fs::path TestHarness::create_test_dir(const std::string& test_name) {
    fs::path base = fs::current_path() / "test_outputs" / test_name;
    fs::create_directories(base);
    
    // Clear if exists
    if (fs::exists(base)) {
        for (auto& entry : fs::directory_iterator(base)) {
            fs::remove_all(entry.path());
        }
    }
    
    return base;
}

// ============================================================================
// Command Execution
// ============================================================================

CommandResult TestHarness::run_cmd(const std::string& command, const fs::path& working_dir) {
    CommandResult result;
    
#ifdef _WIN32
    // Windows: use cmd /c with output redirection
    fs::path out_file = working_dir / "cmd_stdout.txt";
    fs::path err_file = working_dir / "cmd_stderr.txt";
    
    std::string full_cmd = "cmd /c \"cd /d " + working_dir.string() + " && " + 
                           command + " > " + out_file.string() + " 2> " + err_file.string() + "\"";
    
    result.exit_code = system(full_cmd.c_str());
    
    // Read outputs
    if (fs::exists(out_file)) {
        std::ifstream f(out_file);
        result.stdout_output = std::string((std::istreambuf_iterator<char>(f)), 
                                            std::istreambuf_iterator<char>());
    }
    if (fs::exists(err_file)) {
        std::ifstream f(err_file);
        result.stderr_output = std::string((std::istreambuf_iterator<char>(f)), 
                                            std::istreambuf_iterator<char>());
    }
#else
    // Unix: use popen
    FILE* pipe = popen((std::string("cd ") + working_dir.string() + " && " + command + " 2>&1").c_str(), "r");
    if (!pipe) {
        result.exit_code = -1;
        result.stderr_output = "Failed to popen";
        return result;
    }
    
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result.stdout_output += buffer;
    }
    
    result.exit_code = pclose(pipe);
#endif
    
    return result;
}

CommandResult TestHarness::run_plan(const fs::path& plan_json, const fs::path& out_dir) {
    fs::path binary = get_binary_path();
    std::string cmd = binary.string() + " plan " + plan_json.string() + " " + out_dir.string();
    return run_cmd(cmd, binary.parent_path());
}

CommandResult TestHarness::run_inspect(const fs::path& step_file) {
    fs::path binary = get_binary_path();
    std::string cmd = binary.string() + " inspect " + step_file.string();
    return run_cmd(cmd, binary.parent_path());
}

ErrorInfo TestHarness::parse_error_json(const CommandResult& result) {
    ErrorInfo info;
    
    std::string combined = result.stderr_output + "\n" + result.stdout_output;
    
    // Look for PRAXIS_ERROR_JSON=... (use rfind to get the last one)
    const std::string prefix = "PRAXIS_ERROR_JSON=";
    size_t pos = combined.rfind(prefix);
    
    if (pos == std::string::npos) {
        return info;  // No error JSON found
    }
    
    // Extract JSON line
    pos += prefix.length();
    size_t end = combined.find('\n', pos);
    std::string json_str = combined.substr(pos, end - pos);
    
    try {
        json j = json::parse(json_str);
        info.code = j.value("code", "");
        info.message = j.value("message", "");
        info.where = j.value("where", "");
        if (j.contains("detail")) {
            info.detail = j["detail"];
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to parse error JSON: " << e.what() << "\n";
        std::cerr << "JSON string: " << json_str << "\n";
    }
    
    return info;
}

// ============================================================================
// Inspection
// ============================================================================

std::vector<BodyInfo> TestHarness::inspect_bodies(const fs::path& step_file) {
    auto inspector = create_inspector();
    if (!inspector->load_artifact(step_file.string())) {
        std::cerr << "FATAL: Failed to load " << step_file << "\n";
        assert(false && "load_artifact failed");
    }
    
    auto occt_bodies = inspector->enumerate_bodies();
    
    std::vector<BodyInfo> result;
    for (const auto& b : occt_bodies) {
        BodyInfo info;
        info.volume = b.volume;
        info.area = 0.0;  // BodyProperties doesn't track surface area
        info.bbox = {b.bbox.min_x, b.bbox.min_y, b.bbox.min_z,
                     b.bbox.max_x, b.bbox.max_y, b.bbox.max_z};
        result.push_back(info);
    }
    
    return result;
}

ShapeSignature TestHarness::compute_signature(const fs::path& step_file) {
    auto bodies = inspect_bodies(step_file);
    
    ShapeSignature sig;
    sig.body_count = static_cast<int>(bodies.size());
    
    for (const auto& b : bodies) {
        sig.volumes.push_back(b.volume);
        
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6);
        oss << "[" << b.bbox[0] << "," << b.bbox[1] << "," << b.bbox[2] << ","
            << b.bbox[3] << "," << b.bbox[4] << "," << b.bbox[5] << "]";
        sig.bbox_strings.push_back(oss.str());
    }
    
    // Sort for determinism
    std::sort(sig.volumes.begin(), sig.volumes.end());
    std::sort(sig.bbox_strings.begin(), sig.bbox_strings.end());
    
    return sig;
}

// ============================================================================
// Assertions
// ============================================================================

void TestHarness::assert_file_exists(const fs::path& path, const std::string& context) {
    if (!fs::exists(path)) {
        std::cerr << "FATAL [" << context << "]: File not found: " << path << "\n";
        std::cerr << "Absolute: " << fs::absolute(path) << "\n";
        assert(false && "Required file missing");
    }
}

void TestHarness::assert_near(double actual, double expected, double tolerance, 
                               const std::string& context) {
    if (std::abs(actual - expected) > tolerance) {
        std::cerr << "FATAL [" << context << "]: Expected " << expected 
                  << ", got " << actual << " (tolerance " << tolerance << ")\n";
        assert(false && "Value out of tolerance");
    }
}

// ============================================================================
// Plan Generation
// ============================================================================

json TestHarness::make_box_plan(double size_x, double size_y, double size_z,
                                 double x, double y, double z) {
    return json{
        {"op_type", "CreateBox"},
        {"args", {
            {"size_x", std::to_string(size_x)},
            {"size_y", std::to_string(size_y)},
            {"size_z", std::to_string(size_z)},
            {"tx", std::to_string(x)},
            {"ty", std::to_string(y)},
            {"tz", std::to_string(z)}
        }}
    };
}

json TestHarness::make_two_box_plan() {
    return json{
        {"plan_id", "test_two_boxes"},
        {"version", 1},
        {"units", "mm"},
        {"steps", {
            {
                {"op_type", "CreateBox"},
                {"args", {
                    {"size_x", "40"},
                    {"size_y", "40"},
                    {"size_z", "40"},
                    {"tx", "0"},
                    {"ty", "0"},
                    {"tz", "0"}
                }}
            },
            {
                {"op_type", "CreateBox"},
                {"args", {
                    {"size_x", "30"},
                    {"size_y", "30"},
                    {"size_z", "30"},
                    {"tx", "50"},
                    {"ty", "0"},
                    {"tz", "0"}
                }}
            }
        }}
    };
}

json TestHarness::make_attach_plan() {
    return json{
        {"plan_id", "test_attach"},
        {"version", 1},
        {"units", "mm"},
        {"steps", {
            {
                {"op_type", "CreateBox"},
                {"args", {
                    {"size_x", "40"},
                    {"size_y", "40"},
                    {"size_z", "40"},
                    {"tx", "0"},
                    {"ty", "0"},
                    {"tz", "0"}
                }}
            },
            {
                {"op_type", "CreateBox"},
                {"args", {
                    {"size_x", "30"},
                    {"size_y", "30"},
                    {"size_z", "30"},
                    {"tx", "50"},
                    {"ty", "0"},
                    {"tz", "0"}
                }}
            },
            {
                {"op_type", "AttachFaceToFace"},
                {"args", {
                    {"moving_face", "{\"ref_type\":\"FaceRef\",\"contract_version\":\"1.0\",\"parent\":{\"ref_type\":\"BodyRef\",\"contract_version\":\"1.0\",\"index\":1},\"predicate\":{\"kind\":\"PlanarFace\",\"normal\":{\"x\":-1.0,\"y\":0.0,\"z\":0.0}}}"},
                    {"fixed_face", "{\"ref_type\":\"FaceRef\",\"contract_version\":\"1.0\",\"parent\":{\"ref_type\":\"BodyRef\",\"contract_version\":\"1.0\",\"index\":0},\"predicate\":{\"kind\":\"PlanarFace\",\"normal\":{\"x\":1.0,\"y\":0.0,\"z\":0.0}}}"}
                }}
            }
        }}
    };
}

} // namespace praxis::testing
