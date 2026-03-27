/**
 * SelectorParser.cpp
 * 
 * Phase C.4: Pure Selector Parser (No Topology Resolution)
 * 
 * Implementation of stateless selector parsing with:
 * - Canonical normalization (deterministic)
 * - Typed error reporting with source spans
 * - No Inspector/artifact dependencies
 */

#include "Selector.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace praxis {
namespace selector {

// ============================================================================
// String Utilities
// ============================================================================

static std::string to_lower(std::string_view str) {
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

static std::string trim(std::string_view str) {
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) {
        start++;
    }
    auto end = str.end();
    while (end > start && std::isspace(*(end - 1))) {
        end--;
    }
    return std::string(start, end);
}

// ============================================================================
// Canonical Formatting
// ============================================================================

static std::string format_canonical_float(double value) {
    // Use fixed precision for deterministic output
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << value;
    std::string result = oss.str();
    
    // Remove trailing zeros
    size_t dot_pos = result.find('.');
    if (dot_pos != std::string::npos) {
        size_t last_nonzero = result.find_last_not_of('0');
        if (last_nonzero > dot_pos) {
            result.erase(last_nonzero + 1);
        }
        if (result.back() == '.') {
            result.pop_back();
        }
    }
    
    return result;
}

static std::string literal_to_canon(const Literal& lit) {
    switch (lit.kind) {
        case Literal::Kind::Int:
            return std::to_string(lit.i);
        case Literal::Kind::Float:
            return format_canonical_float(lit.d);
        case Literal::Kind::String:
            // Only quote if string contains special characters or spaces
            // Otherwise use bare identifier form for canonical representation
            if (lit.s.empty() || 
                lit.s.find_first_of(" \t\n\r\"'=!<>()[]{}") != std::string::npos ||
                std::isdigit(lit.s[0])) {
                return "\"" + lit.s + "\"";
            }
            return lit.s;
        case Literal::Kind::Bool:
            return lit.b ? "true" : "false";
    }
    return "";
}

// ============================================================================
// Tokenizer (Simple Hand-Written Lexer)
// ============================================================================

enum class TokenKind {
    Identifier,
    Number,
    String,
    LParen,
    RParen,
    Comma,
    Equals,
    NotEquals,
    Lt,
    Lte,
    Gt,
    Gte,
    End
};

struct Token {
    TokenKind kind;
    std::string_view text;
    Span span;
};

class Tokenizer {
public:
    explicit Tokenizer(std::string_view input) : input_(input), pos_(0) {}
    
    Token next() {
        skip_whitespace();
        
        if (pos_ >= input_.size()) {
            return Token{TokenKind::End, "", {static_cast<int>(pos_), static_cast<int>(pos_)}};
        }
        
        char c = input_[pos_];
        int start = pos_;
        
        // Single-character tokens
        if (c == '(') {
            pos_++;
            return Token{TokenKind::LParen, input_.substr(start, 1), {start, start + 1}};
        }
        if (c == ')') {
            pos_++;
            return Token{TokenKind::RParen, input_.substr(start, 1), {start, start + 1}};
        }
        if (c == ',') {
            pos_++;
            return Token{TokenKind::Comma, input_.substr(start, 1), {start, start + 1}};
        }
        
        // Operators
        if (c == '=') {
            pos_++;
            return Token{TokenKind::Equals, input_.substr(start, 1), {start, start + 1}};
        }
        if (c == '!') {
            if (pos_ + 1 < input_.size() && input_[pos_ + 1] == '=') {
                pos_ += 2;
                return Token{TokenKind::NotEquals, input_.substr(start, 2), {start, start + 2}};
            }
        }
        if (c == '<') {
            if (pos_ + 1 < input_.size() && input_[pos_ + 1] == '=') {
                pos_ += 2;
                return Token{TokenKind::Lte, input_.substr(start, 2), {start, start + 2}};
            }
            pos_++;
            return Token{TokenKind::Lt, input_.substr(start, 1), {start, start + 1}};
        }
        if (c == '>') {
            if (pos_ + 1 < input_.size() && input_[pos_ + 1] == '=') {
                pos_ += 2;
                return Token{TokenKind::Gte, input_.substr(start, 2), {start, start + 2}};
            }
            pos_++;
            return Token{TokenKind::Gt, input_.substr(start, 1), {start, start + 1}};
        }
        
        // String literals
        if (c == '"') {
            return parse_string();
        }
        
        // Numbers
        if (std::isdigit(c) || c == '-' || c == '+') {
            return parse_number();
        }
        
        // Identifiers
        if (std::isalpha(c) || c == '_') {
            return parse_identifier();
        }
        
        // Unknown character
        pos_++;
        return Token{TokenKind::End, input_.substr(start, 1), {start, start + 1}};
    }
    
