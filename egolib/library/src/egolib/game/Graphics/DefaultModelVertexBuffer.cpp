#include "egolib/game/Graphics/DefaultModelVertexBuffer.hpp"
#include "egolib/Graphics/AnimatedModel.hpp"
#include "egolib/Graphics/VertexFormat.hpp"

#include <algorithm>
#include <cstring>

namespace Ego {
namespace Graphics {

DefaultModelVertexBuffer::DefaultModelVertexBuffer()
    : m_vertices(new Vertex[0]), m_size(0),
      vertexDescriptor(descriptor_factory<idlib::vertex_format::P3FC4FT2FN3F>()())
{}

DefaultModelVertexBuffer::~DefaultModelVertexBuffer()
{
    delete[] m_vertices;
    m_vertices = nullptr;
    m_size = 0;
}

void *DefaultModelVertexBuffer::lock()
{
    return m_vertices;
}

void DefaultModelVertexBuffer::ensureSize(size_t requiredSize)
{
    if (requiredSize <= m_size)
    {
        return;
    }
    auto newSize = requiredSize;
    auto newVertices = new Vertex[requiredSize];
    memcpy(newVertices, m_vertices, m_size);
    delete[] m_vertices;
    m_vertices = newVertices;
    m_size = newSize;
}


size_t DefaultModelVertexBuffer::getRequiredVertexBufferCapacity(const AnimatedModel& model) const
{
    size_t capacity = 0;
    for (const AnimatedModelDrawCommand& command : model.getDrawCommands())
    {
        capacity = std::max(capacity, command.data.size());
    }
    return capacity;
}

} // namespace Graphics
} // namespace Ego
