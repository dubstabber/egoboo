#include "egolib/Profiles/IProfileSystem.hpp"

#include <stdexcept>

static IProfileSystem* g_activeProfileSystem = nullptr;

void installActiveProfileSystem(IProfileSystem& profileSystem)
{
    if (g_activeProfileSystem)
    {
        throw std::logic_error("profile system already installed");
    }
    g_activeProfileSystem = &profileSystem;
}

void clearActiveProfileSystem()
{
    g_activeProfileSystem = nullptr;
}

IProfileSystem* tryActiveProfileSystem()
{
    return g_activeProfileSystem;
}

IProfileSystem& activeProfileSystem()
{
    if (!g_activeProfileSystem)
    {
        throw std::logic_error("no active profile system");
    }
    return *g_activeProfileSystem;
}