    Token peek() {
        size_t saved_pos = pos_;
        Token tok = next();
        pos_ = saved_pos;
        return tok;
    }
    
private:
    void skip_whitespace() {
        while (pos_ < input_.size() && std::isspace(input_[pos_])) {
            pos_++;
        }
    }
    
    Token parse_identifier() {
        int start = pos_;
        while (pos_ < input_.size() && 
               (std::isalnum(input_[pos_]) || input_[pos_] == '_')) {
            pos_++;
        }
        return Token{TokenKind::Identifier, input_.substr(start, pos_ - start), 
                     {start, static_cast<int>(pos_)}};
    }
    
    Token parse_number() {
        int start = pos_;
        if (input_[pos_] == '-' || input_[pos_] == '+') {
            pos_++;
        }
        while (pos_ < input_.size() && std::isdigit(input_[pos_])) {
            pos_++;
        }
        if (pos_ < input_.size() && input_[pos_] == '.') {
            pos_++;
            while (pos_ < input_.size() && std::isdigit(input_[pos_])) {
                pos_++;
            }
        }
        // Scientific notation: 1e-3, 2.5E+10
        if (pos_ < input_.size() && (input_[pos_] == 'e' || input_[pos_] == 'E')) {
            pos_++;
            if (pos_ < input_.size() && (input_[pos_] == '+' || input_[pos_] == '-')) {
                pos_++;
            }
            while (pos_ < input_.size() && std::isdigit(input_[pos_])) {
                pos_++;
            }
        }
        return Token{TokenKind::Number, input_.substr(start, pos_ - start), 
                     {start, static_cast<int>(pos_)}};
    }
    
    Token parse_string() {
        int start = pos_;
        pos_++;  // Skip opening quote
        while (pos_ < input_.size() && input_[pos_] != '"') {
            if (input_[pos_] == '\\' && pos_ + 1 < input_.size()) {
                pos_ += 2;  // Skip escape sequence
            } else {
                pos_++;
            }
        }
        if (pos_ < input_.size()) {
            pos_++;  // Skip closing quote
        }
        return Token{TokenKind::String, input_.substr(start, pos_ - start), 
                     {start, static_cast<int>(pos_)}};
    }
    
