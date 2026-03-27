#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "praxis/ArtifactSchema.hpp"
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace praxis {
namespace commands {
namespace detail {

inline void emit_error_json(const std::string& code,
                            const std::string& message,
                            const std::string& where = "",
                            const nlohmann::json& detail = nlohmann::json::object()) {
    nlohmann::json payload{{"code", code}, {"message", message}, {"where", where}, {"detail", detail}};
    std::cerr << "PRAXIS_ERROR_JSON=" << payload.dump() << "\n";
}

inline fs::path resolve_output_path(const std::string& out_arg, const std::string& default_filename) {
    fs::path output(out_arg);
    const bool looks_like_directory = out_arg.empty() ||
        out_arg.back() == '/' || out_arg.back() == '\\' ||
        (!output.has_extension());

    if (fs::exists(output) && fs::is_directory(output)) {
        output /= default_filename;
    } else if (looks_like_directory) {
        output /= default_filename;
    }
    return output;
}

inline bool write_json_file(const fs::path& output_path, const nlohmann::json& document, std::string& error_message) {
    try {
        if (!output_path.parent_path().empty())
            fs::create_directories(output_path.parent_path());
        std::ofstream file(output_path);
        if (!file) {
            error_message = "unable to open output file: " + output_path.string();
            return false;
        }
        file << document.dump(2) << "\n";
        return true;
    } catch (const std::exception& error) {
        error_message = error.what();
        return false;
    }
}

inline nlohmann::json issues_to_json(const std::vector<praxis::artifacts::ValidationIssue>& issues) {
    nlohmann::json data = nlohmann::json::array();
    for (const auto& issue : issues)
        data.push_back({{"path", issue.path}, {"message", issue.message}});
    return data;
}

} // namespace detail
} // namespace commands
} // namespace praxis
