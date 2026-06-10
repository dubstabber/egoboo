#include "egolib/Graphics/IGraphicsSystem.hpp"

#include <stdexcept>

namespace Ego {

namespace {
IGraphicsSystem* g_activeGraphicsSystem = nullptr;
}

void installActiveGraphicsSystem(IGraphicsSystem& graphicsSystem)
{
    if (g_activeGraphicsSystem)
    {
        throw std::logic_error("graphics system already installed");
    }
    g_activeGraphicsSystem = &graphicsSystem;
}

void clearActiveGraphicsSystem()
{
    g_activeGraphicsSystem = nullptr;
}

IGraphicsSystem* tryActiveGraphicsSystem()
{
    return g_activeGraphicsSystem;
}

IGraphicsSystem& activeGraphicsSystem()
{
    if (!g_activeGraphicsSystem)
    {
        throw std::logic_error("no active graphics system");
    }
    return *g_activeGraphicsSystem;
}

} // namespace Ego
