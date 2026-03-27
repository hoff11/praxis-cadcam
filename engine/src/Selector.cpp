/**
 * Selector.cpp
 * 
 * Sprint 7 - Epic 1: Selector Grammar Engine
 * Implementation of selector parser and executor per selector_grammar.md
 */

#include "Selector.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>

namespace praxis {
namespace selector {

// ============================================================================
// String Utilities
// ============================================================================

static std::string trim(const std::string& str) {
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) {
        start++;
    }
    auto end = str.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(*end));
    return std::string(start, end + 1);
}

static std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// ============================================================================
// SelectorExecutor Implementation
// ============================================================================

SelectorExecutor::SelectorExecutor(inspection::Inspector* inspector)
    : inspector_(inspector) {}

/**
 * Normalize simplified functional syntax to canonical grammar
 * 
 * Converts:
 *   bodies() → product:body!many
 *   faces() → feature:face!many
 *   edges() → feature:edge!many
 *   faces(?type=planar) → feature:face?type=planar!many
 *   faces(@body(index=0)) → feature:face@body(index=0)!many
 *   
 * Note: Scope syntax @body(?index=0) with ? is non-canonical, normalize to @body(index=0)
 */
static std::string normalize_functional_syntax(const std::string& selector) {
    std::string trimmed = trim(selector);
    
    // Check for functional syntax: target([@scope][?filters])
    // bodies(), faces(), edges(), bodies(?index=0), faces(@body(index=0)), etc.
    
    // Match pattern: <target>([...])
    size_t paren_start = trimmed.find('(');
    if (paren_start == std::string::npos) {
        return selector;  // Not functional syntax, return as-is
    }
    
    size_t paren_end = trimmed.rfind(')');
    if (paren_end == std::string::npos || paren_end < paren_start) {
        return selector;  // Malformed, let main parser handle
    }
    
    std::string target = trim(trimmed.substr(0, paren_start));
    std::string args = trimmed.substr(paren_start + 1, paren_end - paren_start - 1);
    
    // Map functional names to canonical mode:target
    std::string canonical;
    if (target == "bodies") {
        canonical = "product:body";
    } else if (target == "faces") {
        canonical = "feature:face";
    } else if (target == "edges") {
        canonical = "feature:edge";
    } else if (target == "vertices") {
        canonical = "feature:vertex";
    } else if (target == "axes") {
        canonical = "feature:axis";
    } else if (target == "points") {
        canonical = "feature:point";
    } else {
        return selector;  // Not a recognized functional target
    }
    
    // Process arguments: @scope or ?filters or both
    // Normalize scope syntax: remove leading ? from scope filters if present
    if (!args.empty()) {
        // If args start with @, it's a scope
        if (args[0] == '@') {
            // Find the scope portion: @target(filters)
            size_t scope_paren = args.find('(', 1);
            if (scope_paren != std::string::npos) {
                size_t scope_end = args.find(')', scope_paren);
                if (scope_end != std::string::npos) {
                    std::string scope_part = args.substr(0, scope_end + 1);
                    std::string remainder = (scope_end + 1 < args.length()) ? args.substr(scope_end + 1) : "";
                    
                    // Normalize scope filters: @body(?index=0) → @body(index=0)
                    size_t q_pos = scope_part.find("(?");
                    if (q_pos != std::string::npos) {
                        scope_part.erase(q_pos + 1, 1);  // Remove the ?
                    }
                    
                    canonical += scope_part + remainder;
                } else {
                    canonical += args;
                }
            } else {
                canonical += args;
            }
        } else {
            // No scope, just filters
            canonical += args;
        }
    }
    
    // Default to !many cardinality for pluralized forms
    canonical += "!many";
    
    return canonical;
}

