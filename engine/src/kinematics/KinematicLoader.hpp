#pragma once
#include "KinematicTypes.hpp"
#include <string>

namespace praxis {
namespace kinematics {

class KinematicLoader {
public:
    // Load PKM from JSON file
    static PKMLoadResult load_from_json(const std::string& file_path);
    
    // Save PKM to JSON file
    static bool save_to_json(const PKM& pkm, const std::string& file_path);
    
private:
    // JSON parsing helpers (simple manual parsing for v0)
    static PKM parse_json(const std::string& json_content);
    static std::string generate_json(const PKM& pkm);
};

} // namespace kinematics
} // namespace praxis
