#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace praxis {
namespace artifacts {

enum class ArtifactType {
    MachineDefinition,
    CapabilityProfile,
    Manifest,
};

struct ValidationIssue {
    std::string path;
    std::string message;
};

struct ValidationResult {
    bool valid = false;
    std::vector<ValidationIssue> issues;
};

std::optional<ArtifactType> parse_artifact_type(const std::string& value);
const char* artifact_type_to_string(ArtifactType type);

nlohmann::json load_schema(ArtifactType type);
ValidationResult validate_json_against_schema(const nlohmann::json& schema, const nlohmann::json& instance);
ValidationResult validate_artifact_document(ArtifactType type, const nlohmann::json& instance);
ValidationResult validate_artifact_file(ArtifactType type, const std::string& file_path, nlohmann::json* parsed = nullptr);

} // namespace artifacts
} // namespace praxis
