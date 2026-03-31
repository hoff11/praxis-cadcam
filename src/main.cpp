#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

#if defined(_WIN32)
#include <cstdio>
#define popen _popen
#define pclose _pclose
#endif

namespace fs = std::filesystem;

static std::string quote_arg(const std::string& arg) {
    std::string out = "\"";
    for (char c : arg) {
        if (c == '\"') {
            out += "\\\"";
        } else {
            out += c;
        }
    }
    out += "\"";
    return out;
}

static std::string find_engine_bin() {
    if (const char* env = std::getenv("PRAXIS_ENGINE_BIN")) {
        if (fs::exists(env)) {
            return std::string(env);
        }
    }

#if defined(_WIN32)
    const std::vector<std::string> candidates = {
        "C:/dev/praxis-cadcam/engine/build/praxis-cadcam.exe",
        "C:/dev/praxis-cadcam/engine/build/Release/praxis-cadcam.exe",
        "C:/dev/praxis-cadcam/engine/build/Debug/praxis-cadcam.exe",
        "C:/dev/praxis-cadcam/engine/build/praxis-cad.exe",
        "C:/dev/praxis-cadcam/engine/build/Release/praxis-cad.exe",
        "C:/dev/praxis-cadcam/engine/build/Debug/praxis-cad.exe"
    };
#else
    const std::vector<std::string> candidates = {
        "./praxis-cadcam/engine/build/praxis-cadcam",
        "./engine/build/praxis-cadcam",
        "/mnt/c/dev/praxis-cadcam/engine/build/praxis-cadcam",
        "../praxis-cadcam/engine/build/praxis-cadcam",
        "./praxis-cadcam/engine/build/praxis-cad",
        "./engine/build/praxis-cad",
        "/mnt/c/dev/praxis-cadcam/engine/build/praxis-cad",
        "../praxis-cadcam/engine/build/praxis-cad"
    };
#endif

    for (const auto& c : candidates) {
        if (fs::exists(c)) {
            return c;
        }
    }

#if defined(_WIN32)
    return "praxis-cadcam.exe";
#else
    return "praxis-cadcam";
#endif
}

struct RunResult {
    int exit_code = 1;
    std::string stdout_text;
};

static RunResult run_capture_stdout(const std::string& bin, const std::vector<std::string>& args) {
    std::ostringstream cmd;
    cmd << quote_arg(bin);
    for (const auto& a : args) {
        cmd << " " << quote_arg(a);
    }

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        return {1, ""};
    }

    std::string output;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

    int code = pclose(pipe);
#if defined(_WIN32)
    return {code, output};
#else
    if (WIFEXITED(code)) {
        return {WEXITSTATUS(code), output};
    }
    return {1, output};
#endif
}

static void print_usage() {
    std::cout << "praxis-cadcam-cli\n";
    std::cout << "Usage:\n";
    std::cout << "  praxis-cadcam-cli inspect <artifact> [-o output.json]\n";
    std::cout << "  praxis-cadcam-cli select <artifact> <selector> [-o output.json]\n";
    std::cout << "  praxis-cadcam-cli resolve <artifact> <refs_or_json> [--strict] [-o output.json]\n";
    std::cout << "  praxis-cadcam-cli plan <plan.json> <out_root> [-o output.json]\n";
    std::cout << "  praxis-cadcam-cli patch <plan.json> <patch.json> <out_root> [-o output.json]\n";
    std::cout << "Artifact commands:\n";
    std::cout << "  praxis-cadcam-cli export machine-definition --input <pkm.json> --out <machine-definition.json> [--json]\n";
    std::cout << "  praxis-cadcam-cli export capability-profile --input <machine-definition.json> --out <capability-profile.json> [--json]\n";
    std::cout << "  praxis-cadcam-cli validate <machine-definition|capability-profile|manifest> <path> [--json]\n";
    std::cout << "  praxis-cadcam-cli bundle create --input <artifact_dir> --out <manifest.json> [--json]\n";
    std::cout << "  praxis-cadcam-cli version\n";
}

