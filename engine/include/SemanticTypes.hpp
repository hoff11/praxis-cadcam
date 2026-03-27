/**
 * SemanticTypes.hpp
 * 
 * Semantic type enumeration for Praxis objects
 * Sprint 10 - Epic 1: Semantic Object Model
 * 
 * Purpose: Closed enumeration of all semantic objects in Praxis
 * Contract: Per semantic_objects.md - authoritative type system
 * 
 * Used by:
 * - SelectCommand: Mode validation and type checking
 * - ResolveCommand: Reference type validation
 * - InspectCommand: Property classification
 * - Report JSON: Semantic type annotation
 * - Output classification: Preview capability determination
 */

#pragma once

#include <string>
#include <optional>
#include <cctype>
#include <algorithm>

namespace praxis {
namespace semantic {

/**
 * SemanticType - Closed enumeration of all semantic objects
 * 
 * This enum is the code-level enforcement of semantic_objects.md
 * No stringly-typed semantic references should exist in the codebase
 */
enum class SemanticType {
    Product,    // Complete manufacturable assembly
    Body,       // Solid geometric volume
    Face,       // Surface boundary of body
    Edge,       // Curve boundary of face
    Artifact,   // Serialized geometric output (STEP/STL)
    Output      // Named execution result
};

/**
 * SelectionMode - Valid selection mode enumeration
 * 
 * Determines what semantic types may be selected
 * Enforces selection_modes.md contract
 */
enum class SelectionMode {
    ProductSelection,  // Select products and bodies
    BodySelection,     // Select individual bodies
    FaceSelection,     // Select individual faces
    EdgeSelection      // Select individual edges
};

/**
 * Convert SemanticType to canonical string representation
 */
inline std::string to_string(SemanticType type) {
    switch (type) {
        case SemanticType::Product:  return "Product";
        case SemanticType::Body:     return "Body";
        case SemanticType::Face:     return "Face";
        case SemanticType::Edge:     return "Edge";
        case SemanticType::Artifact: return "Artifact";
        case SemanticType::Output:   return "Output";
    }
    return "Unknown";
}

/**
 * Convert SelectionMode to canonical string representation
 */
inline std::string to_string(SelectionMode mode) {
    switch (mode) {
        case SelectionMode::ProductSelection: return "ProductSelection";
        case SelectionMode::BodySelection:    return "BodySelection";
        case SelectionMode::FaceSelection:    return "FaceSelection";
        case SelectionMode::EdgeSelection:    return "EdgeSelection";
    }
    return "Unknown";
}

/**
 * Parse SemanticType from string (case-insensitive)
 * Returns nullopt if string doesn't match a valid type
 */
inline std::optional<SemanticType> parse_semantic_type(const std::string& str) {
    std::string lower = str;
    for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    
    if (lower == "product")  return SemanticType::Product;
    if (lower == "body")     return SemanticType::Body;
    if (lower == "face")     return SemanticType::Face;
    if (lower == "edge")     return SemanticType::Edge;
    if (lower == "artifact") return SemanticType::Artifact;
    if (lower == "output")   return SemanticType::Output;
    
    return std::nullopt;
}

/**
 * Parse SelectionMode from string (case-insensitive)
 * Returns nullopt if string doesn't match a valid mode
 */
inline std::optional<SelectionMode> parse_selection_mode(const std::string& str) {
    std::string lower = str;
    for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    
    if (lower == "productselection" || lower == "product") return SelectionMode::ProductSelection;
    if (lower == "bodyselection" || lower == "body")       return SelectionMode::BodySelection;
    if (lower == "faceselection" || lower == "face")       return SelectionMode::FaceSelection;
    if (lower == "edgeselection" || lower == "edge")       return SelectionMode::EdgeSelection;
    
    return std::nullopt;
}

/**
 * Validate if a semantic type is selectable in a given selection mode
 * Enforces selection_modes.md contract matrix
 * 
 * Returns true if (mode, type) combination is legal
 */
inline bool is_selectable_in_mode(SelectionMode mode, SemanticType type) {
    switch (mode) {
        case SelectionMode::ProductSelection:
            return type == SemanticType::Product || type == SemanticType::Body;
        
        case SelectionMode::BodySelection:
            return type == SemanticType::Body;
        
        case SelectionMode::FaceSelection:
            return type == SemanticType::Face;
        
        case SelectionMode::EdgeSelection:
            return type == SemanticType::Edge;
    }
    
    return false;
}

/**
 * Generate contract-compliant failure message for invalid mode/type combination
 * Messages reference selection_modes.md explicitly
 */
inline std::string get_invalid_mode_message(SelectionMode mode, SemanticType type) {
    std::string mode_str = to_string(mode);
    std::string type_str = to_string(type);
    
    switch (mode) {
        case SelectionMode::ProductSelection:
            if (type == SemanticType::Face || type == SemanticType::Edge) {
                return type_str + " selection is not allowed in " + mode_str + " mode. Use " + 
                       to_string(type == SemanticType::Face ? SelectionMode::FaceSelection : SelectionMode::EdgeSelection) + 
                       " mode.";
            }
            break;
        
        case SelectionMode::BodySelection:
            if (type == SemanticType::Face || type == SemanticType::Edge) {
                return type_str + " selection is not allowed in " + mode_str + " mode. Use " + 
                       to_string(type == SemanticType::Face ? SelectionMode::FaceSelection : SelectionMode::EdgeSelection) + 
                       " mode.";
            }
            if (type == SemanticType::Product) {
                return "Product selection is not allowed in " + mode_str + " mode. Use ProductSelection mode.";
            }
            break;
        
        case SelectionMode::FaceSelection:
            return type_str + " selection is not allowed in " + mode_str + " mode. Use appropriate selection mode.";
        
        case SelectionMode::EdgeSelection:
            return type_str + " selection is not allowed in " + mode_str + " mode. Use appropriate selection mode.";
    }
    
    return type_str + " is not selectable in " + mode_str + " mode.";
}

/**
 * Determine SelectionMode from selector syntax
 * Per selection_modes.md mode detection rules
 * 
 * Supports both canonical grammar and functional syntax:
 * - product:... or product(...) → ProductSelection
 * - body:... or bodies(...) → BodySelection  
 * - face:... or faces(...) → FaceSelection
 * - edge:... or edges(...) → EdgeSelection
 */
inline std::optional<SelectionMode> detect_mode_from_selector(const std::string& selector) {
    // Canonical grammar: mode:target
    if (selector.find("product:") == 0 || selector.find("product(") == 0) {
        return SelectionMode::ProductSelection;
    }
    
    // Functional syntax: bodies() or canonical body:
    if (selector.find("body:") == 0 || selector.find("Body:") == 0 ||
        selector.find("bodies(") == 0 || selector.find("body(") == 0) {
        return SelectionMode::BodySelection;
    }
    
    // Functional syntax: faces() or canonical feature:face
    if (selector.find("face:") == 0 || selector.find("Face:") == 0 ||
        selector.find("faces(") == 0 || selector.find("face(") == 0 ||
        selector.find("feature:face") == 0) {
        return SelectionMode::FaceSelection;
    }
    
    // Functional syntax: edges() or canonical feature:edge
    if (selector.find("edge:") == 0 || selector.find("Edge:") == 0 ||
        selector.find("edges(") == 0 || selector.find("edge(") == 0 ||
        selector.find("feature:edge") == 0) {
        return SelectionMode::EdgeSelection;
    }
    
    return std::nullopt;
}

} // namespace semantic
} // namespace praxis