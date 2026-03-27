#include "praxis/Intent.hpp"
#include "../kernel/KernelOps.hpp"
#include "../kernel/StepIO.hpp"
#include "../kernel/StlIO.hpp"
#include "InteractionEmit.hpp"
#include <iostream>
#include <sstream>
#include <filesystem>

namespace praxis {

static bool parseDoubleParam(const std::map<std::string, std::string>& params, const std::string& key, double defVal, double& out, std::vector<std::string>& warnings, std::vector<std::string>& errors, bool required = false) {
    auto it = params.find(key);
    if (it == params.end()) { 
        out = defVal; 
        return true; 
    }
    try {
        size_t idx = 0; 
        out = std::stod(it->second, &idx);
        if (idx != it->second.size()) {
            if (required) {
                errors.push_back("Parameter '" + key + "' has invalid numeric value: '" + it->second + "'");
                return false;
            }
            throw std::runtime_error("extra chars");
        }
        return true;
    } catch (...) {
        if (required) {
            errors.push_back("Parameter '" + key + "' has invalid numeric value: '" + it->second + "'");
            return false;
        }
        std::ostringstream oss; 
        oss << "Param '" << key << "' invalid ('" << it->second << "'), using default " << defVal;
        warnings.push_back(oss.str());
        out = defVal; 
        return false;
    }
}

// EPIC 1.1 micro-intent: CreateBox (Phase A execution)
// Params (CLI-friendly for now; Plan JSON later):
//   size_x, size_y, size_z (mm) — REQUIRED (v0.6.0 schema validation)
//   tx, ty, tz (mm) — defaults 0
// Output: box.step and box.stl in output_dir
IntentResult createBoxIntent(const IntentRequest& request) {
    IntentResult result;
    result.summary.intent = "CreateBox";

    // v0.6.0: Validate required parameters
    if (request.params.find("size_x") == request.params.end()) {
        result.success = false;
        result.error_code = "InvalidParameter";
        result.errors.push_back("Missing required parameter: size_x");
        return result;
    }
    if (request.params.find("size_y") == request.params.end()) {
        result.success = false;
        result.error_code = "InvalidParameter";
        result.errors.push_back("Missing required parameter: size_y");
        return result;
    }
    if (request.params.find("size_z") == request.params.end()) {
        result.success = false;
        result.error_code = "InvalidParameter";
        result.errors.push_back("Missing required parameter: size_z");
        return result;
    }

    // Resolve params with validation
    double sx=10.0, sy=10.0, sz=10.0;
    double tx=0.0, ty=0.0, tz=0.0;
    
    // Parse size parameters (required, strict validation)
    if (!parseDoubleParam(request.params, "size_x", 10.0, sx, result.warnings, result.errors, true) ||
        !parseDoubleParam(request.params, "size_y", 10.0, sy, result.warnings, result.errors, true) ||
        !parseDoubleParam(request.params, "size_z", 10.0, sz, result.warnings, result.errors, true)) {
        result.success = false;
        result.error_code = "InvalidParameter";
        return result;
    }
    
    // Validate positive sizes
    if (sx <= 0.0 || sy <= 0.0 || sz <= 0.0) {
        result.success = false;
        result.error_code = "InvalidParameter";
        result.errors.push_back("Box dimensions must be positive (got size_x=" + std::to_string(sx) + 
                               ", size_y=" + std::to_string(sy) + ", size_z=" + std::to_string(sz) + ")");
        return result;
    }
    
    // Parse translation parameters (optional, default 0)
    parseDoubleParam(request.params, "tx", 0.0, tx, result.warnings, result.errors, false);
    parseDoubleParam(request.params, "ty", 0.0, ty, result.warnings, result.errors, false);
    parseDoubleParam(request.params, "tz", 0.0, tz, result.warnings, result.errors, false);

    result.resolved_params["size_x"] = sx;
    result.resolved_params["size_y"] = sy;
    result.resolved_params["size_z"] = sz;
    result.resolved_params["tx"] = tx;
    result.resolved_params["ty"] = ty;
    result.resolved_params["tz"] = tz;

    // Construct box
    auto box_res = kernel::KernelOps::make_box(sx, sy, sz);
    if (!box_res.success) {
        result.success = false;
        result.errors.push_back(box_res.error_message);
        return result;
    }
    result.kernel_ops.insert(result.kernel_ops.end(), box_res.operations.begin(), box_res.operations.end());

    // Apply translation (no rotation in Phase A)
    auto xform_res = kernel::KernelOps::transform(box_res.shape, tx, ty, tz);
    if (!xform_res.success) {
        result.success = false;
        result.errors.push_back(xform_res.error_message);
        return result;
    }
    result.kernel_ops.insert(result.kernel_ops.end(), xform_res.operations.begin(), xform_res.operations.end());

    // Validate shape
    bool is_valid = kernel::KernelOps::validate(xform_res.shape);
    result.metrics["shape_valid"] = is_valid ? "true" : "false";
    if (!is_valid) {
        result.warnings.push_back("Box shape reported invalid by kernel analyzer; proceeding with write for debug.");
    }

    // Write artifacts
    const std::filesystem::path out_root = std::filesystem::path(request.output_dir);
    std::filesystem::create_directories(out_root);
    const auto step_path = (out_root / "box.step").string();
    const auto stl_path  = (out_root / "box.stl").string();

    auto step_write = kernel::StepIO::write_step(xform_res.shape, step_path);
    if (!step_write.success) {
        result.success = false;
        result.errors.push_back(step_write.error_message);
        return result;
    }
    auto stl_write = kernel::StlIO::write_stl(xform_res.shape, stl_path, 0.5);
    if (!stl_write.success) {
        // Non-fatal for Phase A; record warning
        result.warnings.push_back(stl_write.error_message);
    }

    result.artifacts.push_back(step_path);
    if (stl_write.success) result.artifacts.push_back(stl_path);

    // Populate summary
    result.summary.artifact_count = static_cast<int>(result.artifacts.size());
    result.summary.produced = {"Product", "Body"};
    result.summary.body_count = 1;
    result.summary.face_count = 6;
    result.summary.edge_count = 12;

    // Previewable outputs
    praxis::PreviewableOutput step_out; step_out.filename = "box.step"; step_out.type = "step"; step_out.semantic_type = "Product"; step_out.previewable = true;
    result.summary.previewable_outputs.push_back(step_out);
    if (stl_write.success) {
        praxis::PreviewableOutput stl_out; stl_out.filename = "box.stl"; stl_out.type = "stl"; stl_out.semantic_type = "Product"; stl_out.previewable = true;
        result.summary.previewable_outputs.push_back(stl_out);
    }

    // Phase C: Populate selectables for viewer interaction
    populate_selectables_from_artifact(result, step_path);

    result.success = true;
    result.confidence = 0.9; // Geometry-only confidence (no reasoning yet)
    return result;
}

} // namespace praxis
