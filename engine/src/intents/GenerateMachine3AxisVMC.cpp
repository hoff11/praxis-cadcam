#include "praxis/Intent.hpp"
#include "BuildFromRecipe.hpp"
#include "../kernel/KernelOps.hpp"
#include "../kernel/StepIO.hpp"
#include "../kernel/StlIO.hpp"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <filesystem>

namespace praxis {

// Helper: lowercase conversion
static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return s;
}

// Helper: parse double from string
static bool tryParseDouble(const std::string& s, double& out) {
    try {
        size_t idx = 0;
        out = std::stod(s, &idx);
        return idx == s.size();
    } catch (...) {
        return false;
    }
}

// Helper: get parameter with range validation
static double getParamMm(
    const std::map<std::string, std::string>& params,
    const std::string& key,
    double defVal,
    double minVal,
    double maxVal,
    std::vector<std::string>& warnings
) {
    auto it = params.find(key);
    if (it == params.end()) return defVal;

    double v = defVal;
    if (!tryParseDouble(it->second, v)) {
        warnings.push_back("Param '" + key + "' invalid ('" + it->second + "'), using default " + std::to_string((int)defVal));
        return defVal;
    }
    if (v < minVal || v > maxVal) {
        warnings.push_back("Param '" + key + "' out of range (" + std::to_string((int)minVal) + ".." + std::to_string((int)maxVal) + "), using default " + std::to_string((int)defVal));
        return defVal;
    }
    return v;
}

// Fidelity levels
enum class Fidelity { Low, Medium, High };

static Fidelity getFidelity(
    const std::map<std::string, std::string>& params,
    std::vector<std::string>& warnings
) {
    auto it = params.find("fidelity");
    if (it == params.end()) return Fidelity::Medium;
    auto s = toLower(it->second);
    if (s == "low") return Fidelity::Low;
    if (s == "medium") return Fidelity::Medium;
    if (s == "high") return Fidelity::High;

    warnings.push_back("Param 'fidelity' invalid ('" + it->second + "'), using default 'medium'");
    return Fidelity::Medium;
}

// Helper: create box at position using kernel layer
static kernel::ShapeHandle makeBoxAt(
    double x, double y, double z, 
    double dx, double dy, double dz,
    IntentResult& result
) {
    using namespace praxis::kernel;
    
    auto box_result = KernelOps::make_box(dx, dy, dz);
    if (!box_result.success) {
        return kernel::ShapeHandle();  // Return null shape on error
    }
    
    auto transform_result = KernelOps::transform(box_result.shape, x, y, z);
    if (!transform_result.success) {
        return kernel::ShapeHandle();
    }
    
    // Collect operations for tracing
    result.kernel_ops.insert(result.kernel_ops.end(), 
        box_result.operations.begin(), box_result.operations.end());
    result.kernel_ops.insert(result.kernel_ops.end(),
        transform_result.operations.begin(), transform_result.operations.end());
    
    return transform_result.shape;
}

