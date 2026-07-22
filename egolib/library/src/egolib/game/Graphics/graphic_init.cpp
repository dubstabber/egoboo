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

/// @file egolib/game/Graphics/graphic_init.cpp
/// @brief Construction/teardown of the 3D scene-rendering cluster (egolib-game-graphics).
/// @details Holds the GFX GameApp ctor/dtor (which make_unique the 11 concrete RenderPasses),
///   GFX::renderBillboards (which calls the concrete BillboardSystem::render_all), and the
///   GameAppImpl ctor/dtor (BillboardSystem + ModelVertexBuffer + TextureAtlasManager). These
///   bodies were relocated out of egolib-library's graphic.c so that the only TU naming the
///   concrete cluster types lives ABOVE egolib-library, in the egolib-game-graphics archive —
///   the 9th link-split. The construction is triggered from egolib-library
///   (GameEngine::initialize() -> Ego::Graphics::runGraphicsBootstrapInit()) via the bootstrap
///   hook installed here by installDefaultGraphicsSystems(), called once from the game's
///   Main.cpp above egolib-library (mirrors Ego::Script::installDefaultScriptSystem()).

#include "egolib/game/graphic.h"

#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/Graphics/GraphicsBootstrap.hpp"
#include "egolib/game/Graphics/RenderPasses/BackgroundRenderPass.hpp"
#include "egolib/game/Graphics/RenderPasses/EntityReflectionsRenderPass.hpp"
#include "egolib/game/Graphics/RenderPasses/EntityShadowsRenderPass.hpp"
#include "egolib/game/Graphics/RenderPasses/ForegroundRenderPass.hpp"
#include "egolib/game/Graphics/RenderPasses/HeightmapRenderPass.hpp"
#include "egolib/game/Graphics/RenderPasses/NonOpaqueEntitiesRenderPass.hpp"
#include "egolib/game/Graphics/RenderPasses/NonReflectiveTilesRenderPass.hpp"
#include "egolib/game/Graphics/RenderPasses/OpaqueEntitiesRenderPass.hpp"
#include "egolib/game/Graphics/RenderPasses/ReflectiveTilesFirstRenderPass.hpp"
#include "egolib/game/Graphics/RenderPasses/ReflectiveTilesSecondRenderPass.hpp"
#include "egolib/game/Graphics/RenderPasses/WaterTilesRenderPass.hpp"
#include "egolib/game/Graphics/DefaultModelVertexBuffer.hpp"
#include "egolib/game/Graphics/BillboardSystem.hpp"
#include "egolib/game/Graphics/TextureAtlasManager.hpp"
#include "egolib/game/Graphics/CameraSystem.hpp"

#include <exception>
#include <memory>

//--------------------------------------------------------------------------------------------
// GFX construction (relocated from graphic.c so the concrete RenderPass / BillboardSystem
// references live above egolib-library).
//--------------------------------------------------------------------------------------------

GFX::GFX() :
    GameApp<GFX>("Egoboo", GameEngine::GAME_VERSION),
    update_object_instances_timer("update.object.instances", 512),
    update_particle_instances_timer("update.particle.instances", 512),
    nonOpaqueEntities(std::make_unique<Ego::Graphics::NonOpaqueEntitiesRenderPass>()),
    opaqueEntities(std::make_unique<Ego::Graphics::OpaqueEntitiesRenderPass>()),
    reflective0(std::make_unique<Ego::Graphics::ReflectiveTilesFirstRenderPass>()),
    reflective1(std::make_unique<Ego::Graphics::ReflectiveTilesSecondRenderPass>()),
    nonReflective(std::make_unique<Ego::Graphics::NonReflectiveTilesRenderPass>()),
    entityShadows(std::make_unique<Ego::Graphics::EntityShadowsRenderPass>()),
    water(std::make_unique<Ego::Graphics::WaterTilesRenderPass>()),
    entityReflections(std::make_unique<Ego::Graphics::EntityReflectionsRenderPass>()),
    foreground(std::make_unique<Ego::Graphics::ForegroundRenderPass>()),
    background(std::make_unique<Ego::Graphics::BackgroundRenderPass>()),
    heightmap(std::make_unique<Ego::Graphics::HeightmapRenderPass>())
{}

GFX::~GFX()
{}

void GFX::renderBillboards(Camera& camera)
{
    getBillboardSystem().render_all(camera);
}

//--------------------------------------------------------------------------------------------
// GameAppImpl construction (relocated from graphic.c).
//--------------------------------------------------------------------------------------------

GameAppImpl::GameAppImpl() :
    dynalist(),
    // The renderer is resolved here, at the composition root: the App base has already
    // installed it by the time this GameApp member constructs, and it is uninitialized
    // only after this member is destroyed (~GameAppImpl runs before ~AppImpl).
    billboardSystem(std::make_unique<Ego::Graphics::BillboardSystem>(EngineContext::get().renderer())),
    modelVertexBuffer(std::make_unique<Ego::Graphics::DefaultModelVertexBuffer>())
{
    // Initialize the texture atlas manager.
    try
    {
        Ego::Graphics::TextureAtlasManager::initialize();
    }
    catch (...)
    {
        std::rethrow_exception(std::current_exception());
    }
    // Publish the texture atlas manager through the engine context.
    EngineContext::get().installTextureAtlasManager(Ego::Graphics::TextureAtlasManager::get());
}

GameAppImpl::~GameAppImpl()
{
    // Unpublish the texture atlas manager from the engine context.
    EngineContext::get().clearTextureAtlasManager();
    // Uninitialize the texture atlas manager.
    Ego::Graphics::TextureAtlasManager::uninitialize();
}

dynalist_t& GameAppImpl::getDynalist()
{
    return dynalist;
}

Ego::Graphics::BillboardSystem& GameAppImpl::getBillboardSystem() const
{
    return *billboardSystem;
}

Ego::Graphics::ModelVertexBuffer& GameAppImpl::getModelVertexBuffer() const
{
    return *modelVertexBuffer;
}

//--------------------------------------------------------------------------------------------
// Bootstrap injection: register the construction/teardown of the cluster singletons so
// egolib-library's GameEngine::initialize()/teardown can trigger them without naming the
// concrete types. Installed once from the game's Main.cpp (above egolib-library).
//--------------------------------------------------------------------------------------------

namespace Ego
{
namespace Graphics
{

void installDefaultGraphicsSystems()
{
    registerGraphicsBootstrap(
        []()
        {
            // Initialize the GFX system. (Order-identical to the former GameEngine::initialize
            // block: GFX + billboard install, then camera system + turn-mode from config.)
            GFX::initialize();
            EngineContext::get().installGFX(GFX::get());
            EngineContext::get().installBillboardSystem(GFX::get().getBillboardSystem());

            // camera options
            CameraSystem::initialize();
            EngineContext::get().installCameraSystem(CameraSystem::get());
            EngineContext::get().cameraSystem().getCameraOptions().turnMode =
                EngineContext::get().config().camera_control.getValue();
        },
        []()
        {
            // Uninitialize the GFX system (order-identical to the former GameEngine teardown).
            EngineContext::get().clearCameraSystem();
            EngineContext::get().clearBillboardSystem();
            EngineContext::get().clearGFX();
            GFX::uninitialize();
        });
}

void clearDefaultGraphicsSystems()
{
    registerGraphicsBootstrap(nullptr, nullptr);
}

} // namespace Graphics
} // namespace Ego
