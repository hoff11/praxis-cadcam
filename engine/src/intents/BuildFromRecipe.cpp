#include "BuildFromRecipe.hpp"
#include "../recipe/RecipeValidator.hpp"
#include "../recipe/RecipeExecutor.hpp"
#include "../kinematics/KinematicLoader.hpp"
#include "../kinematics/KinematicValidator.hpp"
#include "OCCTInspector.hpp"
#include "InteractionEmit.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <unordered_set>
#include <algorithm>
#include <regex>

namespace praxis {

// ============================================================================
// Helper Functions for Recipe Composition (Sprint 3 EPIC 2)
// ============================================================================

// Find matching closing brace for proper JSON parsing
static size_t find_matching_brace(const std::string& content, size_t open_pos) {
    if (open_pos >= content.size() || content[open_pos] != '{') {
        return std::string::npos;
    }
    
    int depth = 0;
    bool in_string = false;
    
    for (size_t i = open_pos; i < content.size(); ++i) {
        char c = content[i];
        
        // Handle string escapes
        if (c == '\\' && i + 1 < content.size()) {
            ++i;  // Skip next char
            continue;
        }
        
        // Toggle string state
        if (c == '"') {
            in_string = !in_string;
            continue;
        }
        
        // Only count braces outside strings
        if (!in_string) {
            if (c == '{') depth++;
            else if (c == '}') {
                depth--;
                if (depth == 0) return i;
            }
        }
    }
    
    return std::string::npos;
}

// Validate that path is relative (no absolute, no drive letters, no UNC)
static bool validate_relative_path(const std::string& path, std::string& error) {
    // Check for drive letters (Windows)
    if (path.find(':') != std::string::npos) {
        error = "Include path cannot contain drive letters: " + path;
        return false;
    }
    
    // Check for UNC paths (\\server\share)
    if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\') {
        error = "Include path cannot be UNC path: " + path;
        return false;
    }
    
    // Check using filesystem (catches /absolute/path)
    std::filesystem::path p(path);
    if (p.is_absolute()) {
        error = "Include path must be relative: " + path;
        return false;
    }
    
    return true;
}

// Validate namespace identifier (Sprint 4 Phase 2)
static bool validate_namespace(const std::string& ns, std::string& error) {
    // Reserved namespaces
    static const std::unordered_set<std::string> reserved = {
        "params", "op", "include", "root", "derived", "node", "nodes"
    };
    
    if (reserved.count(ns) > 0) {
        error = "Namespace '" + ns + "' is reserved";
        return false;
    }
    
    // Must match [A-Za-z_][A-Za-z0-9_]*
    if (ns.empty()) {
        error = "Namespace cannot be empty";
        return false;
    }
    
    if (!std::isalpha(ns[0]) && ns[0] != '_') {
        error = "Namespace must start with letter or underscore: " + ns;
        return false;
    }
    
    for (char c : ns) {
        if (!std::isalnum(c) && c != '_') {
            error = "Namespace can only contain letters, digits, underscore: " + ns;
            return false;
        }
    }
    
    return true;
}

// Rewrite param references in expression to use qualified names (Sprint 4 Phase 2)
static std::string rewrite_identifiers(const std::string& expr, const std::string& ns_prefix) {
    // Simple regex replacement: find standalone identifiers and prefix them
    // Matches [A-Za-z_][A-Za-z0-9_]* not already qualified (no preceding dot)
    std::regex id_regex(R"((?:^|[^A-Za-z0-9_.])([A-Za-z_][A-Za-z0-9_]*))");
    std::string result;
    auto words_begin = std::sregex_iterator(expr.begin(), expr.end(), id_regex);
    auto words_end = std::sregex_iterator();
    
    size_t last_pos = 0;
    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        std::string id = match[1].str();
        
        // Copy text before match
        result += expr.substr(last_pos, match.position(1) - last_pos);
        
        // Qualify the identifier
        result += ns_prefix + "." + id;
        
        last_pos = match.position(1) + match.length(1);
    }
    result += expr.substr(last_pos);
    return result;
}

// Context for tracking include stack and detecting cycles
struct IncludeContext {
    std::vector<std::string> include_stack;  // Canonical absolute paths
    std::vector<std::string> display_path_stack;  // Relative paths for display
    
    bool is_circular(const std::string& canonical_path) const {
        return std::find(include_stack.begin(), include_stack.end(), canonical_path) 
               != include_stack.end();
    }
    
    void push(const std::string& canonical_path, const std::string& display_path) {
        include_stack.push_back(canonical_path);
        display_path_stack.push_back(display_path);
    }
    
    void pop() {
        if (!include_stack.empty()) {
            include_stack.pop_back();
        }
        if (!display_path_stack.empty()) {
            display_path_stack.pop_back();
        }
    }
    
    std::string get_display_chain() const {
        std::string chain;
        for (size_t i = 0; i < display_path_stack.size(); ++i) {
            if (i > 0) chain += "/";
            chain += display_path_stack[i];
        }
        return chain;
    }
    
    std::string get_chain() const {
        std::string chain;
        for (size_t i = 0; i < include_stack.size(); ++i) {
            if (i > 0) chain += " -> ";
            chain += std::filesystem::path(include_stack[i]).filename().string();
        }
        return chain;
    }
};

// ============================================================================
// End Helper Functions
// ============================================================================

// Forward declarations
recipe::RecipeLoadResult load_recipe_json(const std::string& recipe_path);
recipe::RecipeLoadResult load_recipe_with_includes(const std::string& recipe_path, IncludeContext& ctx);

