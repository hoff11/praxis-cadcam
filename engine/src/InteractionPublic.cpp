#include "InteractionPublic.hpp"
#include "praxis/CanonicalFormat.hpp"
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>

namespace praxis {
namespace interaction_public {

// LEGACY: Commented out - SelectionResult/Failure are pre-C.5 types
/*
SelectionStatusPublic map_selection_status(const selector::SelectionResult& result) {
    if (result.success) {
        return SelectionStatusPublic::Success;
    }

    // Choose the most specific first failure if available
    for (const auto& f : result.failures) {
        switch (f.type) {
            case selector::FailureType::InvalidSelector:
                return SelectionStatusPublic::Invalid;
            case selector::FailureType::Missing:
                return SelectionStatusPublic::Missing;
            case selector::FailureType::Ambiguous:
                return SelectionStatusPublic::Ambiguous;
            default:
                break;
        }
    }
    // Fallback
    return SelectionStatusPublic::Invalid;
}
*/

ResolutionStatusPublic map_resolution_status(reference::ResolutionStatus status) {
    switch (status) {
        case reference::ResolutionStatus::Success:
            return ResolutionStatusPublic::Exact;
        case reference::ResolutionStatus::Drifted:
            return ResolutionStatusPublic::Drifted;
        case reference::ResolutionStatus::Ambiguous:
            return ResolutionStatusPublic::Ambiguous;
        case reference::ResolutionStatus::Missing:
        case reference::ResolutionStatus::ContractMismatch:
            // Contract mismatch will be reported as a failure elsewhere; public status is Missing
            return ResolutionStatusPublic::Missing;
        default:
            return ResolutionStatusPublic::Missing;
    }
}

const char* to_string(SelectionStatusPublic s) {
    switch (s) {
        case SelectionStatusPublic::Success: return "Success";
        case SelectionStatusPublic::Missing: return "Missing";
        case SelectionStatusPublic::Ambiguous: return "Ambiguous";
        case SelectionStatusPublic::Invalid: return "Invalid";
        default: return "Unknown";
    }
}

const char* to_string(ResolutionStatusPublic s) {
    switch (s) {
        case ResolutionStatusPublic::Exact: return "Exact";
        case ResolutionStatusPublic::Drifted: return "Drifted";
        case ResolutionStatusPublic::Ambiguous: return "Ambiguous";
        case ResolutionStatusPublic::Missing: return "Missing";
        default: return "Unknown";
    }
}

std::string resolution_to_public_json(const reference::ResolutionResult& res) {
    ResolutionStatusPublic pub = map_resolution_status(res.status);
    std::ostringstream oss;
    oss << "{";
    oss << "\"status\":\"" << to_string(pub) << "\"";
    // max_deviation if present (Sprint 9: use canonical formatting)
    auto it = res.signature_deltas.find("max_deviation");
    if (it != res.signature_deltas.end()) {
        oss << ",\"max_deviation\":" << canonical::format_float(it->second);
    }
    // matched entity ids not available in current struct; omit for now
    oss << "}";
    return oss.str();
}

// LEGACY: Commented out - Failure is pre-C.5 type
/*
std::vector<std::string> selection_failures_to_public_json(const std::vector<selector::Failure>& failures) {
    std::vector<std::string> out;
    out.reserve(failures.size());
    for (const auto& f : failures) {
        std::ostringstream oss;
        oss << "{";
        oss << "\"type\":\"" << canonical::escape_json(selector::failure_type_to_string(f.type)) << "\"";
        oss << ",\"message\":\"" << canonical::escape_json(f.message) << "\"";
        if (f.selector.has_value()) {
            oss << ",\"selector\":\"" << canonical::escape_json(*f.selector) << "\"";
        }
        oss << "}";
        out.push_back(oss.str());
    }
    return out;
}
*/

} // namespace interaction_public
} // namespace praxis
