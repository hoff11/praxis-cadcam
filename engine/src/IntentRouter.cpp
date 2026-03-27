#include "praxis/IntentRouter.hpp"
#include "praxis/Report.hpp"
#include <iostream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <sys/stat.h>
#include <sys/types.h>

namespace fs = std::filesystem;

namespace praxis {

IntentResult IntentRouter::execute(const IntentRequest& request) {
    std::cout << "Intent: " << request.intent_name << "\n";
    std::cout << "Output dir: " << request.output_dir << "\n\n";
    
    // Ensure output directory exists
    if (!ensureOutputDir(request.output_dir)) {
        IntentResult result;
        result.success = false;
        result.errors.push_back("Failed to create output directory");
        return result;
    }
    
    // Route to appropriate handler
    IntentResult result;
    if (request.intent_name == "GenerateMachine3AxisVMC") {
        result = executeGenerateVMC(request);
    } else if (request.intent_name == "HealAndNormalize") {
        result = executeHeal(request);
    } else if (request.intent_name == "BuildFromRecipe") {
        result = executeBuildFromRecipe(request);
    } else if (request.intent_name == "CreateBox") {
        std::cout << "Executing CreateBox (micro-intent)...\n";
        extern IntentResult createBoxIntent(const IntentRequest& request);
        result = createBoxIntent(request);
    } else if (request.intent_name == "AttachFaceToFace") {
        std::cout << "Executing AttachFaceToFace (micro-intent)...\n";
        extern IntentResult attachFaceToFaceIntent(const IntentRequest& request);
        result = attachFaceToFaceIntent(request);
    } else {
        result.success = false;
        result.errors.push_back("Unknown intent: " + request.intent_name);
        return result;
    }
    
    // Normalize artifact paths to canonical form
    for (auto& artifact : result.artifacts) {
        try {
            artifact = fs::weakly_canonical(artifact).string();
        } catch (...) {
            // If canonicalization fails, keep original path
        }
    }
    
    // Universal previewable_outputs: ensure all intents populate summary for UI
    if (result.summary.previewable_outputs.empty() && !result.artifacts.empty()) {
        for (const auto& artifact_path : result.artifacts) {
            PreviewableOutput output;
            output.filename = fs::path(artifact_path).filename().string();
            
            // Detect type from extension
            auto ext = fs::path(artifact_path).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".stl") {
                output.type = "stl";
            } else if (ext == ".step" || ext == ".stp") {
                output.type = "step";
            } else if (ext == ".brep") {
                output.type = "brep";
            } else {
                output.type = "file";
            }
            
            output.semantic_type = "Product"; // Conservative default
            output.previewable = (output.type == "stl" || output.type == "step" || output.type == "brep");
            result.summary.previewable_outputs.push_back(output);
        }
    }
    
    // Do NOT auto-populate execution_timestamp for byte-stability
    // Timestamp should only be set if explicitly requested (e.g., debug mode)
    // This preserves determinism across runs
    
    return result;
}

IntentResult IntentRouter::executeGenerateVMC(const IntentRequest& request) {
    std::cout << "Executing GenerateMachine3AxisVMC...\n";
    
    // Forward declare - implementation in separate file
    extern IntentResult generateMachine3AxisVMC(const IntentRequest& request);
    return generateMachine3AxisVMC(request);
}

IntentResult IntentRouter::executeHeal(const IntentRequest& request) {
    std::cout << "Executing HealAndNormalize...\n";
    
    // Forward declare - implementation in separate file
    extern IntentResult healAndNormalize(const IntentRequest& request);
    return healAndNormalize(request);
}

IntentResult IntentRouter::executeBuildFromRecipe(const IntentRequest& request) {
    std::cout << "Executing BuildFromRecipe...\n";
    
    // Forward declare - implementation in separate file
    extern IntentResult buildFromRecipe(const IntentRequest& request);
    return buildFromRecipe(request);
}

bool IntentRouter::ensureOutputDir(const std::string& path) {
    try {
        fs::create_directories(path);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error creating directory: " << e.what() << "\n";
        return false;
    }
}

} // namespace praxis
