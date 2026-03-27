/**
 * Selector.hpp
 * 
 * Phase C.4: Selector Grammar Parser (Pure Parsing - No Topology Resolution)
 * 
 * Purpose: Parse selector strings into canonical AST (artifact-independent)
 * Contract: Deterministic, stateless, typed errors
 * 
 * Simplified Grammar (Phase C.4):
 *   bodies()
 *   faces()
 *   faces(body=0)
 *   faces(body=0, index=3)
 *   edges(type=linear)
 * 
 * Phase C.5 will add resolution (AST → stable_ids)
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <cstdint>

namespace praxis {
namespace selector {

// ============================================================================
// Phase C.4: Pure Parsing (No Inspection/Resolution Dependencies)
// ============================================================================

// Source span for error reporting
struct Span {
    int start = 0;
    int end = 0;
};

// Typed parse errors
enum class SelectorErrorKind {
    UnexpectedToken,
    UnexpectedEOF,
    InvalidNumber,
    InvalidIdentifier,
    InvalidOperator,
    InvalidFunction,
    TrailingInput,
    MissingClosingParen,
    MissingEquals,
    InvalidFilterValue
};

struct SelectorError {
    SelectorErrorKind kind;
    std::string message;
    Span span;
};

// Selector target types
enum class SelectorTarget {
    Bodies,
    Faces,
    Edges,
    Vertices
};

// Filter operators
enum class FilterOp {
    Eq,   // =
    Neq,  // !=
    Lt,   // <
    Lte,  // <=
    Gt,   // >
    Gte   // >=
};

// Literal value types
struct Literal {
    enum class Kind { Int, Float, String, Bool } kind;
    int64_t i = 0;
    double d = 0.0;
    std::string s;
    bool b = false;
    
    std::string canon;  // Canonical string representation for sorting/hashing
};

// Filter clause
struct Filter {
    std::string field;
    FilterOp op;
    Literal value;
};

// Selector AST (Phase C.4)
struct Selector {
    SelectorTarget target;
    std::vector<Filter> filters;
    
    std::string original;   // Original input string
    std::string canonical;  // Canonical normalized form
};

// Parse result with typed error
struct ParseResult {
    std::optional<Selector> selector;
    std::optional<SelectorError> error;
};

// ============================================================================
// Phase C.4 Parser (Stateless, Artifact-Free)
// ============================================================================

class SelectorParser {
public:
    /**
     * Parse selector string into AST
     * Pure function - no state, no artifact access
     */
    static ParseResult parse(std::string_view input);
    
    /**
     * Generate canonical form from AST
     * Deterministic: same meaning → same canonical string
     */
    static std::string canonicalize(const Selector& selector);
    
    /**
     * Serialize AST to JSON (for CLI output)
     */
    static std::string to_json(const Selector& selector);
    
    /**
     * Serialize parse error to JSON
     */
    static std::string error_to_json(const SelectorError& error);
};

// ============================================================================
// Utilities
// ============================================================================

std::string to_string(SelectorTarget target);
std::string to_string(FilterOp op);
std::string to_string(SelectorErrorKind kind);
std::string to_string(Literal::Kind kind);

}  // namespace selector

// ============================================================================
// Phase C.5 Forward Declaration (Resolution - Not Yet Implemented)
// ============================================================================

// TODO: Add SelectorResolver in Phase C.5
// class SelectorResolver {
// public:
//     static ResolveResult resolve(const selector::Selector& sel, const Artifact& artifact);
// };

}  // namespace praxis
