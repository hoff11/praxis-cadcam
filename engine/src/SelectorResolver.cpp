/**
 * SelectorResolver.cpp
 * 
 * Phase C.5: Selector Resolution Implementation
 * 
 * v0.5.0 Correctness Fix:
 * - Candidate-based resolution (no index parsing from stable_ids)
 * - Direct property access from cached structures
 * - Multi-body safe: works correctly regardless of body count
 * - Performance: enumerate once per resolve()
 * 
 * Resolves parsed selector AST against artifact topology using Inspector queries.
 */

#include "SelectorResolver.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace praxis {
namespace selector {

// ============================================================================
// Constants
// ============================================================================

// Stable ID signature prefix length (v0.5.0)
// Increased from 8 to 16 to reduce collision probability
static constexpr size_t SIGNATURE_PREFIX_LENGTH = 16;

// ============================================================================
// Helper Functions
// ============================================================================

static std::string to_lower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return str;
}

// Generate stable_id from properties (v0.5.0: collision-resistant format)
// Format: {type}_{index}_{signature_prefix}
// Used for globally-unique entities (bodies, vertices)
static std::string make_stable_id(const std::string& prefix, int index, const std::string& signature) {
    size_t sig_len = std::min(SIGNATURE_PREFIX_LENGTH, signature.length());
    return prefix + "_" + std::to_string(index) + "_" + signature.substr(0, sig_len);
}

// Generate stable_id for body-scoped entities (faces, edges)
// Format: {type}_b{body_index}_{local_index}_{signature_prefix}
// Includes body_index to prevent collisions across bodies with identical geometry
static std::string make_body_scoped_stable_id(const std::string& prefix, int body_index, int index, const std::string& signature) {
    size_t sig_len = std::min(SIGNATURE_PREFIX_LENGTH, signature.length());
    return prefix + "_b" + std::to_string(body_index) + "_" + std::to_string(index) + "_" + signature.substr(0, sig_len);
}

// ============================================================================
// SelectorResolver Implementation
// ============================================================================

SelectorResolver::SelectorResolver(inspection::Inspector& inspector)
    : inspector_(inspector) {}

ResolveResult SelectorResolver::resolve(const Selector& selector) {
    ResolveResult result;
    result.selector_canonical = selector.canonical;
    
    try {
        // Step 1: Build candidates once (enumerate + create stable_ids)
        std::vector<Candidate> candidates = build_candidates(selector.target);
        
        if (candidates.empty()) {
            result.error = ResolveError{
                ResolveErrorKind::NoMatches,
                "No topology found for target type",
                selector.canonical
            };
            return result;
        }
        
        // Step 2: Apply filters using candidate properties (no re-enumeration)
        std::vector<std::string> matches;
        for (const auto& candidate : candidates) {
            if (matches_filters(candidate, selector.target, selector.filters)) {
                matches.push_back(candidate.stable_id);
            }
        }
        
        // Step 3: Check for matches
        if (matches.empty()) {
            result.error = ResolveError{
                ResolveErrorKind::NoMatches,
                "No topology matches filters",
                selector.canonical
            };
            return result;
        }
        
        result.stable_ids = std::move(matches);
        result.match_count = static_cast<int>(result.stable_ids.size());
        return result;
        
    } catch (const PropertyNotFoundException& e) {
        result.error = ResolveError{
            ResolveErrorKind::PropertyNotFound,
            e.what(),
            selector.canonical
        };
        return result;
        
    } catch (const TypeMismatchException& e) {
        result.error = ResolveError{
            ResolveErrorKind::TypeMismatch,
            e.what(),
            selector.canonical
        };
        return result;
        
    } catch (const InspectorException& e) {
        result.error = ResolveError{
            ResolveErrorKind::InspectorError,
            e.what(),
            selector.canonical
        };
        return result;
        
    } catch (const std::exception& e) {
        // Unexpected error
        result.error = ResolveError{
            ResolveErrorKind::InspectorError,
            std::string("Unexpected error: ") + e.what(),
            selector.canonical
        };
        return result;
    }
}

// ============================================================================
// Candidate Building (Enumerate Once)
// ============================================================================

