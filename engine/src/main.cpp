// Praxis-CAD — Intent Compiler
// Day 2: Intent router with CLI

#include "praxis/Intent.hpp"
#include "praxis/IntentRouter.hpp"
#include "praxis/Report.hpp"
#include "../src/cache/OpCache.hpp"
#include "praxis/ArtifactSchema.hpp"
#include <iostream>
#include <string>
#include <filesystem>
#include <iomanip>
#include <fstream>
#include <sstream>

// Forward declare command handlers
namespace praxis {
namespace commands {
    int handle_inspect(const std::string& artifact_path, bool json_output);
    // int handle_select(...);  // LEGACY: Disabled - needs C.5 migration
    int handle_resolve(const std::string& artifact_path, const std::string& reference_json, bool json_output, bool strict_mode, std::ostream& out = std::cout);
    int handle_plan(const std::string& plan_path, const std::string& out_root, bool json_output);
    int handle_patch(const std::string& plan_path, const std::string& patch_path, const std::string& out_root, bool json_output);
    int handle_parse_selector(const std::string& selector_expr, bool json_output);
    int handle_resolve_selector(const std::string& artifact_path, const std::string& selector_expr, bool json_output);
#if PRAXIS_ENABLE_ARTIFACT_COMMANDS
    int handle_validate_artifact(const std::string& artifact_type, const std::string& input_path, bool json_output);
    int handle_inspect_artifact(const std::string& artifact_type, const std::string& input_path, bool json_output);
    int handle_export_machine_definition(const std::string& input_path, const std::string& out_arg, bool json_output);
    int handle_export_capability_profile(const std::string& input_path, const std::string& out_arg, bool json_output);
    int handle_bundle_create(const std::string& input_dir, const std::string& out_arg, bool json_output);
#endif
}
}

void printUsage(bool is_cadcam = false) {
    if (is_cadcam) {
        std::cout << "Praxis-CADcam — Artifact CLI\n";
        std::cout << "============================\n\n";
        std::cout << "Artifact commands:\n";
        std::cout << "  export machine-definition --input <pkm.json> --out <path.json> [--json]\n";
        std::cout << "  export capability-profile --input <machine-def.json> --out <path.json> [--json]\n";
        std::cout << "  validate <type> <path> [--json]\n";
        std::cout << "  inspect  <type> <path> [--json]\n";
        std::cout << "  bundle create --input <dir> --out <manifest.json> [--json]\n\n";
        std::cout << "Artifact types: machine-definition | capability-profile | manifest\n\n";
    } else {
        std::cout << "Praxis-CAD — Intent Compiler\n";
        std::cout << "=============================\n\n";
    }
    std::cout << "Usage:\n";
    std::cout << "  Inspect artifact:\n";
    std::cout << "    ./praxis-cad inspect <artifact> --json\n\n";
    /*  LEGACY: select command disabled
    std::cout << "  Select geometry:\n";
    std::cout << "    ./praxis-cad select <artifact> <selector> --json [--include-selection]\n\n";
    */
    std::cout << "  Resolve reference:\n";
    std::cout << "    ./praxis-cad resolve <artifact> <reference_json_or_path> --json [--strict]\n\n";
    std::cout << "  Select geometry (compat alias to resolve-selector):\n";
    std::cout << "    ./praxis-cad select <artifact> <selector> --json [--include-selection]\n\n";
    std::cout << "  Version info:\n";
    std::cout << "    ./praxis-cad version\n\n";
    std::cout << "  Parse selector (Phase C.4):\n";
    std::cout << "    ./praxis-cad parse-selector <selector_expr> [--json]\n\n";
    std::cout << "  Resolve selector (Phase C.5):\n";
    std::cout << "    ./praxis-cad resolve-selector <artifact> <selector_expr> [--json]\n\n";
    std::cout << "  Execute plan (Phase A):\n";
    std::cout << "    ./praxis-cad plan <plan.json> <out_root> [--json]\n\n";
    std::cout << "  Apply patch (Phase B demo):\n";
    std::cout << "    ./praxis-cad patch <plan.json> <patch.json> <out_root> [--json]\n\n";
    std::cout << "  Generate machine:\n";
    std::cout << "    ./praxis-cad --intent GenerateMachine3AxisVMC --out <output_dir>\n";
    std::cout << "                  [--param travel_x=800] [--param travel_y=600] [--param travel_z=500]\n";
    std::cout << "                  [--param table_width=1000] [--param table_depth=600]\n";
    std::cout << "                  [--param fidelity=medium]\n\n";
    std::cout << "  Heal geometry:\n";
    std::cout << "    ./praxis-cad --intent HealAndNormalize --input <step_file> --out <output_dir>\n\n";
    std::cout << "  Cache control:\n";
    std::cout << "    --no-cache           Disable execution cache (force fresh execution)\n";
    std::cout << "    --clear-cache        Clear cache directory before execution\n";
    std::cout << "    --cache-stats        Show cache statistics and exit\n";
    std::cout << "    --prune-cache        Remove old cache entries\n";
    std::cout << "    --max-age-days N     Max age for prune (default: 30 days)\n\n";
    if (is_cadcam) {
        std::cout << "  (Engine/intent commands above also available in praxis-cadcam)\n\n";
    }
}

