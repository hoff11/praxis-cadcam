#include "ArtifactCommandCommon.hpp"
#include "praxis/ArtifactSchema.hpp"
#include "praxis/CanonicalFormat.hpp"
#include <iostream>

namespace praxis {
namespace commands {

int handle_validate_artifact(const std::string& artifact_type, const std::string& input_path, bool json_output) {
    auto parsed_type = artifacts::parse_artifact_type(artifact_type);
    if (!parsed_type.has_value()) {
        detail::emit_error_json("UnknownArtifactType", "Unsupported artifact type", "ValidateArtifact", {{"artifactType", artifact_type}});
        return 2;
    }

    auto validation = artifacts::validate_artifact_file(*parsed_type, input_path);
    if (!validation.valid) {
        if (json_output) {
            std::cout << nlohmann::json({
                {"ok", false},
                {"artifactType", artifact_type},
                {"inputPath", input_path},
                {"issues", detail::issues_to_json(validation.issues)}
            }).dump(2) << "\n";
        } else {
            detail::emit_error_json(
                "ArtifactValidationFailed",
                "Artifact did not satisfy the schema",
                "ValidateArtifact",
                {{"inputPath", input_path}, {"issues", detail::issues_to_json(validation.issues)}}
            );
        }
        return 3;
    }

    if (json_output) {
        std::cout << nlohmann::json({{"ok", true}, {"artifactType", artifact_type}, {"inputPath", input_path}}).dump(2) << "\n";
    } else {
        std::cout << "Validated " << canonical::escape_json(artifact_type) << ": " << canonical::escape_json(input_path) << "\n";
    }
    return 0;
}

} // namespace commands
} // namespace praxis