std::vector<SelectorResolver::Candidate> SelectorResolver::build_candidates(SelectorTarget target) {
    std::vector<Candidate> candidates;
    
    switch (target) {
        case SelectorTarget::Bodies: {
            auto bodies = inspector_.enumerate_bodies();
            for (const auto& body : bodies) {
                Candidate c;
                c.stable_id = make_stable_id("body", body.index, body.signature);
                c.properties = body;
                candidates.push_back(std::move(c));
            }
            break;
        }
        
        case SelectorTarget::Faces: {
            auto faces = inspector_.enumerate_all_faces();
            for (const auto& face : faces) {
                Candidate c;
                // Use body-scoped ID to prevent collisions across bodies
                c.stable_id = make_body_scoped_stable_id("face", face.body_index, face.index, face.signature);
                c.properties = face;
                candidates.push_back(std::move(c));
            }
            break;
        }
        
        case SelectorTarget::Edges: {
            auto edges = inspector_.enumerate_all_edges();
            for (const auto& edge : edges) {
                Candidate c;
                // Edges: derive body from first adjacent face
                int body_idx = edge.adjacent_faces.empty() ? 0 : edge.adjacent_faces[0].body_index;
                c.stable_id = make_body_scoped_stable_id("edge", body_idx, edge.index, edge.signature);
                c.properties = edge;
                candidates.push_back(std::move(c));
            }
            break;
        }
        
        case SelectorTarget::Vertices: {
            auto vertices = inspector_.enumerate_all_vertices();
            for (const auto& vertex : vertices) {
                Candidate c;
                // Vertices: use global index without body prefix
                // Vertices are deduplicated across all bodies by the inspector,
                // so vertex.index is globally unique within the artifact
                c.stable_id = make_stable_id("vertex", vertex.index, vertex.signature);
                c.properties = vertex;
                candidates.push_back(std::move(c));
            }
            break;
        }
    }
    
    return candidates;
}

// ============================================================================
// Filter Execution (Using Candidates)
// ============================================================================

bool SelectorResolver::matches_filters(const Candidate& candidate,
                                       SelectorTarget target,
                                       const std::vector<Filter>& filters) {
    // Empty filters = match all
    if (filters.empty()) {
        return true;
    }
    
    // AND semantics: all filters must pass
    for (const auto& filter : filters) {
        auto prop_value = get_property(candidate, target, filter.field);
        
        if (!prop_value.has_value()) {
            throw PropertyNotFoundException(filter.field, selector::to_string(target));
        }
        
        if (!compare_values(*prop_value, filter.op, filter.value)) {
            return false;  // Filter failed
        }
    }
    
    return true;  // All filters passed
}

std::optional<std::any> SelectorResolver::get_property(const Candidate& candidate,
                                                        SelectorTarget target,
                                                        const std::string& field) {
    std::string field_lower = to_lower(field);
    
    // Get index from properties (not from parsing stable_id!)
    int index = -1;
    
    switch (target) {
        case SelectorTarget::Bodies: {
            const auto& body = std::get<inspection::BodyProperties>(candidate.properties);
            index = body.index;
            
            // Common property
            if (field_lower == "index") {
                return std::any(static_cast<int64_t>(index));
            }
            
            // Body-specific properties
            if (field_lower == "volume") return std::any(body.volume);
            break;
        }
        
        case SelectorTarget::Faces: {
            const auto& face = std::get<inspection::FaceProperties>(candidate.properties);
            index = face.index;
            
            // Common property
            if (field_lower == "index") {
                return std::any(static_cast<int64_t>(index));
            }
            
            // Face-specific properties
            if (field_lower == "type") {
                return std::any(inspection::surface_type_to_string(face.surface_type));
            }
            if (field_lower == "area") return std::any(face.area);
            if (field_lower == "body") return std::any(static_cast<int64_t>(face.body_index));
            break;
        }
        
        case SelectorTarget::Edges: {
            const auto& edge = std::get<inspection::EdgeProperties>(candidate.properties);
            index = edge.index;
            
            // Common property
            if (field_lower == "index") {
                return std::any(static_cast<int64_t>(index));
            }
            
            // Edge-specific properties
            if (field_lower == "type") {
                return std::any(inspection::edge_type_to_string(edge.edge_type));
            }
            if (field_lower == "length") return std::any(edge.length);
            if (field_lower == "radius") return std::any(edge.radius);
            break;
        }
        
        case SelectorTarget::Vertices: {
            const auto& vertex = std::get<inspection::VertexProperties>(candidate.properties);
            index = vertex.index;
            
            // Common property
            if (field_lower == "index") {
                return std::any(static_cast<int64_t>(index));
            }
            
            // Vertex-specific properties (could add x, y, z if needed)
            break;
        }
    }
    
    return std::nullopt;
}