IntentResult buildFromRecipe(const IntentRequest& request) {
    auto start = std::chrono::high_resolution_clock::now();
    
    IntentResult result;
    result.success = true;
    result.confidence = 1.0;
    
    // Get recipe path from params
    std::string recipe_path;
    if (request.params.count("recipe_path") > 0) {
        recipe_path = request.params.at("recipe_path");
    } else {
        result.success = false;
        result.errors.push_back("Missing required parameter: recipe_path");
        result.confidence = 0.0;
        return result;
    }
    
    std::string output_dir = request.output_dir;
    
    // Create output directory
    std::filesystem::create_directories(output_dir);
    
    // Load recipe with includes (Sprint 3 EPIC 2)
    IncludeContext include_ctx;
    auto recipe_result = load_recipe_with_includes(recipe_path, include_ctx);
    if (!recipe_result.success) {
        result.success = false;
        result.errors.push_back("Failed to load recipe: " + recipe_result.error_message);
        result.confidence = 0.0;
        return result;
    }
    
    const auto& recipe = recipe_result.recipe;
    
    // Resolve PKM path (relative to recipe directory)
    std::filesystem::path recipe_dir = std::filesystem::path(recipe_path).parent_path();
    std::filesystem::path pkm_path = recipe_dir / recipe.kinematics_path;
    
    // Load PKM
    auto pkm = load_pkm_json(pkm_path.string());
    if (pkm.bodies.empty()) {
        result.success = false;
        result.errors.push_back("Failed to load PKM: " + pkm_path.string());
        result.confidence = 0.0;
        return result;
    }
    
    // Validate PKM
    auto pkm_validation = kinematics::KinematicValidator::validate(pkm);
    if (!pkm_validation.valid) {
        result.success = false;
        result.errors.push_back("PKM validation failed: " + 
            (pkm_validation.errors.empty() ? "unknown" : pkm_validation.errors[0]));
        result.confidence = 0.0;
        return result;
    }
    
    // Validate recipe against PKM (check body references)
    auto recipe_pkm_validation = recipe::RecipeValidator::validate_with_pkm(recipe, pkm);
    if (!recipe_pkm_validation.valid) {
        result.success = false;
        result.errors.push_back("Recipe validation failed: " + 
            (recipe_pkm_validation.errors.empty() ? "unknown" : recipe_pkm_validation.errors[0]));
        result.confidence = 0.0;
        return result;
    }
    
    // Collect CLI parameters (from request.params, skip recipe_path)
    std::map<std::string, std::string> cli_params;
    for (const auto& kv : request.params) {
        if (kv.first != "recipe_path") {
            cli_params[kv.first] = kv.second;
        }
    }
    
    // Execute recipe
    auto exec_result = recipe::RecipeExecutor::execute(recipe, pkm, cli_params, output_dir, 
                                                        recipe_result.all_includes_used,
                                                        request.no_cache,
                                                        request.clear_cache);
    
    if (!exec_result.success) {
        result.success = false;
        result.errors.push_back(exec_result.error_message);
        result.confidence = 0.0;
        return result;
    }
    
    // Populate result
    result.artifacts.push_back(exec_result.step_file_path);
    if (!exec_result.pkm_file_path.empty()) {
        result.artifacts.push_back(exec_result.pkm_file_path);
    }
    if (!exec_result.bindings_file_path.empty()) {
        result.artifacts.push_back(exec_result.bindings_file_path);
    }
    
    result.kernel_ops = exec_result.kernel_ops;
    result.metrics = exec_result.metrics;
    result.report_version = exec_result.report_version;
    result.resolved_params = exec_result.resolved_params;
    result.derived_values = exec_result.derived_values;
    result.includes = recipe_result.all_includes_used;  // Sprint 4: all includes (flattened)
    
    // Plan hash (Sprint 5 Phase 3)
    result.plan_hash = exec_result.resolved_plan.plan_hash;
    result.plan_hash_valid = exec_result.resolved_plan.hash_valid;
    result.plan_hash_status = exec_result.resolved_plan.hash_status_reason;
    
    // Cache info (Sprint 5 Phase 4-6)
    result.cache.hit = exec_result.cache.hit;
    result.cache.plan_hash = exec_result.cache.plan_hash;
    result.cache.reason = exec_result.cache.reason;
    result.cache.reused_artifacts = exec_result.cache.reused_artifacts;
    result.cache.cache_entry_age_seconds = exec_result.cache.cache_entry_age_seconds;
    
    // Op cache info (Sprint 6 Phase 3)
    result.op_cache.executed_count = exec_result.op_cache.executed_count;
    result.op_cache.reused_count = exec_result.op_cache.reused_count;
    result.op_cache.cache_hit_rate = exec_result.op_cache.cache_hit_rate;
    result.op_cache.cache_dir = exec_result.op_cache.cache_dir;
    for (const auto& node : exec_result.op_cache.node_status) {
        IntentResult::NodeExecutionStatus status;
        status.node_id = node.node_id;
        status.node_index = node.node_index;
        status.op_type = node.op_type;
        status.op_hash = node.op_hash;
        status.status = node.status;
        status.cache_hit = node.cache_hit;
        status.artifact_path = node.artifact_path;
        result.op_cache.node_status.push_back(status);
    }
    
    // Add execution warnings
    result.warnings = exec_result.warnings;
    
    // Pass reasoning notes to result (for JSON serialization)
    result.reasoning = exec_result.reasoning;
    
    // Add reasoning notes as warnings (formatted for display)
    // NOTE: This is temporary for Sprint 3 - long-term, we should have separate
    // CLI display for reasoning notes vs system/execution warnings
    for (const auto& note : exec_result.reasoning) {
        std::string prefix = (note.level == reasoning::ReasoningLevel::Warning) ? "⚠" : "ℹ";
        std::string formatted = prefix + " [" + note.code + "] " + note.message + 
                               " (source: " + note.source + 
                               ", value: " + std::to_string(note.actual_value) + 
                               ", threshold: " + std::to_string(note.threshold) + ")";
        result.warnings.push_back(formatted);
    }
    
    // EPIC 1.6: Populate selectables inventory from final artifact (v2.0 contract)
    if (!exec_result.step_file_path.empty()) {
        populate_selectables_from_artifact(result, exec_result.step_file_path);
    }
    
    // Calculate duration
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    result.duration_ms = duration.count();
    
    return result;
}

