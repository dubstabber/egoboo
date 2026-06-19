#pragma once

#include "egolib/game/Graphics/ModelVertexBuffer.hpp"
#include "egolib/integrations/video.hpp"  // idlib::vertex_descriptor
#include <vector>

namespace Ego {
namespace Graphics {

/// @brief Default renderer for models.
class DefaultModelVertexBuffer : public ModelVertexBuffer
{
public:
    // Forward declaration.
    struct Vertex;
protected:
    /// @brief The vertex descriptor.
    idlib::vertex_descriptor vertexDescriptor;

    /// @brief The size of the vertex buffer.
    size_t m_size;

    /// @brief The vertices of the vertex buffer-
    Vertex *m_vertices;

public:
    /// @brief Construct this model vertex buffer.
    DefaultModelVertexBuffer();

    /// @brief Destruct this model vertex buffer.
    virtual ~DefaultModelVertexBuffer();

    /// @copydoc ModelVertexBuffer::ensureSize
    void ensureSize(size_t requiredSize) override;

    /// @copydoc ModelVertexBuffer::getRequiredVertexBufferCapacity
    size_t getRequiredVertexBufferCapacity(const AnimatedModel& model) const override;

    /// @copydoc ModelVertexBuffer::lock()
    void *lock() override;

    /// @brief A vertex.
    struct Vertex
    {
        struct
        {
            float x, y, z;
        } position;
        struct
        {
            float r, g, b, a;
        } colour;
        struct
        {
            float s, t;
        } texture;
        struct
        {
            float x, y, z;
        } normal;
    };

    /// @brief List of vertices to be rendered.
    std::vector<Vertex> vertices;

}; // class DefaultModelVertexBuffer

} // namespace Graphics
} // namespace Ego
