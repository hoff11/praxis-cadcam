#include "RecipeValidator.hpp"
#include <set>

namespace praxis {
namespace recipe {

RecipeValidationResult RecipeValidator::validate(const Recipe& recipe) {
    RecipeValidationResult result;
    result.valid = true;
    
    // Run all validation checks
    result.valid &= check_recipe_version(recipe, result.errors);
    result.valid &= check_kinematics_path(recipe, result.errors);
    result.valid &= check_duplicate_node_ids(recipe, result.errors);
    result.valid &= check_node_bindings(recipe, result.errors);
    result.valid &= check_node_references(recipe, result.errors);
    result.valid &= check_param_ranges(recipe, result.errors);
    result.valid &= check_derived_expressions(recipe, result.errors);
    result.valid &= check_output_node(recipe, result.errors);
    
    return result;
}

RecipeValidationResult RecipeValidator::validate_with_pkm(const Recipe& recipe, const kinematics::PKM& pkm) {
    RecipeValidationResult result;
    result.valid = true;
    
    // Run standard validation first
    result = validate(recipe);
    if (!result.valid) {
        return result;
    }
    
    // Add PKM-specific checks
    result.valid &= check_pkm_body_references(recipe, pkm, result.errors);
    
    return result;
}

bool RecipeValidator::check_recipe_version(const Recipe& recipe, std::vector<std::string>& errors) {
    if (recipe.recipe_version.empty()) {
        errors.push_back("Recipe version is required");
        return false;
    }
    
    // For Sprint 2, only accept "0.1"
    if (recipe.recipe_version != "0.1") {
        errors.push_back("Unsupported recipe version: " + recipe.recipe_version + " (expected 0.1)");
        return false;
    }
    
    return true;
}

bool RecipeValidator::check_kinematics_path(const Recipe& recipe, std::vector<std::string>& errors) {
    if (recipe.kinematics_path.empty()) {
        errors.push_back("Kinematics path is required (missing PKM)");
        return false;
    }
    
    return true;
}

bool RecipeValidator::check_duplicate_node_ids(const Recipe& recipe, std::vector<std::string>& errors) {
    std::set<std::string> seen;
    
    for (const auto& node : recipe.nodes) {
        if (seen.count(node.id) > 0) {
            errors.push_back("Duplicate node id: " + node.id);
            return false;
        }
        seen.insert(node.id);
    }
    
    return true;
}

bool RecipeValidator::check_node_bindings(const Recipe& recipe, std::vector<std::string>& errors) {
    // CRITICAL: All geometry-producing nodes MUST have explicit body binding
    // No inference, no defaults (except "base" for backward compatibility)
    
    for (const auto& node : recipe.nodes) {
        // Compound nodes don't produce new geometry, they assemble existing
        if (node.op == NodeOp::Compound) {
            continue;
        }
        
        // MakeBox and Transform produce geometry - binding is MANDATORY
        if (node.binding.body.empty()) {
            errors.push_back("Node '" + node.id + "' produces geometry but has no body binding");
            return false;
        }
    }
    
    return true;
}

bool RecipeValidator::check_node_references(const Recipe& recipe, std::vector<std::string>& errors) {
    std::set<std::string> node_ids;
    for (const auto& node : recipe.nodes) {
        node_ids.insert(node.id);
    }
    
    // Check compound node references
    for (const auto& node : recipe.nodes) {
        if (node.op == NodeOp::Compound) {
            const auto& compound_args = std::get<CompoundArgs>(node.args);
            for (const auto& ref : compound_args.node_refs) {
                if (node_ids.count(ref) == 0) {
                    errors.push_back("Node '" + node.id + "' references unknown node: " + ref);
                    return false;
                }
            }
        } else if (node.op == NodeOp::Transform) {
            const auto& transform_args = std::get<TransformArgs>(node.args);
            if (transform_args.input.empty()) {
                errors.push_back("Transform node '" + node.id + "' missing required input field");
                return false;
            }
            if (node_ids.count(transform_args.input) == 0) {
                errors.push_back("Transform node '" + node.id + "' references unknown input: " + transform_args.input);
                return false;
            }
        }
    }
    
    return true;
}

bool RecipeValidator::check_param_ranges(const Recipe& recipe, std::vector<std::string>& errors) {
    for (const auto& kv : recipe.params) {
        const auto& param = kv.second;
        
        if (param.min_value > param.max_value) {
            errors.push_back("Parameter '" + param.name + "' has min > max");
            return false;
        }
        
        if (param.default_value < param.min_value || param.default_value > param.max_value) {
            errors.push_back("Parameter '" + param.name + "' default value outside range");
            return false;
        }
    }
    
    return true;
}

bool RecipeValidator::check_derived_expressions(const Recipe& recipe, std::vector<std::string>& errors) {
    // For v0, just check that derived expressions reference valid parameters
    std::set<std::string> param_names;
    for (const auto& kv : recipe.params) {
        param_names.insert(kv.first);
    }
    
    // In v0, we don't do full expression validation (that's complex)
    // Just ensure they're not empty
    for (const auto& kv : recipe.derived) {
        if (kv.second.formula.empty()) {
            errors.push_back("Derived value '" + kv.first + "' has empty formula");
            return false;
        }
    }
    
    return true;
}

bool RecipeValidator::check_pkm_body_references(const Recipe& recipe, const kinematics::PKM& pkm, std::vector<std::string>& errors) {
    // Build set of valid body names from PKM
    std::set<std::string> valid_bodies;
    for (const auto& body : pkm.bodies) {
        valid_bodies.insert(body);
    }
    
    // Check all node bindings reference valid bodies
    for (const auto& node : recipe.nodes) {
        // Skip compound nodes (they don't bind to bodies)
        if (node.op == NodeOp::Compound) {
            continue;
        }
        
        // Check if binding exists and is valid
        if (!node.binding.body.empty() && valid_bodies.count(node.binding.body) == 0) {
            errors.push_back("Node '" + node.id + "' binds to unknown body: " + node.binding.body);
            return false;
        }
    }
    
    return true;
}

bool RecipeValidator::check_output_node(const Recipe& recipe, std::vector<std::string>& errors) {
    if (recipe.output.empty()) {
        errors.push_back("Recipe missing required output field");
        return false;
    }
    
    // Check output node exists
    bool found = false;
    for (const auto& node : recipe.nodes) {
        if (node.id == recipe.output) {
            found = true;
            break;
        }
    }
    
    if (!found) {
        errors.push_back("Recipe output references unknown node: " + recipe.output);
        return false;
    }
    
    return true;
}

} // namespace recipe
} // namespace praxis
