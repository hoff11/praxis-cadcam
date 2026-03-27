#include "ReasoningEngine.hpp"
#include <cmath>

namespace praxis {
namespace reasoning {

ReasoningEngine::ReasoningEngine() {
    // Register all rules (always enabled in Sprint 3)
    rules_.push_back(ReasoningRule(
        "TRAVEL_ENVELOPE",
        "Check if travel dimensions exceed typical VMC envelopes",
        check_travel_envelope
    ));
    
    rules_.push_back(ReasoningRule(
        "TABLE_ASPECT_RATIO",
        "Check for unusual table aspect ratios",
        check_table_aspect_ratio
    ));
    
    rules_.push_back(ReasoningRule(
        "SPINDLE_CLEARANCE",
        "Check for potential spindle clearance issues",
        check_spindle_clearance
    ));
}

std::vector<ReasoningNote> ReasoningEngine::evaluate(
    const ResolvedRecipe& recipe,
    const kinematics::PKM& pkm
) const {
    std::vector<ReasoningNote> notes;
    
    // Run all registered rules
    for (const auto& rule : rules_) {
        auto rule_notes = rule.evaluate(recipe, pkm);
        notes.insert(notes.end(), rule_notes.begin(), rule_notes.end());
    }
    
    return notes;
}

// Rule: Check travel envelope against typical VMC limits
std::vector<ReasoningNote> ReasoningEngine::check_travel_envelope(
    const ResolvedRecipe& recipe,
    const kinematics::PKM& pkm
) {
    std::vector<ReasoningNote> notes;
    
    // Typical 3-axis VMC envelope limits (mm)
    const double TYPICAL_X_MAX = 1200.0;
    const double TYPICAL_Y_MAX = 800.0;
    const double TYPICAL_Z_MAX = 600.0;
    
    // Check X travel
    if (recipe.params.count("travel_x") > 0) {
        double travel_x = recipe.params.at("travel_x");
        if (travel_x > TYPICAL_X_MAX) {
            notes.push_back(ReasoningNote(
                ReasoningLevel::Warning,
                "check_travel_envelope",
                "TRAVEL_X_LARGE",
                "X travel exceeds common 3-axis VMC envelope",
                "travel_x",
                travel_x,
                TYPICAL_X_MAX,
                "mm"
            ));
        }
    }
    
    // Check Y travel
    if (recipe.params.count("travel_y") > 0) {
        double travel_y = recipe.params.at("travel_y");
        if (travel_y > TYPICAL_Y_MAX) {
            notes.push_back(ReasoningNote(
                ReasoningLevel::Warning,
                "check_travel_envelope",
                "TRAVEL_Y_LARGE",
                "Y travel exceeds common 3-axis VMC envelope",
                "travel_y",
                travel_y,
                TYPICAL_Y_MAX,
                "mm"
            ));
        }
    }
    
    // Check Z travel
    if (recipe.params.count("travel_z") > 0) {
        double travel_z = recipe.params.at("travel_z");
        if (travel_z > TYPICAL_Z_MAX) {
            notes.push_back(ReasoningNote(
                ReasoningLevel::Warning,
                "check_travel_envelope",
                "TRAVEL_Z_LARGE",
                "Z travel exceeds common 3-axis VMC envelope",
                "travel_z",
                travel_z,
                TYPICAL_Z_MAX,
                "mm"
            ));
        }
    }
    
    return notes;
}

// Rule: Check table aspect ratio
std::vector<ReasoningNote> ReasoningEngine::check_table_aspect_ratio(
    const ResolvedRecipe& recipe,
    const kinematics::PKM& pkm
) {
    std::vector<ReasoningNote> notes;
    
    const double MAX_ASPECT_RATIO = 3.0;
    
    if (recipe.params.count("table_width") > 0 && recipe.params.count("table_depth") > 0) {
        double width = recipe.params.at("table_width");
        double depth = recipe.params.at("table_depth");
        
        if (depth > 0) {
            double aspect_ratio = width / depth;
            
            if (aspect_ratio > MAX_ASPECT_RATIO || aspect_ratio < (1.0 / MAX_ASPECT_RATIO)) {
                notes.push_back(ReasoningNote(
                    ReasoningLevel::Info,
                    "check_table_aspect_ratio",
                    "TABLE_ASPECT_UNUSUAL",
                    "Table aspect ratio is unusual (may affect rigidity)",
                    "table_width",
                    aspect_ratio,
                    MAX_ASPECT_RATIO,
                    "ratio"
                ));
            }
        }
    }
    
    return notes;
}

// Rule: Check spindle clearance risks
std::vector<ReasoningNote> ReasoningEngine::check_spindle_clearance(
    const ResolvedRecipe& recipe,
    const kinematics::PKM& pkm
) {
    std::vector<ReasoningNote> notes;
    
    // Typical spindle nose-to-table clearance requirement
    const double MIN_CLEARANCE_RATIO = 0.5;  // Z travel should be at least 50% of spindle length
    
    if (recipe.params.count("travel_z") > 0 && recipe.params.count("spindle_length") > 0) {
        double travel_z = recipe.params.at("travel_z");
        double spindle_length = recipe.params.at("spindle_length");
        
        if (spindle_length > 0) {
            double clearance_ratio = travel_z / spindle_length;
            
            if (clearance_ratio < MIN_CLEARANCE_RATIO) {
                notes.push_back(ReasoningNote(
                    ReasoningLevel::Warning,
                    "check_spindle_clearance",
                    "SPINDLE_CLEARANCE_RISK",
                    "Z travel may be insufficient for spindle length (collision risk)",
                    "travel_z",
                    clearance_ratio,
                    MIN_CLEARANCE_RATIO,
                    "ratio"
                ));
            }
        }
    }
    
    return notes;
}

} // namespace reasoning
} // namespace praxis
