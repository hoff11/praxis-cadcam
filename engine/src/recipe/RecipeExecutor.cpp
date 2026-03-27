#include "RecipeExecutor.hpp"
#include "RecipeValidator.hpp"
#include "../kernel/StepIO.hpp"
#include "../kinematics/KinematicLoader.hpp"
#include "../reasoning/ReasoningEngine.hpp"
#include "../cache/ExecutionCache.hpp"
#include "../cache/OpCache.hpp"
#include "../cache/HashContext.hpp"
#include "../cache/KernelOpHasher.hpp"
#include "../plan/ExecutionGraph.hpp"
#include "../kernel/KernelVersion.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <set>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <openssl/sha.h>

namespace praxis {
namespace recipe {

// Helper: Compute SHA256 hash of file content (Sprint 5 Phase 3)
static std::string compute_file_sha256(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        return "";  // File not found or unreadable
    }
    
    // Read entire file
    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // Compute SHA256
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(content.c_str()), 
           content.length(), hash);
    
    // Convert to hex string
    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        hex << std::setw(2) << static_cast<int>(hash[i]);
    }
    
    return hex.str();
}

RecipeExecutionResult RecipeExecutor::execute(
    const Recipe& recipe,
    const kinematics::PKM& pkm,
    const std::map<std::string, std::string>& cli_params,
    const std::string& output_dir,
    const std::vector<RecipeInclude>& all_includes,
    bool no_cache,
    bool clear_cache
) {
    RecipeExecutionResult result;
    
    // Validate recipe
    auto validation = RecipeValidator::validate(recipe);
    if (!validation.valid) {
        result.success = false;
        result.error_message = "Recipe validation failed: " + 
            (validation.errors.empty() ? "unknown" : validation.errors[0]);
        result.warnings = validation.warnings;
        return result;
    }
    
    // Merge parameters (defaults + CLI overrides)
    std::vector<std::string> param_errors;
    auto merged_params = merge_params(recipe, cli_params, param_errors);
    if (!param_errors.empty()) {
        result.success = false;
        result.error_message = "Parameter error: " + param_errors[0];
        return result;
    }
    
    // Evaluate derived expressions
    std::vector<std::string> derived_errors;
    auto derived_values = evaluate_derived(recipe, merged_params, derived_errors);
    if (!derived_errors.empty()) {
        result.success = false;
        result.error_message = "Derived expression error: " + derived_errors[0];
        return result;
    }
    
    // Combine params + derived into single value map
    std::map<std::string, double> all_values = merged_params;
    for (const auto& kv : derived_values) {
        all_values[kv.first] = kv.second;
    }
    
    // Validate bindings against PKM bodies
    std::vector<std::string> binding_errors;
    if (!validate_bindings(recipe, pkm, binding_errors)) {
        result.success = false;
        result.error_message = "Binding validation failed: " + binding_errors[0];
        return result;
    }
    
    // Store resolved inputs for reporting (Sprint 3 EPIC 3)
    result.resolved_params = merged_params;
    result.derived_values = derived_values;
    
    // Build resolved plan (Sprint 5 Phase 1)
    result.resolved_plan = build_resolved_plan(recipe, all_values, all_includes, recipe.kinematics_path);
    
    // Cache control (Sprint 5 Phase 6)
    cache::ExecutionCache cache;
    
    // Handle --clear-cache flag (Gap C fix: safety guards)
    if (clear_cache) {
        auto cache_root = cache.get_cache_root();
        std::string cache_str = cache_root.string();
        
        // Safety checks before deletion
        bool safe_to_delete = true;
        std::string safety_error;
        
        if (cache_str.empty() || cache_str == "/" || cache_str == "\\" || cache_str.length() < 5) {
            safe_to_delete = false;
            safety_error = "Cache root path too short or dangerous: " + cache_str;
        } else if (cache_str.find("cache") == std::string::npos && cache_str.find(".praxis") == std::string::npos) {
            safe_to_delete = false;
            safety_error = "Cache root doesn't contain 'cache' or '.praxis': " + cache_str;
        } else if (!std::filesystem::exists(cache_root)) {
            // Already doesn't exist - that's fine
            std::cout << "Cache directory doesn't exist, nothing to clear\n";
            safe_to_delete = false;
        }
        
        if (safe_to_delete) {
            std::cout << "Clearing cache directory: " << cache_str << "\n";
            try {
                std::filesystem::remove_all(cache_root);
                std::cout << "✓ Cache cleared\n";
            } catch (const std::exception& e) {
                std::cerr << "⚠ Failed to clear cache: " << e.what() << "\n";
            }
        } else if (!safety_error.empty()) {
            std::cerr << "⚠ Refusing to clear cache: " << safety_error << "\n";
        }
    }
    
    // Try cache restore (Sprint 5 Phase 4) unless --no-cache is set
    if (!no_cache && cache.try_restore(result.resolved_plan, output_dir, result)) {
        // Cache hit - execution was skipped
        std::cout << "Cache hit, geometry execution skipped\n";
        return result;
    }
    
    // Gap D fix: preserve detailed miss reason from try_restore if meaningful
    std::string cache_miss_reason;
    if (no_cache) {
        cache_miss_reason = "Cache disabled via --no-cache flag";
        std::cout << cache_miss_reason << ", executing geometry\n";
    } else if (!result.cache.reason.empty() && result.cache.reason != "No cache entry found") {
        // Keep the detailed reason from try_restore (e.g., "Plan hash invalid", "Cache entry missing or invalid")
        cache_miss_reason = result.cache.reason;
        std::cout << "Cache miss (" << cache_miss_reason << "), executing geometry\n";
    } else {
        cache_miss_reason = "No cache entry found";
        std::cout << "Cache miss, executing geometry\n";
    }
    
    // Populate cache miss info for reporting (Sprint 5 Phase 5)
    result.cache.hit = false;
    result.cache.plan_hash = result.resolved_plan.plan_hash;
    result.cache.reason = cache_miss_reason;
    result.cache.reused_artifacts.clear();
    result.cache.cache_entry_age_seconds = 0;
    
    // Run reasoning engine (after validation, before geometry)
    reasoning::ReasoningEngine reasoning_engine;
    reasoning::ResolvedRecipe resolved;
    resolved.params = merged_params;
    resolved.derived = derived_values;
    if (cli_params.count("variant") > 0) {
        resolved.variant_name = cli_params.at("variant");
    }
    result.reasoning = reasoning_engine.evaluate(resolved, pkm);
    
    // Reset kernel metrics for this execution
    kernel::KernelOps::reset_metrics();
    
    // Sprint 6 Phase 3: Build execution graph and compute op hashes
    auto exec_graph = plan::build_graph_from_resolved_plan(recipe, result.resolved_plan);
    
    cache::HashContext hash_ctx;
    #ifdef PRAXIS_ENGINE_VERSION
    hash_ctx.engine_version = PRAXIS_ENGINE_VERSION;
    #else
    hash_ctx.engine_version = "dev";
    #endif
    hash_ctx.kernel_version = kernel::get_kernel_version_string();
    hash_ctx.arg_precision = 6;
    hash_ctx.schema_version = 1;
    hash_ctx.output_contract = "out";
    
    exec_graph = plan::compute_hashes(exec_graph, hash_ctx);
    
    // Initialize op-level cache
    cache::OpCache op_cache(cache.get_cache_root());
    
    // Set cache directory for report normalization early (needed for relative paths)
    result.op_cache.cache_dir = op_cache.get_cache_root().string();
    
    // Execute nodes in order with op-level caching
    std::map<std::string, kernel::ShapeHandle> node_outputs;
    std::map<size_t, std::filesystem::path> node_artifact_paths;  // node_index -> cached artifact path
    
    for (size_t i = 0; i < recipe.nodes.size(); ++i) {
        const auto& node = recipe.nodes[i];
        const auto& graph_node = exec_graph.nodes[i];
        
        NodeExecutionStatus node_status;
        node_status.node_id = node.id;
        node_status.node_index = i;
        node_status.op_type = graph_node.op_type;
        node_status.op_hash = graph_node.op_hash;
        node_status.cache_hit = false;
        node_status.status = "executed";  // Default, may change
        
        // Try op-level cache first (if not disabled)
        bool executed_from_cache = false;
        if (!no_cache && !graph_node.op_hash.empty()) {
            auto cache_hit = op_cache.try_load(graph_node.op_hash);
            
            if (cache_hit.hit && cache_hit.output_paths.count("out") > 0) {
                // Cache hit! Load shape from cached artifact
                auto cached_step = cache_hit.output_paths["out"];
                
                auto load_result = kernel::StepIO::read_step(cached_step.string());
                if (load_result.success && !load_result.shape.is_null()) {
                    node_outputs[node.id] = load_result.shape;
                    node_artifact_paths[i] = cached_step;
                    
                    node_status.cache_hit = true;
                    node_status.status = "reused";
                    // P1-3: Store relative path from cache root for determinism
                    auto cache_root = std::filesystem::path(result.op_cache.cache_dir);
                    node_status.artifact_path = std::filesystem::relative(cached_step, cache_root).string();
                    // P1-4: Store in outputs map
                    node_status.outputs["out"] = node_status.artifact_path;
                    executed_from_cache = true;
                    
                    result.op_cache.reused_count++;
                    
                    std::cout << "  Node[" << i << "] " << node.id << ": cache hit (reused)\n";
                }
            }
        }
        
        // Execute node if not cached
        if (!executed_from_cache) {
            auto node_result = execute_node(node, all_values, node_outputs);
            
            if (!node_result.success) {
                node_status.status = "failed";
                result.op_cache.node_status.push_back(node_status);
                
                result.success = false;
                result.error_message = "Node '" + node.id + "' failed: " + node_result.error_message;
                return result;
            }
            
            // Store output shape
            if (!node_result.shape.is_null()) {
                node_outputs[node.id] = node_result.shape;
                
                // Store to op cache (if not disabled)
                if (!no_cache && !graph_node.op_hash.empty()) {
                    auto store = op_cache.begin_store(graph_node.op_hash);
                    
                    // P0-2 FIX: Guard against lock timeout or staging failure
                    if (store.active) {
                        // Export shape to cache staging directory
                        auto staged_step = store.get_staging_path("out", ".step");
                        auto write_result = kernel::StepIO::write_step(node_result.shape, staged_step.string());
                        
                        if (write_result.success) {
                            // Commit to cache
                            cache::OpCacheEntryMeta meta;
                            meta.op_hash = graph_node.op_hash;
                            meta.engine_version = hash_ctx.engine_version;
                            meta.kernel_version = hash_ctx.kernel_version;
                            
                            // P0-3 FIX: Real UTC ISO-8601 timestamp
                            auto now = std::chrono::system_clock::now();
                            auto now_t = std::chrono::system_clock::to_time_t(now);
                            std::tm utc_tm;
                            #ifdef _WIN32
                                gmtime_s(&utc_tm, &now_t);
                            #else
                                gmtime_r(&now_t, &utc_tm);
                            #endif
                            char time_buf[32];
                            std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%SZ", &utc_tm);
                            meta.created_at = time_buf;
                            
                            cache::OutputArtifact artifact;
                            artifact.name = "out";
                            artifact.filename = "out.step";
                            artifact.bytes = std::filesystem::file_size(staged_step);
                            
                            // Phase 4 EPIC 2: Compute SHA256 for integrity checking
                            artifact.sha256 = op_cache.compute_file_sha256(staged_step);
                            
                            // Sprint 10 Task 2.1: Preview artifact classification
                            // Determine type from file extension
                            std::filesystem::path file_path(artifact.filename);
                            std::string ext = file_path.extension().string();
                            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                            
                            if (ext == ".step" || ext == ".stp") {
                                artifact.type = "step";
                                artifact.previewable = true;
                                artifact.semantic_type = "Body";  // STEP files typically contain bodies
                            } else if (ext == ".stl") {
                                artifact.type = "stl";
                                artifact.previewable = true;
                                artifact.semantic_type = "Body";  // STL files represent tessellated bodies
                            } else if (ext == ".brep") {
                                artifact.type = "brep";
                                artifact.previewable = false;  // BREP is internal format, not previewable
                                artifact.semantic_type = "Body";
                            }
                            
                            meta.outputs.push_back(artifact);
                            
                            if (op_cache.commit_store(store, meta)) {
                                auto loaded = op_cache.try_load(graph_node.op_hash);
                                node_artifact_paths[i] = loaded.output_paths["out"];
                                // P1-3: Store relative path from cache root for determinism
                                auto cache_root = std::filesystem::path(result.op_cache.cache_dir);
                                node_status.artifact_path = std::filesystem::relative(node_artifact_paths[i], cache_root).string();
                                // P1-4: Store in outputs map
                                node_status.outputs["out"] = node_status.artifact_path;
                            }
                        } else {
                            // Write failed - abort store
                            op_cache.abort_store(store);
                        }
                    } else {
                        // Lock timeout or staging creation failed - skip caching this node
                        std::cerr << "Warning: Could not acquire cache lock for node " << node.id 
                                  << " (op_hash: " << graph_node.op_hash << "), skipping cache store\n";
                    }
                }
                
                // Ensure artifact_path is set even if caching failed or was disabled
                // This maintains backwards compatibility with tests that expect artifact paths
                if (node_status.artifact_path.empty()) {
                    // If we have a cached path, use it
                    if (node_artifact_paths.count(i) > 0) {
                        auto cache_root = std::filesystem::path(result.op_cache.cache_dir);
                        node_status.artifact_path = std::filesystem::relative(node_artifact_paths[i], cache_root).string();
                        node_status.outputs["out"] = node_status.artifact_path;
                    }
                }
            }
            
            // Collect operations
            result.kernel_ops.insert(result.kernel_ops.end(),
                node_result.operations.begin(), node_result.operations.end());
            
            result.op_cache.executed_count++;
            
            std::cout << "  Node[" << i << "] " << node.id << ": executed\n";
        }
        
        // Record binding
        if (node.op != NodeOp::Compound && !node.binding.body.empty()) {
            NodeBodyBinding binding;
            binding.node_id = node.id;
            binding.body_id = node.binding.body;
            binding.frame_id = node.binding.frame;
            result.bindings.push_back(binding);
        }
        
        result.op_cache.node_status.push_back(node_status);
    }
    
    // Compute cache hit rate
    int total_nodes = result.op_cache.executed_count + result.op_cache.reused_count;
    if (total_nodes > 0) {
        result.op_cache.cache_hit_rate = static_cast<double>(result.op_cache.reused_count) / total_nodes;
    }
    
    // Get final output shape from explicit output node
    kernel::ShapeHandle final_shape;
    if (!recipe.output.empty() && node_outputs.count(recipe.output) > 0) {
        final_shape = node_outputs[recipe.output];
    }
    
    if (final_shape.is_null()) {
        result.success = false;
        result.error_message = "Output node '" + recipe.output + "' produced no shape";
        return result;
    }
    
    // Write STEP file
    std::string step_path = output_dir + "/model.step";
    auto write_result = kernel::StepIO::write_step(final_shape, step_path);
    
    if (!write_result.success) {
        result.success = false;
        result.error_message = "Failed to write STEP: " + write_result.error_message;
        return result;
    }
    
    result.step_file_path = step_path;
    result.kernel_ops.insert(result.kernel_ops.end(),
        write_result.operations.begin(), write_result.operations.end());
    
    // Write PKM file (copy normalized PKM)
    std::string pkm_path = output_dir + "/model.pkm";
    if (!kinematics::KinematicLoader::save_to_json(pkm, pkm_path)) {
        result.warnings.push_back("Failed to write PKM file");
    } else {
        result.pkm_file_path = pkm_path;
    }
    
    // Write bindings file
    std::string bindings_path = output_dir + "/bindings.json";
    if (!write_bindings_file(result.bindings, bindings_path)) {
        result.warnings.push_back("Failed to write bindings file");
    } else {
        result.bindings_file_path = bindings_path;
    }
    
    // Collect metrics
    auto kernel_metrics = kernel::KernelOps::get_metrics();
    for (const auto& kv : kernel_metrics) {
        result.metrics["op_" + kv.first + "_count"] = std::to_string(kv.second);
    }
    
    result.metrics["recipe_id"] = recipe.id;
    result.metrics["node_count"] = std::to_string(recipe.nodes.size());
    result.metrics["body_count"] = std::to_string(pkm.bodies.size());
    result.metrics["joint_count"] = std::to_string(pkm.joints.size());
    
    // Store in cache on successful execution (Sprint 5 Phase 4), unless --no-cache is set
    if (!no_cache) {
        cache.store(result.resolved_plan, result, output_dir);
    }
    
    // Print op cache stats for user visibility (Sprint 6 Phase 3 Step 6)
    if (result.op_cache.executed_count > 0 || result.op_cache.reused_count > 0) {
        int total = result.op_cache.executed_count + result.op_cache.reused_count;
        if (result.op_cache.reused_count > 0) {
            std::cout << "✓ Executed " << result.op_cache.executed_count << " ops, "
                      << "reused " << result.op_cache.reused_count << " from cache "
                      << "(" << std::fixed << std::setprecision(0) 
                      << (result.op_cache.cache_hit_rate * 100) << "% hit rate)\n";
        } else {
            std::cout << "✓ Executed " << result.op_cache.executed_count << " ops (cold start)\n";
        }
    }
    
    // Sprint 10 Task 3.1: Generate structured result summary
    generate_result_summary(result, recipe);
    
    return result;
}

