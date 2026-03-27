/**
 * FailureMessages.hpp
 * 
 * Sprint 10 - EPIC 3 Task 3.2: Failure Narrative Standardization
 * 
 * Purpose: Bounded, deterministic human-readable failure messages
 * Contract: One sentence maximum, no internal stack traces, 1:1 mapping with failure types
 * 
 * All failure messages must be:
 * - Deterministic (same failure type → same message template)
 * - Bounded (one sentence, no multi-line)
 * - Traceable (reference contract documents when appropriate)
 * - User-facing (no internal implementation details)
 */

#pragma once

#include <string>
#include <map>
#include <cstdio>
#include <algorithm>

namespace praxis {
namespace failures {

/**
 * Sanitize context string to prevent multi-sentence messages
 * Replaces newlines with spaces, ensures single sentence
 */
inline std::string sanitize_context(const std::string& context) {
    std::string result = context;
    std::replace(result.begin(), result.end(), '\n', ' ');
    std::replace(result.begin(), result.end(), '\r', ' ');
    // Trim trailing whitespace
    while (!result.empty() && std::isspace(static_cast<unsigned char>(result.back()))) {
        result.pop_back();
    }
    return result;
}

/**
 * Get standardized failure message for a given failure type
 * Returns deterministic, bounded message per Sprint 10 contract
 */
inline std::string get_failure_message(const std::string& failure_type, 
                                       const std::string& raw_context = "") {    std::string context = sanitize_context(raw_context);
        // Selection mode failures (per selection_modes.md)
    if (failure_type == "InvalidSelectionMode") {
        return "Selection mode violation: " + context;
    }
    if (failure_type == "InvalidSelector") {
        return "Invalid selector syntax" + (context.empty() ? "." : ": " + context);
    }
    if (failure_type == "InvalidBodySelector") {
        return "Invalid body selector syntax" + (context.empty() ? "." : ": " + context);
    }
    if (failure_type == "InvalidFaceSelector") {
        return "Invalid face selector syntax" + (context.empty() ? "." : ": " + context);
    }
    if (failure_type == "InvalidEdgeSelector") {
        return "Invalid edge selector syntax" + (context.empty() ? "." : ": " + context);
    }
    if (failure_type == "NoFacesMatched" || failure_type == "NoEdgesMatched" || 
        failure_type == "Missing") {
        return "No entities matched the selector criteria.";
    }
    if (failure_type == "Ambiguous" || failure_type == "AmbiguousSelection") {
        return "Multiple entities matched the selector. Use more specific criteria.";
    }
    if (failure_type == "BodyIndexOutOfRange") {
        return "Body index out of range" + (context.empty() ? "." : ": " + context);
    }
    
    // Artifact and loading failures
    if (failure_type == "ArtifactLoadFailure") {
        return "Failed to load artifact" + (context.empty() ? "." : ": " + context);
    }
    if (failure_type == "EmptyProduct") {
        return "Product contains no bodies.";
    }
    
    // Selection payload failures
    if (failure_type == "InvalidSelectionPayload") {
        return "Invalid selection data payload" + (context.empty() ? "." : ": " + context);
    }
    
    // Reference resolution failures
    if (failure_type == "ResolutionFailure") {
        return "Failed to resolve reference" + (context.empty() ? "." : ": " + context);
    }
    if (failure_type == "ReferenceNotFound") {
        return "Reference not found in artifact.";
    }
    if (failure_type == "AmbiguousReference") {
        return "Reference matched multiple entities. Provide more specific signature.";
    }
    
    // Cache failures
    if (failure_type == "CacheMiss") {
        return "Cache entry not found.";
    }
    if (failure_type == "CacheCorruption") {
        return "Cache entry is corrupted or incomplete.";
    }
    if (failure_type == "CacheLockTimeout") {
        return "Could not acquire cache lock within timeout period.";
    }
    
    // Execution failures
    if (failure_type == "ExecutionFailure") {
        return "Operation execution failed" + (context.empty() ? "." : ": " + context);
    }
    if (failure_type == "KernelOpFailure") {
        return "Kernel operation failed" + (context.empty() ? "." : ": " + context);
    }
    
    // Validation failures
    if (failure_type == "ValidationFailure") {
        return "Validation failed" + (context.empty() ? "." : ": " + context);
    }
    if (failure_type == "InvalidParameter") {
        return "Invalid parameter value" + (context.empty() ? "." : ": " + context);
    }
    if (failure_type == "MissingParameter") {
        return "Required parameter missing" + (context.empty() ? "." : ": " + context);
    }
    
    // Recipe failures
    if (failure_type == "RecipeLoadFailure") {
        return "Failed to load recipe" + (context.empty() ? "." : ": " + context);
    }
    if (failure_type == "RecipeParseError") {
        return "Recipe syntax error" + (context.empty() ? "." : ": " + context);
    }
    if (failure_type == "RecipeValidationFailure") {
        return "Recipe validation failed" + (context.empty() ? "." : ": " + context);
    }
    
    // Kinematics failures
    if (failure_type == "PKMLoadFailure") {
        return "Failed to load PKM file" + (context.empty() ? "." : ": " + context);
    }
    if (failure_type == "PKMValidationFailure") {
        return "PKM validation failed" + (context.empty() ? "." : ": " + context);
    }
    
    // IO failures
    if (failure_type == "FileNotFound") {
        return "File not found" + (context.empty() ? "." : ": " + context);
    }
    if (failure_type == "FileReadError") {
        return "Failed to read file" + (context.empty() ? "." : ": " + context);
    }
    if (failure_type == "FileWriteError") {
        return "Failed to write file" + (context.empty() ? "." : ": " + context);
    }
    
    // STEP IO failures
    if (failure_type == "StepReadFailure") {
        return "Failed to read STEP file" + (context.empty() ? "." : ": " + context);
    }
    if (failure_type == "StepWriteFailure") {
        return "Failed to write STEP file" + (context.empty() ? "." : ": " + context);
    }
    
    // Generic fallback
    return "Operation failed" + (context.empty() ? "." : ": " + context);
}

/**
 * Get standardized failure message with format string support
 * Example: get_failure_message_fmt("BodyIndexOutOfRange", "index={}, max={}", 5, 3)
 */
template<typename... Args>
std::string get_failure_message_fmt(const std::string& failure_type, 
                                    const std::string& fmt, 
                                    Args... args) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), fmt.c_str(), args...);
    return get_failure_message(failure_type, std::string(buffer));
}

