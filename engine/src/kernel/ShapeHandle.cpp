#include "ShapeHandleInternal.hpp"

namespace praxis {
namespace kernel {

// Impl definition is in ShapeHandleInternal.hpp

ShapeHandle::ShapeHandle() : impl_(std::make_shared<Impl>()) {}
ShapeHandle::~ShapeHandle() = default;

ShapeHandle::ShapeHandle(const ShapeHandle&) = default;
ShapeHandle& ShapeHandle::operator=(const ShapeHandle&) = default;
ShapeHandle::ShapeHandle(ShapeHandle&&) noexcept = default;
ShapeHandle& ShapeHandle::operator=(ShapeHandle&&) noexcept = default;

bool ShapeHandle::is_valid() const {
    return impl_ && !impl_->shape.IsNull();
}

} // namespace kernel
} // namespace praxis
