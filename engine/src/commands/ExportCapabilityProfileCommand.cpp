#include "ArtifactCommandCommon.hpp"
#include "praxis/ArtifactSchema.hpp"
#include <iostream>

namespace praxis {
namespace commands {

namespace {

std::string derive_capability_version(const nlohmann::json& machine_definition) {
    if (machine_definition.contains("metadata") && machine_definition["metadata"].is_object()) {
        const auto& metadata = machine_definition["metadata"];
        if (metadata.contains("capabilityVersion") && metadata["capabilityVersion"].is_string())
            return metadata["capabilityVersion"].get<std::string>();
    }
    return "1.0.0";
}

nlohmann::json build_source_provenance(const nlohmann::json& machine_definition) {
    nlohmann::json source = nlohmann::json::object();
    if (machine_definition.contains("source") && machine_definition["source"].is_object()) {
        const auto& md_source = machine_definition["source"];
        if (md_source.contains("inputType")) source["inputType"] = md_source["inputType"];
        if (md_source.contains("inputPath")) source["inputPath"] = md_source["inputPath"];
        if (md_source.contains("pkmVersion")) source["pkmVersion"] = md_source["pkmVersion"];
    }

    nlohmann::json fidelity = nlohmann::json::object();
    nlohmann::json constraint_source = nlohmann::json::object();
    if (machine_definition.contains("metadata") && machine_definition["metadata"].is_object()) {
        const auto& md = machine_definition["metadata"];
        if (md.contains("fidelity")) fidelity["level"] = md["fidelity"];
        if (md.contains("truthLevel")) fidelity["truthLevel"] = md["truthLevel"];
        if (md.contains("constraintSource")) constraint_source["source"] = md["constraintSource"];
        if (md.contains("constraintEvidence")) constraint_source["evidence"] = md["constraintEvidence"];
        if (md.contains("lastAlignedAt")) constraint_source["lastAlignedAt"] = md["lastAlignedAt"];
    }

    nlohmann::json provenance = {
        {"machineDefinitionSource", source}
    };
    if (!fidelity.empty()) provenance["fidelity"] = fidelity;
    if (!constraint_source.empty()) provenance["constraintSource"] = constraint_source;
    return provenance;
}

nlohmann::json build_capability_profile(const nlohmann::json& machine_definition) {
    const std::string capability_id = machine_definition.at("machineId").get<std::string>();
    const std::string capability_version = derive_capability_version(machine_definition);

    nlohmann::json axis_ids = nlohmann::json::array();
    nlohmann::json axis_types = nlohmann::json::array();
    for (const auto& joint : machine_definition.at("kinematics").at("joints")) {
        axis_ids.push_back(joint.at("id"));
        axis_types.push_back(joint.at("type"));
    }

    return {
        {"schemaVersion", "1.0"},
        {"capabilityId", capability_id},
        {"capabilityVersion", capability_version},
        {"machineCapabilityId", capability_id},
        {"id", capability_id},
        {"version", capability_version},
        {"machineDefinitionRef", {
            {"machineId", machine_definition.at("machineId")},
            {"schemaVersion", machine_definition.at("schemaVersion")}
        }},
        {"units", machine_definition.at("units")},
        {"spindle", {
            {"rpmMin", machine_definition.at("spindle").at("rpmMin")},
            {"rpmMax", machine_definition.at("spindle").at("rpmMax")},
            {"orientationCapability", machine_definition.at("spindle").at("orientationCapability")}
        }},
        {"sourceProvenance", build_source_provenance(machine_definition)},
        {"kinematicsSummary", {
            {"bodyCount", machine_definition.at("kinematics").at("bodies").size()},
            {"jointCount", machine_definition.at("kinematics").at("joints").size()},
            {"axisIds", axis_ids},
            {"axisTypes", axis_types}
        }}
    };
}

} // namespace

int handle_export_capability_profile(const std::string& input_path, const std::string& out_arg, bool json_output) {
    nlohmann::json machine_definition;
    auto machine_validation = artifacts::validate_artifact_file(artifacts::ArtifactType::MachineDefinition, input_path, &machine_definition);
    if (!machine_validation.valid) {
        detail::emit_error_json(
            "InvalidMachineDefinition",
            "Input machine-definition failed validation",
            "ExportCapabilityProfile",
            {{"inputPath", input_path}, {"issues", detail::issues_to_json(machine_validation.issues)}}
        );
        return 2;
    }

    nlohmann::json profile = build_capability_profile(machine_definition);
    auto profile_validation = artifacts::validate_artifact_document(artifacts::ArtifactType::CapabilityProfile, profile);
    if (!profile_validation.valid) {
        detail::emit_error_json(
            "CapabilityProfileValidationFailed",
            "Generated capability-profile did not satisfy the schema",
            "ExportCapabilityProfile",
            {{"issues", detail::issues_to_json(profile_validation.issues)}}
        );
        return 3;
    }

    const fs::path output_path = detail::resolve_output_path(out_arg, "capability-profile.json");
    std::string write_error;
    if (!detail::write_json_file(output_path, profile, write_error)) {
        detail::emit_error_json("ArtifactWriteFailed", write_error, "ExportCapabilityProfile", {{"outputPath", output_path.string()}});
        return 4;
    }

    if (json_output) {
        std::cout << nlohmann::json({
            {"ok", true},
            {"artifactType", "capability-profile"},
            {"outputPath", output_path.string()},
            {"capabilityId", profile["capabilityId"]},
            {"capabilityVersion", profile["capabilityVersion"]}
        }).dump(2) << "\n";
    } else {
        std::cout << "Wrote capability-profile: " << output_path.string() << "\n";
    }
    return 0;
}

} // namespace commands
} // namespace praxis
