#include "egolib/game/Graphics/IGFX.hpp"

#include <stdexcept>

namespace {
IGFX* g_activeGFX = nullptr;
}

void installActiveGFX(IGFX& gfx)
{
    if (g_activeGFX)
    {
        throw std::logic_error("GFX already installed");
    }
    g_activeGFX = &gfx;
}

void clearActiveGFX()
{
    g_activeGFX = nullptr;
}

IGFX* tryActiveGFX()
{
    return g_activeGFX;
}

IGFX& activeGFX()
{
    if (!g_activeGFX)
    {
        throw std::logic_error("no active GFX");
    }
    return *g_activeGFX;
}