// Load recipe with includes (recursive, with cycle detection)
recipe::RecipeLoadResult load_recipe_with_includes(
    const std::string& recipe_path,
    IncludeContext& ctx
) {
    // Resolve to canonical absolute path for cycle detection
    std::filesystem::path abs_path;
    try {
        abs_path = std::filesystem::weakly_canonical(std::filesystem::absolute(recipe_path));
    } catch (const std::exception& e) {
        recipe::RecipeLoadResult error_result;
        error_result.success = false;
        error_result.error_message = "Cannot resolve recipe path: " + recipe_path + " (" + e.what() + ")";
        return error_result;
    }
    
    std::string canonical = abs_path.string();
    
    // Check for circular include
    if (ctx.is_circular(canonical)) {
        recipe::RecipeLoadResult error_result;
        error_result.success = false;
        error_result.error_message = "Circular include detected: " + ctx.get_chain() + " -> " + 
                                     std::filesystem::path(canonical).filename().string();
        return error_result;
    }
    
    // Push onto stack
    ctx.push(canonical, recipe_path);
    
    // Load base recipe
    auto result = load_recipe_json(recipe_path);
    if (!result.success) {
        ctx.pop();
        return result;
    }
    
    // If no includes, return as-is (but mark nodes with source)
    if (result.recipe.includes.empty()) {
        int op_index = 0;
        for (auto& node : result.recipe.nodes) {
            node.source.recipe_path_canonical = canonical;
            node.source.recipe_path_display = ctx.get_display_chain();
            node.source.namespace_alias = "";
            node.source.operation_index = op_index++;
        }
        ctx.pop();
        return result;
    }
    
    // Process includes (depth-first, included nodes come first)
    recipe::Recipe merged_recipe = result.recipe;
    std::unordered_set<std::string> node_ids;  // Track for collision detection
    std::unordered_set<std::string> namespaces_used;  // Track namespace uniqueness
    std::vector<recipe::RecipeInclude> all_includes;  // Accumulate flattened includes
    
    // Validate namespace uniqueness and validity upfront
    for (const auto& include : result.recipe.includes) {
        if (!include.namespace_alias.empty()) {
            std::string ns_error;
            if (!validate_namespace(include.namespace_alias, ns_error)) {
                ctx.pop();
                recipe::RecipeLoadResult error_result;
                error_result.success = false;
                error_result.error_message = ns_error + " (from " + include.recipe_path + ")";
                return error_result;
            }
            
            if (namespaces_used.count(include.namespace_alias) > 0) {
                ctx.pop();
                recipe::RecipeLoadResult error_result;
                error_result.success = false;
                error_result.error_message = "Duplicate namespace '" + include.namespace_alias + 
                                            "' in recipe " + recipe_path;
                return error_result;
            }
            
            namespaces_used.insert(include.namespace_alias);
        }
    }
    
    // Clear nodes, we'll rebuild with includes first
    std::vector<recipe::RecipeNode> local_nodes = merged_recipe.nodes;
    merged_recipe.nodes.clear();
    
    for (const auto& include : result.recipe.includes) {
        // Validate path is relative
        std::string path_error;
        if (!validate_relative_path(include.recipe_path, path_error)) {
            ctx.pop();
            recipe::RecipeLoadResult error_result;
            error_result.success = false;
            error_result.error_message = path_error;
            return error_result;
        }
        
        // Resolve path relative to current recipe
        std::filesystem::path include_path = std::filesystem::path(recipe_path).parent_path() 
                                            / include.recipe_path;
        
        // Recursive load
        auto inc_result = load_recipe_with_includes(include_path.string(), ctx);
        if (!inc_result.success) {
            ctx.pop();
            return inc_result;
        }
        
        // Add this include to flattened list (with full context path for nested)
        recipe::RecipeInclude flattened_include = include;
        flattened_include.recipe_path = include_path.string();  // Full resolved path
        all_includes.push_back(flattened_include);
        
        // Add any nested includes from the included recipe
        all_includes.insert(all_includes.end(), 
                           inc_result.all_includes_used.begin(), 
                           inc_result.all_includes_used.end());
        
        // Validate included recipe is a "fragment" (no top-level controlling fields)
        if (!inc_result.recipe.kinematics_path.empty()) {
            ctx.pop();
            recipe::RecipeLoadResult error_result;
            error_result.success = false;
            error_result.error_message = "Included recipe cannot specify 'kinematics': " + include.recipe_path;
            return error_result;
        }
        
        if (!inc_result.recipe.output.empty()) {
            ctx.pop();
            recipe::RecipeLoadResult error_result;
            error_result.success = false;
            error_result.error_message = "Included recipe cannot specify 'output': " + include.recipe_path;
            return error_result;
        }
        
        // Validate units match (if included recipe specifies units)
        if (!inc_result.recipe.units.empty() && !merged_recipe.units.empty()) {
            if (inc_result.recipe.units != merged_recipe.units) {
                ctx.pop();
                recipe::RecipeLoadResult error_result;
                error_result.success = false;
                error_result.error_message = "Included recipe units mismatch: expected '" + 
                                            merged_recipe.units + "', got '" + 
                                            inc_result.recipe.units + "' (from " + 
                                            include.recipe_path + ")";
                return error_result;
            }
        }
        
        // Validate param_overrides against included recipe
        for (const auto& [key, value] : include.param_overrides) {
            if (inc_result.recipe.params.count(key) == 0) {
                ctx.pop();
                recipe::RecipeLoadResult error_result;
                error_result.success = false;
                error_result.error_message = "Unknown parameter in include override: " + key + 
                                            " (from " + include.recipe_path + ")";
                return error_result;
            }
            
            // Apply override to included recipe's default
            inc_result.recipe.params[key].default_value = value;
        }
        
        // Merge nodes with collision detection
        int op_index = 0;
        for (auto node : inc_result.recipe.nodes) {
            if (node_ids.count(node.id) > 0) {
                ctx.pop();
                recipe::RecipeLoadResult error_result;
                error_result.success = false;
                error_result.error_message = "Duplicate node id '" + node.id + "' from include: " + include.recipe_path;
                return error_result;
            }
            node_ids.insert(node.id);
            
            // Rewrite param references if namespace present (Sprint 4 Phase 2)
            if (!include.namespace_alias.empty()) {
                // Helper lambda to rewrite Scalar expressions
                auto rewrite_scalar = [&](recipe::Scalar& s) {
                    if (std::holds_alternative<recipe::Expression>(s)) {
                        auto& expr = std::get<recipe::Expression>(s);
                        expr.formula = rewrite_identifiers(expr.formula, include.namespace_alias);
                    }
                };
                
                // Visit the variant and rewrite expressions in each arg type
                if (std::holds_alternative<recipe::MakeBoxArgs>(node.args)) {
                    auto& args = std::get<recipe::MakeBoxArgs>(node.args);
                    rewrite_scalar(args.dx);
                    rewrite_scalar(args.dy);
                    rewrite_scalar(args.dz);
                } else if (std::holds_alternative<recipe::TransformArgs>(node.args)) {
                    auto& args = std::get<recipe::TransformArgs>(node.args);
                    // input is a node ref, not an expression - don't rewrite
                    rewrite_scalar(args.tx);
                    rewrite_scalar(args.ty);
                    rewrite_scalar(args.tz);
                    rewrite_scalar(args.rx);
                    rewrite_scalar(args.ry);
                    rewrite_scalar(args.rz);
                }
                // CompoundArgs only has node_refs, no expressions to rewrite
            }
            
            // Populate source provenance
            node.source.recipe_path_canonical = std::filesystem::weakly_canonical(
                std::filesystem::absolute(include_path)).string();
            node.source.recipe_path_display = ctx.get_display_chain() + "/" + include.recipe_path;
            node.source.namespace_alias = include.namespace_alias;  // Sprint 4 Phase 2: namespace tracking
            node.source.operation_index = op_index++;
            
            merged_recipe.nodes.push_back(node);
        }
        
        // Merge params based on namespace mode
        if (include.namespace_alias.empty()) {
            // Flat merge: base recipe wins if defined
            for (const auto& [key, param] : inc_result.recipe.params) {
                if (merged_recipe.params.count(key) == 0) {
                    merged_recipe.params[key] = param;
                }
            }
        } else {
            // Namespaced: qualify all param names with namespace prefix
            for (const auto& [key, param] : inc_result.recipe.params) {
                std::string qualified_key = include.namespace_alias + "." + key;
                merged_recipe.params[qualified_key] = param;
            }
        }
        
        // Merge derived (base recipe wins)
        for (const auto& [key, expr] : inc_result.recipe.derived) {
            if (merged_recipe.derived.count(key) == 0) {
                merged_recipe.derived[key] = expr;
            }
        }
    }
    
    // Add local nodes after included nodes (check collisions)
    int local_op_index = 0;
    for (auto node : local_nodes) {
        if (node_ids.count(node.id) > 0) {
            ctx.pop();
            recipe::RecipeLoadResult error_result;
            error_result.success = false;
            error_result.error_message = "Duplicate node id '" + node.id + "' conflicts with included recipe";
            return error_result;
        }
        node_ids.insert(node.id);
        
        // Populate source provenance for root recipe nodes
        node.source.recipe_path_canonical = canonical;
        node.source.recipe_path_display = ctx.get_display_chain();
        node.source.namespace_alias = "";
        node.source.operation_index = local_op_index++;
        
        merged_recipe.nodes.push_back(node);
    }
    
    ctx.pop();
    result.recipe = merged_recipe;
    result.all_includes_used = all_includes;  // Return flattened includes list
    return result;
}

