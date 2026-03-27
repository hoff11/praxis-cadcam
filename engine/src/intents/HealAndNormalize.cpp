#include "praxis/Intent.hpp"
#include "../kernel/KernelOps.hpp"
#include "../kernel/StepIO.hpp"
#include "../kernel/StlIO.hpp"
#include <TopExp_Explorer.hxx>
#include <TopAbs.hxx>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>

namespace praxis {

IntentResult healAndNormalize(const IntentRequest& request) {
    IntentResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        // Validate input file exists
        if (request.input_step_path.empty()) {
            result.success = false;
            result.errors.push_back("No input STEP file specified");
            result.confidence = 0.0;
            return result;
        }
        
        std::ifstream infile(request.input_step_path);
        if (!infile.good()) {
            result.success = false;
            result.errors.push_back("Input file does not exist: " + request.input_step_path);
            result.confidence = 0.0;
            return result;
        }
        
        std::cout << "Reading STEP file: " << request.input_step_path << "\n";
        
        // Read STEP file using kernel layer
        using namespace praxis::kernel;
        auto read_result = StepIO::read_step(request.input_step_path);
        
        if (!read_result.success) {
            result.success = false;
            result.errors.push_back("Failed to read STEP file: " + read_result.error_message);
            result.confidence = 0.0;
            return result;
        }
        
        // Collect read operations
        result.kernel_ops.insert(result.kernel_ops.end(),
            read_result.operations.begin(), read_result.operations.end());
        
        kernel::ShapeHandle shape = read_result.shape;
        std::cout << "✓ STEP file read successfully\n";
        
        // Validate shape topology BEFORE healing using kernel layer
        bool isValidBefore = KernelOps::validate(shape);
        
        std::cout << "Shape validation (before): " << (isValidBefore ? "VALID" : "INVALID") << "\n";
        result.metrics["valid_before"] = isValidBefore ? "true" : "false";
        
        // Attempt healing using kernel layer
        std::cout << "Running heal...\n";
        auto heal_result = KernelOps::heal(shape);
        
        if (!heal_result.success) {
            result.warnings.push_back("Healing failed: " + heal_result.error_message + ", using original");
            heal_result.shape = shape;
        }
        
        // Collect heal operations
        result.kernel_ops.insert(result.kernel_ops.end(),
            heal_result.operations.begin(), heal_result.operations.end());
        
        // Validate shape topology AFTER healing using kernel layer
        bool isValidAfter = KernelOps::validate(heal_result.shape);
        
        std::cout << "Shape validation (after): " << (isValidAfter ? "VALID" : "INVALID") << "\n";
        result.metrics["valid_after"] = isValidAfter ? "true" : "false";
        
        // TODO: Count topology elements would require OCCT access through Layer 1 API
        // For now, skip topology counting to maintain layer boundary
        /*
        int numSolids = 0, numShells = 0, numFaces = 0, numEdges = 0, numVertices = 0;
        for (TopExp_Explorer exp(healedShape, TopAbs_SOLID); exp.More(); exp.Next()) numSolids++;
        for (TopExp_Explorer exp(healedShape, TopAbs_SHELL); exp.More(); exp.Next()) numShells++;
        for (TopExp_Explorer exp(healedShape, TopAbs_FACE); exp.More(); exp.Next()) numFaces++;
        for (TopExp_Explorer exp(healedShape, TopAbs_EDGE); exp.More(); exp.Next()) numEdges++;
        for (TopExp_Explorer exp(healedShape, TopAbs_VERTEX); exp.More(); exp.Next()) numVertices++;
        
        result.metrics["num_solids"] = std::to_string(numSolids);
        result.metrics["num_shells"] = std::to_string(numShells);
        result.metrics["num_faces"] = std::to_string(numFaces);
        result.metrics["num_edges"] = std::to_string(numEdges);
        result.metrics["num_vertices"] = std::to_string(numVertices);
        */
        
        // Report healing outcome
        if (!isValidBefore && isValidAfter) {
            result.warnings.push_back("Shape was invalid, ShapeFix_Shape resolved issues");
        } else if (isValidBefore && !isValidAfter) {
            result.warnings.push_back("Warning: Shape became invalid after healing (rare)");
        } else if (!isValidBefore && !isValidAfter) {
            result.warnings.push_back("Shape remains invalid after healing - may need manual repair");
        }
        
        // Export healed STEP using kernel layer
        std::string outputPath = request.output_dir + "/healed.step";
        std::cout << "Writing healed STEP to: " << outputPath << "\n";
        
        auto write_result = StepIO::write_step(heal_result.shape, outputPath);
        
        if (!write_result.success) {
            result.success = false;
            result.errors.push_back("Failed to write STEP: " + write_result.error_message);
            result.confidence = 0.0;
            return result;
        }
        
        // Collect write operations
        result.kernel_ops.insert(result.kernel_ops.end(),
            write_result.operations.begin(), write_result.operations.end());
        
        std::cout << "✓ Healed STEP exported\n";
        
        // Export STL for preview
        std::string stlPath = request.output_dir + "/healed.stl";
        auto stl_result = kernel::StlIO::write_stl(heal_result.shape, stlPath, 0.5);
        
        if (stl_result.success) {
            result.artifacts.push_back(stlPath);
            result.kernel_ops.insert(result.kernel_ops.end(),
                stl_result.operations.begin(), stl_result.operations.end());
            std::cout << "✓ STL exported for preview\n";
        } else {
            result.warnings.push_back("STL export failed (non-fatal): " + stl_result.error_message);
        }
        
        // Success - adjust confidence based on validation
        result.success = true;
        result.artifacts.push_back(outputPath);
        
        // Confidence scoring based on healing outcome:
        // - 0.85 if was invalid and now valid (real fix)
        // - 0.75 if was valid and remains valid (no damage)
        // - 0.4 if remains invalid (needs more work)
        // - 0.5 if became invalid (rare, conservative)
        if (!isValidBefore && isValidAfter) {
            result.confidence = 0.85;  // Successfully healed
        } else if (isValidBefore && isValidAfter) {
            result.confidence = 0.75;  // Already valid, preserved
        } else if (isValidBefore && !isValidAfter) {
            result.confidence = 0.50;  // Degraded (rare)
        } else {
            result.confidence = 0.40;  // Still invalid
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
