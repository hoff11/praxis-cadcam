#pragma once
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <stdexcept>

namespace praxis {
namespace kinematics {

// 3D vector for positions and directions
struct Vec3 {
    double x, y, z;
    
    Vec3() : x(0), y(0), z(0) {}
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
    
    double magnitude() const;
    Vec3 normalized() const;
};

// Joint types
enum class JointType {
    Revolute,    // Rotational joint (degrees)
    Prismatic    // Linear joint (mm)
};

// Joint limits
struct JointLimits {
    double min;
    double max;
    
    bool is_valid() const {
        return std::isfinite(min) && std::isfinite(max) && min <= max;
    }
};

// Named reference frame (optional in v0)
struct Frame {
    std::string name;
    Vec3 origin;
    Vec3 x_axis;
    Vec3 y_axis;
    Vec3 z_axis;
};

// Joint definition
struct Joint {
    std::string id;              // Unique joint identifier
    JointType type;              // revolute or prismatic
    std::string parent_body;     // Parent body name
    std::string child_body;      // Child body name
    Vec3 axis_origin;            // Joint origin in mm
    Vec3 axis_direction;         // Joint axis direction (should be unit vector)
    JointLimits limits;          // Joint limits (deg for revolute, mm for prismatic)
};

// Praxis Kinematic Manifest (PKM)
struct PKM {
    std::string pkm_version;     // PKM format version (e.g., "0.1")
    std::string machine;         // Machine identifier
    std::string units;           // "mm" only for v0
    std::vector<std::string> bodies;    // Unique body identifiers
    std::vector<Joint> joints;          // Joint definitions
    std::vector<std::string> chain;     // Ordered joint IDs defining kinematic chain
    std::vector<Frame> frames;          // Optional named frames (v0 optional)
    
    // Metadata (optional)
    std::map<std::string, std::string> metadata;
};

// Result of PKM loading
struct PKMLoadResult {
    bool success = true;
    PKM pkm;
    std::string error_message;
    std::vector<std::string> warnings;
};

// Result of PKM validation
struct PKMValidationResult {
    bool valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// Helper functions
std::string joint_type_to_string(JointType type);
JointType string_to_joint_type(const std::string& str);

} // namespace kinematics
} // namespace praxis