std::optional<ParsedSelector> SelectorExecutor::parse(const std::string& selector) {
    if (selector.empty() || trim(selector).empty()) {
        return std::nullopt;
    }
    
    // Normalize functional syntax to canonical grammar
    std::string normalized = normalize_functional_syntax(selector);
    
    ParsedSelector parsed;
    parsed.original = selector;  // Keep original for provenance
    
    // Parse: <mode>:<target>[@<scope>][?<filters>][!<cardinality>]
    
    // 1. Split by cardinality marker (!)
    std::string working = normalized;
    size_t card_pos = working.rfind('!');
    if (card_pos != std::string::npos) {
        std::string card_str = working.substr(card_pos + 1);
        auto card = parse_cardinality(trim(card_str));
        if (!card) {
            return std::nullopt;
        }
        parsed.cardinality = *card;
        working = working.substr(0, card_pos);
    } else {
        parsed.cardinality = Cardinality::One;  // Default
    }
    
    // 2. Split by filter marker (?)
    size_t filter_pos = working.find('?');
    std::string filters_str;
    if (filter_pos != std::string::npos) {
        filters_str = working.substr(filter_pos + 1);
        working = working.substr(0, filter_pos);
    }
    
    // 3. Split by scope marker (@)
    size_t scope_pos = working.find('@');
    if (scope_pos != std::string::npos) {
        std::string scope_str = working.substr(scope_pos + 1);
        working = working.substr(0, scope_pos);
        
        // Parse scope: target(filters)
        size_t paren_start = scope_str.find('(');
        size_t paren_end = scope_str.rfind(')');
        if (paren_start == std::string::npos || paren_end == std::string::npos || paren_end < paren_start) {
            return std::nullopt;  // Invalid scope syntax
        }
        
        ScopeSelector scope;
        scope.target = trim(scope_str.substr(0, paren_start));
        std::string scope_filters = scope_str.substr(paren_start + 1, paren_end - paren_start - 1);
        scope.filters = parse_filters(scope_filters);
        parsed.scope = scope;
    }
    
    // 4. Parse mode:target
    size_t colon_pos = working.find(':');
    if (colon_pos == std::string::npos) {
        return std::nullopt;  // Invalid: no mode
    }
    
    std::string mode_str = trim(working.substr(0, colon_pos));
    auto mode = parse_mode(mode_str);
    if (!mode) {
        return std::nullopt;
    }
    parsed.mode = *mode;
    
    parsed.target = trim(working.substr(colon_pos + 1));
    if (parsed.target.empty()) {
        return std::nullopt;
    }
    
    // 5. Parse filters
    if (!filters_str.empty()) {
        parsed.filters = parse_filters(filters_str);
        
        // Extract aggregates from filters
        for (const auto& filter : parsed.filters) {
            if (filter.op == "max" || filter.op == "min") {
                parsed.aggregate = filter.op;
                parsed.aggregate_attribute = filter.attribute;
                break;  // Only one aggregate supported
            }
        }
    }
    
    // 6. Validate mode/target combination (contract compliance)
    bool valid_combination = false;
    if (parsed.mode == SelectionMode::Product) {
        // Product mode: product, body, region
        valid_combination = (parsed.target == "product" || 
                            parsed.target == "body" || 
                            parsed.target == "region");
    } else if (parsed.mode == SelectionMode::Feature) {
        // Feature mode: face, edge, axis, point, vertex (but NOT sketch, feature, extrude, etc.)
        valid_combination = (parsed.target == "face" || 
                            parsed.target == "edge" || 
                            parsed.target == "axis" || 
                            parsed.target == "point" || 
                            parsed.target == "vertex");
    }
    
    if (!valid_combination) {
        return std::nullopt;  // Contract violation
    }
    
    return parsed;
}

SelectionResult SelectorExecutor::execute(const ParsedSelector& selector) {
    SelectionResult result;
    result.success = false;
    
    // Generate ISO 8601 timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%SZ");
    result.timestamp = ss.str();
    
    // Enumerate candidates
    auto candidates = enumerate_candidates(selector);
    
    // Apply filters
    auto filtered = apply_filters_to_candidates(candidates, selector.filters);
    
    // Apply aggregates if specified
    if (!selector.aggregate.empty()) {
        filtered = apply_aggregate_reducer(filtered, selector.aggregate, selector.aggregate_attribute);
    }
    
    // Enforce cardinality
    bool cardinality_ok = enforce_cardinality(filtered, selector.cardinality, result);
    if (!cardinality_ok) {
        return result;
    }
    
    // Success
    result.success = true;
    result.references = filtered;
    return result;
}

SelectionResult SelectorExecutor::select(const std::string& selector) {
    auto parsed = parse(selector);
    if (!parsed) {
        SelectionResult result;
        result.success = false;
        Failure failure;
        failure.type = FailureType::InvalidSelector;
        failure.message = "Failed to parse selector: invalid syntax";
        failure.selector = selector;
        result.failures.push_back(failure);
        return result;
    }
    
    return execute(*parsed);
}

std::optional<SelectionMode> SelectorExecutor::parse_mode(const std::string& mode_str) {
    if (mode_str == "product") return SelectionMode::Product;
    if (mode_str == "feature") return SelectionMode::Feature;
    return std::nullopt;
}

std::optional<Cardinality> SelectorExecutor::parse_cardinality(const std::string& card_str) {
    if (card_str == "one") return Cardinality::One;
    if (card_str == "many") return Cardinality::Many;
    if (card_str == "optional") return Cardinality::Optional;
    return std::nullopt;
}