RecipeExecutionResult RecipeExecutor::validate_only(
    const Recipe& recipe,
    const kinematics::PKM& pkm,
    const std::map<std::string, std::string>& cli_params
) {
    RecipeExecutionResult result;
    std::vector<std::string> errors;
    
    // 1) Validate bindings against PKM bodies and frames
    if (!validate_bindings(recipe, pkm, errors)) {
        result.success = false;
        // Join all binding errors
        for (size_t i = 0; i < errors.size(); ++i) {
            result.error_message += errors[i];
            if (i + 1 < errors.size()) result.error_message += "; ";
        }
        return result;
    }
    
    // 2) Merge params (defaults -> variant -> CLI) with range validation
    auto params = merge_params(recipe, cli_params, errors);
    if (!errors.empty()) {
        result.success = false;
        for (size_t i = 0; i < errors.size(); ++i) {
            result.error_message += errors[i];
            if (i + 1 < errors.size()) result.error_message += "; ";
        }
        return result;
    }
    
    // 3) Evaluate derived expressions (no kernel ops)
    errors.clear();
    auto derived = evaluate_derived(recipe, params, errors);
    if (!errors.empty()) {
        result.success = false;
        for (size_t i = 0; i < errors.size(); ++i) {
            result.error_message += errors[i];
            if (i + 1 < errors.size()) result.error_message += "; ";
        }
        return result;
    }
    
    // All validation passed
    result.success = true;
    return result;
}

