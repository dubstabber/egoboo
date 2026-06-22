#include "egolib/InputControl/IInputSystem.hpp"

#include <stdexcept>

namespace Ego {
namespace Input {

namespace {
IInputSystem* g_activeInputSystem = nullptr;
}

void installActiveInputSystem(IInputSystem& inputSystem)
{
    if (g_activeInputSystem)
    {
        throw std::logic_error("input system already installed");
    }
    g_activeInputSystem = &inputSystem;
}

void clearActiveInputSystem()
{
    g_activeInputSystem = nullptr;
}

IInputSystem* tryActiveInputSystem()
{
    return g_activeInputSystem;
}

IInputSystem& activeInputSystem()
{
    if (!g_activeInputSystem)
    {
        throw std::logic_error("no active input system");
    }
    return *g_activeInputSystem;
}

} // namespace Input
} // namespace Ego
