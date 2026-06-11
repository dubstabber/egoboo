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

/// @file egolib/game/Graphics/GraphicsBootstrap.cpp
/// @brief egolib-library-side holder for the graphics-systems bootstrap hooks.
/// @details Holds the registered init/teardown std::function hooks. The default
///   implementation that constructs the concrete cluster (installDefaultGraphicsSystems) is
///   defined in graphic_init.cpp in the above-library egolib-game-graphics archive.

#include "egolib/game/Graphics/GraphicsBootstrap.hpp"

namespace Ego
{
namespace Graphics
{

namespace
{
GraphicsBootstrapHook g_init;
GraphicsBootstrapHook g_teardown;
}

void registerGraphicsBootstrap(GraphicsBootstrapHook init, GraphicsBootstrapHook teardown)
{
    g_init = std::move(init);
    g_teardown = std::move(teardown);
}

void runGraphicsBootstrapInit()
{
    if (g_init)
    {
        g_init();
    }
}

void runGraphicsBootstrapTeardown()
{
    if (g_teardown)
    {
        g_teardown();
    }
}

} // namespace Graphics
} // namespace Ego
