#include "egolib/Renderer/Renderer.hpp"

#include <stdexcept>

namespace Ego {

namespace {
Renderer* g_activeRenderer = nullptr;
}

void installActiveRenderer(Renderer& renderer)
{
    if (g_activeRenderer)
    {
        throw std::logic_error("renderer already installed");
    }
    g_activeRenderer = &renderer;
}

void clearActiveRenderer()
{
    g_activeRenderer = nullptr;
}

Renderer* tryActiveRenderer()
{
    return g_activeRenderer;
}

Renderer& activeRenderer()
{
    if (!g_activeRenderer)
    {
        throw std::logic_error("no active renderer");
    }
    return *g_activeRenderer;
}

} // namespace Ego
