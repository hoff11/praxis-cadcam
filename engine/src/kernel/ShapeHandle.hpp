#pragma once
#include <memory>

// Forward declare OCCT type in global namespace
class TopoDS_Shape;

namespace praxis {
namespace kernel {

// Forward declarations for Layer 1 friends
class KernelOps;
class StepIO;

// Forward declare internal accessor functions (defined in ShapeHandleInternal.hpp)
class ShapeHandle;
TopoDS_Shape& get_shape_internal(ShapeHandle&);
const TopoDS_Shape& get_shape_internal(const ShapeHandle&);
void set_shape_internal(ShapeHandle&, const TopoDS_Shape&);

// Opaque handle for OCCT shapes - hides OCCT types from Layer 2+
// Layer 1 (KernelOps, StepIO) creates and manipulates these
// Layer 2+ (RecipeExecutor, Intents) only stores and passes them
class ShapeHandle {
public:
    ShapeHandle();
    ~ShapeHandle();
    
    ShapeHandle(const ShapeHandle&);
    ShapeHandle& operator=(const ShapeHandle&);
    
    ShapeHandle(ShapeHandle&&) noexcept;
    ShapeHandle& operator=(ShapeHandle&&) noexcept;
    
    // Check if handle contains a valid shape
    bool is_valid() const;
    
    // Deprecated - use is_valid() instead
    bool is_null() const { return !is_valid(); }
    
private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
    
    // Only Layer 1 can access OCCT internals
    friend class KernelOps;
    friend class StepIO;
    
    // Friend declarations for accessor functions (defined in ShapeHandleInternal.hpp)
    friend TopoDS_Shape& get_shape_internal(ShapeHandle&);
    friend const TopoDS_Shape& get_shape_internal(const ShapeHandle&);
    friend void set_shape_internal(ShapeHandle&, const TopoDS_Shape&);
};

} // namespace kernel
} // namespace praxis
