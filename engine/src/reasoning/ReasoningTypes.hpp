#pragma once
#include <string>
#include <vector>

namespace praxis {
namespace reasoning {

// Reasoning note levels
enum class ReasoningLevel {
    Info,
    Warning
};

// Individual reasoning note with full context
struct ReasoningNote {
    ReasoningLevel level;
    std::string rule_id;        // Stable rule identifier (e.g., "check_travel_envelope")
    std::string code;           // Machine-readable (e.g., "TRAVEL_X_LARGE")
    std::string message;        // Human-readable explanation
    std::string source;         // Parameter/field that triggered this
    double actual_value;        // Value that triggered the rule
    double threshold;           // Limit that was checked
    std::string units;          // Units for values (e.g., "mm", "ratio", "degrees")
    
    ReasoningNote(
        ReasoningLevel lvl,
        const std::string& rule,
        const std::string& c,
        const std::string& msg,
        const std::string& src,
        double actual,
        double thresh,
        const std::string& u
    ) : level(lvl), rule_id(rule), code(c), message(msg), source(src), 
        actual_value(actual), threshold(thresh), units(u) {}
};

// Convert level to string for serialization
inline std::string level_to_string(ReasoningLevel level) {
    switch (level) {
        case ReasoningLevel::Info: return "info";
        case ReasoningLevel::Warning: return "warning";
        default: return "unknown";
    }
}

} // namespace reasoning
} // namespace praxis
