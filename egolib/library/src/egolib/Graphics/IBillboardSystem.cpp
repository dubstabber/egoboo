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

/// @file egolib/Graphics/IBillboardSystem.cpp
/// @brief Ownership of the installed active billboard system.

#include "egolib/Graphics/IBillboardSystem.hpp"

#include <stdexcept>

namespace Ego
{
namespace Graphics
{

namespace
{
IBillboardSystem* g_activeBillboardSystem = nullptr;
}

void installActiveBillboardSystem(IBillboardSystem& billboardSystem)
{
    if (g_activeBillboardSystem)
    {
        throw std::logic_error("billboard system already installed");
    }
    g_activeBillboardSystem = &billboardSystem;
}

void clearActiveBillboardSystem()
{
    g_activeBillboardSystem = nullptr;
}

IBillboardSystem* tryActiveBillboardSystem()
{
    return g_activeBillboardSystem;
}

IBillboardSystem& activeBillboardSystem()
{
    if (!g_activeBillboardSystem)
    {
        throw std::logic_error("no active billboard system");
    }
    return *g_activeBillboardSystem;
}

} // namespace Graphics
} // namespace Ego