// Parse key=value from string like "travel_x=800"
static bool parseParamKV(const std::string& kv, std::string& key, std::string& value) {
    auto pos = kv.find('=');
    if (pos == std::string::npos || pos == 0 || pos == kv.size() - 1) {
        return false;
    }
    key = kv.substr(0, pos);
    value = kv.substr(pos + 1);
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::string program_name = std::filesystem::path(argv[0]).filename().string();
        bool is_cadcam = program_name.find("praxis-cadcam") != std::string::npos;
        printUsage(is_cadcam);
        return 1;
    }
    
    std::string cmd = argv[1];

    // Determine binary identity
    std::string program_name = std::filesystem::path(argv[0]).filename().string();
    bool is_cadcam = program_name.find("praxis-cadcam") != std::string::npos;

#if PRAXIS_ENABLE_ARTIFACT_COMMANDS
    // --- Artifact commands (praxis-cadcam only) ---
    if (cmd == "export" && argc >= 3) {
        std::string sub = argv[2];
        std::string input_path, out_arg;
        bool json_output = false;
        for (int i = 3; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--input" && i + 1 < argc) input_path = argv[++i];
            else if (a == "--out" && i + 1 < argc) out_arg = argv[++i];
            else if (a == "--json") json_output = true;
        }
        if (sub == "machine-definition") {
            return praxis::commands::handle_export_machine_definition(input_path, out_arg, json_output);
        } else if (sub == "capability-profile") {
            return praxis::commands::handle_export_capability_profile(input_path, out_arg, json_output);
        }
        std::cerr << "{\"error\":\"unknown_export_type\",\"message\":\"Unknown export type: " << sub << "\"}\n";
        return 1;
    }

    if (cmd == "validate" && argc >= 4) {
        bool json_output = false;
        for (int i = 4; i < argc; ++i) {
            if (std::string(argv[i]) == "--json") json_output = true;
        }
        return praxis::commands::handle_validate_artifact(argv[2], argv[3], json_output);
    }

    if (cmd == "bundle" && argc >= 3 && std::string(argv[2]) == "create") {
        std::string input_dir, out_arg;
        bool json_output = false;
        for (int i = 3; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--input" && i + 1 < argc) input_dir = argv[++i];
            else if (a == "--out" && i + 1 < argc) out_arg = argv[++i];
            else if (a == "--json") json_output = true;
        }
        return praxis::commands::handle_bundle_create(input_dir, out_arg, json_output);
    }

    // Typed inspect (praxis-cadcam): inspect <type> <path>
    // Disambiguate from legacy inspect <path>: type is one of the known artifact type strings
    if (cmd == "inspect" && argc >= 4 && is_cadcam) {
        std::string maybe_type = argv[2];
        if (maybe_type == "machine-definition" || maybe_type == "capability-profile" || maybe_type == "manifest") {
            bool json_output = false;
            for (int i = 4; i < argc; ++i) {
                if (std::string(argv[i]) == "--json") json_output = true;
            }
            return praxis::commands::handle_inspect_artifact(maybe_type, argv[3], json_output);
        }
    }
#else
    if (cmd == "export" ||
        (cmd == "validate" && argc >= 4) ||
        (cmd == "bundle" && argc >= 3 && std::string(argv[2]) == "create") ||
        (cmd == "inspect" && argc >= 4 &&
         (std::string(argv[2]) == "machine-definition" ||
          std::string(argv[2]) == "capability-profile" ||
          std::string(argv[2]) == "manifest"))) {
        std::cerr << "{\"error\":\"artifact_commands_unavailable\",\"message\":\"Artifact commands require the praxis-cadcam binary\"}\n";
        return 1;
    }
