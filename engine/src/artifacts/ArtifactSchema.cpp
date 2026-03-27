#include "praxis/ArtifactSchema.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace praxis {
namespace artifacts {

namespace {

fs::path schema_root() {
    return fs::path(PRAXIS_REPO_ROOT) / "contracts" / "schemas";
}

fs::path schema_path_for_type(ArtifactType type) {
    return schema_root() / (std::string(artifact_type_to_string(type)) + ".schema.json");
}

void add_issue(std::vector<ValidationIssue>& issues, const std::string& path, const std::string& message) {
    issues.push_back({path, message});
}

bool type_matches(const std::string& type_name, const nlohmann::json& value) {
    if (type_name == "object")  return value.is_object();
    if (type_name == "array")   return value.is_array();
    if (type_name == "string")  return value.is_string();
    if (type_name == "number")  return value.is_number();
    if (type_name == "integer") return value.is_number_integer();
    if (type_name == "boolean") return value.is_boolean();
    if (type_name == "null")    return value.is_null();
    return false;
}

void validate_node(const nlohmann::json& schema,
                   const nlohmann::json& instance,
                   const std::string& path,
                   std::vector<ValidationIssue>& issues) {
    if (schema.contains("const") && instance != schema["const"]) {
        add_issue(issues, path, "must equal const value");
    }

    if (schema.contains("enum")) {
        bool matched = false;
        for (const auto& candidate : schema["enum"]) {
            if (candidate == instance) { matched = true; break; }
        }
        if (!matched) add_issue(issues, path, "must be one of the allowed enum values");
    }

    if (schema.contains("type") && schema["type"].is_string()) {
        const std::string type_name = schema["type"].get<std::string>();
        if (!type_matches(type_name, instance)) {
            add_issue(issues, path, "has wrong type, expected " + type_name);
            return;
        }

        if (type_name == "string") {
            const std::string value = instance.get<std::string>();
            if (schema.contains("minLength") && value.size() < schema["minLength"].get<size_t>())
                add_issue(issues, path, "is shorter than minLength");
            if (schema.contains("pattern") && schema["pattern"].is_string()) {
                const std::regex pattern(schema["pattern"].get<std::string>());
                if (!std::regex_match(value, pattern))
                    add_issue(issues, path, "does not match required pattern");
            }
        }

        if (type_name == "array") {
            if (schema.contains("minItems") && instance.size() < schema["minItems"].get<size_t>())
                add_issue(issues, path, "has fewer items than minItems");
            if (schema.contains("maxItems") && instance.size() > schema["maxItems"].get<size_t>())
                add_issue(issues, path, "has more items than maxItems");
            if (schema.contains("items")) {
                for (size_t i = 0; i < instance.size(); ++i)
                    validate_node(schema["items"], instance[i], path + "[" + std::to_string(i) + "]", issues);
            }
        }

        if (type_name == "object") {
            if (schema.contains("required") && schema["required"].is_array()) {
                for (const auto& req : schema["required"]) {
                    const std::string key = req.get<std::string>();
                    if (!instance.contains(key))
                        add_issue(issues, path + "." + key, "is required");
                }
            }

            std::set<std::string> known;
            if (schema.contains("properties") && schema["properties"].is_object()) {
                for (auto it = schema["properties"].begin(); it != schema["properties"].end(); ++it) {
                    known.insert(it.key());
                    if (instance.contains(it.key()))
                        validate_node(it.value(), instance[it.key()], path + "." + it.key(), issues);
                }
            }

            if (schema.contains("additionalProperties")) {
                const auto& additional = schema["additionalProperties"];
                for (auto it = instance.begin(); it != instance.end(); ++it) {
                    if (known.count(it.key()) > 0) continue;
                    if (additional.is_boolean() && !additional.get<bool>())
                        add_issue(issues, path + "." + it.key(), "is not allowed");
                    else if (additional.is_object())
                        validate_node(additional, it.value(), path + "." + it.key(), issues);
                }
            }
        }
    }
}

ValidationResult semantic_validate_artifact(ArtifactType type, const nlohmann::json& instance) {
    ValidationResult result;
    result.valid = true;

    if (type == ArtifactType::MachineDefinition) {
        const auto& kinematics = instance.at("kinematics");
        const auto& joints = kinematics.at("joints");
        const auto& chain = kinematics.at("chain");
        if (!joints.empty() && chain.empty())
            add_issue(result.issues, "$.kinematics.chain", "must not be empty when joints are declared");
    }

    if (type == ArtifactType::CapabilityProfile) {
        const std::string cid = instance.at("capabilityId").get<std::string>();
        const std::string cver = instance.at("capabilityVersion").get<std::string>();
        if (instance.at("machineCapabilityId").get<std::string>() != cid)
            add_issue(result.issues, "$.machineCapabilityId", "must match capabilityId");
        if (instance.at("id").get<std::string>() != cid)
            add_issue(result.issues, "$.id", "must match capabilityId");
        if (instance.at("version").get<std::string>() != cver)
            add_issue(result.issues, "$.version", "must match capabilityVersion");
    }

    if (type == ArtifactType::Manifest) {
        std::set<std::string> seen;
        for (const auto& artifact : instance.at("artifacts")) {
            const std::string p = artifact.at("path").get<std::string>();
            if (!seen.insert(p).second)
                add_issue(result.issues, "$.artifacts", "contains duplicate artifact path: " + p);
        }
    }

    result.valid = result.issues.empty();
    return result;
}

} // namespace

std::optional<ArtifactType> parse_artifact_type(const std::string& value) {
    if (value == "machine-definition") return ArtifactType::MachineDefinition;
    if (value == "capability-profile") return ArtifactType::CapabilityProfile;
    if (value == "manifest")           return ArtifactType::Manifest;
    return std::nullopt;
}

const char* artifact_type_to_string(ArtifactType type) {
    switch (type) {
        case ArtifactType::MachineDefinition: return "machine-definition";
        case ArtifactType::CapabilityProfile: return "capability-profile";
        case ArtifactType::Manifest:          return "manifest";
    }
    return "unknown";
}

nlohmann::json load_schema(ArtifactType type) {
    const fs::path schema_path = schema_path_for_type(type);
    std::ifstream file(schema_path);
    if (!file) throw std::runtime_error("Unable to read schema file: " + schema_path.string());
    std::ostringstream buf;
    buf << file.rdbuf();
    return nlohmann::json::parse(buf.str());
}

ValidationResult validate_json_against_schema(const nlohmann::json& schema, const nlohmann::json& instance) {
    ValidationResult result;
    result.valid = true;
    validate_node(schema, instance, "$", result.issues);
    result.valid = result.issues.empty();
    return result;
}

ValidationResult validate_artifact_document(ArtifactType type, const nlohmann::json& instance) {
    ValidationResult schema_result = validate_json_against_schema(load_schema(type), instance);
    ValidationResult semantic_result = semantic_validate_artifact(type, instance);
    schema_result.issues.insert(schema_result.issues.end(), semantic_result.issues.begin(), semantic_result.issues.end());
    schema_result.valid = schema_result.issues.empty();
    return schema_result;
}

ValidationResult validate_artifact_file(ArtifactType type, const std::string& file_path, nlohmann::json* parsed) {
    ValidationResult result;
    result.valid = false;

    std::ifstream file(file_path);
    if (!file) {
        add_issue(result.issues, "$", "unable to read file: " + file_path);
        return result;
    }

    std::ostringstream buf;
    buf << file.rdbuf();

    try {
        nlohmann::json document = nlohmann::json::parse(buf.str());
        if (parsed != nullptr) *parsed = document;
        return validate_artifact_document(type, document);
    } catch (const std::exception& error) {
        add_issue(result.issues, "$", std::string("invalid JSON: ") + error.what());
        return result;
    }
}

} // namespace artifacts
} // namespace praxis