std::vector<Filter> SelectorExecutor::parse_filters(const std::string& filter_str) {
    std::vector<Filter> filters;
    
    if (filter_str.empty()) {
        return filters;
    }
    
    // Split by & for multiple filters
    auto filter_parts = split(filter_str, '&');
    
    for (const auto& part : filter_parts) {
        std::string trimmed = trim(part);
        if (trimmed.empty()) continue;
        
        Filter filter;
        
        // Determine operator
        size_t op_pos = std::string::npos;
        
        // Check for ~= (approximate match)
        if ((op_pos = trimmed.find("~=")) != std::string::npos) {
            filter.op = "~=";
            filter.attribute = trim(trimmed.substr(0, op_pos));
            filter.value = trim(trimmed.substr(op_pos + 2));
        }
        // Check for = (exact match)
        else if ((op_pos = trimmed.find('=')) != std::string::npos) {
            filter.op = "=";
            filter.attribute = trim(trimmed.substr(0, op_pos));
            filter.value = trim(trimmed.substr(op_pos + 1));
        }
        else {
            // No operator, skip invalid filter
            continue;
        }
        
        // Handle aggregates (max, min)
        if (filter.value == "max") {
            filter.op = "max";
            filter.value = "";  // Aggregate has no value
        } else if (filter.value == "min") {
            filter.op = "min";
            filter.value = "";
        }
        
        // Remove quotes from string values
        if (!filter.value.empty() && filter.value.front() == '"' && filter.value.back() == '"') {
            filter.value = filter.value.substr(1, filter.value.length() - 2);
        }
        
        filters.push_back(filter);
    }
    
    return filters;
}

std::vector<Reference> SelectorExecutor::enumerate_candidates(const ParsedSelector& selector) {
    if (!inspector_) return {};
    
    std::vector<Reference> refs;
    
    if (selector.target == "face") {
        auto bodies = inspector_->enumerate_bodies();
        for (const auto& body : bodies) {
            auto faces = inspector_->enumerate_faces(body.index);
            for (const auto& face : faces) {
                Reference ref;
                ref.type = "face";
                ref.index = face.index;
                ref.properties = face;
                refs.push_back(ref);
            }
        }
    } else if (selector.target == "edge") {
        // Use enumerate_all_edges() to get unique edges (not duplicated per face)
        auto edges = inspector_->enumerate_all_edges();
        for (const auto& edge : edges) {
            Reference ref;
            ref.type = "edge";
            ref.index = edge.index;
            ref.properties = edge;
            refs.push_back(ref);
        }
    } else if (selector.target == "body") {
        auto bodies = inspector_->enumerate_bodies();
        for (const auto& body : bodies) {
            Reference ref;
            ref.type = "body";
            ref.index = body.index;
            ref.properties = body;
            refs.push_back(ref);
        }
    }
    // TODO: product, region when needed
    
    return refs;
}

std::vector<Reference> SelectorExecutor::apply_filters_to_candidates(
    const std::vector<Reference>& candidates,
    const std::vector<Filter>& filters) {
    
    if (filters.empty()) return candidates;
    
    std::vector<Reference> filtered;
    for (const auto& candidate : candidates) {
        bool matches = true;
        
        // All filters must match (AND semantics)
        // Skip aggregate operators (they're handled separately)
        for (const auto& filter : filters) {
            if (filter.op == "max" || filter.op == "min") {
                continue;  // Skip aggregates
            }
            
            if (!evaluate_filter(candidate, filter)) {
                matches = false;
                break;
            }
        }
        
        if (matches) {
            filtered.push_back(candidate);
        }
    }
    
    return filtered;
}

bool SelectorExecutor::evaluate_filter(const Reference& ref, const Filter& filter) {
    std::string attr_value = extract_attribute(ref, filter.attribute);
    
    if (filter.op == "=") {
        // Exact match
        return attr_value == filter.value;
    } else if (filter.op == "~=") {
        // Approximate match (numeric tolerance or string equality)
        try {
            double actual = std::stod(attr_value);
            double expected = std::stod(filter.value);
            double tolerance = 1e-6;  // TODO: Make configurable
            return std::abs(actual - expected) < tolerance;
        } catch (...) {
            // Non-numeric: fall back to exact
            return attr_value == filter.value;
        }
    }
    
    return false;
}

