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

/// @file egolib/game/graphic.c
/// @brief Simple Egoboo renderer
/// @details All sorts of stuff related to drawing the game

#include "egolib/game/graphic.h"

#include "egolib/game/graphic_internal.h"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/FileFormats/Globals.hpp"
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
#include "egolib/game/Graphics/DefaultMd2ModelRenderer.hpp"
#include "egolib/game/Graphics/BillboardSystem.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/Graphics/TextureAtlasManager.hpp"

//--------------------------------------------------------------------------------------------

#define SPARKLE_SIZE ICON_SIZE
#define SPARKLE_AND  (SPARKLE_SIZE - 1)

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

/// @todo All this crap can be implemented using a single clock with a window size of 1 and a histogram.

using namespace Ego::Time;

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

gfx_config_t     gfx;

float            indextoenvirox[MD2Model::normalCount];

using namespace gfx_internal;

//--------------------------------------------------------------------------------------------

void reinitClocks() {
	sortDoListUnreflected_timer.reinit();
	sortDoListReflected_timer.reinit();

	render_scene_init_timer.reinit();
	render_scene_mesh_timer.reinit();

	gfx_make_tileList_timer.reinit();
	gfx_make_entityList_timer.reinit();
	do_grid_lighting_timer.reinit();
	light_fans_timer.reinit();
	GFX::get().update_object_instances_timer.reinit();
	GFX::get().update_particle_instances_timer.reinit();

	GFX::get().getEntityReflections().clock.reinit();
    GFX::get().getEntityShadows().clock.reinit();
	GFX::get().getOpaqueEntities().clock.reinit();
    GFX::get().getNonOpaqueEntities().clock.reinit();
    GFX::get().getWater().clock.reinit();
    GFX::get().getReflective0().clock.reinit();
    GFX::get().getReflective1().clock.reinit();
    GFX::get().getNonReflective().clock.reinit();
    GFX::get().getForeground().clock.reinit();
    GFX::get().getBackground().clock.reinit();
}

//--------------------------------------------------------------------------------------------
// GFX implementation
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

void gfx_system_load_assets()
{
    /// @author ZF
    /// @details This function loads all the graphics based on the game settings
    // Enable prespective correction?
    Ego::Renderer::get().setPerspectiveCorrectionEnabled(gfx.perspective);
    // Enable dithering?
    Ego::Renderer::get().setDitheringEnabled(gfx.dither);
    // Enable Gouraud shading?
    Ego::Renderer::get().setGouraudShadingEnabled(gfx.gouraudShading_enable);
    // Enable antialiasing (via multisamples)?
    Ego::Renderer::get().setMultisamplesEnabled(gfx.antialiasing);
}

//--------------------------------------------------------------------------------------------
void gfx_system_init_all_graphics()
{
    font_bmp_init();
	reinitClocks();
}

//--------------------------------------------------------------------------------------------
void gfx_system_release_all_graphics()
{
    GFX::get().getBillboardSystem().reset();
    Ego::TextureManager::get().release_all();
}

//--------------------------------------------------------------------------------------------
void gfx_system_make_enviro()
{
    /// @author ZZ
    /// @details This function sets up the environment mapping table

    // Find the environment map positions
    for (size_t i = 0; i < MD2Model::normalCount; ++i)
    {
        float x = MD2Model::getMD2Normal(i, 0);
        float y = MD2Model::getMD2Normal(i, 1);
        indextoenvirox[i] = std::atan2(y, x) * idlib::inv_two_pi<float>();
    }
}

//--------------------------------------------------------------------------------------------
void gfx_system_reload_all_textures()
{
    /// @author BB
    /// @details function is called when the graphics mode is changed or the program is
    /// restored from a minimized state. Otherwise, all OpenGL bitmaps return to a random state.

    Ego::TextureManager::get().reupload();
    Ego::Graphics::TextureAtlasManager::get().reupload();
}

