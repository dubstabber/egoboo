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

/// @file egolib/Physics/ICollisionWorld.cpp
/// @brief Ownership of the installed active collision world.

#include "egolib/Physics/ICollisionWorld.hpp"

#include <stdexcept>

namespace Ego
{
namespace Physics
{

namespace
{
ICollisionWorld* g_activeCollisionWorld = nullptr;
}

void installCollisionWorld(ICollisionWorld* world)
{
    g_activeCollisionWorld = world;
}

void clearCollisionWorld()
{
    g_activeCollisionWorld = nullptr;
}

ICollisionWorld* tryActiveCollisionWorld()
{
    return g_activeCollisionWorld;
}

ICollisionWorld& activeCollisionWorld()
{
    if (!g_activeCollisionWorld)
    {
        // Mirrors GameSessionContext::activeModule(), which the previous Collidable
        // implementation routed through.
        throw std::logic_error("no active collision world");
    }
    return *g_activeCollisionWorld;
}

} // namespace Physics
} // namespace Ego
