#include "praxis/MicroPlanHistory.hpp"
#include "praxis/PatchParser.hpp"
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <algorithm>

namespace praxis {
namespace plan {

std::string MicroPlanHistory::generate_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto now_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc;
    #ifdef _WIN32
    gmtime_s(&tm_utc, &now_t);
    #else
    gmtime_r(&now_t, &tm_utc);
    #endif
    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string MicroPlanHistory::compute_steps_hash(const std::vector<ParsedPlanStep>& steps) const {
    // Serialize steps to canonical JSON (sorted keys)
    nlohmann::json j = nlohmann::json::array();
    for (const auto& step : steps) {
        nlohmann::json sj;
        sj["op_type"] = step.op_type;
        nlohmann::json args = nlohmann::json::object();
        // Sort param keys for determinism
        std::vector<std::string> keys;
        for (const auto& kv : step.params) keys.push_back(kv.first);
        std::sort(keys.begin(), keys.end());
        for (const auto& k : keys) args[k] = step.params.at(k);
        sj["args"] = args;
        j.push_back(sj);
    }
    std::string canonical = j.dump();

    // Compute SHA256
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) { EVP_MD_CTX_free(ctx); return ""; }
    if (EVP_DigestUpdate(ctx, canonical.data(), canonical.size()) != 1) { EVP_MD_CTX_free(ctx); return ""; }
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) { EVP_MD_CTX_free(ctx); return ""; }
    EVP_MD_CTX_free(ctx);

    static const char* hex = "0123456789abcdef";
    std::string out; out.resize(digest_len * 2);
    for (unsigned int i = 0; i < digest_len; ++i) {
        out[2*i] = hex[(digest[i] >> 4) & 0xF];
        out[2*i+1] = hex[digest[i] & 0xF];
    }
    return out;
}

int MicroPlanHistory::create_initial(const std::string& plan_id, const ParsedPlan& plan, const std::string& reason) {
    MicroPlanVersion v1;
    v1.version = 1;
    v1.parent_version = -1;
    v1.steps = plan.steps;
    v1.reason = reason;
    v1.created_at = generate_timestamp();
    v1.steps_hash = compute_steps_hash(v1.steps);
    history_[plan_id].push_back(v1);
    return 1;
}

int MicroPlanHistory::apply_patch(const std::string& plan_id, const ParsedPatch& patch) {
    auto it = history_.find(plan_id);
    if (it == history_.end() || it->second.empty()) return -1;
    const auto& latest = it->second.back();
    if (patch.target_version != latest.version) return -1; // version mismatch

    // Apply operations
    auto steps = latest.steps;
    for (const auto& op : patch.operations) {
        if (op.op_type == "AddStep") {
            if (op.new_step.has_value()) steps.push_back(op.new_step.value());
        } else if (op.op_type == "ModifyStep") {
            if (op.step_index >= 0 && op.step_index < static_cast<int>(steps.size()) && op.new_step.has_value()) {
                steps[op.step_index] = op.new_step.value();
            }
        } else if (op.op_type == "RemoveStep") {
            if (op.step_index >= 0 && op.step_index < static_cast<int>(steps.size())) {
                steps.erase(steps.begin() + op.step_index);
            }
        }
    }

    MicroPlanVersion v_new;
    v_new.version = latest.version + 1;
    v_new.parent_version = latest.version;
    v_new.steps = steps;
    v_new.reason = patch.reason;
    v_new.created_at = generate_timestamp();
    v_new.steps_hash = compute_steps_hash(v_new.steps);
    history_[plan_id].push_back(v_new);
    return v_new.version;
}

std::vector<ParsedPlanStep> MicroPlanHistory::get_version_steps(const std::string& plan_id, int version) const {
    auto it = history_.find(plan_id);
    if (it == history_.end()) return {};
    for (const auto& v : it->second) {
        if (v.version == version) return v.steps;
    }
    return {};
}

int MicroPlanHistory::get_latest_version(const std::string& plan_id) const {
    auto it = history_.find(plan_id);
    if (it == history_.end() || it->second.empty()) return -1;
    return it->second.back().version;
}

bool MicroPlanHistory::write_ledger(const std::string& plan_id, const std::string& path) const {
    auto it = history_.find(plan_id);
    if (it == history_.end()) return false;
    
    nlohmann::json ledger;
    ledger["plan_id"] = plan_id;
    ledger["versions"] = nlohmann::json::array();
    
    for (const auto& v : it->second) {
        nlohmann::json vj;
        vj["version"] = v.version;
        vj["parent_version"] = v.parent_version;
        vj["reason"] = v.reason;
        vj["created_at"] = v.created_at;
        vj["steps_hash"] = v.steps_hash;
        vj["step_count"] = v.steps.size();
        ledger["versions"].push_back(vj);
    }
    
    try {
        std::ofstream file(path);
        if (!file) return false;
        file << ledger.dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace plan
} // namespace praxis