recipe::RecipeLoadResult load_recipe_json(const std::string& recipe_path) {
    recipe::RecipeLoadResult result;
    
    // Read JSON file
    std::ifstream file(recipe_path);
    if (!file.good()) {
        result.success = false;
        result.error_message = "Cannot open recipe file: " + recipe_path;
        return result;
    }
    
    // Read file content
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // Helper: extract string value for a key
    auto extract_string = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\"";
        size_t pos = content.find(search);
        if (pos == std::string::npos) return "";
        
        size_t colon = content.find(":", pos);
        if (colon == std::string::npos) return "";
        
        size_t quote1 = content.find("\"", colon);
        if (quote1 == std::string::npos) return "";
        
        size_t quote2 = content.find("\"", quote1 + 1);
        if (quote2 == std::string::npos) return "";
        
        return content.substr(quote1 + 1, quote2 - quote1 - 1);
    };
    
    // Helper: extract number value for a key
    auto extract_number = [&](const std::string& key) -> double {
        std::string search = "\"" + key + "\"";
        size_t pos = content.find(search);
        if (pos == std::string::npos) return 0.0;
        
        size_t colon = content.find(":", pos);
        if (colon == std::string::npos) return 0.0;
        
        size_t num_start = colon + 1;
        while (num_start < content.size() && std::isspace(content[num_start])) num_start++;
        if (num_start >= content.size()) return 0.0;
        
        size_t num_end = num_start;
        while (num_end < content.size() && 
               (std::isdigit(content[num_end]) || content[num_end] == '.' || 
                content[num_end] == '-' || content[num_end] == 'e' || content[num_end] == 'E')) {
            num_end++;
        }
        
        if (num_end > num_start) {
            try {
                return std::stod(content.substr(num_start, num_end - num_start));
            } catch (...) {
                return 0.0;
            }
        }
        return 0.0;
    };
    
    try {
        // Extract top-level fields
        result.recipe.recipe_version = extract_string("recipe_version");
        result.recipe.id = extract_string("id");
        result.recipe.units = extract_string("units");
        result.recipe.kinematics_path = extract_string("kinematics");
        result.recipe.output = extract_string("output");
        
        if (result.recipe.recipe_version.empty()) {
            result.success = false;
            result.error_message = "Missing recipe_version field";
            return result;
        }
        
        // Parse includes array
        size_t includes_pos = content.find("\"includes\"");
        if (includes_pos != std::string::npos) {
            size_t bracket = content.find("[", includes_pos);
            if (bracket != std::string::npos) {
                size_t bracket_end = bracket;
                int depth = 0;
                bool in_string = false;
                
                // Find matching closing bracket
                for (size_t i = bracket; i < content.size(); i++) {
                    if (content[i] == '"' && (i == 0 || content[i-1] != '\\')) {
                        in_string = !in_string;
                    }
                    if (!in_string) {
                        if (content[i] == '[') depth++;
                        else if (content[i] == ']') {
                            depth--;
                            if (depth == 0) {
                                bracket_end = i;
                                break;
                            }
                        }
                    }
                }
                
                // Parse each include object
                size_t current = bracket + 1;
                while (current < bracket_end) {
                    // Find next include object
                    size_t obj_start = content.find("{", current);
                    if (obj_start == std::string::npos || obj_start >= bracket_end) break;
                    
                    // Find matching closing brace
                    size_t obj_end = find_matching_brace(content, obj_start);
                    if (obj_end == std::string::npos) break;
                    
                    std::string inc_obj = content.substr(obj_start, obj_end - obj_start + 1);
                    
                    // Extract recipe_path
                    auto extract_from_inc = [&inc_obj](const std::string& key) -> std::string {
                        std::string search = "\"" + key + "\"";
                        size_t pos = inc_obj.find(search);
                        if (pos == std::string::npos) return "";
                        size_t colon = inc_obj.find(":", pos);
                        if (colon == std::string::npos) return "";
                        size_t quote1 = inc_obj.find("\"", colon);
                        if (quote1 == std::string::npos) return "";
                        size_t quote2 = inc_obj.find("\"", quote1 + 1);
                        if (quote2 == std::string::npos) return "";
                        return inc_obj.substr(quote1 + 1, quote2 - quote1 - 1);
                    };
                    
                    recipe::RecipeInclude inc;
                    inc.recipe_path = extract_from_inc("recipe_path");
                    inc.namespace_alias = extract_from_inc("namespace_alias");  // Sprint 4 Phase 2
                    
                    // Parse param_overrides object
                    size_t overrides_pos = inc_obj.find("\"param_overrides\"");
                    if (overrides_pos != std::string::npos) {
                        size_t brace = inc_obj.find("{", overrides_pos);
                        if (brace != std::string::npos) {
                            size_t override_end = find_matching_brace(inc_obj, brace);
                            if (override_end != std::string::npos) {
                                size_t search_pos = brace + 1;
                                while (search_pos < override_end) {
                                    // Skip whitespace
                                    while (search_pos < override_end && std::isspace(inc_obj[search_pos])) search_pos++;
                                    if (inc_obj[search_pos] == '}') break;
                                    
                                    // Find param name
                                    if (inc_obj[search_pos] == '"') {
                                        size_t name_start = search_pos + 1;
                                        size_t name_end = inc_obj.find("\"", name_start);
                                        if (name_end == std::string::npos) break;
                                        
                                        std::string param_name = inc_obj.substr(name_start, name_end - name_start);
                                        
                                        // Find value
                                        size_t colon = inc_obj.find(":", name_end);
                                        if (colon == std::string::npos) break;
                                        
                                        size_t num_start = colon + 1;
                                        while (num_start < override_end && std::isspace(inc_obj[num_start])) num_start++;
                                        
                                        size_t num_end = num_start;
                                        while (num_end < override_end && 
                                               (std::isdigit(inc_obj[num_end]) || inc_obj[num_end] == '.' || 
                                                inc_obj[num_end] == '-')) {
                                            num_end++;
                                        }
                                        
                                        if (num_end > num_start) {
                                            try {
                                                double value = std::stod(inc_obj.substr(num_start, num_end - num_start));
                                                inc.param_overrides[param_name] = value;
                                            } catch (...) {
                                                // Skip invalid numbers
                                            }
                                        }
                                        
                                        search_pos = num_end;
                                    } else {
                                        search_pos++;
                                    }
                                }
                            }
                        }
                    }
                    
                    if (!inc.recipe_path.empty()) {
                        result.recipe.includes.push_back(inc);
                    }
                    
                    current = obj_end + 1;
                }
            }
        }
        
        // Parse params (simplified - just extract param names for now)
        // Full param parsing would extract default_value, min_value, max_value, etc.
        // Parse params - handle nested object structure
        size_t params_pos = content.find("\"params\"");
        if (params_pos != std::string::npos) {
            size_t params_brace = content.find("{", params_pos);
            if (params_brace != std::string::npos) {
                size_t search_pos = params_brace + 1;
                
                // Find param keys at the top level of the params object
                while (search_pos < content.size()) {
                    // Skip whitespace
                    while (search_pos < content.size() && std::isspace(content[search_pos])) search_pos++;
                    if (content[search_pos] == '}') break; // End of params object
                    
                    // Find param name (key before ':')
                    if (content[search_pos] == '"') {
                        size_t name_start = search_pos + 1;
                        size_t name_end = content.find("\"", name_start);
                        if (name_end == std::string::npos) break;
                        
                        std::string param_key = content.substr(name_start, name_end - name_start);
                        
                        // Skip keys that are field names inside param objects
                        if (param_key == "name" || param_key == "default_value" || 
                            param_key == "min_value" || param_key == "max_value" || 
                            param_key == "description") {
                            search_pos = name_end + 1;
                            continue;
                        }
                        
                        // This is a param key - find its nested object
                        size_t colon_pos = content.find(":", name_end);
                        if (colon_pos == std::string::npos) break;
                        
                        size_t obj_start = content.find("{", colon_pos);
                        if (obj_start == std::string::npos) break;
                        
                        // Find closing brace for this param object
                        size_t closing_brace = content.find("}", obj_start);
                        if (closing_brace == std::string::npos) break;
                        
                        // Find default_value within this param's object
                        size_t default_key = content.find("\"default_value\"", obj_start);
                        if (default_key != std::string::npos) {
                            size_t default_colon = content.find(":", default_key);
                            if (default_colon != std::string::npos) {
                                size_t num_start = default_colon + 1;
                                while (num_start < content.size() && std::isspace(content[num_start])) num_start++;
                                size_t num_end = num_start;
                                while (num_end < content.size() && 
                                       (std::isdigit(content[num_end]) || content[num_end] == '.' || content[num_end] == '-')) {
                                    num_end++;
                                }
                                
                                recipe::ParamDef param;
                                param.name = param_key;
                                try {
                                    param.default_value = std::stod(content.substr(num_start, num_end - num_start));
                                } catch (...) {
                                    param.default_value = 0.0;
                                }
                                
                                // Parse min_value if present
                                size_t min_key = content.find("\"min_value\"", obj_start);
                                if (min_key != std::string::npos && min_key < closing_brace) {
                                    size_t min_colon = content.find(":", min_key);
                                    if (min_colon != std::string::npos) {
                                        size_t min_start = min_colon + 1;
                                        while (min_start < content.size() && std::isspace(content[min_start])) min_start++;
                                        size_t min_end = min_start;
                                        while (min_end < content.size() && 
                                               (std::isdigit(content[min_end]) || content[min_end] == '.' || content[min_end] == '-')) {
                                            min_end++;
                                        }
                                        try {
                                            param.min_value = std::stod(content.substr(min_start, min_end - min_start));
                                        } catch (...) {
                                            param.min_value = 0.0;
                                        }
                                    }
                                } else {
                                    param.min_value = 0.0;
                                }
                                
                                // Parse max_value if present
                                size_t max_key = content.find("\"max_value\"", obj_start);
                                if (max_key != std::string::npos && max_key < closing_brace) {
                                    size_t max_colon = content.find(":", max_key);
                                    if (max_colon != std::string::npos) {
                                        size_t max_start = max_colon + 1;
                                        while (max_start < content.size() && std::isspace(content[max_start])) max_start++;
                                        size_t max_end = max_start;
                                        while (max_end < content.size() && 
                                               (std::isdigit(content[max_end]) || content[max_end] == '.' || content[max_end] == '-')) {
                                            max_end++;
                                        }
                                        try {
                                            param.max_value = std::stod(content.substr(max_start, max_end - max_start));
                                        } catch (...) {
                                            param.max_value = 10000.0;
                                        }
                                    }
                                } else {
                                    param.max_value = 10000.0;
                                }
                                
                                result.recipe.params[param_key] = param;
                            }
                        }
                        
                        // Move past this param's closing brace (already found above)
                        search_pos = closing_brace + 1;
                    } else {
                        search_pos++;
                    }
                }
            }
        }
        
        // Parse derived - handle nested object structure like params
        size_t derived_pos = content.find("\"derived\"");
        if (derived_pos != std::string::npos) {
            size_t derived_brace = content.find("{", derived_pos);
            if (derived_brace != std::string::npos) {
                size_t search_pos = derived_brace + 1;
                
                // Find derived keys at the top level of the derived object
                while (search_pos < content.size()) {
                    // Skip whitespace
                    while (search_pos < content.size() && std::isspace(content[search_pos])) search_pos++;
                    if (content[search_pos] == '}') break; // End of derived object
                    
                    // Find derived name (key before ':')
                    if (content[search_pos] == '"') {
                        size_t name_start = search_pos + 1;
                        size_t name_end = content.find("\"", name_start);
                        if (name_end == std::string::npos) break;
                        
                        std::string derived_key = content.substr(name_start, name_end - name_start);
                        
                        // Skip keys that are field names inside derived objects
                        if (derived_key == "name" || derived_key == "formula" || derived_key == "description") {
                            search_pos = name_end + 1;
                            continue;
                        }
                        
                        // This is a derived key - find its nested object
                        size_t colon_pos = content.find(":", name_end);
                        if (colon_pos == std::string::npos) break;
                        
                        size_t obj_start = content.find("{", colon_pos);
                        if (obj_start == std::string::npos) break;
                        
                        // Find formula within this derived's object
                        size_t formula_key = content.find("\"formula\"", obj_start);
                        if (formula_key != std::string::npos) {
                            size_t formula_colon = content.find(":", formula_key);
                            if (formula_colon != std::string::npos) {
                                size_t formula_quote1 = content.find("\"", formula_colon);
                                if (formula_quote1 != std::string::npos) {
                                    size_t formula_quote2 = content.find("\"", formula_quote1 + 1);
                                    if (formula_quote2 != std::string::npos) {
                                        std::string formula = content.substr(formula_quote1 + 1, formula_quote2 - formula_quote1 - 1);
                                        recipe::Expression expr;
                                        expr.formula = formula;
                                        result.recipe.derived[derived_key] = expr;
                                    }
                                }
                            }
                        }
                        
                        // Move past this derived's closing brace
                        size_t closing_brace = content.find("}", obj_start);
                        if (closing_brace == std::string::npos) break;
                        search_pos = closing_brace + 1;
                    } else {
                        search_pos++;
                    }
                }
            }
        }
        
        // Parse variants - similar structure to params
        size_t variants_pos = content.find("\"variants\"");
        if (variants_pos != std::string::npos) {
            size_t variants_brace = content.find("{", variants_pos);
            if (variants_brace != std::string::npos) {
                size_t search_pos = variants_brace + 1;
                
                while (search_pos < content.size()) {
                    // Skip whitespace
                    while (search_pos < content.size() && std::isspace(content[search_pos])) search_pos++;
                    if (content[search_pos] == '}') break;
                    
                    // Find variant name
                    if (content[search_pos] == '"') {
                        size_t name_start = search_pos + 1;
                        size_t name_end = content.find("\"", name_start);
                        if (name_end == std::string::npos) break;
                        
                        std::string variant_name = content.substr(name_start, name_end - name_start);
                        
                        // Skip if this is param_overrides key inside a variant object
                        if (variant_name == "param_overrides") {
                            search_pos = name_end + 1;
                            continue;
                        }
                        
                        // Find variant object
                        size_t colon_pos = content.find(":", name_end);
                        if (colon_pos == std::string::npos) break;
                        
                        size_t obj_start = content.find("{", colon_pos);
                        if (obj_start == std::string::npos) break;
                        
                        // Find param_overrides within this variant
                        size_t overrides_key = content.find("\"param_overrides\"", obj_start);
                        if (overrides_key != std::string::npos) {
                            size_t overrides_brace = content.find("{", overrides_key);
                            if (overrides_brace != std::string::npos) {
                                recipe::Variant variant;
                                variant.name = variant_name;
                                
                                // Parse param_overrides as key-value pairs
                                size_t override_pos = overrides_brace + 1;
                                while (override_pos < content.size()) {
                                    while (override_pos < content.size() && std::isspace(content[override_pos])) override_pos++;
                                    if (content[override_pos] == '}') break;
                                    
                                    if (content[override_pos] == '"') {
                                        size_t pname_start = override_pos + 1;
                                        size_t pname_end = content.find("\"", pname_start);
                                        if (pname_end == std::string::npos) break;
                                        
                                        std::string param_name = content.substr(pname_start, pname_end - pname_start);
                                        
                                        size_t val_colon = content.find(":", pname_end);
                                        if (val_colon == std::string::npos) break;
                                        
                                        size_t val_start = val_colon + 1;
                                        while (val_start < content.size() && std::isspace(content[val_start])) val_start++;
                                        size_t val_end = val_start;
                                        while (val_end < content.size() && 
                                               (std::isdigit(content[val_end]) || content[val_end] == '.' || content[val_end] == '-')) {
                                            val_end++;
                                        }
                                        
                                        try {
                                            double value = std::stod(content.substr(val_start, val_end - val_start));
                                            variant.param_overrides[param_name] = value;
                                        } catch (...) {}
                                        
                                        override_pos = val_end;
                                    } else {
                                        override_pos++;
                                    }
                                }
                                
                                result.recipe.variants[variant_name] = variant;
                            }
                        }
                        
                        // Move past variant closing brace
                        size_t closing_brace = content.find("}", obj_start);
                        if (closing_brace == std::string::npos) break;
                        search_pos = closing_brace + 1;
                    } else {
                        search_pos++;
                    }
                }
            }
        }
        
        // Parse nodes array
        size_t nodes_pos = content.find("\"nodes\"");
        if (nodes_pos != std::string::npos) {
            size_t bracket = content.find("[", nodes_pos);
            if (bracket != std::string::npos) {
                size_t nodes_end = bracket;
                int bracket_depth = 0;
                bool in_string = false;
                
                for (size_t i = bracket; i < content.size(); i++) {
                    if (content[i] == '"' && (i == 0 || content[i-1] != '\\')) {
                        in_string = !in_string;
                    }
                    if (!in_string) {
                        if (content[i] == '[') bracket_depth++;
                        else if (content[i] == ']') {
                            bracket_depth--;
                            if (bracket_depth == 0) {
                                nodes_end = i;
                                break;
                            }
                        }
                    }
                }
                
                // Parse each node object
                size_t current = bracket + 1;
                while (current < nodes_end) {
                    // Find next node object
                    size_t obj_start = content.find("{", current);
                    if (obj_start == std::string::npos || obj_start >= nodes_end) break;
                    
                    // Find matching closing brace with depth tracking
                    size_t obj_end = obj_start;
                    int brace_depth = 0;
                    in_string = false;
                    
                    for (size_t i = obj_start; i < nodes_end; i++) {
                        if (content[i] == '"' && (i == 0 || content[i-1] != '\\')) {
                            in_string = !in_string;
                        }
                        if (!in_string) {
                            if (content[i] == '{') brace_depth++;
                            else if (content[i] == '}') {
                                brace_depth--;
                                if (brace_depth == 0) {
                                    obj_end = i;
                                    break;
                                }
                            }
                        }
                    }
                    
                    if (obj_end == obj_start) break;
                    
                    std::string node_str = content.substr(obj_start, obj_end - obj_start + 1);
                    
                    // Extract node fields using lambda with captured node_str
                    auto extract_from_node = [&node_str](const std::string& key) -> std::string {
                        std::string search = "\"" + key + "\"";
                        size_t pos = node_str.find(search);
                        if (pos == std::string::npos) return "";
                        
                        size_t colon = node_str.find(":", pos);
                        if (colon == std::string::npos) return "";
                        
                        size_t quote1 = node_str.find("\"", colon);
                        if (quote1 == std::string::npos) return "";
                        
                        size_t quote2 = node_str.find("\"", quote1 + 1);
                        if (quote2 == std::string::npos) return "";
                        
                        return node_str.substr(quote1 + 1, quote2 - quote1 - 1);
                    };
                    
                    auto extract_number_from_node = [&node_str](const std::string& key) -> double {
                        std::string search = "\"" + key + "\"";
                        size_t pos = node_str.find(search);
                        if (pos == std::string::npos) return 0.0;
                        
                        size_t colon = node_str.find(":", pos);
                        if (colon == std::string::npos) return 0.0;
                        
                        size_t num_start = colon + 1;
                        while (num_start < node_str.size() && std::isspace(node_str[num_start])) num_start++;
                        
                        size_t num_end = num_start;
                        while (num_end < node_str.size() && 
                               (std::isdigit(node_str[num_end]) || node_str[num_end] == '.' || 
                                node_str[num_end] == '-')) {
                            num_end++;
                        }
                        
                        if (num_end > num_start) {
                            try {
                                return std::stod(node_str.substr(num_start, num_end - num_start));
                            } catch (...) {
                                return 0.0;
                            }
                        }
                        return 0.0;
                    };
                    
                    // Helper: parse scalar (number or expression string)
                    auto extract_scalar_from_node = [&node_str](const std::string& key) -> recipe::Scalar {
                        std::string search = "\"" + key + "\"";
                        size_t pos = node_str.find(search);
                        if (pos == std::string::npos) return 0.0;
                        
                        size_t colon = node_str.find(":", pos);
                        if (colon == std::string::npos) return 0.0;
                        
                        size_t value_start = colon + 1;
                        while (value_start < node_str.size() && std::isspace(node_str[value_start])) value_start++;
                        
                        // Check if value is a quoted string (expression)
                        if (value_start < node_str.size() && node_str[value_start] == '"') {
                            size_t quote_end = node_str.find("\"", value_start + 1);
                            if (quote_end != std::string::npos) {
                                std::string formula = node_str.substr(value_start + 1, quote_end - value_start - 1);
                                recipe::Expression expr;
                                expr.formula = formula;
                                return expr;
                            }
                        }
                        
                        // Otherwise parse as number
                        size_t num_end = value_start;
                        while (num_end < node_str.size() && 
                               (std::isdigit(node_str[num_end]) || node_str[num_end] == '.' || 
                                node_str[num_end] == '-')) {
                            num_end++;
                        }
                        
                        if (num_end > value_start) {
                            try {
                                return std::stod(node_str.substr(value_start, num_end - value_start));
                            } catch (...) {
                                return 0.0;
                            }
                        }
                        return 0.0;
                    };
                    
                    recipe::RecipeNode node;
                    node.id = extract_from_node("id");
                    node.description = extract_from_node("description");
                    
                    std::string op_str = extract_from_node("op");
                    if (op_str == "make_box") {
                        node.op = recipe::NodeOp::MakeBox;
                        recipe::MakeBoxArgs args;
                        args.dx = extract_scalar_from_node("dx");
                        args.dy = extract_scalar_from_node("dy");
                        args.dz = extract_scalar_from_node("dz");
                        node.args = args;
                    } else if (op_str == "transform") {
                        node.op = recipe::NodeOp::Transform;
                        recipe::TransformArgs args;
                        args.input = extract_from_node("input");
                        args.tx = extract_scalar_from_node("tx");
                        args.ty = extract_scalar_from_node("ty");
                        args.tz = extract_scalar_from_node("tz");
                        args.rx = extract_scalar_from_node("rx");
                        args.ry = extract_scalar_from_node("ry");
                        args.rz = extract_scalar_from_node("rz");
                        node.args = args;
                    } else if (op_str == "compound") {
                        node.op = recipe::NodeOp::Compound;
                        recipe::CompoundArgs args;
                        
                        // Parse node_refs array
                        size_t refs_pos = node_str.find("\"node_refs\"");
                        if (refs_pos != std::string::npos) {
                            size_t bracket = node_str.find("[", refs_pos);
                            if (bracket != std::string::npos) {
                                size_t close_bracket = node_str.find("]", bracket);
                                if (close_bracket != std::string::npos) {
                                    std::string refs_content = node_str.substr(bracket + 1, close_bracket - bracket - 1);
                                    
                                    // Extract each quoted string
                                    size_t pos = 0;
                                    while (pos < refs_content.size()) {
                                        size_t quote1 = refs_content.find("\"", pos);
                                        if (quote1 == std::string::npos) break;
                                        size_t quote2 = refs_content.find("\"", quote1 + 1);
                                        if (quote2 == std::string::npos) break;
                                        
                                        std::string ref = refs_content.substr(quote1 + 1, quote2 - quote1 - 1);
                                        args.node_refs.push_back(ref);
                                        pos = quote2 + 1;
                                    }
                                }
                            }
                        }
                        node.args = args;
                    } else {
                        // Unknown op type - skip or log
                        current = obj_end + 1;
                        continue;
                    }
                    
                    // Extract binding.body and binding.frame
                    size_t binding_pos = node_str.find("\"binding\"");
                    if (binding_pos != std::string::npos) {
                        size_t body_pos = node_str.find("\"body\"", binding_pos);
                        if (body_pos != std::string::npos) {
                            size_t colon = node_str.find(":", body_pos);
                            size_t quote1 = node_str.find("\"", colon);
                            size_t quote2 = node_str.find("\"", quote1 + 1);
                            if (quote1 != std::string::npos && quote2 != std::string::npos) {
                                node.binding.body = node_str.substr(quote1 + 1, quote2 - quote1 - 1);
                            }
                        }
                        
                        // Extract frame if present
                        size_t frame_pos = node_str.find("\"frame\"", binding_pos);
                        if (frame_pos != std::string::npos) {
                            size_t frame_colon = node_str.find(":", frame_pos);
                            size_t frame_quote1 = node_str.find("\"", frame_colon);
                            size_t frame_quote2 = node_str.find("\"", frame_quote1 + 1);
                            if (frame_quote1 != std::string::npos && frame_quote2 != std::string::npos) {
                                node.binding.frame = node_str.substr(frame_quote1 + 1, frame_quote2 - frame_quote1 - 1);
                            }
                        }
                    }
                    
                    result.recipe.nodes.push_back(node);
                    current = obj_end + 1;
                }
            }
        }
        
        result.success = true;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "Recipe parsing error: " + std::string(e.what());
    }
    
    return result;
}

kinematics::PKM load_pkm_json(const std::string& pkm_path) {
    // Use existing loader
    auto result = kinematics::KinematicLoader::load_from_json(pkm_path);
    return result.pkm;
}

} // namespace praxis