std::map<std::string, double> RecipeExecutor::merge_params(
    const Recipe& recipe,
    const std::map<std::string, std::string>& cli_params,
    std::vector<std::string>& errors
) {
    std::map<std::string, double> result;
    
    // Start with defaults
    for (const auto& kv : recipe.params) {
        result[kv.first] = kv.second.default_value;
    }
    
    // Apply variant overrides if variant parameter is provided
    std::string variant_name;
    if (cli_params.count("variant") > 0) {
        variant_name = cli_params.at("variant");
        
        if (recipe.variants.count(variant_name) == 0) {
            errors.push_back("Unknown variant: " + variant_name);
            return {};
        }
        
        // Apply variant parameter overrides with range validation
        const auto& variant = recipe.variants.at(variant_name);
        for (const auto& kv : variant.param_overrides) {
            if (recipe.params.count(kv.first) == 0) {
                errors.push_back("Variant '" + variant_name + "' references unknown parameter: " + kv.first);
                return {};
            }
            
            // Validate variant override value against param ranges
            const auto& param_def = recipe.params.at(kv.first);
            if (kv.second < param_def.min_value || kv.second > param_def.max_value) {
                errors.push_back("Variant '" + variant_name + "' parameter '" + kv.first + 
                               "' value out of range: " + std::to_string(kv.second) + 
                               " (expected [" + std::to_string(param_def.min_value) + ", " + 
                               std::to_string(param_def.max_value) + "])");
                return {};
            }
            
            result[kv.first] = kv.second;
        }
    }
    
    // Apply CLI overrides (takes precedence over variant)
    for (const auto& kv : cli_params) {
        // Skip the variant parameter itself
        if (kv.first == "variant") continue;
        
        if (recipe.params.count(kv.first) == 0) {
            errors.push_back("Unknown parameter: " + kv.first);
            return {};
        }
        
        // Parse value
        double value;
        try {
            value = std::stod(kv.second);
        } catch (...) {
            errors.push_back("Invalid value for parameter '" + kv.first + "': " + kv.second);
            return {};
        }
        
        // Validate range
        const auto& param_def = recipe.params.at(kv.first);
        if (value < param_def.min_value || value > param_def.max_value) {
            errors.push_back("Parameter '" + kv.first + "' value out of range: " + 
                           std::to_string(value) + " (expected [" + 
                           std::to_string(param_def.min_value) + ", " + 
                           std::to_string(param_def.max_value) + "])");
            return {};
        }
        
        result[kv.first] = value;
    }
    
    return result;
}

