/**
 * InteractionEmit.hpp
 *
 * Thin helpers to populate IntentResult::interaction with
 * Sprint 8-compliant outward JSON using public mapping.
 *
 * Keeps internal engine types unchanged and centralizes
 * JSON boundary responsibilities.
 */

#pragma once

#include "praxis/Intent.hpp"
#include "InteractionPublic.hpp"
#include "Selector.hpp"
#include "Reference.hpp"
#include "Inspection.hpp"  // For FaceRef
#include <string>
#include <vector>

namespace praxis {

// Append a mapped resolution result (public JSON) to interaction.resolutions_json
void emit_resolution_public(IntentResult& result, const reference::ResolutionResult& res);

// LEGACY: Commented out - Failure is pre-C.5 type
// Append a selection failure (typed) to interaction.failures
// void emit_selection_failure(IntentResult& result, const selector::Failure& failure);

// Append a Reference JSON string safely
void emit_reference_json(IntentResult& result, const std::string& ref_json);

// Deterministic stable_id builders (engine-side only). Viewer/planner treat IDs as opaque.
namespace stable_id {
    std::string body(int body_index);
    std::string face(int body_index, int face_index);
    std::string edge(int edge_index, const std::vector<inspection::FaceLocation>& adjacent_faces);
    std::string vertex(int vertex_index, const std::vector<int>& edge_indices);
}

// Append a selectable (ref_type + stable_id + label)
void emit_selectable(IntentResult& result,
					 const std::string& ref_type,
					 const std::string& stable_id,
					 const std::string& label);

// Set inspection JSON (optional)
void set_inspection_json(IntentResult& result, const std::string& inspection_json);

// Ensure execution timestamp present (ISO 8601) if desired
void ensure_execution_timestamp(IntentResult& result, const std::string& iso8601);

// Finalize ordering for deterministic emission (lexical sort of JSON arrays)
void finalize_interaction_ordering(IntentResult& result);

// Populate selectables from artifact inspection (faces, edges, bodies)
// Generates stable_id for each selectable entity using topology-based references
void populate_selectables_from_artifact(IntentResult& result, const std::string& artifact_path);

} // namespace praxis
