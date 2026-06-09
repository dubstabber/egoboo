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

} // namespace Entities
} // namespace Ego