std::map<std::string, double> RecipeExecutor::evaluate_derived(
    const Recipe& recipe,
    const std::map<std::string, double>& params,
    std::vector<std::string>& errors
) {
    std::map<std::string, double> result;
    
    // Start with params as the base environment
    std::map<std::string, double> eval_env = params;
    
    // Evaluate each derived value, adding it to the environment
    // This allows later derived values to reference earlier ones
    for (const auto& kv : recipe.derived) {
        try {
            double value = kv.second.evaluate(eval_env);
            result[kv.first] = value;
            eval_env[kv.first] = value;  // Add to environment for next evaluations
        } catch (const std::exception& e) {
            errors.push_back("Failed to evaluate derived '" + kv.first + "': " + e.what());
            return {};
        }
    }
    
    return result;
}

kernel::KernelOpResult RecipeExecutor::execute_node(
    const RecipeNode& node,
    const std::map<std::string, double>& all_values,
    std::map<std::string, kernel::ShapeHandle>& node_outputs
) {
    using namespace kernel;
    
    // Build source info string for provenance
    std::string source_info = "";
    if (!node.source.recipe_path_display.empty() && !node.source.namespace_alias.empty()) {
        source_info = " [" + node.source.recipe_path_display + ":" + node.source.namespace_alias + "#" + 
                      std::to_string(node.source.operation_index) + "]";
    } else if (!node.source.recipe_path_display.empty()) {
        source_info = " [" + node.source.recipe_path_display + "#" + 
                      std::to_string(node.source.operation_index) + "]";
    }
    
    if (node.op == NodeOp::MakeBox) {
        const auto& args = std::get<MakeBoxArgs>(node.args);
        
        // Evaluate scalar args with current parameter values
        double dx = evaluate_scalar(args.dx, all_values);
        double dy = evaluate_scalar(args.dy, all_values);
        double dz = evaluate_scalar(args.dz, all_values);
        
        auto result = KernelOps::make_box(dx, dy, dz);
        result.operations.insert(result.operations.begin(),
            "node[" + node.id + "]" + source_info + ": " + result.operations[0]);
        return result;
        
    } else if (node.op == NodeOp::Transform) {
        const auto& args = std::get<TransformArgs>(node.args);
        
        // Get input shape from specified node
        if (node_outputs.count(args.input) == 0) {
            KernelOpResult result;
            result.success = false;
            result.error_message = "Transform node '" + node.id + "' input '" + args.input + "' not found";
            return result;
        }
        
        kernel::ShapeHandle input_shape = node_outputs[args.input];
        
        if (input_shape.is_null()) {
            KernelOpResult result;
            result.success = false;
            result.error_message = "Transform node input shape is null";
            return result;
        }
        
        // Evaluate scalar args
        double tx = evaluate_scalar(args.tx, all_values);
        double ty = evaluate_scalar(args.ty, all_values);
        double tz = evaluate_scalar(args.tz, all_values);
        double rx = evaluate_scalar(args.rx, all_values);
        double ry = evaluate_scalar(args.ry, all_values);
        double rz = evaluate_scalar(args.rz, all_values);
        
        auto result = KernelOps::transform(input_shape, tx, ty, tz, rx, ry, rz);
        result.operations.insert(result.operations.begin(),
            "node[" + node.id + "]" + source_info + ": " + result.operations[0]);
        return result;
        
    } else if (node.op == NodeOp::Compound) {
        const auto& args = std::get<CompoundArgs>(node.args);
        
        std::vector<kernel::ShapeHandle> shapes;
        for (const auto& ref : args.node_refs) {
            if (node_outputs.count(ref) > 0) {
                shapes.push_back(node_outputs[ref]);
            }
        }
        
        auto result = KernelOps::make_compound(shapes);
        result.operations.insert(result.operations.begin(),
            "node[" + node.id + "]" + source_info + ": " + result.operations[0]);
        return result;
    }
    
    KernelOpResult result;
    result.success = false;
    result.error_message = "Unknown node operation";
    return result;
}

