/**
 * InteractionPublic.hpp
 *
 * Public-facing status mapping adapters to preserve Sprint 8 wording
 * without changing internal engine enums or result shapes.
 *
 * This provides a thin boundary for CLI/report emitters.
 */

#pragma once

#include "Selector.hpp"
#include "Reference.hpp"

namespace praxis {
namespace interaction_public {

// Sprint 8 public selection status names
enum class SelectionStatusPublic {
    Success,
    Missing,
    Ambiguous,
    Invalid
};

// Sprint 8 public resolution status names
enum class ResolutionStatusPublic {
    Exact,
    Drifted,
    Ambiguous,
    Missing
};

// LEGACY: Commented out - SelectionResult/Failure are pre-C.5 types
// TODO: Remove or migrate to C.5 ResolveResult when refactoring select command
// Map internal SelectionResult → public status
// SelectionStatusPublic map_selection_status(const selector::SelectionResult& result);

// Map internal ResolutionStatus → public status
ResolutionStatusPublic map_resolution_status(reference::ResolutionStatus status);

// String helpers (for JSON emitters)
const char* to_string(SelectionStatusPublic s);
const char* to_string(ResolutionStatusPublic s);

// Serialize resolution result to public JSON string
std::string resolution_to_public_json(const reference::ResolutionResult& res);

// LEGACY: Commented out - Failure is pre-C.5 type
// Serialize selection failures to public JSON strings (array)
// std::vector<std::string> selection_failures_to_public_json(const std::vector<selector::Failure>& failures);

} // namespace interaction_public
} // namespace praxis
