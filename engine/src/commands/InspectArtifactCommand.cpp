#include "ArtifactCommandCommon.hpp"
#include "praxis/ArtifactSchema.hpp"
#include <iostream>

namespace praxis {
namespace commands {

namespace {

nlohmann::json summarize_artifact(artifacts::ArtifactType type, const nlohmann::json& document) {
    if (type == artifacts::ArtifactType::MachineDefinition) {
        return {
            {"artifactType", "machine-definition"},
            {"machineId", document.at("machineId")},
            {"machineName", document.at("machineName")},
            {"units", document.at("units")},
            {"bodyCount", document.at("kinematics").at("bodies").size()},
            {"jointCount", document.at("kinematics").at("joints").size()},
            {"frameCount", document.at("kinematics").at("frames").size()}
        };
    }
    if (type == artifacts::ArtifactType::CapabilityProfile) {
        return {
            {"artifactType", "capability-profile"},
            {"capabilityId", document.at("capabilityId")},
            {"capabilityVersion", document.at("capabilityVersion")},
            {"machineDefinitionRef", document.at("machineDefinitionRef")},
            {"jointCount", document.at("kinematicsSummary").at("jointCount")}
        };
    }
    return {
        {"artifactType", "manifest"},
        {"bundleId", document.at("bundleId")},
        {"artifactCount", document.at("artifacts").size()}
    };
}

} // namespace

int handle_inspect_artifact(const std::string& artifact_type, const std::string& input_path, bool json_output) {
    auto parsed_type = artifacts::parse_artifact_type(artifact_type);
    if (!parsed_type.has_value()) {
        detail::emit_error_json("UnknownArtifactType", "Unsupported artifact type", "InspectArtifact", {{"artifactType", artifact_type}});
        return 2;
    }

    nlohmann::json document;
    auto validation = artifacts::validate_artifact_file(*parsed_type, input_path, &document);
    if (!validation.valid) {
        detail::emit_error_json(
            "ArtifactInspectionFailed",
            "Artifact failed validation before inspection",
            "InspectArtifact",
            {{"inputPath", input_path}, {"issues", detail::issues_to_json(validation.issues)}}
        );
        return 3;
    }

    nlohmann::json summary = summarize_artifact(*parsed_type, document);
    std::cout << summary.dump(2) << "\n";
    return 0;
}

} // namespace commands
} // namespace praxis