bool RecipeExecutor::validate_bindings(
    const Recipe& recipe,
    const kinematics::PKM& pkm,
    std::vector<std::string>& errors
) {
    std::set<std::string> body_ids(pkm.bodies.begin(), pkm.bodies.end());
    
    // Build frame name set from PKM
    std::set<std::string> frame_names;
    for (const auto& frame : pkm.frames) {
        frame_names.insert(frame.name);
    }
    
    for (const auto& node : recipe.nodes) {
        if (node.op == NodeOp::Compound) {
            continue;  // Compound nodes don't produce geometry
        }
        
        // Validate body binding
        if (!node.binding.body.empty() && body_ids.count(node.binding.body) == 0) {
            errors.push_back("Node '" + node.id + "' binds to unknown body: " + node.binding.body);
            return false;
        }
        
        // Validate frame binding if specified
        if (!node.binding.frame.empty() && frame_names.count(node.binding.frame) == 0) {
            errors.push_back("Node '" + node.id + "' binds to unknown frame: " + node.binding.frame);
            return false;
        }
    }
    
    return true;
}

bool RecipeExecutor::write_bindings_file(
    const std::vector<NodeBodyBinding>& bindings,
    const std::string& file_path
) {
    try {
        std::ofstream file(file_path);
        if (!file.good()) {
            return false;
        }
        
        file << "{\n";
        file << "  \"bindings\": [\n";
        
        for (size_t i = 0; i < bindings.size(); i++) {
            const auto& binding = bindings[i];
            file << "    {\n";
            file << "      \"node_id\": \"" << binding.node_id << "\",\n";
            file << "      \"body_id\": \"" << binding.body_id << "\"";
            
            if (!binding.frame_id.empty()) {
                file << ",\n      \"frame_id\": \"" << binding.frame_id << "\"";
            }
            
            file << "\n    }";
            if (i < bindings.size() - 1) {
                file << ",";
            }
            file << "\n";
        }
        
        file << "  ]\n";
        file << "}\n";
        
        return true;
        
    } catch (...) {
        return false;
    }
}

