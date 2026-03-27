/**
 * ResolveSelectorCommand.cpp
 * 
 * Phase C.5: Resolve selector against artifact topology
 * 
 * Usage:
 *   praxis-cad resolve-selector <artifact> "<selector>" [--json]
 */

#include "SelectorResolver.hpp"
#include "Selector.hpp"
#include "Inspection.hpp"
#include "praxis/CanonicalFormat.hpp"
#include <iostream>
#include <iomanip>

namespace praxis {
namespace commands {

int handle_resolve_selector(const std::string& artifact_path, 
                            const std::string& selector_expr,
                            bool json_output) {
    using namespace praxis::selector;
    using namespace praxis::inspection;
    using namespace praxis::canonical;
    
    // Exit codes: 0=success, 1=parse_error, 2=load_error, 3=no_matches, 4=property_error, 5=type_error
    
    // Step 1: Parse selector
    ParseResult parse_result = SelectorParser::parse(selector_expr);
    
    if (!parse_result.selector.has_value()) {
        if (json_output) {
            std::cout << "{\n";
            std::cout << "  \"success\": false,\n";
            std::cout << "  \"parse_error\": " << SelectorParser::error_to_json(*parse_result.error) << "\n";
            std::cout << "}\n";
        } else {
            std::cout << "✗ Parse failed\n\n";
            const auto& err = *parse_result.error;
            std::cout << "Error:   " << to_string(err.kind) << "\n";
            std::cout << "Message: " << err.message << "\n";
        }
        return 1;  // Exit code: parse error
    }
    
    const Selector& selector = *parse_result.selector;
    
    // Step 2: Load artifact
    auto inspector = create_inspector();
    if (!inspector->load_artifact(artifact_path)) {
        if (json_output) {
            std::cout << "{\n";
            std::cout << "  \"success\": false,\n";
            std::cout << "  \"error\": \"Failed to load artifact: " << escape_json(artifact_path) << "\"\n";
            std::cout << "}\n";
        } else {
            std::cout << "✗ Failed to load artifact: " << artifact_path << "\n";
        }
        return 2;  // Exit code: artifact load error
    }
    
    // Step 3: Resolve selector
    SelectorResolver resolver(*inspector);
    ResolveResult result = resolver.resolve(selector);
    
    // Step 4: Output results
    if (json_output) {
        std::cout << "{\n";
        std::cout << "  \"success\": " << (result.success() ? "true" : "false") << ",\n";
        std::cout << "  \"selector\": \"" << escape_json(selector.canonical) << "\",\n";
        std::cout << "  \"artifact\": \"" << escape_json(artifact_path) << "\",\n";
        
        if (result.success()) {
            std::cout << "  \"match_count\": " << result.match_count << ",\n";
            std::cout << "  \"stable_ids\": [\n";
            for (size_t i = 0; i < result.stable_ids.size(); ++i) {
                std::cout << "    \"" << result.stable_ids[i] << "\"";
                if (i + 1 < result.stable_ids.size()) std::cout << ",";
                std::cout << "\n";
            }
            std::cout << "  ]\n";
        } else {
            const auto& err = *result.error;
            std::cout << "  \"error\": {\n";
            std::cout << "    \"kind\": \"" << escape_json(to_string(err.kind)) << "\",\n";
            std::cout << "    \"message\": \"" << escape_json(err.message) << "\"\n";
            std::cout << "  }\n";
        }
        
        std::cout << "}\n";
    } else {
        // Human-readable output
        if (result.success()) {
            std::cout << "✓ Resolution successful\n\n";
            std::cout << "Selector:  " << selector.canonical << "\n";
            std::cout << "Artifact:  " << artifact_path << "\n";
            std::cout << "Matches:   " << result.match_count << "\n\n";
            
            std::cout << "Stable IDs:\n";
            int display_limit = std::min(10, result.match_count);
            for (int i = 0; i < display_limit; ++i) {
                std::cout << "  " << (i + 1) << ". " << result.stable_ids[i] << "\n";
            }
            if (result.match_count > display_limit) {
                std::cout << "  ... and " << (result.match_count - display_limit) << " more\n";
            }
        } else {
            std::cout << "✗ Resolution failed\n\n";
            const auto& err = *result.error;
            std::cout << "Selector: " << selector.canonical << "\n";
            std::cout << "Error:    " << to_string(err.kind) << "\n";
            std::cout << "Message:  " << err.message << "\n";
        }
    }
    
    // Map error kinds to distinct exit codes
    if (result.success()) {
        return 0;
    }
    
    const auto& err = *result.error;
    switch (err.kind) {
        case ResolveErrorKind::NoMatches:
            return 3;  // No topology matched
        case ResolveErrorKind::PropertyNotFound:
            return 4;  // Unknown property in filter
        case ResolveErrorKind::TypeMismatch:
            return 5;  // Type incompatibility
        case ResolveErrorKind::InspectorError:
            return 6;  // Topology query failure
        default:
            return 1;  // Generic error
    }
}

}  // namespace commands
}  // namespace praxis