    std::string_view input_;
    size_t pos_;
};

// ============================================================================
// Parser Implementation
// ============================================================================

static std::optional<SelectorTarget> parse_target(std::string_view name) {
    std::string lower = to_lower(name);
    if (lower == "bodies") return SelectorTarget::Bodies;
    if (lower == "faces") return SelectorTarget::Faces;
    if (lower == "edges") return SelectorTarget::Edges;
    if (lower == "vertices") return SelectorTarget::Vertices;
    return std::nullopt;
}

static std::optional<FilterOp> token_to_op(TokenKind kind) {
    switch (kind) {
        case TokenKind::Equals: return FilterOp::Eq;
        case TokenKind::NotEquals: return FilterOp::Neq;
        case TokenKind::Lt: return FilterOp::Lt;
        case TokenKind::Lte: return FilterOp::Lte;
        case TokenKind::Gt: return FilterOp::Gt;
        case TokenKind::Gte: return FilterOp::Gte;
        default: return std::nullopt;
    }
}

static Literal parse_literal(const Token& tok) {
    Literal lit;
    
    if (tok.kind == TokenKind::Number) {
        std::string str(tok.text);
        // Check for float indicators: decimal point or scientific notation
        if (str.find('.') != std::string::npos || 
            str.find('e') != std::string::npos || 
            str.find('E') != std::string::npos) {
            lit.kind = Literal::Kind::Float;
            lit.d = std::stod(str);
        } else {
            lit.kind = Literal::Kind::Int;
            lit.i = std::stoll(str);
        }
    } else if (tok.kind == TokenKind::String) {
        lit.kind = Literal::Kind::String;
        // Remove quotes
        if (tok.text.size() >= 2 && tok.text.front() == '"' && tok.text.back() == '"') {
            lit.s = std::string(tok.text.substr(1, tok.text.size() - 2));
        } else {
            lit.s = std::string(tok.text);
        }
    } else if (tok.kind == TokenKind::Identifier) {
        std::string lower = to_lower(tok.text);
        if (lower == "true") {
            lit.kind = Literal::Kind::Bool;
            lit.b = true;
        } else if (lower == "false") {
            lit.kind = Literal::Kind::Bool;
            lit.b = false;
        } else {
            // Treat as string
            lit.kind = Literal::Kind::String;
            lit.s = std::string(tok.text);
        }
    }
    
    lit.canon = literal_to_canon(lit);
    return lit;
}

static ParseResult parse_impl(std::string_view input) {
    Tokenizer tokenizer(input);
    
    // Parse target function: bodies(), faces(), etc.
    Token target_tok = tokenizer.next();
    if (target_tok.kind != TokenKind::Identifier) {
        return ParseResult{std::nullopt, SelectorError{
            SelectorErrorKind::InvalidFunction,
            "Expected selector function (bodies, faces, edges, vertices)",
            target_tok.span
        }};
    }
    
    auto target_opt = parse_target(target_tok.text);
    if (!target_opt) {
        return ParseResult{std::nullopt, SelectorError{
            SelectorErrorKind::InvalidFunction,
            "Unknown selector function: " + std::string(target_tok.text),
            target_tok.span
        }};
    }
    
    Selector sel;
    sel.target = *target_opt;
    sel.original = std::string(input);
    
    // Check for opening paren
    Token lparen = tokenizer.next();
    if (lparen.kind != TokenKind::LParen) {
        return ParseResult{std::nullopt, SelectorError{
            SelectorErrorKind::MissingClosingParen,
            "Expected '(' after function name",
            lparen.span
        }};
    }
    
    // Parse filters (if any)
    Token next = tokenizer.peek();
    while (next.kind == TokenKind::Identifier) {
        tokenizer.next();  // Consume identifier
        Token field_tok = next;
        
        // Expect operator
        Token op_tok = tokenizer.next();
        auto op_opt = token_to_op(op_tok.kind);
        if (!op_opt) {
            return ParseResult{std::nullopt, SelectorError{
                SelectorErrorKind::InvalidOperator,
                "Expected comparison operator (=, !=, <, <=, >, >=)",
                op_tok.span
            }};
        }
        
        // Parse value
        Token value_tok = tokenizer.next();
        if (value_tok.kind != TokenKind::Number && 
            value_tok.kind != TokenKind::String && 
            value_tok.kind != TokenKind::Identifier) {
            return ParseResult{std::nullopt, SelectorError{
                SelectorErrorKind::InvalidFilterValue,
                "Expected filter value (number, string, or identifier)",
                value_tok.span
            }};
        }
        
        Filter filter;
        filter.field = to_lower(field_tok.text);
        filter.op = *op_opt;
        filter.value = parse_literal(value_tok);
        sel.filters.push_back(filter);
        
        // Check for comma or closing paren
        next = tokenizer.peek();
        if (next.kind == TokenKind::Comma) {
            tokenizer.next();  // Consume comma
            next = tokenizer.peek();
            // Trailing comma error: comma must be followed by another filter
            if (next.kind != TokenKind::Identifier) {
                return ParseResult{std::nullopt, SelectorError{
                    SelectorErrorKind::InvalidFilterValue,
                    "Trailing comma without following filter",
                    next.span
                }};
            }
        }
    }
    
    // Expect closing paren
    Token rparen = tokenizer.next();
    if (rparen.kind != TokenKind::RParen) {
        return ParseResult{std::nullopt, SelectorError{
            SelectorErrorKind::MissingClosingParen,
            "Expected ')' to close function call",
            rparen.span
        }};
    }
    
    // Check for trailing input
    Token end = tokenizer.next();
    if (end.kind != TokenKind::End) {
        return ParseResult{std::nullopt, SelectorError{
            SelectorErrorKind::TrailingInput,
            "Unexpected input after selector",
            end.span
        }};
    }
    
    return ParseResult{sel, std::nullopt};
}

// ============================================================================
// Normalization
// ============================================================================

static void normalize(Selector& sel) {
    // Sort filters lexicographically: (field, op_string, value_canon)
    std::sort(sel.filters.begin(), sel.filters.end(), [](const Filter& a, const Filter& b) {
        if (a.field != b.field) return a.field < b.field;
        if (a.op != b.op) {
            // Compare by string representation for deterministic sorting
            return to_string(a.op) < to_string(b.op);
        }
        return a.value.canon < b.value.canon;
    });
    
    // Generate canonical form
    sel.canonical = SelectorParser::canonicalize(sel);
}

// ============================================================================
// Public API
// ============================================================================

ParseResult SelectorParser::parse(std::string_view input) {
    std::string trimmed_input = trim(input);
    auto result = parse_impl(trimmed_input);
    
    if (result.selector) {
        normalize(*result.selector);
    }
    
    return result;
}

std::string SelectorParser::canonicalize(const Selector& sel) {
    std::ostringstream oss;
    oss << to_string(sel.target) << "(";
    
    for (size_t i = 0; i < sel.filters.size(); ++i) {
        if (i > 0) oss << ", ";
        const Filter& f = sel.filters[i];
        oss << f.field << to_string(f.op) << f.value.canon;
    }
    
    oss << ")";
    return oss.str();
}

std::string SelectorParser::to_json(const Selector& sel) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"target\": \"" << to_string(sel.target) << "\",\n";
    oss << "  \"filters\": [";
    