IntentResult generateMachine3AxisVMC(const IntentRequest& request) {
    // Sprint 2: Recipe system is in progress
    // For now, use legacy implementation to maintain Sprint 1 compatibility
    // TODO: Complete recipe JSON parser and enable recipe path below
    
    bool use_recipe_system = true;  // Sprint 5: recipe system is default (enables cache)
    
    if (use_recipe_system) {
        std::cout << "GenerateMachine3AxisVMC: delegating to BuildFromRecipe\n";
        
        // Locate recipe file (Gap B fix: use env var, not CWD)
        std::filesystem::path recipe_path;
        const char* recipe_env = std::getenv("PRAXIS_RECIPE_PATH");
        if (recipe_env && std::strlen(recipe_env) > 0) {
            recipe_path = recipe_env;
        } else {
            // Fallback: relative to CWD (for backward compat, but warns)
            recipe_path = std::filesystem::current_path() / "../../recipes/vmc/vmc_v0.json";
            std::cerr << "⚠ PRAXIS_RECIPE_PATH not set, using CWD-relative path (may break if run from different directory)\n";
        }
        recipe_path = std::filesystem::absolute(recipe_path);
        
        std::cout << "Using recipe: " << recipe_path << "\n";
        
        // Build new request for BuildFromRecipe
        IntentRequest recipe_request;
        recipe_request.intent_name = "BuildFromRecipe";
        recipe_request.output_dir = request.output_dir;
        recipe_request.params = request.params;
        recipe_request.params["recipe_path"] = recipe_path.string();
        
        // Forward cache control flags (Sprint 5 Phase 6)
        recipe_request.no_cache = request.no_cache;
        recipe_request.clear_cache = request.clear_cache;
        
        // Execute via recipe system
        auto result = buildFromRecipe(recipe_request);
        
        // Map result artifacts (Gap A fix: also update cache.reused_artifacts)
        if (result.success && !result.artifacts.empty()) {
            std::string model_path = result.artifacts[0];
            std::string vmc_path = request.output_dir + "/vmc1.step";
            
            try {
                if (std::filesystem::exists(model_path)) {
                    std::filesystem::rename(model_path, vmc_path);
                    result.artifacts[0] = vmc_path;
                    
                    // Update cache.reused_artifacts to reflect rename
                    for (auto& artifact : result.cache.reused_artifacts) {
                        if (artifact == model_path) {
                            artifact = vmc_path;
                        }
                    }
                    
                    // Export STL for preview (Sprint 12)
                    // Load the STEP we just created and export as STL
                    auto step_read = kernel::StepIO::read_step(vmc_path);
                    if (step_read.success) {
                        std::string stl_path = request.output_dir + "/vmc1.stl";
                        auto stl_write = kernel::StlIO::write_stl(step_read.shape, stl_path, 0.5);
                        if (stl_write.success) {
                            result.artifacts.push_back(stl_path);
                            result.kernel_ops.insert(result.kernel_ops.end(),
                                stl_write.operations.begin(), stl_write.operations.end());
                            std::cout << "✓ STL exported for preview\n";
                        } else {
                            result.warnings.push_back("STL export failed (non-fatal): " + stl_write.error_message);
                        }
                    }
                    
                    // Sprint 12: Populate previewable_outputs
                    praxis::PreviewableOutput step_output;
                    step_output.filename = "vmc1.step";
                    step_output.type = "step";
                    step_output.semantic_type = "Product";
                    step_output.previewable = true;
                    result.summary.previewable_outputs.push_back(step_output);
                    
                    if (std::filesystem::exists(request.output_dir + "/vmc1.stl")) {
                        praxis::PreviewableOutput stl_output;
                        stl_output.filename = "vmc1.stl";
                        stl_output.type = "stl";
                        stl_output.semantic_type = "Product";
                        stl_output.previewable = true;
                        result.summary.previewable_outputs.push_back(stl_output);
                    }
                }
            } catch (...) {
            }
        }
        
        return result;
    }
    
    // LEGACY IMPLEMENTATION (Sprint 1 compatibility)
    IntentResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        // Parse parameters with validation
        const double travel_x = getParamMm(request.params, "travel_x", 800, 200, 2000, result.warnings);
        const double travel_y = getParamMm(request.params, "travel_y", 600, 200, 2000, result.warnings);
        const double travel_z = getParamMm(request.params, "travel_z", 500, 200, 2000, result.warnings);
        const double table_w = getParamMm(request.params, "table_width", 1000, 400, 2000, result.warnings);
        const double table_d = getParamMm(request.params, "table_depth", 600, 400, 2000, result.warnings);
        const Fidelity fidelity = getFidelity(request.params, result.warnings);
        
        std::string fidelityStr = (fidelity == Fidelity::Low ? "low" : 
                                   fidelity == Fidelity::High ? "high" : "medium");
        
        // Log parameters
        {
            std::ostringstream ss;
            ss << "Parameters: X=" << (int)travel_x << ", Y=" << (int)travel_y << ", Z=" << (int)travel_z
               << ", table=" << (int)table_w << "x" << (int)table_d
               << ", fidelity=" << fidelityStr;
            result.kernel_ops.push_back(ss.str());
        }
        
        std::cout << "Creating parametric machine...\n";
        std::cout << "  Travel: " << (int)travel_x << " x " << (int)travel_y << " x " << (int)travel_z << " mm\n";
        std::cout << "  Table: " << (int)table_w << " x " << (int)table_d << " mm\n";
        std::cout << "  Fidelity: " << fidelityStr << "\n";
        
        // Coordinate system: +X right, +Y back/away, +Z up
        // Origin: front-left corner of base
        
        // Proportional sizing based on parameters
        const double base_w = table_w + 400.0;
        const double base_d = table_d + 400.0;
        const double base_h = 200.0;
        
        const double col_w = 400.0;
        const double col_d = 600.0;
        const double col_h = travel_z + 500.0;
        
        const double table_h = 100.0;
        
        const double head_w = 300.0;
        const double head_d = 300.0;
        const double head_h = 400.0;
        
        // Component placement
        const double base_x = 0, base_y = 0, base_z = 0;
        
        const double table_x = (base_w - table_w) * 0.5;
        const double table_y = (base_d - table_d) * 0.5;
        const double table_z = base_h;
        
        const double col_x = (base_w - col_w) * 0.5;
        const double col_y = base_d - col_d;  // rear
        const double col_z = base_h;
        
        const double head_x = (base_w - head_w) * 0.5;
        const double head_y = col_y - (head_d * 0.5);  // slightly in front of column
        const double head_z = base_h + std::max(200.0, travel_z * 0.4);  // above table
        
        // Build component list based on fidelity
        std::vector<kernel::ShapeHandle> parts;
        
        // Always: base, column, table, spindle head (4 components for low fidelity)
        parts.push_back(makeBoxAt(base_x, base_y, base_z, base_w, base_d, base_h, result));
        parts.push_back(makeBoxAt(col_x, col_y, col_z, col_w, col_d, col_h, result));
        parts.push_back(makeBoxAt(table_x, table_y, table_z, table_w, table_d, table_h, result));
        parts.push_back(makeBoxAt(head_x, head_y, head_z, head_w, head_d, head_h, result));
        
        // Medium fidelity: add carriages + mount (7 components total)
        if (fidelity == Fidelity::Medium || fidelity == Fidelity::High) {
            // X carriage under table (simplified saddle)
            const double xcar_w = table_w + 80;
            const double xcar_d = table_d * 0.35;
            const double xcar_h = 120;
            parts.push_back(makeBoxAt(
                (base_w - xcar_w) * 0.5,
                table_y + (table_d - xcar_d) * 0.5,
                base_h + (table_h * 0.2),
                xcar_w, xcar_d, xcar_h, result));
            
            // Y carriage (knee) between base and table
            const double ycar_w = table_w * 0.6;
            const double ycar_d = table_d * 0.6;
            const double ycar_h = 140;
            parts.push_back(makeBoxAt(
                (base_w - ycar_w) * 0.5,
                (base_d - ycar_d) * 0.5,
                base_h + 10,
                ycar_w, ycar_d, ycar_h, result));
            
            // Spindle mount (between head and column)
            const double mount_w = 320, mount_d = 200, mount_h = 500;
            parts.push_back(makeBoxAt(
                (base_w - mount_w) * 0.5,
                col_y - mount_d + 50,
                head_z - 50,
                mount_w, mount_d, mount_h, result));
        }
        
        // High fidelity: add covers, tray, cabinet, tank (10+ components)
        if (fidelity == Fidelity::High) {
            // Chip tray (front overhang)
            parts.push_back(makeBoxAt(0, -150, base_h - 40, base_w, 150, 40, result));
            
            // Coolant tank (right side)
            parts.push_back(makeBoxAt(base_w + 50, base_d * 0.2, 0, 300, 500, 500, result));
            
            // Electronics cabinet (left side) - negative X is fine for realism
            parts.push_back(makeBoxAt(-350, base_d * 0.2, 0, 300, 500, 800, result));
            
            // Way covers (simple)
            parts.push_back(makeBoxAt(table_x, table_y, base_h + 20, table_w, 80, 60, result));
        }
        
        std::cout << "✓ Created " << parts.size() << " components\n";
        
        // Combine into compound using kernel layer
        using namespace praxis::kernel;
        auto compound_result = KernelOps::make_compound(parts);
        
        if (!compound_result.success) {
            result.success = false;
            result.errors.push_back("Failed to create compound: " + compound_result.error_message);
            result.confidence = 0.0;
            return result;
        }
        
        // Collect compound operations
        result.kernel_ops.insert(result.kernel_ops.end(),
            compound_result.operations.begin(), compound_result.operations.end());
        
        std::cout << "✓ Machine assembly created\n";
        
        // Export to STEP using kernel layer
        std::string outputPath = request.output_dir + "/vmc1.step";
        std::cout << "Exporting to: " << outputPath << "\n";
        
        auto write_result = StepIO::write_step(compound_result.shape, outputPath);
        
        if (!write_result.success) {
            result.success = false;
            result.errors.push_back("Failed to write STEP: " + write_result.error_message);
            result.confidence = 0.0;
            return result;
        }
        
        // Collect STEP write operations
        result.kernel_ops.insert(result.kernel_ops.end(),
            write_result.operations.begin(), write_result.operations.end());
        
        std::cout << "✓ STEP exported successfully\n";
        
        // Export STL for preview (Sprint 12)
        std::string stlPath = request.output_dir + "/vmc1.stl";
        auto stl_result = StlIO::write_stl(compound_result.shape, stlPath, 0.5);
        
        if (stl_result.success) {
            result.artifacts.push_back(stlPath);
            result.kernel_ops.insert(result.kernel_ops.end(),
                stl_result.operations.begin(), stl_result.operations.end());
            std::cout << "✓ STL exported for preview\n";
        } else {
            result.warnings.push_back("STL export failed (non-fatal): " + stl_result.error_message);
        }
        
        // Store metrics (traceability for actual parameters used)
        result.metrics["param_travel_x"] = std::to_string((int)travel_x);
        result.metrics["param_travel_y"] = std::to_string((int)travel_y);
        result.metrics["param_travel_z"] = std::to_string((int)travel_z);
        result.metrics["param_table_width"] = std::to_string((int)table_w);
        result.metrics["param_table_depth"] = std::to_string((int)table_d);
        result.metrics["param_fidelity"] = fidelityStr;
        result.metrics["num_components"] = std::to_string((int)parts.size());
        
        // Get kernel operation metrics
        auto kernel_metrics = KernelOps::get_metrics();
        for (const auto& kv : kernel_metrics) {
            result.metrics["op_" + kv.first + "_count"] = std::to_string(kv.second);
        }
        
        // EPIC 0.2.5: Report version
        result.report_version = "1.1";
        
        // Success - confidence based on fidelity
        result.success = true;
        result.artifacts.push_back(outputPath);
        
        // Sprint 12: Populate previewable_outputs for contract compliance
        praxis::PreviewableOutput step_output;
        step_output.filename = "vmc1.step";
        step_output.type = "step";
        step_output.semantic_type = "Product";
        step_output.previewable = true;
        result.summary.previewable_outputs.push_back(step_output);
        
        if (stl_result.success) {
            praxis::PreviewableOutput stl_output;
            stl_output.filename = "vmc1.stl";
            stl_output.type = "stl";
            stl_output.semantic_type = "Product";
            stl_output.previewable = true;
            result.summary.previewable_outputs.push_back(stl_output);
        }
        
        if (fidelity == Fidelity::Low) {
            result.confidence = 0.70;
        } else if (fidelity == Fidelity::High) {
            result.confidence = 0.80;
        } else {
            result.confidence = 0.75;
        }
        
        // Warning with actual parameters used
        {
            std::ostringstream w;
            w << "Generated machine from parameters: travel=" << (int)travel_x << "x" << (int)travel_y << "x" << (int)travel_z
              << " table=" << (int)table_w << "x" << (int)table_d
              << " fidelity=" << fidelityStr;
            result.warnings.push_back(w.str());
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        result.duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
        
        return result;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.errors.push_back(std::string("Exception: ") + e.what());
        result.confidence = 0.0;
        return result;
    }
}

} // namespace praxis
