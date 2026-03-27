#include "ArtifactCommandCommon.hpp"
#include "praxis/ArtifactSchema.hpp"
#include <openssl/sha.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>

namespace praxis {
namespace commands {

namespace {

std::string compute_file_sha256(const fs::path& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) throw std::runtime_error("Unable to read file for hashing: " + file_path.string());

    SHA256_CTX context;
    SHA256_Init(&context);
    char buffer[4096];
    while (file.good()) {
        file.read(buffer, sizeof(buffer));
        if (file.gcount() > 0)
            SHA256_Update(&context, buffer, static_cast<size_t>(file.gcount()));
    }
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256_Final(digest, &context);

    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        output << std::setw(2) << static_cast<int>(digest[i]);
    return output.str();
}

std::optional<artifacts::ArtifactType> artifact_type_for_filename(const std::string& filename) {
    if (filename == "machine-definition.json")  return artifacts::ArtifactType::MachineDefinition;
    if (filename == "capability-profile.json")  return artifacts::ArtifactType::CapabilityProfile;
    if (filename == "manifest.json")            return artifacts::ArtifactType::Manifest;
    return std::nullopt;
}

} // namespace

int handle_bundle_create(const std::string& input_dir, const std::string& out_arg, bool json_output) {
    const fs::path input_root(input_dir);
    if (!fs::exists(input_root) || !fs::is_directory(input_root)) {
        detail::emit_error_json("InvalidBundleInput", "Input directory does not exist", "BundleCreate", {{"inputDir", input_dir}});
        return 2;
    }

    nlohmann::json artifacts_json = nlohmann::json::array();
    size_t artifact_count = 0;
    for (const auto& entry : fs::directory_iterator(input_root)) {
        if (!entry.is_regular_file()) continue;
        const auto parsed_type = artifact_type_for_filename(entry.path().filename().string());
        if (!parsed_type.has_value() || *parsed_type == artifacts::ArtifactType::Manifest) continue;

        auto validation = artifacts::validate_artifact_file(*parsed_type, entry.path().string());
        if (!validation.valid) {
            detail::emit_error_json(
                "BundleArtifactInvalid",
                "Artifact failed validation before bundling",
                "BundleCreate",
                {{"artifactPath", entry.path().string()}, {"issues", detail::issues_to_json(validation.issues)}}
            );
            return 3;
        }

        artifacts_json.push_back({
            {"artifactType", artifacts::artifact_type_to_string(*parsed_type)},
            {"path", fs::relative(entry.path(), input_root).generic_string()},
            {"sha256", compute_file_sha256(entry.path())}
        });
        ++artifact_count;
    }

    if (artifact_count == 0) {
        detail::emit_error_json("BundleEmpty", "No recognized boundary artifacts were found to bundle", "BundleCreate", {{"inputDir", input_dir}});
        return 4;
    }

    nlohmann::json manifest = {
        {"schemaVersion", "1.0"},
        {"bundleId", input_root.filename().string().empty() ? "bundle" : input_root.filename().string()},
        {"artifacts", artifacts_json}
    };

    auto manifest_validation = artifacts::validate_artifact_document(artifacts::ArtifactType::Manifest, manifest);
    if (!manifest_validation.valid) {
        detail::emit_error_json(
            "ManifestValidationFailed",
            "Generated manifest did not satisfy the schema",
            "BundleCreate",
            {{"issues", detail::issues_to_json(manifest_validation.issues)}}
        );
        return 5;
    }

    const fs::path output_path = detail::resolve_output_path(out_arg, "manifest.json");
    std::string write_error;
    if (!detail::write_json_file(output_path, manifest, write_error)) {
        detail::emit_error_json("ArtifactWriteFailed", write_error, "BundleCreate", {{"outputPath", output_path.string()}});
        return 6;
    }

    if (json_output) {
        std::cout << nlohmann::json({
            {"ok", true}, {"artifactType", "manifest"},
            {"outputPath", output_path.string()}, {"artifactCount", artifact_count}
        }).dump(2) << "\n";
    } else {
        std::cout << "Wrote manifest: " << output_path.string() << "\n";
    }
    return 0;
}

} // namespace commands
} // namespace praxis
