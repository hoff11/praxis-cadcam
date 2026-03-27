#include "InteractionEmit.hpp"
#include "OCCTInspector.hpp"
#include "Inspection.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <numeric>
#include <openssl/sha.h>

using namespace praxis::inspection;

namespace praxis {

void emit_resolution_public(IntentResult& result, const reference::ResolutionResult& res) {
    std::string json = interaction_public::resolution_to_public_json(res);
    result.interaction.resolutions_json.push_back(json);
    // Contract mismatch should also record a failure entry
    if (res.status == reference::ResolutionStatus::ContractMismatch) {
        IntentResult::InteractionInfo::Failure f;
        f.type = "ContractMismatch";
        f.message = "Reference contract_version mismatch";
        result.interaction.failures.push_back(f);
    }
}

// LEGACY: Commented out - Failure is pre-C.5 type
/*
void emit_selection_failure(IntentResult& result, const selector::Failure& failure) {
    IntentResult::InteractionInfo::Failure f;
    f.type = selector::failure_type_to_string(failure.type);
    f.message = failure.message;
    if (failure.selector.has_value()) {
        f.selector = *failure.selector;
    }
    result.interaction.failures.push_back(f);
}
*/

void emit_reference_json(IntentResult& result, const std::string& ref_json) {
    result.interaction.references_json.push_back(ref_json);
}

// Compute SHA256 hex for a stable identifier
static std::string sha256_hex(const std::string& bytes) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(bytes.c_str()), bytes.size(), hash);
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::setw(2) << static_cast<int>(hash[i]);
    }
    return ss.str();
}

namespace stable_id {
    std::string body(int body_index) {
        return "body:" + std::to_string(body_index);
    }

    std::string face(int body_index, int face_index) {
        return "face:" + std::to_string(body_index) + ":" + std::to_string(face_index);
    }

    std::string edge(int edge_index, const std::vector<FaceLocation>& adjacent_faces) {
        // Sort face references by (body, local_face) for determinism
        std::vector<FaceLocation> sorted = adjacent_faces;
        std::sort(sorted.begin(), sorted.end(), [](const FaceLocation& a, const FaceLocation& b) {
            if (a.body_index != b.body_index) return a.body_index < b.body_index;
            return a.local_face_index < b.local_face_index;
        });

        std::ostringstream oss;
        oss << "edge:" << edge_index;
        if (!sorted.empty()) {
            oss << ":faces:";
            for (size_t i = 0; i < sorted.size(); ++i) {
                if (i > 0) oss << ",";
                oss << sorted[i].body_index << ":" << sorted[i].local_face_index;
            }
        }
        return oss.str();
    }
    
    std::string vertex(int vertex_index, const std::vector<int>& edge_indices) {
        std::vector<int> sorted = edge_indices;
        std::sort(sorted.begin(), sorted.end());

        std::ostringstream oss;
        oss << "vertex:" << vertex_index;
        if (!sorted.empty()) {
            oss << ":edges:";
            for (size_t i = 0; i < sorted.size(); ++i) {
                if (i > 0) oss << ",";
                oss << sorted[i];
            }
        }
        return oss.str();
    }
} // namespace stable_id

void emit_selectable(IntentResult& result,
                     const std::string& ref_type,
                     const std::string& stable_id,
                     const std::string& label) {
    IntentResult::InteractionInfo::Selectable s;
    s.ref_type = ref_type;
    // NOTE: stable_id should be provided by the caller using engine-known invariants.
    // This fallback is deterministic but not semantically stable across topology changes.
    s.stable_id = stable_id.empty()
      ? (ref_type + ":anon:" + std::to_string(result.interaction.selectables.size()))
      : stable_id;
    s.label = label;
    result.interaction.selectables.push_back(std::move(s));
}

void set_inspection_json(IntentResult& result, const std::string& inspection_json) {
    result.interaction.inspection_json = inspection_json;
}

void ensure_execution_timestamp(IntentResult& result, const std::string& iso8601) {
    if (result.interaction.execution_timestamp.empty()) {
        result.interaction.execution_timestamp = iso8601;
    }
}

