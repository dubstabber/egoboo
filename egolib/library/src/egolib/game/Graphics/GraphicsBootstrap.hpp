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

/// @file egolib/game/Graphics/GraphicsBootstrap.hpp
/// @brief Lower-layer injection seam for constructing the 3D scene-rendering cluster.
/// @details The GFX GameApp, the CameraSystem, the BillboardSystem and the 11 concrete
///   RenderPasses live in the above-egolib-library egolib-game-graphics archive. Their
///   construction is order-sensitive (it happens mid-GameEngine::initialize(), after the
///   gfx-config download and before gfx_system_init_all_graphics / the console rect), so it
///   cannot simply move to Main.cpp pre-start. Instead the construction/teardown bodies are
///   registered from above egolib-library (installDefaultGraphicsSystems, defined in
///   graphic_init.cpp) as std::function hooks held HERE in egolib-library, and
///   GameEngine::initialize()/teardown invoke them at the exact former call sites — so
///   egolib-library never names the concrete cluster types, yet ordering is byte-identical.
///   This is the construction-time analogue of the Ego::Script::IScriptSystem driver seam.

#pragma once

#include <functional>

namespace Ego
{
namespace Graphics
{

/// @brief A graphics-app bootstrap step: construct+install (init) or clear+destroy (teardown)
///        the stateful render cluster singletons.
using GraphicsBootstrapHook = std::function<void()>;

/// @brief Register the init/teardown hooks. Called once at boot from above egolib-library
///        (installDefaultGraphicsSystems), BEFORE GameEngine::initialize() runs.
void registerGraphicsBootstrap(GraphicsBootstrapHook init, GraphicsBootstrapHook teardown);

/// @brief Run the registered init hook (no-op if none registered, e.g. the test harness which
///        never drives GameEngine::initialize()). Invoked from GameEngine::initialize() at the
///        exact point the GFX/camera bootstrap used to live, preserving ordering.
void runGraphicsBootstrapInit();

/// @brief Run the registered teardown hook (no-op if none registered). Invoked from the
///        GameEngine teardown at the exact point the GFX/camera teardown used to live.
void runGraphicsBootstrapTeardown();

/// @brief Install the default graphics-systems bootstrap (the GFX GameApp + CameraSystem +
///        BillboardSystem + TextureAtlasManager construction/teardown). DEFINED in
///        graphic_init.cpp in the egolib-game-graphics archive (above egolib-library), so this
///        install must be called from above the library — the game's Main.cpp. Mirrors
///        Ego::Script::installDefaultScriptSystem().
void installDefaultGraphicsSystems();

/// @brief Clear the default graphics-systems bootstrap registration.
void clearDefaultGraphicsSystems();

} // namespace Graphics
} // namespace Ego
