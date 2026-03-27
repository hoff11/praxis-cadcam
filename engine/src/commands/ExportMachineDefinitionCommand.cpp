#include "ArtifactCommandCommon.hpp"
#include "praxis/ArtifactSchema.hpp"
#include "../kinematics/KinematicLoader.hpp"
#include <iostream>

namespace praxis {
namespace commands {

namespace {

nlohmann::json to_vector_json(const kinematics::Vec3& v) {
    return nlohmann::json::array({v.x, v.y, v.z});
}

nlohmann::json build_machine_definition(const kinematics::PKM& pkm, const std::string& source_path) {
    nlohmann::json joints = nlohmann::json::array();
    for (const auto& joint : pkm.joints) {
        joints.push_back({
            {"id", joint.id},
            {"type", kinematics::joint_type_to_string(joint.type)},
            {"parentBody", joint.parent_body},
            {"childBody", joint.child_body},
            {"axisOrigin", to_vector_json(joint.axis_origin)},
            {"axisDirection", to_vector_json(joint.axis_direction)},
            {"limits", {{"min", joint.limits.min}, {"max", joint.limits.max}}}
        });
    }

    nlohmann::json frames = nlohmann::json::array();
    for (const auto& frame : pkm.frames) {
        frames.push_back({
            {"name", frame.name},
            {"origin", to_vector_json(frame.origin)},
            {"xAxis", to_vector_json(frame.x_axis)},
            {"yAxis", to_vector_json(frame.y_axis)},
            {"zAxis", to_vector_json(frame.z_axis)}
        });
    }

    nlohmann::json metadata = nlohmann::json::object();
    for (const auto& entry : pkm.metadata) metadata[entry.first] = entry.second;

    return {
        {"schemaVersion", "1.0"},
        {"machineId", pkm.machine},
        {"machineName", pkm.machine},
        {"units", pkm.units},
        {"source", {
            {"inputType", "pkm"},
            {"inputPath", fs::absolute(source_path).string()},
            {"pkmVersion", pkm.pkm_version}
        }},
        {"metadata", metadata},
        {"kinematics", {
            {"pkmVersion", pkm.pkm_version},
            {"bodies", pkm.bodies},
            {"joints", joints},
            {"chain", pkm.chain},
            {"frames", frames}
        }}
    };
}

} // namespace

int handle_export_machine_definition(const std::string& input_path, const std::string& out_arg, bool json_output) {
    auto load_result = kinematics::KinematicLoader::load_from_json(input_path);
    if (!load_result.success) {
        detail::emit_error_json(
            "InvalidMachineSource",
            load_result.error_message,
            "ExportMachineDefinition",
            {{"inputPath", input_path}, {"warnings", load_result.warnings}}
        );
        return 2;
    }

    nlohmann::json document = build_machine_definition(load_result.pkm, input_path);
    auto validation = artifacts::validate_artifact_document(artifacts::ArtifactType::MachineDefinition, document);
    if (!validation.valid) {
        detail::emit_error_json(
            "MachineDefinitionValidationFailed",
            "Generated machine-definition did not satisfy the schema",
            "ExportMachineDefinition",
            {{"issues", detail::issues_to_json(validation.issues)}}
        );
        return 3;
    }

    const fs::path output_path = detail::resolve_output_path(out_arg, "machine-definition.json");
    std::string write_error;
    if (!detail::write_json_file(output_path, document, write_error)) {
        detail::emit_error_json("ArtifactWriteFailed", write_error, "ExportMachineDefinition", {{"outputPath", output_path.string()}});
        return 4;
    }

    if (json_output) {
        std::cout << nlohmann::json({
            {"ok", true},
            {"artifactType", "machine-definition"},
            {"outputPath", output_path.string()},
            {"machineId", document["machineId"]},
            {"jointCount", document["kinematics"]["joints"].size()}
        }).dump(2) << "\n";
    } else {
        std::cout << "Wrote machine-definition: " << output_path.string() << "\n";
    }
    return 0;
}

} // namespace commands
} // namespace praxis
