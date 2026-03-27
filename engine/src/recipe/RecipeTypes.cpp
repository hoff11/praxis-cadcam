#include "RecipeTypes.hpp"
#include <stdexcept>
#include <sstream>
#include <cctype>
#include <cmath>
#include <algorithm>
#include <functional>
#include <iomanip>
#include <openssl/sha.h>
#include <openssl/evp.h>

namespace praxis {
namespace recipe {

// Simple expression evaluator (v0: + - * / () only)
double Expression::evaluate(const std::map<std::string, double>& values) const {
    // Recursive descent parser for arithmetic expressions
    // Grammar:
    //   expr   = term (('+' | '-') term)*
    //   term   = factor (('*' | '/') factor)*
    //   factor = number | variable | '(' expr ')'
    
    std::string input = formula;
    // Remove all whitespace
    input.erase(std::remove_if(input.begin(), input.end(), ::isspace), input.end());
    
    size_t pos = 0;
    
    // Forward declarations for recursive functions
    std::function<double()> parse_expr;
    std::function<double()> parse_term;
    std::function<double()> parse_factor;
    
    parse_factor = [&]() -> double {
        if (pos >= input.size()) {
            throw std::runtime_error("Unexpected end of expression");
        }
        
        // Handle parentheses
        if (input[pos] == '(') {
            pos++; // consume '('
            double result = parse_expr();
            if (pos >= input.size() || input[pos] != ')') {
                throw std::runtime_error("Missing closing parenthesis");
            }
            pos++; // consume ')'
            return result;
        }
        
        // Handle negative numbers
        if (input[pos] == '-') {
            pos++; // consume '-'
            return -parse_factor();
        }
        
        // Handle numbers
        if (std::isdigit(input[pos]) || input[pos] == '.') {
            size_t start = pos;
            while (pos < input.size() && 
                   (std::isdigit(input[pos]) || input[pos] == '.')) {
                pos++;
            }
            return std::stod(input.substr(start, pos - start));
        }
        
        // Handle variables (support qualified names like "spindle.width")
        if (std::isalpha(input[pos]) || input[pos] == '_') {
            size_t start = pos;
            while (pos < input.size() && 
                   (std::isalnum(input[pos]) || input[pos] == '_' || input[pos] == '.')) {
                pos++;
            }
            std::string var_name = input.substr(start, pos - start);
            
            if (values.count(var_name) == 0) {
                throw std::runtime_error("Unknown variable: " + var_name);
            }
            return values.at(var_name);
        }
        
        throw std::runtime_error("Invalid expression at position " + std::to_string(pos));
    };
    
    parse_term = [&]() -> double {
        double result = parse_factor();
        
        while (pos < input.size() && (input[pos] == '*' || input[pos] == '/')) {
            char op = input[pos];
            pos++; // consume operator
            double right = parse_factor();
            
            if (op == '*') {
                result *= right;
            } else {
                if (right == 0.0) {
                    throw std::runtime_error("Division by zero");
                }
                result /= right;
            }
        }
        
        return result;
    };
    
    parse_expr = [&]() -> double {
        double result = parse_term();
        
        while (pos < input.size() && (input[pos] == '+' || input[pos] == '-')) {
            char op = input[pos];
            pos++; // consume operator
            double right = parse_term();
            
            if (op == '+') {
                result += right;
            } else {
                result -= right;
            }
        }
        
        return result;
    };
    
    try {
        double result = parse_expr();
        
        // Check for unconsumed input
        if (pos < input.size()) {
            throw std::runtime_error("Unexpected character at position " + std::to_string(pos) + ": " + input[pos]);
        }
        
        return result;
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Expression evaluation failed for '" + formula + "': " + e.what());
    }
}

// Helper: evaluate a Scalar (literal or expression) to double
double evaluate_scalar(const Scalar& scalar, const std::map<std::string, double>& values) {
    if (std::holds_alternative<double>(scalar)) {
        return std::get<double>(scalar);
    } else {
        const Expression& expr = std::get<Expression>(scalar);
        return expr.evaluate(values);
    }
}

std::string node_op_to_string(NodeOp op) {
    switch (op) {
        case NodeOp::MakeBox:
            return "make_box";
        case NodeOp::Transform:
            return "transform";
        case NodeOp::Compound:
            return "compound";
        default:
            return "unknown";
    }
}

NodeOp string_to_node_op(const std::string& str) {
    if (str == "make_box") {
        return NodeOp::MakeBox;
    } else if (str == "transform") {
        return NodeOp::Transform;
    } else if (str == "compound") {
        return NodeOp::Compound;
    }
    throw std::invalid_argument("Unknown node operation: " + str);
}

// Helper: format double with fixed precision, trim trailing zeros
static std::string format_canonical_number(double value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.10f", value);
    std::string result(buffer);
    
    // Trim trailing zeros
    size_t dot_pos = result.find('.');
    if (dot_pos != std::string::npos) {
        size_t last_nonzero = result.find_last_not_of('0');
        if (last_nonzero >= dot_pos) {
            result = result.substr(0, last_nonzero + 1);
            // Remove trailing decimal point if no fractional part
            if (result.back() == '.') {
                result.pop_back();
            }
        }
    }
    
    return result;
}

// Serialize ResolvedPlan to canonical JSON (Sprint 5 Phase 2)
std::string ResolvedPlan::to_canonical_json() const {
    std::ostringstream json;
    json << "{\n";
    
    // Engine version
    json << "  \"engine_version\": \"" << engine_version << "\",\n";
    json << "  \"kernel_version\": \"" << kernel_version << "\",\n";
    
    // Kinematics (use content hash, not path - Sprint 5 Phase 3)
    json << "  \"kinematics_sha256\": \"" << kinematics_sha256 << "\",\n";
    json << "  \"output_node\": \"" << output_node << "\",\n";
    
    // Final params (sorted keys for determinism)
    json << "  \"final_params\": {\n";
    std::vector<std::string> param_keys;
    for (const auto& kv : final_params) {
        param_keys.push_back(kv.first);
    }
    std::sort(param_keys.begin(), param_keys.end());
    
    for (size_t i = 0; i < param_keys.size(); ++i) {
        const auto& key = param_keys[i];
        json << "    \"" << key << "\": " << format_canonical_number(final_params.at(key));
        if (i < param_keys.size() - 1) {
            json << ",";
        }
        json << "\n";
    }
    json << "  },\n";
    
    // Includes
    json << "  \"includes\": [\n";
    for (size_t i = 0; i < includes.size(); ++i) {
        json << "    {\n";
        json << "      \"recipe_path\": \"" << includes[i].recipe_path << "\",\n";
        json << "      \"namespace_alias\": \"" << includes[i].namespace_alias << "\",\n";
        json << "      \"param_overrides\": {\n";
        
        std::vector<std::string> override_keys;
        for (const auto& kv : includes[i].param_overrides) {
            override_keys.push_back(kv.first);
        }
        std::sort(override_keys.begin(), override_keys.end());
        
        for (size_t j = 0; j < override_keys.size(); ++j) {
            const auto& key = override_keys[j];
            json << "        \"" << key << "\": " << format_canonical_number(includes[i].param_overrides.at(key));
            if (j < override_keys.size() - 1) {
                json << ",";
            }
            json << "\n";
        }
        
        json << "      }\n";
        json << "    }";
        if (i < includes.size() - 1) {
            json << ",";
        }
        json << "\n";
    }
    json << "  ],\n";
    
    // Operations
    json << "  \"operations\": [\n";
    for (size_t i = 0; i < operations.size(); ++i) {
        const auto& op = operations[i];
        json << "    {\n";
        json << "      \"node_id\": \"" << op.node_id << "\",\n";
        json << "      \"op\": \"" << node_op_to_string(op.op) << "\",\n";
        json << "      \"source\": {\n";
        json << "        \"recipe\": \"" << op.source.recipe_path_canonical << "\",\n";
        json << "        \"namespace\": \"" << op.source.namespace_alias << "\",\n";
        json << "        \"index\": " << op.source.operation_index << "\n";
        json << "      },\n";
        json << "      \"resolved_args\": {\n";
        
        std::vector<std::string> arg_keys;
        for (const auto& kv : op.resolved_args) {
            arg_keys.push_back(kv.first);
        }
        std::sort(arg_keys.begin(), arg_keys.end());
        
        for (size_t j = 0; j < arg_keys.size(); ++j) {
            const auto& key = arg_keys[j];
            json << "        \"" << key << "\": " << format_canonical_number(op.resolved_args.at(key));
            if (j < arg_keys.size() - 1) {
                json << ",";
            }
            json << "\n";
        }
        
        json << "      }\n";
        json << "    }";
        if (i < operations.size() - 1) {
            json << ",";
        }
        json << "\n";
    }
    json << "  ]\n";
    
    json << "}";
    return json.str();
}

// Compute SHA256 hash of canonical JSON (Sprint 5 Phase 2)
std::string ResolvedPlan::compute_hash() const {
    std::string canonical = to_canonical_json();
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        throw std::runtime_error("EVP_MD_CTX_new failed");
    }
    std::string out;
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestInit_ex failed");
    }
    if (EVP_DigestUpdate(ctx, canonical.data(), canonical.size()) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestUpdate failed");
    }
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestFinal_ex failed");
    }
    EVP_MD_CTX_free(ctx);

    static const char* hex = "0123456789abcdef";
    out.resize(digest_len * 2);
    for (unsigned int i = 0; i < digest_len; ++i) {
        out[2*i] = hex[(digest[i] >> 4) & 0xF];
        out[2*i+1] = hex[digest[i] & 0xF];
    }
    return out;
}