//--------------------------------------------------------------------------------------------
// gfx_config_t FUNCTIONS
//--------------------------------------------------------------------------------------------
void gfx_config_t::download(gfx_config_t& self, egoboo_config_t& cfg)
{
    // Load GFX configuration values, even if no Egoboo configuration is provided.
    init(self);

    self.antialiasing = cfg.graphic_antialiasing.getValue() > 0;

    self.refon = cfg.graphic_reflections_enable.getValue();

    self.shadows_enable = cfg.graphic_shadows_enable.getValue();
    self.shadows_highQuality_enable = cfg.graphic_shadows_highQuality_enable.getValue();

    self.gouraudShading_enable = cfg.graphic_gouraudShading_enable.getValue();
    self.dither = cfg.graphic_dithering_enable.getValue();
    self.perspective = cfg.graphic_perspectiveCorrection_enable.getValue();
    self.phongon = cfg.graphic_specularHighlights_enable.getValue();

    self.draw_background = cfg.graphic_background_enable.getValue();
    self.draw_overlay = cfg.graphic_overlay_enable.getValue();

    self.dynalist_max = Ego::Math::constrain(cfg.graphic_simultaneousDynamicLights_max.getValue(), (uint16_t)0, (uint16_t)TOTAL_MAX_DYNA);
}

void gfx_config_t::init(gfx_config_t& self)
{
    self.gouraudShading_enable = true;
    self.refon = true;
    self.antialiasing = false;
    self.dither = false;
    self.perspective = false;
    self.phongon = true;
    self.shadows_enable = true;
    self.shadows_highQuality_enable = true;

    self.draw_background = false;
    self.draw_overlay = false;
    self.draw_water_0 = true;
    self.draw_water_1 = true;

    self.dynalist_max = 8;
}

//--------------------------------------------------------------------------------------------
// MODE CONTROL
//--------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------
void gfx_do_clear_screen()
{
    auto& renderer = Ego::Renderer::get();
    // Clear the depth buffer.
    renderer.setDepthWriteEnabled(true);
	renderer.getDepthBuffer().clear();
    // Clear the colour buffer.
	renderer.getColourBuffer().clear();
}

//--------------------------------------------------------------------------------------------
void gfx_do_flip_pages()
{
    Ego::Core::Console::get().draw();
    SDL_GL_SwapWindow(Ego::GraphicsSystem::get().window->get());
}

// UTILITY FUNCTIONS
#if 0
float calc_light_rotation(int rotation, int normal)
{
    /// @author ZZ
    /// @details This function helps make_lighttable
	Vector3f nrm, nrm2;
    float sinrot, cosrot;

    nrm[kX] = MD2Model::getMD2Normal(normal, 0);
    nrm[kY] = MD2Model::getMD2Normal(normal, 1);
    nrm[kZ] = MD2Model::getMD2Normal(normal, 2);

    sinrot = sinlut[rotation];
    cosrot = coslut[rotation];

    nrm2[kX] = cosrot * nrm[kX] + sinrot * nrm[kY];
    nrm2[kY] = cosrot * nrm[kY] - sinrot * nrm[kX];
    nrm2[kZ] = nrm[kZ];

    return (nrm2[kX] < 0) ? 0 : (nrm2[kX] * nrm2[kX]);
}
#endif

//--------------------------------------------------------------------------------------------
#if 0
float calc_light_global(int rotation, int normal, float lx, float ly, float lz)
{
    /// @author ZZ
    /// @details This function helps make_lighttable
    float fTmp;
	Vector3f nrm, nrm2;
    float sinrot, cosrot;

    nrm[kX] = MD2Model::getMD2Normal(normal, 0);
    nrm[kY] = MD2Model::getMD2Normal(normal, 1);
    nrm[kZ] = MD2Model::getMD2Normal(normal, 2);

    sinrot = sinlut[rotation];
    cosrot = coslut[rotation];

    nrm2[kX] = cosrot * nrm[kX] + sinrot * nrm[kY];
    nrm2[kY] = cosrot * nrm[kY] - sinrot * nrm[kX];
    nrm2[kZ] = nrm[kZ];

    fTmp = nrm2[kX] * lx + nrm2[kY] * ly + nrm2[kZ] * lz;
    if (fTmp < 0) fTmp = 0;

    return fTmp * fTmp;
}
#endif

