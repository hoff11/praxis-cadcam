#pragma once
#include "ShapeHandle.hpp"
#include <TopoDS_Shape.hxx>

// Internal Layer 1 accessor - do NOT include from Layer 2
namespace praxis {
namespace kernel {

// Define Impl here so Layer 1 can access it
struct ShapeHandle::Impl {
    TopoDS_Shape shape;
};

// Forward declare internal accessor functions (Layer 1 only)
TopoDS_Shape& get_shape_internal(ShapeHandle&);
const TopoDS_Shape& get_shape_internal(const ShapeHandle&);
void set_shape_internal(ShapeHandle&, const TopoDS_Shape&);

// Access OCCT shape internals - only for KernelOps and StepIO
inline TopoDS_Shape& get_shape_internal(ShapeHandle& handle) {
    return handle.impl_->shape;
}

inline const TopoDS_Shape& get_shape_internal(const ShapeHandle& handle) {
    return handle.impl_->shape;
}

inline void set_shape_internal(ShapeHandle& handle, const TopoDS_Shape& shape) {
    handle.impl_->shape = shape;
}

} // namespace kernel
} // namespace praxis