/**
 * Failure type registry for documentation and validation
 * Maps failure types to their canonical category
 */
inline std::map<std::string, std::string> get_failure_type_categories() {
    return {
        // Selection failures
        {"InvalidSelectionMode", "Selection"},
        {"InvalidSelector", "Selection"},
        {"NoFacesMatched", "Selection"},
        {"NoEdgesMatched", "Selection"},
        {"Missing", "Selection"},
        {"Ambiguous", "Selection"},
        {"AmbiguousSelection", "Selection"},
        {"BodyIndexOutOfRange", "Selection"},
        {"EmptyProduct", "Selection"},
        {"InvalidSelectionPayload", "Selection"},
        
        // Reference failures
        {"ResolutionFailure", "Reference"},
        {"ReferenceNotFound", "Reference"},
        {"AmbiguousReference", "Reference"},
        
        // Artifact failures
        {"ArtifactLoadFailure", "Artifact"},
        
        // Cache failures
        {"CacheMiss", "Cache"},
        {"CacheCorruption", "Cache"},
        {"CacheLockTimeout", "Cache"},
        
        // Execution failures
        {"ExecutionFailure", "Execution"},
        {"KernelOpFailure", "Execution"},
        
        // Validation failures
        {"ValidationFailure", "Validation"},
        {"InvalidParameter", "Validation"},
        {"MissingParameter", "Validation"},
        
        // Recipe failures
        {"RecipeLoadFailure", "Recipe"},
        {"RecipeParseError", "Recipe"},
        {"RecipeValidationFailure", "Recipe"},
        
        // Kinematics failures
        {"PKMLoadFailure", "Kinematics"},
        {"PKMValidationFailure", "Kinematics"},
        
        // IO failures
        {"FileNotFound", "IO"},
        {"FileReadError", "IO"},
        {"FileWriteError", "IO"},
        {"StepReadFailure", "IO"},
        {"StepWriteFailure", "IO"}
    };
}

} // namespace failures
} // namespace praxis
