#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace praxis {
namespace commands {
int handle_validate_artifact(const std::string& artifact_type, const std::string& input_path, bool json_output);
int handle_inspect_artifact(const std::string& artifact_type, const std::string& input_path, bool json_output);
int handle_export_machine_definition(const std::string& input_path, const std::string& out_arg, bool json_output);
int handle_export_capability_profile(const std::string& input_path, const std::string& out_arg, bool json_output);
int handle_bundle_create(const std::string& input_dir, const std::string& out_arg, bool json_output);
}
}

namespace {

fs::path fixture_path(const std::string& name) {
    fs::path test_dir = fs::path(__FILE__).parent_path();
    return test_dir.parent_path() / "fixtures" / "artifact" / name;
}

fs::path temp_case_dir(const std::string& name) {
    fs::path dir = fs::temp_directory_path() / "praxis-cadcam-artifact-tests" / name;
    std::error_code error;
    fs::remove_all(dir, error);
    fs::create_directories(dir);
    return dir;
}

nlohmann::json read_json(const fs::path& path) {
    std::ifstream file(path);
    REQUIRE(file.good());
    nlohmann::json document;
    file >> document;
    return document;
}

} // namespace

TEST_CASE("export machine-definition from valid PKM", "[artifact]") {
    const fs::path temp_dir = temp_case_dir("export-machine-definition");
    const fs::path output_path = temp_dir / "machine-definition.json";

    const int rc = praxis::commands::handle_export_machine_definition(
        fixture_path("sample-pkm.json").string(),
        output_path.string(),
        true
    );

    REQUIRE(rc == 0);
    REQUIRE(fs::exists(output_path));

    const auto document = read_json(output_path);
    REQUIRE(document.at("schemaVersion") == "1.0");
    REQUIRE(document.at("machineId") == "fanuc-3x-vmc");
    REQUIRE(document.at("kinematics").at("joints").size() == 3);
    REQUIRE(document.at("kinematics").at("frames").size() == 1);
}

TEST_CASE("artifact lifecycle generates valid bundle", "[artifact]") {
    const fs::path temp_dir = temp_case_dir("artifact-lifecycle");
    const fs::path machine_definition = temp_dir / "machine-definition.json";
    const fs::path capability_profile = temp_dir / "capability-profile.json";
    const fs::path manifest = temp_dir / "manifest.json";

    REQUIRE(praxis::commands::handle_export_machine_definition(
        fixture_path("sample-pkm.json").string(),
        machine_definition.string(),
        false
    ) == 0);

    REQUIRE(praxis::commands::handle_export_capability_profile(
        machine_definition.string(),
        capability_profile.string(),
        false
    ) == 0);

    REQUIRE(praxis::commands::handle_validate_artifact(
        "machine-definition",
        machine_definition.string(),
        true
    ) == 0);

    REQUIRE(praxis::commands::handle_validate_artifact(
        "capability-profile",
        capability_profile.string(),
        true
    ) == 0);

    REQUIRE(praxis::commands::handle_inspect_artifact(
        "machine-definition",
        machine_definition.string(),
        true
    ) == 0);

    REQUIRE(praxis::commands::handle_inspect_artifact(
        "capability-profile",
        capability_profile.string(),
        true
    ) == 0);

    REQUIRE(praxis::commands::handle_bundle_create(temp_dir.string(), manifest.string(), true) == 0);
    REQUIRE(praxis::commands::handle_validate_artifact("manifest", manifest.string(), true) == 0);

    const auto manifest_json = read_json(manifest);
    REQUIRE(manifest_json.at("manifestVersion") == "1.0.0");
    REQUIRE(manifest_json.at("artifactSetVersion") == "1.0.0");
    REQUIRE(manifest_json.at("hashAlgorithm") == "sha256");
    REQUIRE(manifest_json.contains("generator"));
    REQUIRE(manifest_json.at("generator").at("name") == "praxis-cadcam-cli");
    REQUIRE(manifest_json.at("artifacts").size() == 2);
    for (const auto& artifact : manifest_json.at("artifacts")) {
        REQUIRE(artifact.contains("path"));
        REQUIRE(artifact.contains("sha"));
        REQUIRE(artifact.contains("sizeBytes"));
    }
}

TEST_CASE("reject unsupported PKM version during export", "[artifact]") {
    const fs::path temp_dir = temp_case_dir("bad-pkm-version");
    const fs::path output_path = temp_dir / "machine-definition.json";

    const int rc = praxis::commands::handle_export_machine_definition(
        fixture_path("bad-version-pkm.json").string(),
        output_path.string(),
        true
    );

    REQUIRE(rc == 2);
    REQUIRE_FALSE(fs::exists(output_path));
}

TEST_CASE("reject corrupt artifact input", "[artifact]") {
    const int rc = praxis::commands::handle_validate_artifact(
        "machine-definition",
        fixture_path("corrupt.json").string(),
        true
    );

    REQUIRE(rc == 3);
}

TEST_CASE("reject semantic machine-definition with joints but empty chain", "[artifact]") {
    const int rc = praxis::commands::handle_validate_artifact(
        "machine-definition",
        fixture_path("semantic-invalid-machine-definition.json").string(),
        true
    );

    REQUIRE(rc == 3);
}

TEST_CASE("reject semantic capability-profile alias mismatch", "[artifact]") {
    const int rc = praxis::commands::handle_validate_artifact(
        "capability-profile",
        fixture_path("semantic-invalid-capability-profile.json").string(),
        true
    );

    REQUIRE(rc == 3);
}

TEST_CASE("reject semantic manifest duplicate artifact path", "[artifact]") {
    const int rc = praxis::commands::handle_validate_artifact(
        "manifest",
        fixture_path("semantic-invalid-manifest-duplicate-paths.json").string(),
        true
    );

    REQUIRE(rc == 3);
}

TEST_CASE("export machine-definition from realistic 4-axis PKM fixture", "[artifact]") {
    const fs::path temp_dir = temp_case_dir("export-machine-definition-4axis");
    const fs::path output_path = temp_dir / "machine-definition.json";

    const int rc = praxis::commands::handle_export_machine_definition(
        fixture_path("fanuc-4x-vmc.pkm.json").string(),
        output_path.string(),
        true
    );

    REQUIRE(rc == 0);
    REQUIRE(fs::exists(output_path));

    const auto document = read_json(output_path);
    REQUIRE(document.at("machineId") == "fanuc-4x-vmc");
    REQUIRE(document.at("kinematics").at("joints").size() == 4);
    REQUIRE(document.at("kinematics").at("chain").size() == 4);

    bool has_revolute = false;
    for (const auto& joint : document.at("kinematics").at("joints")) {
        if (joint.at("type") == "revolute") {
            has_revolute = true;
            break;
        }
    }
    REQUIRE(has_revolute);
}