void finalize_interaction_ordering(IntentResult& result) {
    auto lex_sort = [](std::vector<std::string>& arr) {
        std::sort(arr.begin(), arr.end());
    };
    // Top-level JSON arrays
    lex_sort(result.interaction.references_json);
    lex_sort(result.interaction.resolutions_json);

    // Sort nested references for each selection for stability
    for (auto& sel : result.interaction.selections) {
        std::sort(sel.references_json.begin(), sel.references_json.end());
    }

    // Selections: stable, content-based key only (no timestamps)
    std::sort(result.interaction.selections.begin(), result.interaction.selections.end(),
              [](const IntentResult::InteractionInfo::Selection& a,
                 const IntentResult::InteractionInfo::Selection& b) {
                    if (a.mode != b.mode) return a.mode < b.mode;
                    if (a.target != b.target) return a.target < b.target;
                    if (a.selector != b.selector) return a.selector < b.selector;
                    // Tie-breaker: joined references_json (already sorted)
                    auto join_len = [](const std::vector<std::string>& refs) {
                        std::ostringstream oss;
                        for (const auto& r : refs) {
                            oss << r.size() << ':' << r << '\n';
                        }
                        return oss.str();
                    };
                    const std::string aj = join_len(a.references_json);
                    const std::string bj = join_len(b.references_json);
                    return aj < bj;
              });

    // Failures: type, message, selector, reference_json
    std::sort(result.interaction.failures.begin(), result.interaction.failures.end(),
              [](const IntentResult::InteractionInfo::Failure& a,
                 const IntentResult::InteractionInfo::Failure& b) {
                    if (a.type != b.type) return a.type < b.type;
                    if (a.message != b.message) return a.message < b.message;
                    if (a.selector != b.selector) return a.selector < b.selector;
                    return a.reference_json < b.reference_json;
              });

     // Selectables: sort by (ref_type, stable_id, label)
     std::sort(result.interaction.selectables.begin(), result.interaction.selectables.end(),
                  [](const IntentResult::InteractionInfo::Selectable& a,
                      const IntentResult::InteractionInfo::Selectable& b) {
                          if (a.ref_type != b.ref_type) return a.ref_type < b.ref_type;
                          if (a.stable_id != b.stable_id) return a.stable_id < b.stable_id;
                          return a.label < b.label;
                  });
}

void populate_selectables_from_artifact(IntentResult& result, const std::string& artifact_path) {
    // Load artifact and inspect topology
    inspection::OCCTInspector inspector;
    if (!inspector.load_artifact(artifact_path)) {
        return;  // Silently skip if artifact can't be loaded
    }
    
    // Enumerate bodies
    auto bodies = inspector.enumerate_bodies();
    for (const auto& body : bodies) {
        emit_selectable(result, "BodyRef", 
                       stable_id::body(body.index),
                       "Body " + std::to_string(body.index));
    }
    
    // Enumerate faces (use cached indices for consistency with edge adjacency)
    auto all_faces = inspector.enumerate_all_faces();
    for (const auto& face : all_faces) {
        std::string label = "Face " + std::to_string(face.body_index) + ":" + std::to_string(face.index);
        label += " (" + inspection::surface_type_to_string(face.surface_type) + ")";
        emit_selectable(result, "FaceRef",
                       stable_id::face(face.body_index, face.index),
                       label);
    }
    
    // Enumerate edges (unique, not duplicated)
    auto edges = inspector.enumerate_all_edges();
    for (const auto& edge : edges) {
        std::string label = "Edge " + std::to_string(edge.index);
        label += " (" + inspection::edge_type_to_string(edge.edge_type) + ")";
        
        // v2.0 contract: emit minimal stable_id, populate structured adjacency fields
        IntentResult::InteractionInfo::Selectable s;
        s.ref_type = "EdgeRef";
        s.stable_id = "edge:" + std::to_string(edge.index);  // Identity only (no diagnostic suffix)
        s.label = label;
        
        // Populate structured adjacency fields
        for (const auto& floc : edge.adjacent_faces) {
            IntentResult::InteractionInfo::Selectable::FaceLocation face_loc;
            face_loc.body = floc.body_index;
            face_loc.face = floc.local_face_index;
            s.faces.push_back(face_loc);
        }
        s.vertices = edge.vertex_indices;
        
        result.interaction.selectables.push_back(std::move(s));
    }
    
    // Enumerate vertices (unique, deduplicated)
    auto vertices = inspector.enumerate_all_vertices();
    for (const auto& vertex : vertices) {
        std::string label = "Vertex " + std::to_string(vertex.index);
        
        // v2.0 contract: emit minimal stable_id, populate structured adjacency fields
        IntentResult::InteractionInfo::Selectable s;
        s.ref_type = "VertexRef";
        s.stable_id = "vertex:" + std::to_string(vertex.index);  // Identity only (no diagnostic suffix)
        s.label = label;
        s.edges = vertex.edge_indices;
        
        result.interaction.selectables.push_back(std::move(s));
    }
}

} // namespace praxis
