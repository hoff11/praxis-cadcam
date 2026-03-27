#include "KinematicTypes.hpp"
#include <cmath>

namespace praxis {
namespace kinematics {

double Vec3::magnitude() const {
    return std::sqrt(x*x + y*y + z*z);
}

Vec3 Vec3::normalized() const {
    double mag = magnitude();
    if (mag < 1e-10) {
        return Vec3(0, 0, 0);
    }
    return Vec3(x / mag, y / mag, z / mag);
}

std::string joint_type_to_string(JointType type) {
    switch (type) {
        case JointType::Revolute:
            return "revolute";
        case JointType::Prismatic:
            return "prismatic";
        default:
            return "unknown";
    }
}

JointType string_to_joint_type(const std::string& str) {
    if (str == "revolute") {
        return JointType::Revolute;
    } else if (str == "prismatic") {
        return JointType::Prismatic;
    }
    throw std::invalid_argument("Unknown joint type: " + str);
}

} // namespace kinematics
} // namespace praxis
