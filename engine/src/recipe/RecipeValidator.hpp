#pragma once
#include "RecipeTypes.hpp"
#include "../kinematics/KinematicTypes.hpp"

namespace praxis {
namespace recipe {

class RecipeValidator {
public:
    // Validate a recipe according to Sprint 2 rules
    static RecipeValidationResult validate(const Recipe& recipe);
    
    // Validate recipe bindings against PKM bodies
    static RecipeValidationResult validate_with_pkm(const Recipe& recipe, const kinematics::PKM& pkm);
    
private:
    // Individual validation checks
    static bool check_recipe_version(const Recipe& recipe, std::vector<std::string>& errors);
    static bool check_kinematics_path(const Recipe& recipe, std::vector<std::string>& errors);
    static bool check_duplicate_node_ids(const Recipe& recipe, std::vector<std::string>& errors);
    static bool check_node_bindings(const Recipe& recipe, std::vector<std::string>& errors);
    static bool check_node_references(const Recipe& recipe, std::vector<std::string>& errors);
    static bool check_param_ranges(const Recipe& recipe, std::vector<std::string>& errors);
    static bool check_derived_expressions(const Recipe& recipe, std::vector<std::string>& errors);
    static bool check_pkm_body_references(const Recipe& recipe, const kinematics::PKM& pkm, std::vector<std::string>& errors);
    static bool check_output_node(const Recipe& recipe, std::vector<std::string>& errors);
};

} // namespace recipe
} // namespace praxis
