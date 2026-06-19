#pragma once

#include <cstddef>  // size_t

namespace Ego {
namespace Graphics {

class AnimatedModel;

/// @brief Abstract renderer for models.
class ModelVertexBuffer
{
protected:
    /// @brief Construct this abstract model vertex buffer.
    /// @remark Intentionally protected.
    ModelVertexBuffer();

public:
    /// @brief Destruct this model vertex buffer.
    virtual ~ModelVertexBuffer();

    /// @brief Ensure the size of the vertex buffer is greater than or equal to a required size.
    /// @param requiredSize the required size
    virtual void ensureSize(size_t requiredSize) = 0;

    /// @brief Compute the required vertex buffer capacity for a model.
    /// @param model the model
    /// @return the required vertex buffer capacity
    virtual size_t getRequiredVertexBufferCapacity(const AnimatedModel& model) const = 0;

    virtual void *lock() = 0;

}; // class ModelVertexBuffer

} // namespace Graphics
} // namespace Ego
