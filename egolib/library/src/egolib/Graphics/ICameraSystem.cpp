/// @file egolib/Graphics/ICameraSystem.cpp
/// @brief Ownership of the installed active camera system.

#include "egolib/Graphics/ICameraSystem.hpp"

#include <stdexcept>

namespace
{
ICameraSystem* g_activeCameraSystem = nullptr;
}

void installActiveCameraSystem(ICameraSystem& cameraSystem)
{
    if (g_activeCameraSystem)
    {
        throw std::logic_error("camera system already installed");
    }
    g_activeCameraSystem = &cameraSystem;
}

void clearActiveCameraSystem()
{
    g_activeCameraSystem = nullptr;
}

ICameraSystem* tryActiveCameraSystem()
{
    return g_activeCameraSystem;
}

ICameraSystem& activeCameraSystem()
{
    if (!g_activeCameraSystem)
    {
        throw std::logic_error("no active camera system");
    }
    return *g_activeCameraSystem;
}
