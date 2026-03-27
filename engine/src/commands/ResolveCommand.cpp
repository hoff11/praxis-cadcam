/**
 * ResolveCommand.cpp
 * Command handler for `praxis-cad resolve <artifact> <reference_json> --json [--strict]`
 */

#include "praxis/Intent.hpp"
#include "praxis/CanonicalFormat.hpp"
#include "InteractionEmit.hpp"
#include "OCCTInspector.hpp"
#include "Reference.hpp"
#include "InteractionPublic.hpp"
#include <iostream>
#include <sstream>

namespace praxis {
namespace commands {

int handle_resolve(const std::string& artifact_path, const std::string& reference_json_str,
                   bool json_output, bool strict_mode, std::ostream& out) {
    using namespace praxis::inspection;
    using namespace praxis::reference;
    
    // Parse reference discriminator first (before loading artifact for early validation)
    // Extract ref_type discriminator
    std::string ref_type;
    size_t ref_type_pos = reference_json_str.find("\"ref_type\"");
    if (ref_type_pos == std::string::npos) {
        std::cerr << "Error: InvalidReference - missing 'ref_type' field\n";
        return 4;
    }
    
    // Simple extraction (assumes valid JSON structure)
    size_t colon_pos = reference_json_str.find(":", ref_type_pos);
    size_t quote1 = reference_json_str.find("\"", colon_pos);
    size_t quote2 = reference_json_str.find("\"", quote1 + 1);
    if (quote1 != std::string::npos && quote2 != std::string::npos) {
        ref_type = reference_json_str.substr(quote1 + 1, quote2 - quote1 - 1);
    }
    
    if (ref_type.empty()) {
        std::cerr << "Error: InvalidReference - malformed 'ref_type' field\n";
        return 4;
    }
    
    // Validate ref_type is known
    if (ref_type != "BodyRef" && ref_type != "FaceRef" && ref_type != "EdgeRef") {
        std::cerr << "Error: InvalidReference - unknown ref_type '" << ref_type << "'\n";
        return 4;
    }
    
    // Now load artifact (after validating reference format)
    OCCTInspector inspector;
    if (!inspector.load_artifact(artifact_path)) {
        std::cerr << "Error: Failed to load artifact: " << artifact_path << "\n";
        return 1;
    }
    
    // Parse reference from JSON using discriminator-based dispatch
    // Strategy: Dispatch to correct parser based on ref_type
    
    // Dispatch to correct parser based on discriminator
    ResolutionResult resolution;
    std::string ref_contract_version;
    bool parsed_successfully = false;
    
    if (ref_type == "BodyRef") {
        auto body_ref_opt = BodyRef::from_json(reference_json_str);
        if (body_ref_opt.has_value()) {
            parsed_successfully = true;
            BodyRef ref = body_ref_opt.value();
            ref_contract_version = ref.contract_version;
            
            // Enforce contract version
            if (ref.contract_version != CONTRACT_VERSION) {
                resolution.status = ResolutionStatus::ContractMismatch;
                resolution.message = "Contract version mismatch: expected " + 
                                   std::string(CONTRACT_VERSION) + ", got " + ref.contract_version;
            } else {
                ReferenceResolver resolver(&inspector);
                resolution = resolver.resolve(ref);
            }
        }
    } else if (ref_type == "FaceRef") {
        auto face_ref_opt = FaceRef::from_json(reference_json_str);
        if (face_ref_opt.has_value()) {
            parsed_successfully = true;
            FaceRef ref = face_ref_opt.value();
            ref_contract_version = ref.contract_version;
            
            if (ref.contract_version != CONTRACT_VERSION) {
                resolution.status = ResolutionStatus::ContractMismatch;
                resolution.message = "Contract version mismatch: expected " + 
                                   std::string(CONTRACT_VERSION) + ", got " + ref.contract_version;
            } else {
                ReferenceResolver resolver(&inspector);
                resolution = resolver.resolve(ref);
            }
        }
    } else if (ref_type == "EdgeRef") {
        auto edge_ref_opt = EdgeRef::from_json(reference_json_str);
        if (edge_ref_opt.has_value()) {
            parsed_successfully = true;
            EdgeRef ref = edge_ref_opt.value();
            ref_contract_version = ref.contract_version;
            
            if (ref.contract_version != CONTRACT_VERSION) {
                resolution.status = ResolutionStatus::ContractMismatch;
                resolution.message = "Contract version mismatch: expected " + 
                                   std::string(CONTRACT_VERSION) + ", got " + ref.contract_version;
            } else {
                ReferenceResolver resolver(&inspector);
                resolution = resolver.resolve(ref);
            }
        }
    }
    
    if (!parsed_successfully) {
        std::cerr << "Error: InvalidReference - failed to parse " << ref_type << " (malformed JSON)\n";
        return 4;
    }
    
    // Build IntentResult with interaction data using emit helpers
    IntentResult result;
    result.success = (resolution.status == ResolutionStatus::Success);
    result.interaction.contract_version = "1.0";
    result.interaction.artifact_path = artifact_path;
    
    // For ContractMismatch, emit typed failure
    if (resolution.status == ResolutionStatus::ContractMismatch) {
        IntentResult::InteractionInfo::Failure failure;
        failure.type = "ContractMismatch";
        // Include contract version details in message
        std::ostringstream msg;
        msg << resolution.message;
        if (!ref_contract_version.empty()) {
            msg << " (got: " << ref_contract_version << ", expected: " << CONTRACT_VERSION << ")";
        }
        failure.message = msg.str();
        result.interaction.failures.push_back(failure);
    }
    
    // Use emit helper to add resolution with public mapping
    emit_resolution_public(result, resolution);
    
    // Build output JSON using public status names
    auto public_status = interaction_public::map_resolution_status(resolution.status);
    std::ostringstream out_oss;
    out_oss << "{\n";
    out_oss << "  \"status\": \"" << interaction_public::to_string(public_status) << "\",\n";
    out_oss << "  \"message\": \"" << canonical::escape_json(resolution.message) << "\",\n";
    out_oss << "  \"resolutions\": [";
    
    if (!result.interaction.resolutions_json.empty()) {
        out_oss << result.interaction.resolutions_json[0];
    }
    
    out_oss << "],\n";
    out_oss << "  \"failures\": [";
    
    for (size_t i = 0; i < result.interaction.failures.size(); ++i) {
        if (i > 0) out_oss << ", ";
        const auto& f = result.interaction.failures[i];
        out_oss << "{\"type\":\"" << canonical::escape_json(f.type) << "\",\"message\":\"" << canonical::escape_json(f.message) << "\"}";
    }
    
    out_oss << "]\n";
    out_oss << "}\n";
    
    out << out_oss.str();
    
    // Return distinct exit codes for different failure types
    if (result.success) {
        return 0;
    } else if (resolution.status == ResolutionStatus::ContractMismatch) {
        return 5;  // Contract version mismatch
    } else if (resolution.status == ResolutionStatus::Missing) {
        return 2;  // Geometry removed
    } else if (resolution.status == ResolutionStatus::Ambiguous) {
        return 3;  // Multiple matches
    } else if (resolution.status == ResolutionStatus::Drifted) {
        return 6;  // Signature drifted beyond tolerance
    } else {
        return 1;  // Generic failure
    }
}

} // namespace commands
} // namespace praxis
