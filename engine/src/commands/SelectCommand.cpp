/**
 * SelectCommand.cpp
 * Command handler for `praxis-cad select <artifact> <selector> --json [--include-selection]`
 */

#include "praxis/Intent.hpp"
#include "praxis/CanonicalFormat.hpp"
#include "InteractionEmit.hpp"
#include "OCCTInspector.hpp"
#include "Selector.hpp"
#include "Reference.hpp"
#include "SemanticTypes.hpp"
#include "FailureMessages.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace praxis {
namespace commands {

int handle_select(const std::string& artifact_path, const std::string& selector_str, 
                  bool json_output, bool include_selection, std::ostream& out) {
    using namespace praxis::selector;
    using namespace praxis::inspection;
    using namespace praxis::reference;
    using namespace praxis::semantic;
    
    // EPIC 1 Task 1.3: Validate selection mode before artifact loading
    // Detect mode from selector syntax per selection_modes.md
    auto detected_mode = detect_mode_from_selector(selector_str);
    if (!detected_mode) {
        IntentResult result;
        result.success = false;
        IntentResult::InteractionInfo::Failure failure;
        failure.type = "InvalidSelector";
        failure.message = failures::get_failure_message("InvalidSelector", 
                                                        "Cannot determine selection mode from selector: " + selector_str);
        result.interaction.failures.push_back(failure);
        
        out << "{\n";
        out << "  \"success\": false,\n";
        out << "  \"failures\": [{\"type\":\"InvalidSelector\",\"message\":\"" 
            << canonical::escape_json(failure.message) << "\"}]\n";
        out << "}\n";
        return 11;  // Invalid selector syntax exit code per selection_modes.md
    }
    
    // Load artifact
    OCCTInspector inspector;
    if (!inspector.load_artifact(artifact_path)) {
        IntentResult result;
        result.success = false;
        IntentResult::InteractionInfo::Failure failure;
        failure.type = "ArtifactLoadFailure";
        failure.message = failures::get_failure_message("ArtifactLoadFailure", artifact_path);
        result.interaction.failures.push_back(failure);
        
        out << "{\n";
        out << "  \"success\": false,\n";
        out << "  \"failures\": [{\"type\":\"ArtifactLoadFailure\",\"message\":\"" 
            << canonical::escape_json(failure.message) << "\"}]\n";
        out << "}\n";
        return 20;  // Artifact load failure exit code per selection_modes.md
    }
    
    // Execute selector
    SelectorExecutor executor(&inspector);
    auto selection_result = executor.select(selector_str);
    
    // Build IntentResult with interaction data using emit helpers
    IntentResult result;
    result.success = selection_result.success;
    result.interaction.contract_version = "1.0";
    result.interaction.selector_contract_version = "1.0";
    result.interaction.artifact_path = artifact_path;
    
    // Add selection entry with detected mode
    IntentResult::InteractionInfo::Selection sel;
    sel.mode = to_string(*detected_mode);  // Use detected mode
    sel.target = artifact_path;
    sel.selector = selector_str;
    
    // Get timestamp
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    tm = *std::gmtime(&t);
#endif
    std::ostringstream ts_oss;
    ts_oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    sel.timestamp = ts_oss.str();
    
    // Encode references from selected entities using proper reference model
    // EPIC 1 Task 1.3: Validate entity type is legal for the detected selection mode
    reference::ReferenceEncoder encoder(&inspector);
    for (const auto& entity : selection_result.references) {
        // Determine semantic type from entity type string
        SemanticType semantic_type;
        if (entity.type == "body") {
            semantic_type = SemanticType::Body;
        } else if (entity.type == "face") {
            semantic_type = SemanticType::Face;
        } else if (entity.type == "edge") {
            semantic_type = SemanticType::Edge;
        } else {
            // Unknown entity type
            IntentResult::InteractionInfo::Failure failure;
            failure.type = "InvalidSelectionPayload";
            failure.message = "Unknown entity type: " + entity.type;
            result.interaction.failures.push_back(failure);
            result.success = false;
            continue;
        }
        
        // Validate entity type is allowed in detected mode
        if (!is_selectable_in_mode(*detected_mode, semantic_type)) {
            IntentResult::InteractionInfo::Failure failure;
            failure.type = "InvalidSelectionMode";
            failure.message = get_invalid_mode_message(*detected_mode, semantic_type);
            result.interaction.failures.push_back(failure);
            result.success = false;
            continue;
        }
        
        // Encode based on entity type (body/face/edge)
        // Use safe any_cast to prevent crashes on type mismatches
        if (entity.type == "body") {
            // Safe cast with pointer variant
            auto* body_props = std::any_cast<inspection::BodyProperties>(&entity.properties);
            if (body_props) {
                auto body_ref = encoder.encode_body(*body_props);
                sel.references_json.push_back(body_ref.to_json());
                
                // EPIC 1.6: Emit selectable for viewer inventory
                // Use index-based label for now; stable_id computed via SHA-256 fallback
                std::string label = "Body " + std::to_string(body_props->index);
                emit_selectable(result, "BodyRef", stable_id::body(body_props->index), label);
            } else {
                // Emit typed failure for invalid payload
                IntentResult::InteractionInfo::Failure failure;
                failure.type = "InvalidSelectionPayload";
                failure.message = "Expected BodyProperties but got incompatible type";
                result.interaction.failures.push_back(failure);
            }
        } else if (entity.type == "face") {
            auto* face_props = std::any_cast<inspection::FaceProperties>(&entity.properties);
            if (face_props) {
                auto face_ref = encoder.encode_face(*face_props);
                sel.references_json.push_back(face_ref.to_json());
                
                // EPIC 1.6: Emit selectable with surface type in label
                std::string label = "Face " + std::to_string(face_props->index) + 
                                  " (" + inspection::surface_type_to_string(face_props->surface_type) + ")";
                emit_selectable(result, "FaceRef", stable_id::face(face_props->body_index, face_props->index), label);
            } else {
                IntentResult::InteractionInfo::Failure failure;
                failure.type = "InvalidSelectionPayload";
                failure.message = "Expected FaceProperties but got incompatible type";
                result.interaction.failures.push_back(failure);
            }
        } else if (entity.type == "edge") {
            auto* edge_props = std::any_cast<inspection::EdgeProperties>(&entity.properties);
            if (edge_props) {
                auto edge_ref = encoder.encode_edge(*edge_props);
                sel.references_json.push_back(edge_ref.to_json());
                
                // EPIC 1.6: Emit selectable with edge type in label
                std::string label = "Edge " + std::to_string(edge_props->index) + 
                                  " (" + inspection::edge_type_to_string(edge_props->edge_type) + ")";
                emit_selectable(result, "EdgeRef", stable_id::edge(edge_props->index, edge_props->adjacent_faces), label);
            } else {
                IntentResult::InteractionInfo::Failure failure;
                failure.type = "InvalidSelectionPayload";
                failure.message = "Expected EdgeProperties but got incompatible type";
                result.interaction.failures.push_back(failure);
            }
        }
    }
    
    result.interaction.selections.push_back(sel);
    
    // LEGACY: emit_selection_failure commented out - needs migration to C.5 ResolveResult
    // TODO: Refactor SelectCommand to use SelectorParser + SelectorResolver
    /*
    for (const auto& failure : selection_result.failures) {
        emit_selection_failure(result, failure);
    }
    */
    
    // Build output JSON
    std::ostringstream out_oss;
    out_oss << "{\n";
    out_oss << "  \"success\": " << (result.success ? "true" : "false") << ",\n";
    out_oss << "  \"selected_count\": " << selection_result.references.size() << ",\n";
    out_oss << "  \"failures\": [\n";
    
    for (size_t i = 0; i < result.interaction.failures.size(); ++i) {
        if (i > 0) out_oss << ",\n";
        const auto& f = result.interaction.failures[i];
        out_oss << "    {\"type\":\"" << canonical::escape_json(f.type) << "\",\"message\":\"" << canonical::escape_json(f.message) << "\"}";
    }
    
    out_oss << "\n  ]";
    
    if (include_selection) {
        out_oss << ",\n  \"selection\": {\n";
        out_oss << "    \"selector\": \"" << canonical::escape_json(selector_str) << "\",\n";
        out_oss << "    \"references\": [";
        for (size_t i = 0; i < sel.references_json.size(); ++i) {
            if (i > 0) out_oss << ", ";
            out_oss << sel.references_json[i];
        }
        out_oss << "]\n";
        out_oss << "  }";
    }
    
    out_oss << "\n}\n";
    
    out << out_oss.str();
    
    // Return distinct exit codes for different failure types per selection_modes.md
    if (result.success) {
        return 0;
    }
    
    // Check failure types to determine exit code
    for (const auto& failure : result.interaction.failures) {
        if (failure.type == "InvalidSelectionMode") {
            return 10;  // Invalid selection mode per selection_modes.md
        } else if (failure.type == "Ambiguous") {
            return 13;  // Ambiguous selection per selection_modes.md
        } else if (failure.type == "Missing") {
            return 12;  // No matches per selection_modes.md
        } else if (failure.type == "InvalidSelector" || failure.type == "InvalidSelectionPayload") {
            return 11;  // Invalid selector syntax per selection_modes.md
        } else if (failure.type == "ArtifactLoadFailure") {
            return 20;  // Artifact load failure per selection_modes.md
        }
    }
    
    return 1;  // Generic failure
}

} // namespace commands
} // namespace praxis
