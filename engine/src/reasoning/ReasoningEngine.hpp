#pragma once
#include "ReasoningTypes.hpp"
#include "../recipe/RecipeTypes.hpp"
#include "../kinematics/KinematicTypes.hpp"
#include <functional>
#include <vector>
#include <map>

namespace praxis {
namespace reasoning {

// Forward declaration for resolved recipe data
struct ResolvedRecipe {
    std::map<std::string, double> params;
    std::map<std::string, double> derived;
    std::string variant_name;
};

// Reasoning rule with evaluator function
struct ReasoningRule {
    std::string code;
    std::string description;
    std::function<std::vector<ReasoningNote>(
        const ResolvedRecipe&,
        const kinematics::PKM&
    )> evaluate;
    
    ReasoningRule(
        const std::string& c,
        const std::string& desc,
        std::function<std::vector<ReasoningNote>(const ResolvedRecipe&, const kinematics::PKM&)> eval
    ) : code(c), description(desc), evaluate(eval) {}
};

// Main reasoning engine
class ReasoningEngine {
public:
    ReasoningEngine();
    
    // Evaluate all registered rules
    std::vector<ReasoningNote> evaluate(
        const ResolvedRecipe& recipe,
        const kinematics::PKM& pkm
    ) const;
    
    // Get all registered rules (for documentation/testing)
    const std::vector<ReasoningRule>& get_rules() const { return rules_; }
    
private:
    std::vector<ReasoningRule> rules_;
    
    // Rule implementations
    static std::vector<ReasoningNote> check_travel_envelope(
        const ResolvedRecipe& recipe,
        const kinematics::PKM& pkm
    );
    
    static std::vector<ReasoningNote> check_table_aspect_ratio(
        const ResolvedRecipe& recipe,
        const kinematics::PKM& pkm
    );
    
    static std::vector<ReasoningNote> check_spindle_clearance(
        const ResolvedRecipe& recipe,
        const kinematics::PKM& pkm
    );
};

} // namespace reasoning
} // namespace praxis
