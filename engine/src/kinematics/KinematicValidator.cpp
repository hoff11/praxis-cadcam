#include "KinematicValidator.hpp"
#include <set>
#include <map>
#include <queue>
#include <cmath>

namespace praxis {
namespace kinematics {

PKMValidationResult KinematicValidator::validate(const PKM& pkm) {
    PKMValidationResult result;
    result.valid = true;
    
    // Run all validation checks (each returns false if validation fails)
    result.valid &= check_pkm_version(pkm, result.errors);
    result.valid &= check_duplicate_body_ids(pkm, result.errors);
    result.valid &= check_duplicate_joint_ids(pkm, result.errors);
    result.valid &= check_joint_body_references(pkm, result.errors);
    result.valid &= check_joint_self_reference(pkm, result.errors);
    result.valid &= check_axis_directions(pkm, result.errors, result.warnings);
    result.valid &= check_joint_limits(pkm, result.errors);
    result.valid &= check_joint_limits_sanity(pkm, result.errors);
    result.valid &= check_chain_references(pkm, result.errors);
    result.valid &= check_chain_duplicates(pkm, result.errors);
    result.valid &= check_chain_not_empty(pkm, result.errors);
    result.valid &= check_no_cycles(pkm, result.errors);
    
    return result;
}

bool KinematicValidator::check_pkm_version(const PKM& pkm, std::vector<std::string>& errors) {
    if (pkm.pkm_version.empty()) {
        errors.push_back("PKM version is required");
        return false;
    }
    
    // Accept current stable versions while preserving strict rejection for unknown values.
    if (pkm.pkm_version != "0.1" && pkm.pkm_version != "0.2") {
        errors.push_back("Unsupported PKM version: " + pkm.pkm_version + " (expected 0.1 or 0.2)");
        return false;
    }
    
    return true;
}

bool KinematicValidator::check_duplicate_body_ids(const PKM& pkm, std::vector<std::string>& errors) {
    std::set<std::string> seen;
    for (const auto& body : pkm.bodies) {
        if (seen.count(body) > 0) {
            errors.push_back("Duplicate body id: " + body);
            return false;
        }
        seen.insert(body);
    }
    return true;
}

bool KinematicValidator::check_duplicate_joint_ids(const PKM& pkm, std::vector<std::string>& errors) {
    std::set<std::string> seen;
    for (const auto& joint : pkm.joints) {
        if (seen.count(joint.id) > 0) {
            errors.push_back("Duplicate joint id: " + joint.id);
            return false;
        }
        seen.insert(joint.id);
    }
    return true;
}

bool KinematicValidator::check_joint_body_references(const PKM& pkm, std::vector<std::string>& errors) {
    std::set<std::string> body_set(pkm.bodies.begin(), pkm.bodies.end());
    
    for (const auto& joint : pkm.joints) {
        if (body_set.count(joint.parent_body) == 0) {
            errors.push_back("Joint '" + joint.id + "' references unknown parent body: " + joint.parent_body);
            return false;
        }
        if (body_set.count(joint.child_body) == 0) {
            errors.push_back("Joint '" + joint.id + "' references unknown child body: " + joint.child_body);
            return false;
        }
    }
    return true;
}

bool KinematicValidator::check_joint_self_reference(const PKM& pkm, std::vector<std::string>& errors) {
    for (const auto& joint : pkm.joints) {
        if (joint.parent_body == joint.child_body) {
            errors.push_back("Joint '" + joint.id + "' has parent_body == child_body: " + joint.parent_body);
            return false;
        }
    }
    return true;
}

bool KinematicValidator::check_axis_directions(
    const PKM& pkm, 
    std::vector<std::string>& errors, 
    std::vector<std::string>& warnings
) {
    const double tolerance = 0.01;  // 1% tolerance for unit vector check
    
    for (const auto& joint : pkm.joints) {
        const Vec3& dir = joint.axis_direction;
        
        // Check for zero or non-finite values
        if (!std::isfinite(dir.x) || !std::isfinite(dir.y) || !std::isfinite(dir.z)) {
            errors.push_back("Joint '" + joint.id + "' has non-finite axis_direction");
            return false;
        }
        
        double mag = dir.magnitude();
        if (mag < 1e-10) {
            errors.push_back("Joint '" + joint.id + "' has zero-magnitude axis_direction");
            return false;
        }
        
        // Check if unit length (with tolerance)
        if (std::abs(mag - 1.0) > tolerance) {
            Vec3 normalized = dir.normalized();
            warnings.push_back("Joint '" + joint.id + "' axis_direction not unit length (mag=" + 
                             std::to_string(mag) + "), normalized to [" + 
                             std::to_string(normalized.x) + ", " + 
                             std::to_string(normalized.y) + ", " + 
                             std::to_string(normalized.z) + "]");
        }
    }
    return true;
}

bool KinematicValidator::check_joint_limits(const PKM& pkm, std::vector<std::string>& errors) {
    for (const auto& joint : pkm.joints) {
        if (!std::isfinite(joint.limits.min) || !std::isfinite(joint.limits.max)) {
            errors.push_back("Joint '" + joint.id + "' has non-finite limits");
            return false;
        }
        
        if (joint.limits.min > joint.limits.max) {
            errors.push_back("Joint '" + joint.id + "' has min > max: [" + 
                           std::to_string(joint.limits.min) + ", " + 
                           std::to_string(joint.limits.max) + "]");
            return false;
        }
    }
    return true;
}

bool KinematicValidator::check_joint_limits_sanity(const PKM& pkm, std::vector<std::string>& errors) {
    const double MAX_REVOLUTE_DEG = 720.0;   // ±2 full rotations max
    const double MAX_PRISMATIC_MM = 100000.0; // 100 meters max
    
    for (const auto& joint : pkm.joints) {
        if (joint.type == JointType::Revolute) {
            // Check revolute limits are reasonable (degrees)
            if (std::abs(joint.limits.min) > MAX_REVOLUTE_DEG || 
                std::abs(joint.limits.max) > MAX_REVOLUTE_DEG) {
                errors.push_back("Joint '" + joint.id + "' (revolute) has unreasonable limits: [" + 
                               std::to_string(joint.limits.min) + ", " + 
                               std::to_string(joint.limits.max) + "] (expected ±" + 
                               std::to_string(MAX_REVOLUTE_DEG) + "° max)");
                return false;
            }
        } else if (joint.type == JointType::Prismatic) {
            // Check prismatic limits are reasonable (mm)
            if (std::abs(joint.limits.min) > MAX_PRISMATIC_MM || 
                std::abs(joint.limits.max) > MAX_PRISMATIC_MM) {
                errors.push_back("Joint '" + joint.id + "' (prismatic) has unreasonable limits: [" + 
                               std::to_string(joint.limits.min) + ", " + 
                               std::to_string(joint.limits.max) + "] (expected ±" + 
                               std::to_string(MAX_PRISMATIC_MM) + "mm max)");
                return false;
            }
        }
    }
    return true;
}

bool KinematicValidator::check_chain_references(const PKM& pkm, std::vector<std::string>& errors) {
    std::set<std::string> joint_ids;
    for (const auto& joint : pkm.joints) {
        joint_ids.insert(joint.id);
    }
    
    for (const auto& joint_id : pkm.chain) {
        if (joint_ids.count(joint_id) == 0) {
            errors.push_back("Chain references unknown joint id: " + joint_id);
            return false;
        }
    }
    return true;
}

bool KinematicValidator::check_chain_duplicates(const PKM& pkm, std::vector<std::string>& errors) {
    std::set<std::string> seen;
    for (const auto& joint_id : pkm.chain) {
        if (seen.count(joint_id) > 0) {
            errors.push_back("Chain contains duplicate joint id: " + joint_id);
            return false;
        }
        seen.insert(joint_id);
    }
    return true;
}

bool KinematicValidator::check_chain_not_empty(const PKM& pkm, std::vector<std::string>& errors) {
    if (!pkm.joints.empty() && pkm.chain.empty()) {
        errors.push_back("Chain is empty but joints are declared");
        return false;
    }
    return true;
}

bool KinematicValidator::check_no_cycles(const PKM& pkm, std::vector<std::string>& errors) {
    // Build adjacency map: parent -> list of children
    std::map<std::string, std::vector<std::string>> children_map;
    std::map<std::string, int> in_degree;
    
    // Initialize in_degree for all bodies
    for (const auto& body : pkm.bodies) {
        in_degree[body] = 0;
    }
    
    // Build graph from joints
    for (const auto& joint : pkm.joints) {
        children_map[joint.parent_body].push_back(joint.child_body);
        in_degree[joint.child_body]++;
    }
    
    // Topological sort using Kahn's algorithm
    std::queue<std::string> queue;
    
    // Find all nodes with in-degree 0 (root nodes)
    for (const auto& kv : in_degree) {
        if (kv.second == 0) {
            queue.push(kv.first);
        }
    }
    
    int processed = 0;
    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();
        processed++;
        
        // Process all children
        if (children_map.count(current) > 0) {
            for (const auto& child : children_map[current]) {
                in_degree[child]--;
                if (in_degree[child] == 0) {
                    queue.push(child);
                }
            }
        }
    }
    
    // If we didn't process all nodes, there's a cycle
    if (processed != (int)pkm.bodies.size()) {
        errors.push_back("Kinematic chain contains a cycle (not a DAG)");
        return false;
    }
    
    return true;
}

} // namespace kinematics
} // namespace praxis