std::string SelectorExecutor::extract_attribute(const Reference& ref, const std::string& attr_name) {
    // Extract attribute from typed properties
    if (ref.type == "face") {
        const auto* face = std::any_cast<inspection::FaceProperties>(&ref.properties);
        if (!face) return "";
        
        if (attr_name == "type") {
            static const std::map<inspection::SurfaceType, std::string> type_map = {
                {inspection::SurfaceType::PLANAR, "planar"},
                {inspection::SurfaceType::CYLINDRICAL, "cylindrical"},
                {inspection::SurfaceType::CONICAL, "conical"},
                {inspection::SurfaceType::SPHERICAL, "spherical"},
                {inspection::SurfaceType::TOROIDAL, "toroidal"},
                {inspection::SurfaceType::BSPLINE, "bspline"},
                {inspection::SurfaceType::OTHER, "other"}
            };
            auto it = type_map.find(face->surface_type);
            return (it != type_map.end()) ? it->second : "unknown";
        }
        if (attr_name == "area") return std::to_string(face->area);
        if (attr_name == "normal.x") return std::to_string(face->normal.x);
        if (attr_name == "normal.y") return std::to_string(face->normal.y);
        if (attr_name == "normal.z") return std::to_string(face->normal.z);
    } else if (ref.type == "edge") {
        const auto* edge = std::any_cast<inspection::EdgeProperties>(&ref.properties);
        if (!edge) return "";
        
        if (attr_name == "type") {
            static const std::map<inspection::EdgeType, std::string> type_map = {
                {inspection::EdgeType::LINE, "line"},
                {inspection::EdgeType::CIRCULAR, "circular"},
                {inspection::EdgeType::ELLIPTICAL, "elliptical"},
                {inspection::EdgeType::BSPLINE, "bspline"},
                {inspection::EdgeType::OTHER, "other"}
            };
            auto it = type_map.find(edge->edge_type);
            return (it != type_map.end()) ? it->second : "unknown";
        }
        if (attr_name == "length") return std::to_string(edge->length);
        if (attr_name == "radius") return std::to_string(edge->radius);
    }
    
    return "";
}

std::vector<Reference> SelectorExecutor::apply_aggregate_reducer(
    const std::vector<Reference>& candidates,
    const std::string& aggregate,
    const std::string& attribute) {
    
    if (candidates.empty()) return {};
    if (aggregate != "max" && aggregate != "min") return candidates;
    
    int best_idx = 0;
    double best_val = 0.0;
    bool found_numeric = false;
    
    for (size_t i = 0; i < candidates.size(); i++) {
        std::string val_str = extract_attribute(candidates[i], attribute);
        try {
            double val = std::stod(val_str);
            
            if (!found_numeric) {
                best_val = val;
                best_idx = i;
                found_numeric = true;
            } else if ((aggregate == "max" && val > best_val) ||
                       (aggregate == "min" && val < best_val)) {
                best_val = val;
                best_idx = i;
            }
        } catch (...) {
            // Skip non-numeric
        }
    }
    
    if (!found_numeric) return {};
    return {candidates[best_idx]};
}

std::vector<Reference> SelectorExecutor::apply_filters(
    const std::vector<Reference>& candidates,
    const std::vector<Filter>& filters) {
    // Deprecated - use apply_filters_to_candidates
    return apply_filters_to_candidates(candidates, filters);
}

bool SelectorExecutor::enforce_cardinality(
    const std::vector<Reference>& refs,
    Cardinality card,
    SelectionResult& result) {
    
    size_t count = refs.size();
    
    if (card == Cardinality::One) {
        if (count == 0) {
            Failure failure;
            failure.type = FailureType::Missing;
            failure.message = "Selector matched 0 objects (expected exactly 1)";
            result.failures.push_back(failure);
            return false;
        } else if (count > 1) {
            Failure failure;
            failure.type = FailureType::Ambiguous;
            failure.message = "Selector matched " + std::to_string(count) + 
                             " objects (expected exactly 1)";
            result.failures.push_back(failure);
            return false;
        }
        // count == 1: success
        return true;
    } else if (card == Cardinality::Many) {
        // Sprint 8: many means >= 1 (not 0..N)
        if (count == 0) {
            Failure failure;
            failure.type = FailureType::Missing;
            failure.message = "Selector matched 0 objects (expected one or more)";
            result.failures.push_back(failure);
            return false;
        }
        return true;
    } else { // Cardinality::Optional
        // Allow 0 or 1, >1 is ambiguous
        if (count > 1) {
            Failure failure;
            failure.type = FailureType::Ambiguous;
            failure.message = "Selector matched " + std::to_string(count) +
                              " objects (expected 0 or 1)";
            result.failures.push_back(failure);
            return false;
        }
        return true;
    }
}

// ============================================================================
// Utilities
// ============================================================================

std::string failure_type_to_string(FailureType type) {
    switch (type) {
        case FailureType::InvalidSelector: return "InvalidSelector";
        case FailureType::ContractViolation: return "ContractViolation";
        case FailureType::Missing: return "Missing";
        case FailureType::Ambiguous: return "Ambiguous";
        case FailureType::ResolutionFailure: return "ResolutionFailure";
        default: return "Unknown";
    }
}

std::string selection_mode_to_string(SelectionMode mode) {
    switch (mode) {
        case SelectionMode::Product: return "product";
        case SelectionMode::Feature: return "feature";
        default: return "unknown";
    }
}

std::string cardinality_to_string(Cardinality card) {
    switch (card) {
        case Cardinality::One: return "one";
        case Cardinality::Many: return "many";
        case Cardinality::Optional: return "optional";
        default: return "unknown";
    }
}

} // namespace selector
} // namespace praxis
