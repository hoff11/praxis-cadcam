#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include "praxis/PlanParser.hpp"
#include "praxis/PatchParser.hpp"
#include "praxis/MicroPlanHistory.hpp"
#include "praxis/Intent.hpp"
#include "praxis/IntentRouter.hpp"

namespace fs = std::filesystem;

namespace praxis {
namespace commands {

// Apply a PlanPatch to a Plan JSON and execute resulting steps
int handle_patch(const std::string& plan_path, const std::string& patch_path, const std::string& out_root, bool json_output) {
    try {
        // Read plan
        std::ifstream pfile(plan_path);
        if (!pfile) { std::cerr << "Error: Unable to read plan: " << plan_path << "\n"; return 2; }
        std::ostringstream pss; pss << pfile.rdbuf();
        auto plan_opt = plan::parsePlanJson(pss.str());
        if (!plan_opt.has_value()) { std::cerr << "Error: Failed to parse plan JSON\n"; return 2; }
        auto plan_parsed = plan_opt.value();

        // Read patch
        std::ifstream xfile(patch_path);
        if (!xfile) { std::cerr << "Error: Unable to read patch: " << patch_path << "\n"; return 2; }
        std::ostringstream xss; xss << xfile.rdbuf();
        auto patch_opt = plan::parsePatchJson(xss.str());
        if (!patch_opt.has_value()) { std::cerr << "Error: Failed to parse patch JSON\n"; return 2; }
        auto patch = patch_opt.value();

        // Initialize history v1 from plan, apply patch to create v2
        plan::MicroPlanHistory history;
        std::string plan_id = plan_parsed.plan_id.empty() ? "unnamed-plan" : plan_parsed.plan_id;
        history.create_initial(plan_id, plan_parsed, "Initial plan for patch");
        int new_version = history.apply_patch(plan_id, patch);
        if (new_version < 0) { std::cerr << "Error: Failed to apply patch (version mismatch or invalid ops)\n"; return 2; }

        // Get resulting steps from new version
        auto steps = history.get_version_steps(plan_id, new_version);

        // Execute resulting steps
        fs::path outRoot(out_root);
        fs::create_directories(outRoot);
        struct StepSummary { std::string op_type; bool success; std::vector<std::string> artifacts; };
        std::vector<StepSummary> summaries;

        for (size_t i = 0; i < steps.size(); ++i) {
            const auto& step = steps[i];
            IntentRequest req; req.output_dir = (outRoot / ("step_" + std::to_string(i))).string();
            fs::create_directories(req.output_dir);

            if (step.op_type == "CreateBox") {
                req.intent_name = "CreateBox";
                for (const auto& kv : step.params) req.params[kv.first] = kv.second;
            } else {
                summaries.push_back({ step.op_type, false, {} });
                continue;
            }

            auto res = IntentRouter::execute(req);
            summaries.push_back({ step.op_type, res.success, res.artifacts });
        }

        // Write ledger
        history.write_ledger(plan_id, (outRoot / "history.json").string());

        if (json_output) {
            std::cout << "{\n  \"patch_applied\": true,\n  \"steps\": [";
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
            std::cout << "\nPatch applied and execution complete.\n";
        }

        bool any_fail = false; for (const auto& s : summaries) if (!s.success) { any_fail = true; break; }
        return any_fail ? 1 : 0;

    } catch (const std::exception& e) {
        std::cerr << "Error applying patch: " << e.what() << "\n";
        return 2;
    }
}

} // namespace commands
} // namespace praxis