// Validate plan is eligible for hashing (Sprint 5 Phase 3)
HashValidationResult ResolvedPlan::validate_for_hash(const ResolvedPlan& plan) {
    HashValidationResult result;
    result.valid = true;
    
    // Check engine version
    if (plan.engine_version.empty()) {
        result.valid = false;
        result.reason = "missing engine_version";
        return result;
    }
    
    // Check kernel version (can be "unknown" but must exist)
    if (plan.kernel_version.empty()) {
        result.valid = false;
        result.reason = "missing kernel_version";
        return result;
    }
    
    // Check for NaN or Infinity in final_params
    for (const auto& [key, value] : plan.final_params) {
        if (std::isnan(value)) {
            result.valid = false;
            result.reason = "NaN in final_params[" + key + "]";
            return result;
        }
        if (std::isinf(value)) {
            result.valid = false;
            result.reason = "Infinity in final_params[" + key + "]";
            return result;
        }
    }
    
    // Check for NaN or Infinity in resolved operation args
    for (size_t i = 0; i < plan.operations.size(); ++i) {
        const auto& op = plan.operations[i];
        for (const auto& [key, value] : op.resolved_args) {
            if (std::isnan(value)) {
                result.valid = false;
                result.reason = "NaN in operations[" + std::to_string(i) + "].resolved_args[" + key + "]";
                return result;
            }
            if (std::isinf(value)) {
                result.valid = false;
                result.reason = "Infinity in operations[" + std::to_string(i) + "].resolved_args[" + key + "]";
                return result;
            }
        }
    }
    
    return result;
}

} // namespace recipe
} // namespace praxis
