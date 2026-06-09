//********************************************************************************************
//*
//*    This file is part of Egoboo.
//*
//*    Egoboo is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Egoboo is distributed in the hope that it will be useful, but
//*    WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Egoboo.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************

/// @file egolib/Entities/IObjectWorld.cpp
/// @brief Ownership of the installed active object world.

#include "egolib/Entities/IObjectWorld.hpp"

#include <stdexcept>

namespace Ego
{
namespace Entities
{

namespace
{
IObjectWorld* g_activeObjectWorld = nullptr;
const uint32_t* g_activeWorldUpdateCounter = nullptr;
}

void installObjectWorld(IObjectWorld* world)
{
    g_activeObjectWorld = world;
}

void clearObjectWorld()
{
    g_activeObjectWorld = nullptr;
}

IObjectWorld* tryActiveObjectWorld()
{
    return g_activeObjectWorld;
}

IObjectWorld& activeObjectWorld()
{
    if (!g_activeObjectWorld)
    {
        // Mirrors GameSessionContext::activeModule(), which the physics TUs routed through.
        throw std::logic_error("no active object world");
    }
    return *g_activeObjectWorld;
}

void installWorldUpdateCounter(const uint32_t* counter)
{
    g_activeWorldUpdateCounter = counter;
}

void clearWorldUpdateCounter()
{
    g_activeWorldUpdateCounter = nullptr;
}

uint32_t activeWorldUpdateCount()
{
    // Unlike activeObjectWorld(), this returns a value (not a null-able object), so the
    // uninstalled state yields 0 rather than throwing — matching the session counter's
    // reset-to-0 value outside an active module. The physics call sites only run mid-module
    // (where activeObjectWorld() is installed), so the 0 branch is not reached in practice.
    return g_activeWorldUpdateCounter ? *g_activeWorldUpdateCounter : 0u;
}

} // namespace Entities
} // namespace Ego
