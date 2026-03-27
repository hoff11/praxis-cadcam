#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include "praxis/PlanParser.hpp"
#include "praxis/MicroPlanHistory.hpp"
#include "praxis/Intent.hpp"
#include "praxis/IntentRouter.hpp"
#include "../kernel/KernelOps.hpp"
#include "../kernel/StepIO.hpp"
#include "../kernel/ShapeHandleInternal.hpp"
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace praxis {
namespace commands {

// Structured error emission for deterministic test assertions
static void emit_error_json(const std::string& code,
                            const std::string& message,
                            const std::string& where = "",
                            const nlohmann::json& detail = nlohmann::json::object()) {
    nlohmann::json j{
        {"code", code},
        {"message", message},
        {"where", where},
        {"detail", detail}
    };
    std::cerr << "PRAXIS_ERROR_JSON=" << j.dump() << "\n";
}

// Helper: merge two STEP files into a compound using kernel APIs
static bool merge_steps_into_compound(const std::string& a_step,
                                      const std::string& b_step,
                                      const std::string& out_step,
                                      std::string& error)
{
    using namespace praxis::kernel;

    auto ra = StepIO::read_step(a_step);
    if (!ra.success) {
        error = ra.error_message;
        return false;
    }

    auto rb = StepIO::read_step(b_step);
    if (!rb.success) {
        error = rb.error_message;
        return false;
    }

    auto comp = KernelOps::make_compound({ ra.shape, rb.shape });
    if (!comp.success) {
        error = comp.error_message;
        return false;
    }

    auto w = StepIO::write_step(comp.shape, out_step);
    if (!w.success) {
        error = w.error_message;
        return false;
    }

    return true;
}

// Execute a Plan JSON: runs supported steps sequentially, writing outputs under out_root
int handle_plan(const std::string& plan_path, const std::string& out_root, bool json_output) {
    try {
        // Read file
        std::ifstream file(plan_path);
        if (!file) {
            std::cerr << "Error: Unable to read plan file: " << plan_path << "\n";
            return 2;
        }
        std::ostringstream ss; ss << file.rdbuf();
        auto parsed = plan::parsePlanJson(ss.str());
        if (!parsed.has_value()) {
            emit_error_json("JsonParseError", "Failed to parse plan JSON or no steps found", "PlanCommand");
            std::cerr << "Error: Failed to parse plan JSON or no steps found\n";
            return 2;
        }

        const auto& plan = parsed.value();
        std::cout << "Executing plan (version=" << plan.version << ", units=" << plan.units << ")\n";

        // Initialize MicroPlanHistory v1
        plan::MicroPlanHistory history;
        std::string plan_id = plan.plan_id.empty() ? "unnamed-plan" : plan.plan_id;
        history.create_initial(plan_id, plan, "Initial plan execution");

        // Prepare output root
        fs::path outRoot(out_root);
        fs::create_directories(outRoot);

        struct StepSummary { std::string op_type; bool success; std::vector<std::string> artifacts; };
        std::vector<StepSummary> summaries;
        
        // Accumulated model state
        fs::path state_step = outRoot / "state.step";
        bool has_state = false;
        
        // Helper: find first STEP artifact
        auto find_step_artifact = [](const std::vector<std::string>& artifacts) -> std::string {
            for (const auto& a : artifacts) {
                if (a.length() >= 5 && 
                    (a.substr(a.length() - 5) == ".step" ||
                     a.substr(a.length() - 4) == ".stp"))
                    return a;
            }
            return {};
        };

        for (size_t i = 0; i < plan.steps.size(); ++i) {
            const auto& step = plan.steps[i];
            std::cout << "Step " << i << ": op_type=" << step.op_type << "\n";

            IntentRequest req;
            req.output_dir = (outRoot / ("step_" + std::to_string(i))).string();
            fs::create_directories(req.output_dir);

            // Map supported operations
            if (step.op_type == "CreateBox") {
                req.intent_name = "CreateBox";
                // Convert params to string map (already strings)
                for (const auto& kv : step.params) req.params[kv.first] = kv.second;
            } else if (step.op_type == "AttachFaceToFace") {
                req.intent_name = "AttachFaceToFace";
                // Pass all params as strings (including JSON-encoded FaceRefs)
                for (const auto& kv : step.params) req.params[kv.first] = kv.second;
                // AttachFaceToFace needs accumulated model state
                if (!has_state) {
                    emit_error_json("InvalidPlanOrder", 
                                   "AttachFaceToFace requires existing model state (cannot be first operation)",
                                   "AttachFaceToFace",
                                   {{"step_index", i}});
                    std::cerr << "Error: AttachFaceToFace requires existing model state\n";
                    summaries.push_back({ step.op_type, false, {} });
                    return 1;
                }
                req.input_step_path = state_step.string();
            } else {
                std::cerr << "Warning: Unsupported op_type: " << step.op_type << "; skipping\n";
                summaries.push_back({ step.op_type, false, {} });
                continue;
            }

            // Execute intent
            auto res = IntentRouter::execute(req);
            summaries.push_back({ step.op_type, res.success, res.artifacts });
            
            // Update accumulated state
            if (!res.success) {
                std::string error_code = res.error_code.empty() ? "IntentFailed" : res.error_code;
                emit_error_json(error_code,
                               res.errors.empty() ? "Intent execution failed" : res.errors.front(),
                               step.op_type,
                               {{"step_index", i}});
                std::cerr << "Step " << i << " failed; stopping execution\n";
                return 1;
            }
            
            std::string produced_step = find_step_artifact(res.artifacts);
            if (produced_step.empty())
                continue;
            
            if (!has_state) {
                // First geometry becomes the state
                fs::copy_file(produced_step, state_step,
                              fs::copy_options::overwrite_existing);
                has_state = true;
            } else if (step.op_type == "CreateBox") {
                // CreateBox appends into compound
                std::string err;
                fs::path tmp = outRoot / "state_tmp.step";
                
                if (!merge_steps_into_compound(state_step.string(),
                                               produced_step,
                                               tmp.string(),
                                               err))
                {
                    std::cerr << "Compound merge failed: " << err << "\n";
                    summaries.back().success = false;
                    continue;
                }
                
                fs::rename(tmp, state_step);
            } else {
                // Edit intents replace state
                fs::copy_file(produced_step, state_step,
                              fs::copy_options::overwrite_existing);
            }
        }

        // Write ledger
        history.write_ledger(plan_id, (outRoot / "history.json").string());

        // Output summary
        if (json_output) {
            std::cout << "{\n  \"steps\": [";
            for (size_t i = 0; i < summaries.size(); ++i) {
                const auto& s = summaries[i];
                std::cout << "\n    {\n      \"op_type\": \"" << s.op_type << "\",\n      \"success\": " << (s.success ? "true" : "false") << ",\n      \"artifacts\": [";
                for (size_t j = 0; j < s.artifacts.size(); ++j) {
                    std::cout << (j == 0 ? "\n        \"" : ",\n        \"") << s.artifacts[j] << "\"";
                }
                std::cout << (s.artifacts.empty() ? "]" : "\n      ]") << "\n    }" << (i + 1 < summaries.size() ? "," : "");
            }
            std::cout << (summaries.empty() ? "]\n}" : "\n  ]\n}\n");
        } else {
            std::cout << "\nPlan execution complete.\n";
        }

        // non-zero exit if any step failed
        bool any_fail = false; for (const auto& s : summaries) if (!s.success) { any_fail = true; break; }
        return any_fail ? 1 : 0;

    } catch (const std::exception& e) {
        std::cerr << "Error executing plan: " << e.what() << "\n";
        return 2;
    }
}

} // namespace commands
} // namespace praxis