gfx_rv GFX::update_object_instances(Camera& cam)
{
    gfx_rv retval;

    // assume the best
    retval = gfx_success;

    for (const std::shared_ptr<Object> &pchr : activeModule().getObjectHandler().iterator())
    {
        //Dont do terminated characters
        if (pchr->isTerminated()) {
            continue;
        }

        //Skip objects outside the map
		auto mesh = activeModule().getMeshPointer();
        if (!mesh->grid_is_valid(pchr->getTile())) continue;

        // make sure that the vertices are interpolated
        if(pchr->updateVertices(-1, -1, true) == gfx_error) {
            retval = gfx_error;
        }

        // the instance has changed, refresh the collision bound
        else {
            pchr->getObjectPhysics().updateCollisionSize(true);            
        }

        // do the basic lighting
        pchr->updateLighting();
    }

    return retval;
}

// variables to optimize calls to bind the textures
bool TileRenderer::disableTexturing = false;
TX_REF TileRenderer::image = Ego::Graphics::MESH_IMG_COUNT;
uint8_t TileRenderer::size = 0xFF;

std::shared_ptr<Ego::Texture> TileRenderer::get_texture(uint8_t image, uint8_t size)
{
	if (0 == size) {
		return Ego::Graphics::TextureAtlasManager::get().getSmall(image);
	} else if (1 == size) {
		return Ego::Graphics::TextureAtlasManager::get().getBig(image);
	}  else {
        return nullptr;
    }
}

void TileRenderer::invalidate()
{
	image = Ego::Graphics::MESH_IMG_COUNT;
	size = 0xFF;
}

void TileRenderer::bind(const ego_tile_info_t& tile)
{
	uint8_t newImage, newSize;
    std::shared_ptr<Ego::Texture> texture = nullptr;
	bool needsBinding = false;

	// Disable texturing.
	if (disableTexturing)
	{
        needsBinding = true;
		TileRenderer::invalidate();
	}
	else
	{
		newImage = TILE_GET_LOWER_BITS(tile._img);
		newSize = (tile._type < tile_dict.offset) ? 0 : 1;

		if ((image != newImage) || (size != newSize))
		{
			texture = get_texture(newImage, newSize);
            needsBinding = true;

			image = newImage;
			size = newSize;
		}
	}

	if (needsBinding)
	{
		Ego::Renderer::get().getTextureUnit().setActivated(texture.get());
		if (texture && texture->hasAlpha())
		{
			// MH: Enable alpha blending if the texture requires it.
			Ego::Renderer::get().setBlendingEnabled(true);
			Ego::Renderer::get().setBlendFunction(idlib::color_blend_parameter::one, idlib::color_blend_parameter::one_minus_source0_alpha);
		}
	}
}

gfx_rv GFX::update_particle_instances(Camera& camera)
{
    // only one update per frame
    static uint32_t instance_update = std::numeric_limits<uint32_t>::max();
    if (instance_update == worldUpdateCount()) return gfx_success;
    instance_update = worldUpdateCount();

    // assume the best
    gfx_rv retval = gfx_success;

    for (const std::shared_ptr<Ego::Particle> &particle : ParticleHandler::get().iterator())
    {
        if (particle->isTerminated()) continue;

        Ego::Graphics::ParticleGraphics *pinst = &(particle->inst);

        // only do frame counting for particles that are fully activated!
        particle->frame_count++;

        if (!particle->inst.indolist)
        {
            pinst->valid = false;
            pinst->ref_valid = false;
        }
        else
        {
            // calculate the "billboard" for this particle
            if (gfx_error == Ego::Graphics::ParticleGraphics::update(camera, particle->getParticleID(), 255, true))
            {
                retval = gfx_error;
            }
        }
    }

    return retval;
}

GameAppImpl::GameAppImpl() :
    dynalist(),
    billboardSystem(std::make_unique<Ego::Graphics::BillboardSystem>()),
    md2ModelRenderer(std::make_unique<Ego::Graphics::DefaultMd2ModelRenderer>())
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
}

GameAppImpl::~GameAppImpl()
{
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

Ego::Graphics::Md2ModelRenderer& GameAppImpl::getMd2ModelRenderer() const
{
    return *md2ModelRenderer;
}
