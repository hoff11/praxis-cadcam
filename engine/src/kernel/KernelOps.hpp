#pragma once
#include "ShapeHandle.hpp"
#include <vector>
#include <string>
#include <map>

namespace praxis {
namespace kernel {

// Result structure for kernel operations
struct KernelOpResult {
    bool success = true;
    ShapeHandle shape;
    std::string error_message;
    std::vector<std::string> operations;  // operation trace
    std::map<std::string, std::string> metrics;  // operation-specific metrics
};

// Bounding box result
struct BoundingBox {
    double xmin, ymin, zmin;
    double xmax, ymax, zmax;
    
    double dx() const { return xmax - xmin; }
    double dy() const { return ymax - ymin; }
    double dz() const { return zmax - zmin; }
};

// Layer 1 Kernel Operations - stable API surface
class KernelOps {
public:
    // Primitive creation
    static KernelOpResult make_box(double dx, double dy, double dz);
    
    // Transformation
    static KernelOpResult transform(
        const ShapeHandle& shape,
        double tx, double ty, double tz,  // translation
        double rx = 0.0, double ry = 0.0, double rz = 0.0  // rotation (degrees)
    );
    
    // Assembly
    static KernelOpResult make_compound(const std::vector<ShapeHandle>& shapes);
    
    // Validation
    static bool validate(const ShapeHandle& shape);
    
    // Healing
    static KernelOpResult heal(const ShapeHandle& shape);
    
    // Geometry queries
    static BoundingBox get_bounding_box(const ShapeHandle& shape);
    
    // Counters for metrics tracking
    static void reset_metrics();
    static std::map<std::string, int> get_metrics();
    
private:
    static std::map<std::string, int> op_counters;
    static void increment_op(const std::string& op_name);
};

} // namespace kernel
} // namespace praxis