bool SelectorResolver::compare_values(const std::any& prop_value,
                                      FilterOp op,
                                      const Literal& filter_value) {
    // Try numeric comparison first
    auto prop_double = to_double(prop_value);
    
    if (prop_double.has_value()) {
        // Property is numeric - filter must also be numeric
        if (filter_value.kind == Literal::Kind::String) {
            throw TypeMismatchException("numeric", "string");
        }
        
        double prop_val = *prop_double;
        double filter_val = 0.0;
        
        if (filter_value.kind == Literal::Kind::Int) {
            filter_val = static_cast<double>(filter_value.i);
        } else if (filter_value.kind == Literal::Kind::Float) {
            filter_val = filter_value.d;
        }
        
        const double EPSILON = 1e-9;
        
        switch (op) {
            case FilterOp::Eq:
                return std::abs(prop_val - filter_val) < EPSILON;
            case FilterOp::Neq:
                return std::abs(prop_val - filter_val) >= EPSILON;
            case FilterOp::Lt:
                return prop_val < filter_val;
            case FilterOp::Lte:
                return prop_val <= filter_val;
            case FilterOp::Gt:
                return prop_val > filter_val;
            case FilterOp::Gte:
                return prop_val >= filter_val;
        }
    }
    
    // Try string comparison
    auto prop_string = any_to_string(prop_value);
    
    if (prop_string.has_value()) {
        // Property is string - filter must also be string
        if (filter_value.kind != Literal::Kind::String) {
            throw TypeMismatchException("string", "numeric");
        }
        
        std::string prop_str = to_lower(*prop_string);
        std::string filter_str = to_lower(filter_value.s);
        
        switch (op) {
            case FilterOp::Eq:
                return prop_str == filter_str;
            case FilterOp::Neq:
                return prop_str != filter_str;
            case FilterOp::Lt:
                return prop_str < filter_str;
            case FilterOp::Lte:
                return prop_str <= filter_str;
            case FilterOp::Gt:
                return prop_str > filter_str;
            case FilterOp::Gte:
                return prop_str >= filter_str;
        }
    }
    
    throw InspectorException("Unsupported property type for comparison");
}

// ============================================================================
// Type Coercion Helpers
// ============================================================================

std::optional<double> SelectorResolver::to_double(const std::any& value) {
    if (value.type() == typeid(double)) {
        return std::any_cast<double>(value);
    }
    if (value.type() == typeid(float)) {
        return static_cast<double>(std::any_cast<float>(value));
    }
    if (value.type() == typeid(int)) {
        return static_cast<double>(std::any_cast<int>(value));
    }
    if (value.type() == typeid(int64_t)) {
        return static_cast<double>(std::any_cast<int64_t>(value));
    }
    return std::nullopt;
}

std::optional<int64_t> SelectorResolver::to_int(const std::any& value) {
    if (value.type() == typeid(int64_t)) {
        return std::any_cast<int64_t>(value);
    }
    if (value.type() == typeid(int)) {
        return static_cast<int64_t>(std::any_cast<int>(value));
    }
    return std::nullopt;
}

std::optional<std::string> SelectorResolver::any_to_string(const std::any& value) {
    if (value.type() == typeid(std::string)) {
        return std::any_cast<std::string>(value);
    }
    if (value.type() == typeid(const char*)) {
        return std::string(std::any_cast<const char*>(value));
    }
    return std::nullopt;
}

// ============================================================================
// Utilities
// ============================================================================

std::string to_string(ResolveErrorKind kind) {
    switch (kind) {
        case ResolveErrorKind::NoMatches: return "NoMatches";
        case ResolveErrorKind::PropertyNotFound: return "PropertyNotFound";
        case ResolveErrorKind::TypeMismatch: return "TypeMismatch";
        case ResolveErrorKind::InspectorError: return "InspectorError";
    }
    return "Unknown";
}

}  // namespace selector
}  // namespace praxis
