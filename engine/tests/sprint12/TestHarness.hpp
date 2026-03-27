/**
 * TestHarness.hpp
 * Sprint 12 - Hard gate testing utilities
 * 
 * Hermetic, deterministic test infrastructure:
 * - Run binaries with captured output
 * - Inspect STEP artifacts
 * - Compare canonical signatures
 * - No CWD dependencies, no skips
 */

#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <optional>
#include <nlohmann/json.hpp>

namespace praxis::testing {

namespace fs = std::filesystem;
using json = nlohmann::json;

/**
 * Command execution result
 */
struct CommandResult {
    int exit_code;
    std::string stdout_output;
    std::string stderr_output;
    
    bool success() const { return exit_code == 0; }
};

/**
 * Structured error from PRAXIS_ERROR_JSON
 */
struct ErrorInfo {
    std::string code;
    std::string message;
    std::string where;
    json detail;
    
    bool has_error() const { return !code.empty(); }
};

/**
 * Canonical shape signature for determinism checks
 */
struct ShapeSignature {
    int body_count;
    std::vector<double> volumes;
    std::vector<std::string> bbox_strings;  // Serialized bounding boxes
    
    json to_json() const;
    std::string to_sha256() const;
    
    bool operator==(const ShapeSignature& other) const;
};

/**
 * Body metadata from inspection
 */
struct BodyInfo {
    double volume;
    double area;
    std::array<double, 6> bbox;  // xmin, ymin, zmin, xmax, ymax, zmax
};

/**
 * Core test harness
 */
class TestHarness {
public:
    /**
     * Get path to praxis-cad executable
     */
    static fs::path get_binary_path();
    
    /**
     * Create hermetic temporary directory for test
     */
    static fs::path create_test_dir(const std::string& test_name);
    
    /**
     * Run command and capture output
     */
    static CommandResult run_cmd(const std::string& command, 
                                   const fs::path& working_dir = fs::current_path());
    
    /**
     * Run praxis-cad plan command
     */
    static CommandResult run_plan(const fs::path& plan_json, const fs::path& out_dir);
    
    /**
     * Run praxis-cad inspect command
     */
    static CommandResult run_inspect(const fs::path& step_file);
    
    /**
     * Parse error JSON from command output (PRAXIS_ERROR_JSON=...)
     */
    static ErrorInfo parse_error_json(const CommandResult& result);
    
    /**
     * Load STEP file and extract body info
     */
    static std::vector<BodyInfo> inspect_bodies(const fs::path& step_file);
    
    /**
     * Compute canonical signature for a STEP file
     */
    static ShapeSignature compute_signature(const fs::path& step_file);
    
    /**
     * Assert file exists (hard fail if not)
     */
    static void assert_file_exists(const fs::path& path, const std::string& context);
    
    /**
     * Assert value within tolerance
     */
    static void assert_near(double actual, double expected, double tolerance, 
                            const std::string& context);
    
    /**
     * Create a simple CreateBox plan JSON
     */
    static json make_box_plan(double size_x, double size_y, double size_z,
                               double x = 0, double y = 0, double z = 0);
    
    /**
     * Create a two-box accumulation plan
     */
    static json make_two_box_plan();
    
    /**
     * Create attach plan with FaceRef payloads
     */
    static json make_attach_plan();
};

} // namespace praxis::testing
