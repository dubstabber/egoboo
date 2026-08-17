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

/// @file egolib/tests/egolib/tests/GraphicsTeardown.cpp
/// @brief Pins gfx_system_release_all_graphics()'s divergent-liveness-signal guard (defect (2) of
///        the teardown-try-accessors pass). GFX::is_initialized() / Ego::TextureManager::is_initialized()
///        are the concrete-singleton liveness flags (idlib::singleton<GFX>/<TextureManager>),
///        independent of whether the EngineContext billboardSystem()/textureManager() registries
///        are currently installed. On the abnormal teardown corridor (Main.cpp's catch(...)
///        calling EngineContext::clearEngine() directly, which clears the EngineContext registries
///        without running the graphics-bootstrap teardown that uninitializes the singletons), the
///        singletons stay "initialized" while the EngineContext registries are already gone.
///
/// Only the TextureManager half is reproduced live here: Ego::TextureManager's constructor is
/// headless-safe (no GL calls; verified by reading TextureManager.cpp), matching how App.cpp:28
/// initializes it. The GFX half shares the identical code shape and fix but is not independently
/// reproduced, because constructing the concrete GFX singleton pulls in GraphicsSystem / Renderer
/// / ImageManager / FontManager (a real window/GL context) via its App<T> base -- not safe to do
/// from a headless test binary.

#include "gtest/gtest.h"

#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/graphic.h"
#include "egolib/Graphics/TextureManager.hpp"

namespace
{

// Mirrors App.cpp:28-29's normal pairing (TextureManager::initialize() immediately followed by
// EngineContext::installTextureManager()) but installs only the singleton half, reproducing the
// abnormal-corridor divergence: the singleton is alive while the EngineContext registry is absent.
class ScopedTextureManagerSingleton
{
public:
    ScopedTextureManagerSingleton() { Ego::TextureManager::initialize(); }
    ~ScopedTextureManagerSingleton() { Ego::TextureManager::uninitialize(); }
};

} // namespace

TEST(GfxSystemReleaseAllGraphicsTeardown, SkipsTextureManagerReleaseWhenEngineContextRegistryIsAbsent)
{
    // Nothing in this test binary installs an EngineContext texture manager by default, but make
    // the precondition explicit and independent of test ordering.
    EngineContext::get().clearTextureManager();

    ScopedTextureManagerSingleton textureManagerSingleton;
    ASSERT_TRUE(Ego::TextureManager::is_initialized());
    ASSERT_EQ(EngineContext::get().tryTextureManager(), nullptr);

    // Before the fix, gfx_system_release_all_graphics() called the throwing
    // EngineContext::get().textureManager() here and raised std::logic_error
    // ("no active texture manager") out of this exact reachable state.
    EXPECT_NO_THROW(gfx_system_release_all_graphics());
}
