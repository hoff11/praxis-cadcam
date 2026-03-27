/**
 * InspectCommand.cpp
 * Command handler for `praxis-cad inspect <artifact> --json`
 */

#include "praxis/Intent.hpp"
#include "InteractionEmit.hpp"
#include "OCCTInspector.hpp"
#include "Inspection.hpp"
#include "praxis/CanonicalFormat.hpp"
#include <iostream>
#include <sstream>

namespace praxis {
namespace commands {

int handle_inspect(const std::string& artifact_path, bool json_output) {
    using namespace praxis::inspection;
    
    // Load artifact
    OCCTInspector inspector;
    if (!inspector.load_artifact(artifact_path)) {
        std::cerr << "Error: Failed to load artifact: " << artifact_path << "\n";
        return 1;
    }
    
    // Enumerate bodies
    auto bodies = inspector.enumerate_bodies();
    
    // Build inspection JSON
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"contract_version\": \"1.0\",\n";
    oss << "  \"artifact_path\": \"" << canonical::escape_json(artifact_path) << "\",\n";
    oss << "  \"body_count\": " << bodies.size() << ",\n";
    oss << "  \"bodies\": [\n";
    
    for (size_t i = 0; i < bodies.size(); ++i) {
        const auto& body = bodies[i];
        if (i > 0) oss << ",\n";
        oss << "    {\n";
        oss << "      \"index\": " << body.index << ",\n";
        oss << "      \"volume\": " << body.volume << "\n";
        oss << "    }";
    }
    
    oss << "\n  ]\n";
    oss << "}\n";
    
    std::string inspection_json = oss.str();
    
    if (json_output) {
        std::cout << inspection_json;
    } else {
        std::cout << "Inspection complete: " << bodies.size() << " bodies\n";
        std::cout << inspection_json;
    }
    
    return 0;
}

} // namespace commands
} // namespace praxis