#endif

    // --- Engine commands (both binaries) ---
    if (cmd == "version") {
        std::cout << "{\n";
        std::cout << "  \"cli_version\": \"" << PRAXIS_ENGINE_VERSION << "\",\n";
        std::cout << "  \"contract_version\": \"1.0\",\n";
        std::cout << "  \"selector_contract_version\": \"1.0\"\n";
        std::cout << "}\n";
        return 0;
    }

    if (cmd == "inspect" && argc >= 3) {
        bool json_output = false;
        for (int i = 3; i < argc; ++i) {
            if (std::string(argv[i]) == "--json") json_output = true;
        }
        return praxis::commands::handle_inspect(argv[2], json_output);
    } else if (cmd == "select" && argc >= 4) {
        bool json_output = false;
        for (int i = 4; i < argc; ++i) {
            if (std::string(argv[i]) == "--json") json_output = true;
        }
        // Legacy TS CLI used `select`; map to stable C.5 resolver.
        return praxis::commands::handle_resolve_selector(argv[2], argv[3], json_output);
    /* LEGACY: select command disabled - uses old SelectorExecutor, needs C.5 migration
    } else if (cmd == "select" && argc >= 4) {
        bool json_output = false;
        bool include_selection = false;
        for (int i = 4; i < argc; ++i) {
            if (std::string(argv[i]) == "--json") json_output = true;
            if (std::string(argv[i]) == "--include-selection") include_selection = true;
        }
        return praxis::commands::handle_select(argv[2], argv[3], json_output, include_selection);
    */
    } else if (cmd == "resolve" && argc >= 4) {
        bool json_output = false;
        bool strict_mode = false;
        for (int i = 4; i < argc; ++i) {
            if (std::string(argv[i]) == "--json") json_output = true;
            if (std::string(argv[i]) == "--strict") strict_mode = true;
        }

        std::string reference_input = argv[3];
        std::filesystem::path maybe_ref_path(reference_input);
        if (std::filesystem::exists(maybe_ref_path) && std::filesystem::is_regular_file(maybe_ref_path)) {
            std::ifstream ref_file(maybe_ref_path);
            if (!ref_file) {
                std::cerr << "Error: Failed to read reference file: " << reference_input << "\n";
                return 4;
            }
            std::ostringstream ref_buf;
            ref_buf << ref_file.rdbuf();
            reference_input = ref_buf.str();
        }

        return praxis::commands::handle_resolve(argv[2], reference_input, json_output, strict_mode);
    } else if (cmd == "parse-selector" && argc >= 3) {
        bool json_output = false;
        for (int i = 3; i < argc; ++i) {
            if (std::string(argv[i]) == "--json") json_output = true;
        }
        return praxis::commands::handle_parse_selector(argv[2], json_output);
    } else if (cmd == "resolve-selector" && argc >= 4) {
        bool json_output = false;
        for (int i = 4; i < argc; ++i) {
            if (std::string(argv[i]) == "--json") json_output = true;
        }
        return praxis::commands::handle_resolve_selector(argv[2], argv[3], json_output);
    } else if (cmd == "plan" && argc >= 4) {
        bool json_output = false;
        for (int i = 4; i < argc; ++i) {
            if (std::string(argv[i]) == "--json") json_output = true;
        }
        return praxis::commands::handle_plan(argv[2], argv[3], json_output);
    } else if (cmd == "patch" && argc >= 5) {
        bool json_output = false;
        for (int i = 5; i < argc; ++i) {
            if (std::string(argv[i]) == "--json") json_output = true;
        }
        return praxis::commands::handle_patch(argv[2], argv[3], argv[4], json_output);
    }
    
    // Parse command-line arguments for intent execution
    praxis::IntentRequest request;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--intent" && i + 1 < argc) {
            request.intent_name = argv[++i];
        } else if (arg == "--input" && i + 1 < argc) {
            request.input_step_path = argv[++i];
        } else if (arg == "--out" && i + 1 < argc) {
            request.output_dir = argv[++i];
        } else if (arg == "--no-cache") {
            request.no_cache = true;
        } else if (arg == "--clear-cache") {
            request.clear_cache = true;
        } else if (arg == "--cache-stats") {
            request.cache_stats = true;
        } else if (arg == "--prune-cache") {
            request.prune_cache = true;
        } else if (arg == "--max-age-days" && i + 1 < argc) {
            request.max_age_days = std::stoi(argv[++i]);
        } else if (arg == "--param") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --param requires key=value argument\n";
                return 2;
            }
            std::string kv = argv[++i];
            std::string key, value;
            if (!parseParamKV(kv, key, value)) {
                std::cerr << "Error: --param must be key=value, got: " << kv << "\n";
                return 2;
            }
            request.params[key] = value;
        }
    }
    
    // Sprint 6 Phase 4: Handle cache management commands before validation
    if (request.cache_stats || request.prune_cache) {
        praxis::cache::OpCache cache(std::filesystem::current_path() / "cache");
        
        if (request.cache_stats) {
            std::cout << "Cache Statistics\n";
            std::cout << "================\n\n";
            
            auto stats = cache.get_stats();
            std::cout << "Cache root:    " << stats.cache_root << "\n";
            std::cout << "Schema version: " << stats.schema_version << "\n";
            std::cout << "Total entries:  " << stats.entry_count << "\n";
            std::cout << "Total size:     " << std::fixed << std::setprecision(2) 
                     << (stats.total_bytes / 1024.0 / 1024.0) << " MB\n";
            
            if (stats.entry_count > 0) {
                double avg_size = static_cast<double>(stats.total_bytes) / stats.entry_count;
                std::cout << "Avg entry size: " << std::fixed << std::setprecision(2)
                         << (avg_size / 1024.0 / 1024.0) << " MB\n";
            }
            
            return 0;
        }
        
        if (request.prune_cache) {
            std::cout << "Pruning cache (max age: " << request.max_age_days << " days)\n";
            std::cout << "======================================\n\n";
            
            auto prune_stats = cache.prune_by_age(request.max_age_days);
            std::cout << "Entries removed: " << prune_stats.entries_removed << "\n";
            std::cout << "Space freed:     " << std::fixed << std::setprecision(2)
                     << (prune_stats.bytes_freed / 1024.0 / 1024.0) << " MB\n";
            
            // Also cleanup temp dirs
            cache.cleanup_temp_dirs(24);
            
            return 0;
        }
    }
    
    // Validate required arguments
    if (request.intent_name.empty()) {
        std::cerr << "Error: --intent is required\n";
        printUsage(is_cadcam);
        return 1;
    }
    
    if (request.output_dir.empty()) {
        std::cerr << "Error: --out is required\n";
        printUsage(is_cadcam);
        return 1;
    }
    
    // Convert relative paths to absolute
    try {
        std::filesystem::path outPath(request.output_dir);
        if (!outPath.is_absolute()) {
            outPath = std::filesystem::absolute(outPath);
        }
        request.output_dir = outPath.string();
        
        if (!request.input_step_path.empty()) {
            std::filesystem::path inPath(request.input_step_path);
            if (!inPath.is_absolute()) {
                inPath = std::filesystem::absolute(inPath);
            }
            request.input_step_path = inPath.string();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error resolving paths: " << e.what() << "\n";
        return 1;
    }
    
    std::cout << "Praxis-CAD — Intent Compiler\n";
    std::cout << "=============================\n\n";
    
    // Execute intent
    praxis::IntentResult result = praxis::IntentRouter::execute(request);
    
    // Write report
    std::string reportPath = request.output_dir + "/report.json";
    if (praxis::Report::writeReport(request, result, reportPath)) {
        std::cout << "\n✓ Report written: " << reportPath << "\n";
        result.report_path = reportPath;
    } else {
        std::cerr << "\n⚠ Failed to write report\n";
    }
    
    // Print summary
    std::cout << "\n";
    std::cout << "Results:\n";
    std::cout << "--------\n";
    std::cout << "Success: " << (result.success ? "true" : "false") << "\n";
    std::cout << "Confidence: " << result.confidence << "\n";
    std::cout << "Artifacts: " << result.artifacts.size() << "\n";
    for (const auto& artifact : result.artifacts) {
        std::cout << "  - " << artifact << "\n";
    }
    
    if (!result.errors.empty()) {
        std::cout << "\nErrors:\n";
        for (const auto& error : result.errors) {
            std::cout << "  ✗ " << error << "\n";
        }
    }
    
    if (!result.warnings.empty()) {
        std::cout << "\nWarnings:\n";
        for (const auto& warning : result.warnings) {
            // Check if warning already has a prefix (starts with emoji)
            // Reasoning notes include their own prefix, other warnings don't
            bool has_prefix = !warning.empty() && 
                             (static_cast<unsigned char>(warning[0]) >= 0x80); // UTF-8 multi-byte char
            if (has_prefix) {
                std::cout << "  " << warning << "\n";
            } else {
                std::cout << "  ⚠ " << warning << "\n";
            }
        }
    }
    
    std::cout << "\n";
    
    return result.success ? 0 : 1;
}