static int run_passthrough(const std::string& cmd, const std::vector<std::string>& forward, const std::string& output_path, bool force_json) {
    auto bin = find_engine_bin();
    std::vector<std::string> args;
    if (!cmd.empty()) {
        args.push_back(cmd);
    }
    bool has_json = false;
    for (const auto& a : forward) {
        if (a == "--json") {
            has_json = true;
        }
        args.push_back(a);
    }
    if (force_json && !has_json) {
        args.push_back("--json");
    }

    auto res = run_capture_stdout(bin, args);
    if (!output_path.empty()) {
        std::ofstream out(output_path);
        if (!out) {
            std::cerr << "Failed to write output: " << output_path << "\n";
            return 2;
        }
        out << res.stdout_text;
    } else {
        std::cout << res.stdout_text;
    }
    return res.exit_code;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string cmd = argv[1];
    std::vector<std::string> forward;
    std::string output_path;

    // Parse passthrough args and strip -o/--output for local handling.
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "-o" || a == "--output") && (i + 1) < argc) {
            output_path = argv[++i];
            continue;
        }
        forward.push_back(a);
    }

    if (cmd == "version") {
        auto bin = find_engine_bin();
        auto res = run_capture_stdout(bin, {"version"});
        std::cout << res.stdout_text;
        return res.exit_code;
    }

    if (cmd == "inspect") {
        if (forward.size() < 1) {
            std::cerr << "inspect requires <artifact>\n";
            return 2;
        }
        return run_passthrough("inspect", {forward[0]}, output_path, true);
    }

    if (cmd == "select") {
        if (forward.size() < 2) {
            std::cerr << "select requires <artifact> <selector>\n";
            return 2;
        }
        return run_passthrough("select", {forward[0], forward[1]}, output_path, true);
    }

    if (cmd == "resolve") {
        if (forward.size() < 2) {
            std::cerr << "resolve requires <artifact> <refs_or_json>\n";
            return 2;
        }
        return run_passthrough("resolve", forward, output_path, true);
    }

    if (cmd == "plan") {
        if (forward.size() < 2) {
            std::cerr << "plan requires <plan.json> <out_root>\n";
            return 2;
        }
        return run_passthrough("plan", forward, output_path, true);
    }

    if (cmd == "patch") {
        if (forward.size() < 3) {
            std::cerr << "patch requires <plan.json> <patch.json> <out_root>\n";
            return 2;
        }
        return run_passthrough("patch", forward, output_path, true);
    }

    if (cmd == "export") {
        if (forward.size() < 1) {
            std::cerr << "export requires <machine-definition|capability-profile> ...\n";
            return 2;
        }
        return run_passthrough("export", forward, output_path, false);
    }

    if (cmd == "validate") {
        if (forward.size() < 2) {
            std::cerr << "validate requires <artifact-type> <path>\n";
            return 2;
        }
        return run_passthrough("validate", forward, output_path, false);
    }

    if (cmd == "bundle") {
        if (forward.size() < 1 || forward[0] != "create") {
            std::cerr << "bundle currently supports only 'bundle create ...'\n";
            return 2;
        }
        return run_passthrough("bundle", forward, output_path, false);
    }

    if (cmd == "--intent") {
        std::vector<std::string> intent_args = {cmd};
        for (const auto& a : forward) {
            intent_args.push_back(a);
        }
        return run_passthrough("", intent_args, output_path, false);
    }

    if (cmd.rfind("--", 0) == 0) {
        std::vector<std::string> passthrough = {cmd};
        for (const auto& a : forward) {
            passthrough.push_back(a);
        }
        return run_passthrough("", passthrough, output_path, false);
    }

    std::cerr << "Unknown command: " << cmd << "\n";
    print_usage();
    return 2;
}