// Build resolved plan (Sprint 5 Phase 1)
ResolvedPlan RecipeExecutor::build_resolved_plan(
    const Recipe& recipe,
    const std::map<std::string, double>& final_params,
    const std::vector<RecipeInclude>& all_includes,
    const std::string& kinematics_path
) {
    ResolvedPlan plan;
    
    // Flatten operations with fully resolved args
    for (const auto& node : recipe.nodes) {
        ResolvedOperation op;
        op.op = node.op;
        op.node_id = node.id;
        op.source = node.source;
        
        // Resolve all arguments to numeric values
        if (node.op == NodeOp::MakeBox) {
            const auto& args = std::get<MakeBoxArgs>(node.args);
            op.resolved_args["dx"] = evaluate_scalar(args.dx, final_params);
            op.resolved_args["dy"] = evaluate_scalar(args.dy, final_params);
            op.resolved_args["dz"] = evaluate_scalar(args.dz, final_params);
        } else if (node.op == NodeOp::Transform) {
            const auto& args = std::get<TransformArgs>(node.args);
            op.resolved_args["input"] = 0.0;  // Node reference, not numeric
            op.resolved_args["tx"] = evaluate_scalar(args.tx, final_params);
            op.resolved_args["ty"] = evaluate_scalar(args.ty, final_params);
            op.resolved_args["tz"] = evaluate_scalar(args.tz, final_params);
            op.resolved_args["rx"] = evaluate_scalar(args.rx, final_params);
            op.resolved_args["ry"] = evaluate_scalar(args.ry, final_params);
            op.resolved_args["rz"] = evaluate_scalar(args.rz, final_params);
        }
        // Compound ops have no numeric args
        
        plan.operations.push_back(op);
    }
    
    // Store includes (already flattened from load_recipe_with_includes)
    plan.includes = all_includes;
    
    // Store final parameter table
    plan.final_params = final_params;
    
    // Set version identifiers
    plan.engine_version = "0.1.0";  // TODO: get from build system
    plan.kernel_version = "OCE-0.17";  // TODO: query from kernel
    
    // Canonicalize kinematics path
    try {
        plan.kinematics_path_canonical = std::filesystem::weakly_canonical(
            std::filesystem::absolute(kinematics_path)).string();
    } catch (...) {
        plan.kinematics_path_canonical = kinematics_path;
    }
    
    // Compute kinematics content hash (Sprint 5 Phase 3)
    plan.kinematics_sha256 = compute_file_sha256(kinematics_path);
    
    plan.output_node = recipe.output;
    
    // Validate plan before hashing (Sprint 5 Phase 3)
    auto validation = ResolvedPlan::validate_for_hash(plan);
    if (validation.valid) {
        plan.plan_hash = plan.compute_hash();
        plan.hash_valid = true;
        plan.hash_status_reason = "";
    } else {
        plan.plan_hash = "";
        plan.hash_valid = false;
        plan.hash_status_reason = validation.reason;
    }
    
    return plan;
}