    for (size_t i = 0; i < sel.filters.size(); ++i) {
        if (i > 0) oss << ", ";
        const Filter& f = sel.filters[i];
        oss << "\n    {";
        oss << "\"field\": \"" << f.field << "\", ";
        oss << "\"op\": \"" << to_string(f.op) << "\", ";
        oss << "\"value\": " << f.value.canon;
        oss << "}";
    }
    
    if (!sel.filters.empty()) oss << "\n  ";
    oss << "],\n";
    oss << "  \"original\": \"" << sel.original << "\",\n";
    oss << "  \"canonical\": \"" << sel.canonical << "\"\n";
    oss << "}";
    
    return oss.str();
}

std::string SelectorParser::error_to_json(const SelectorError& error) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"error\": \"" << to_string(error.kind) << "\",\n";
    oss << "  \"message\": \"" << error.message << "\",\n";
    oss << "  \"span\": {\"start\": " << error.span.start << ", \"end\": " << error.span.end << "}\n";
    oss << "}";
    return oss.str();
}

// ============================================================================
// Enum to String Conversions
// ============================================================================

std::string to_string(SelectorTarget target) {
    switch (target) {
        case SelectorTarget::Bodies: return "bodies";
        case SelectorTarget::Faces: return "faces";
        case SelectorTarget::Edges: return "edges";
        case SelectorTarget::Vertices: return "vertices";
    }
    return "unknown";
}

std::string to_string(FilterOp op) {
    switch (op) {
        case FilterOp::Eq: return "=";
        case FilterOp::Neq: return "!=";
        case FilterOp::Lt: return "<";
        case FilterOp::Lte: return "<=";
        case FilterOp::Gt: return ">";
        case FilterOp::Gte: return ">=";
    }
    return "?";
}

std::string to_string(SelectorErrorKind kind) {
    switch (kind) {
        case SelectorErrorKind::UnexpectedToken: return "UnexpectedToken";
        case SelectorErrorKind::UnexpectedEOF: return "UnexpectedEOF";
        case SelectorErrorKind::InvalidNumber: return "InvalidNumber";
        case SelectorErrorKind::InvalidIdentifier: return "InvalidIdentifier";
        case SelectorErrorKind::InvalidOperator: return "InvalidOperator";
        case SelectorErrorKind::InvalidFunction: return "InvalidFunction";
        case SelectorErrorKind::TrailingInput: return "TrailingInput";
        case SelectorErrorKind::MissingClosingParen: return "MissingClosingParen";
        case SelectorErrorKind::MissingEquals: return "MissingEquals";
        case SelectorErrorKind::InvalidFilterValue: return "InvalidFilterValue";
    }
    return "Unknown";
}

std::string to_string(Literal::Kind kind) {
    switch (kind) {
        case Literal::Kind::Int: return "int";
        case Literal::Kind::Float: return "float";
        case Literal::Kind::String: return "string";
        case Literal::Kind::Bool: return "bool";
    }
    return "unknown";
}

}  // namespace selector
}  // namespace praxis
