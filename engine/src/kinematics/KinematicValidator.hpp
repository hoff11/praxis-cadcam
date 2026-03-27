#pragma once
#include "KinematicTypes.hpp"
#include <string>

namespace praxis {
namespace kinematics {

class KinematicValidator {
public:
    // Validate a PKM according to Sprint 2 rules
    static PKMValidationResult validate(const PKM& pkm);
    
private:
    // Individual validation checks
    static bool check_pkm_version(const PKM& pkm, std::vector<std::string>& errors);
    static bool check_duplicate_body_ids(const PKM& pkm, std::vector<std::string>& errors);
    static bool check_duplicate_joint_ids(const PKM& pkm, std::vector<std::string>& errors);
    static bool check_joint_body_references(const PKM& pkm, std::vector<std::string>& errors);
    static bool check_joint_self_reference(const PKM& pkm, std::vector<std::string>& errors);
    static bool check_axis_directions(const PKM& pkm, std::vector<std::string>& errors, std::vector<std::string>& warnings);
    static bool check_joint_limits(const PKM& pkm, std::vector<std::string>& errors);
    static bool check_joint_limits_sanity(const PKM& pkm, std::vector<std::string>& errors);
    static bool check_chain_references(const PKM& pkm, std::vector<std::string>& errors);
    static bool check_chain_duplicates(const PKM& pkm, std::vector<std::string>& errors);
    static bool check_chain_not_empty(const PKM& pkm, std::vector<std::string>& errors);
    static bool check_no_cycles(const PKM& pkm, std::vector<std::string>& errors);
};

} // namespace kinematics
} // namespace praxis
