#pragma once
#include "../../include/praxis/Intent.hpp"
#include "../recipe/RecipeTypes.hpp"
#include "../kinematics/KinematicTypes.hpp"

namespace praxis {

// BuildFromRecipe - execute a recipe JSON file
IntentResult buildFromRecipe(const IntentRequest& request);

// Helper functions
recipe::RecipeLoadResult load_recipe_json(const std::string& recipe_path);
kinematics::PKM load_pkm_json(const std::string& pkm_path);

} // namespace praxis