// Sprint 10 Task 3.1: Generate deterministic result summary
void RecipeExecutor::generate_result_summary(
    RecipeExecutionResult& result,
    const Recipe& recipe
) {
    // Generate intent description based on recipe metadata
    result.summary.intent = "Generate geometry from recipe";
    
    // Try to get more specific intent from metadata
    auto desc_it = recipe.metadata.find("description");
    if (desc_it != recipe.metadata.end() && !desc_it->second.empty()) {
        result.summary.intent = desc_it->second;
    }
    
    // Count produced semantic objects
    // For now, we count artifacts as bodies (will be refined when inspection is integrated)
    result.summary.body_count = 0;
    result.summary.artifact_count = 0;
    
    // Count artifacts from node outputs
    std::set<std::string> unique_artifacts;
    for (const auto& node : result.op_cache.node_status) {
        for (const auto& output : node.outputs) {
            unique_artifacts.insert(output.second);
            result.summary.artifact_count++;
        }
    }
    
    // For each artifact, assume it's a Body (semantic_type from metadata)
    result.summary.body_count = result.summary.artifact_count;
    
    // Build "produced" list with deterministic ordering per Sprint 10 spec
    // Order: Product → Body → Face → Edge → Artifact → Output
    // Within type: alphabetical
    std::vector<std::string> produced_items;
    
    if (result.summary.body_count > 0) {
        std::ostringstream oss;
        oss << result.summary.body_count << " Body";
        if (result.summary.body_count > 1) oss << "(s)";  // Stable pluralization
        produced_items.push_back(oss.str());
    }
    
    if (result.summary.artifact_count > 0) {
        std::ostringstream oss;
        oss << result.summary.artifact_count << " Previewable STEP Artifact";
        if (result.summary.artifact_count > 1) oss << "(s)";
        produced_items.push_back(oss.str());
        
        // Collect previewable outputs
        for (const auto& node : result.op_cache.node_status) {
            for (const auto& output : node.outputs) {
                // Check if output is STEP (previewable)
                std::filesystem::path p(output.second);
                std::string ext = p.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".step" || ext == ".stp") {
                    result.summary.previewable_outputs.push_back(output.first);  // output name
                }
            }
        }
        
        // Sort previewable outputs alphabetically for determinism
        std::sort(result.summary.previewable_outputs.begin(), 
                  result.summary.previewable_outputs.end());
    }
    
    result.summary.produced = produced_items;
}

} // namespace recipe
} // namespace praxis
