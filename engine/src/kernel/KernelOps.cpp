#include "KernelOps.hpp"
#include "ShapeHandleInternal.hpp"
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <ShapeFix_Shape.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace praxis {
namespace kernel {

// Static member initialization
std::map<std::string, int> KernelOps::op_counters;

void KernelOps::increment_op(const std::string& op_name) {
    op_counters[op_name]++;
}

void KernelOps::reset_metrics() {
    op_counters.clear();
}

std::map<std::string, int> KernelOps::get_metrics() {
    return op_counters;
}

KernelOpResult KernelOps::make_box(double dx, double dy, double dz) {
    KernelOpResult result;
    
    try {
        if (dx <= 0 || dy <= 0 || dz <= 0) {
            result.success = false;
            result.error_message = "Box dimensions must be positive";
            return result;
        }
        
        result.shape = ShapeHandle();
        set_shape_internal(result.shape, BRepPrimAPI_MakeBox(dx, dy, dz).Shape());
        increment_op("make_box");
        result.operations.push_back("make_box(" + 
            std::to_string(dx) + ", " + 
            std::to_string(dy) + ", " + 
            std::to_string(dz) + ")");
        result.metrics["dx"] = std::to_string(dx);
        result.metrics["dy"] = std::to_string(dy);
        result.metrics["dz"] = std::to_string(dz);
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("make_box failed: ") + e.what();
    }
    
    return result;
}

KernelOpResult KernelOps::transform(
    const ShapeHandle& shape,
    double tx, double ty, double tz,
    double rx, double ry, double rz
) {
    KernelOpResult result;
    
    try {
        if (shape.is_null()) {
            result.success = false;
            result.error_message = "Cannot transform null shape";
            return result;
        }
        
        gp_Trsf transform;
        
        // Apply translation
        if (tx != 0.0 || ty != 0.0 || tz != 0.0) {
            transform.SetTranslation(gp_Vec(tx, ty, tz));
        }
        
        // Apply rotations if needed (convert degrees to radians)
        // For v0, we'll apply rotations in X-Y-Z order
        if (rx != 0.0 || ry != 0.0 || rz != 0.0) {
            gp_Trsf rot;
            // This is simplified - full rotation composition would be more complex
            if (rz != 0.0) {
                rot.SetRotation(gp::OZ(), rz * M_PI / 180.0);
                transform = transform * rot;
            }
            if (ry != 0.0) {
                rot.SetRotation(gp::OY(), ry * M_PI / 180.0);
                transform = transform * rot;
            }
            if (rx != 0.0) {
                rot.SetRotation(gp::OX(), rx * M_PI / 180.0);
                transform = transform * rot;
            }
        }
        
        result.shape = ShapeHandle();
        set_shape_internal(result.shape, BRepBuilderAPI_Transform(get_shape_internal(shape), transform, true).Shape());
        increment_op("transform");
        result.operations.push_back("transform(t=[" + 
            std::to_string(tx) + "," + std::to_string(ty) + "," + std::to_string(tz) + 
            "], r=[" + std::to_string(rx) + "," + std::to_string(ry) + "," + std::to_string(rz) + "])");
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("transform failed: ") + e.what();
    }
    
    return result;
}

KernelOpResult KernelOps::make_compound(const std::vector<ShapeHandle>& shapes) {
    KernelOpResult result;
    
    try {
        if (shapes.empty()) {
            result.success = false;
            result.error_message = "Cannot make compound from empty shape list";
            return result;
        }
        
        TopoDS_Compound compound;
        BRep_Builder builder;
        builder.MakeCompound(compound);
        
        int valid_count = 0;
        for (const auto& shape : shapes) {
            if (!shape.is_null()) {
                builder.Add(compound, get_shape_internal(shape));
                valid_count++;
            }
        }
        
        if (valid_count == 0) {
            result.success = false;
            result.error_message = "All shapes were null, cannot create compound";
            return result;
        }
        
        result.shape = ShapeHandle();
        set_shape_internal(result.shape, compound);
        increment_op("make_compound");
        result.operations.push_back("make_compound(count=" + std::to_string(valid_count) + ")");
        result.metrics["shape_count"] = std::to_string(valid_count);
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("make_compound failed: ") + e.what();
    }
    
    return result;
}

bool KernelOps::validate(const ShapeHandle& shape) {
    if (shape.is_null()) {
        return false;
    }
    
    try {
        BRepCheck_Analyzer analyzer(get_shape_internal(shape));
        bool is_valid = analyzer.IsValid();
        increment_op("validate");
        return is_valid;
        
    } catch (const std::exception&) {
        return false;
    }
}

KernelOpResult KernelOps::heal(const ShapeHandle& shape) {
    KernelOpResult result;
    
    try {
        if (shape.is_null()) {
            result.success = false;
            result.error_message = "Cannot heal null shape";
            return result;
        }
        
        Handle(ShapeFix_Shape) shape_fixer = new ShapeFix_Shape(get_shape_internal(shape));
        shape_fixer->Perform();
        result.shape = ShapeHandle();
        set_shape_internal(result.shape, shape_fixer->Shape());
        
        bool was_valid = validate(shape);
        bool is_valid = validate(result.shape);
        
        increment_op("heal");
        result.operations.push_back("heal(before=" + 
            std::string(was_valid ? "valid" : "invalid") + 
            ", after=" + std::string(is_valid ? "valid" : "invalid") + ")");
        result.metrics["valid_before"] = was_valid ? "true" : "false";
        result.metrics["valid_after"] = is_valid ? "true" : "false";
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("heal failed: ") + e.what();
    }
    
    return result;
}

BoundingBox KernelOps::get_bounding_box(const ShapeHandle& shape) {
    BoundingBox bbox = {0, 0, 0, 0, 0, 0};
    
    if (shape.is_null()) {
        return bbox;
    }
    
    try {
        Bnd_Box bnd_box;
        BRepBndLib::Add(get_shape_internal(shape), bnd_box);
        
        if (!bnd_box.IsVoid()) {
            bnd_box.Get(bbox.xmin, bbox.ymin, bbox.zmin, bbox.xmax, bbox.ymax, bbox.zmax);
        }
        
        increment_op("get_bounding_box");
        
    } catch (const std::exception&) {
        // Return zero bbox on error
    }
    
    return bbox;
}

} // namespace kernel
} // namespace praxis
