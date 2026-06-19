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
// The concrete RenderPass / BillboardSystem / TextureAtlasManager / model vertex-buffer types are
// constructed in graphic_init.cpp (egolib-game-graphics, above egolib-library). graphic.c only
// needs the RenderPass *base* here (reinitClocks touches RenderPass::clock through the IGFX
// accessors) and reaches the billboard/atlas systems through the EngineContext interfaces.
#include "egolib/game/Graphics/RenderPass.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/font_bmp.h"                  // font_bmp_init
#include "egolib/Graphics/TextureManager.hpp" // Ego::TextureManager
#include "egolib/Console/Console.hpp"         // Ego::Core::Console
#include "egolib/Graphics/GraphicsWindow.hpp" // Ego::GraphicsWindow
#include "egolib/Physics/ICollisionWorld.hpp"

namespace
{

Ego::Physics::ICollisionWorld& collisionWorld()
{
    return Ego::Physics::activeCollisionWorld();
}

} // namespace

//--------------------------------------------------------------------------------------------

#define SPARKLE_SIZE ICON_SIZE
#define SPARKLE_AND  (SPARKLE_SIZE - 1)

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

/// @todo All this crap can be implemented using a single clock with a window size of 1 and a histogram.

using namespace Ego::Time;

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

gfx_config_t     gfx;

float            indextoenvirox[Ego::Graphics::AnimatedModel::normalCount];

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
	EngineContext::get().gfx().updateObjectInstancesTimer().reinit();
	EngineContext::get().gfx().updateParticleInstancesTimer().reinit();

	EngineContext::get().gfx().getEntityReflections().clock.reinit();
    EngineContext::get().gfx().getEntityShadows().clock.reinit();
	EngineContext::get().gfx().getOpaqueEntities().clock.reinit();
    EngineContext::get().gfx().getNonOpaqueEntities().clock.reinit();
    EngineContext::get().gfx().getWater().clock.reinit();
    EngineContext::get().gfx().getReflective0().clock.reinit();
    EngineContext::get().gfx().getReflective1().clock.reinit();
    EngineContext::get().gfx().getNonReflective().clock.reinit();
    EngineContext::get().gfx().getForeground().clock.reinit();
    EngineContext::get().gfx().getBackground().clock.reinit();
}

// GFX / GameAppImpl construction and GFX::renderBillboards were relocated to
// game/Graphics/graphic_init.cpp (egolib-game-graphics, above egolib-library) so the only TU
// naming the concrete RenderPass / BillboardSystem / TextureAtlasManager types sits above the
// library. The bootstrap is triggered from GameEngine via Ego::Graphics::runGraphicsBootstrap*.

void gfx_system_load_assets()
{
    /// @author ZF
    /// @details This function loads all the graphics based on the game settings
    // Enable prespective correction?
    EngineContext::get().renderer().setPerspectiveCorrectionEnabled(gfx.perspective);
    // Enable dithering?
    EngineContext::get().renderer().setDitheringEnabled(gfx.dither);
    // Enable Gouraud shading?
    EngineContext::get().renderer().setGouraudShadingEnabled(gfx.gouraudShading_enable);
    // Enable antialiasing (via multisamples)?
    EngineContext::get().renderer().setMultisamplesEnabled(gfx.antialiasing);
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
    if (GFX::is_initialized())
    {
        EngineContext::get().billboardSystem().reset();
    }

    if (Ego::TextureManager::is_initialized())
    {
        EngineContext::get().textureManager().release_all();
    }
}

//--------------------------------------------------------------------------------------------
void gfx_system_make_enviro()
{
    /// @author ZZ
    /// @details This function sets up the environment mapping table

    // Find the environment map positions
    for (size_t i = 0; i < Ego::Graphics::AnimatedModel::normalCount; ++i)
    {
        float x = Ego::Graphics::AnimatedModel::getLegacyNormal(i, 0);
        float y = Ego::Graphics::AnimatedModel::getLegacyNormal(i, 1);
        indextoenvirox[i] = std::atan2(y, x) * idlib::inv_two_pi<float>();
    }
}

//--------------------------------------------------------------------------------------------
void gfx_system_reload_all_textures()
{
    /// @author BB
    /// @details function is called when the graphics mode is changed or the program is
    /// restored from a minimized state. Otherwise, all OpenGL bitmaps return to a random state.

    EngineContext::get().textureManager().reupload();
    EngineContext::get().textureAtlasManager().reupload();
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
    auto& renderer = EngineContext::get().renderer();
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
    SDL_GL_SwapWindow(EngineContext::get().graphicsSystem().getWindow()->get());
}

// UTILITY FUNCTIONS
#if 0
float calc_light_rotation(int rotation, int normal)
{
    /// @author ZZ
    /// @details This function helps make_lighttable
	Vector3f nrm, nrm2;
    float sinrot, cosrot;

    nrm[kX] = Ego::Graphics::AnimatedModel::getLegacyNormal(normal, 0);
    nrm[kY] = Ego::Graphics::AnimatedModel::getLegacyNormal(normal, 1);
    nrm[kZ] = Ego::Graphics::AnimatedModel::getLegacyNormal(normal, 2);

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

    nrm[kX] = Ego::Graphics::AnimatedModel::getLegacyNormal(normal, 0);
    nrm[kY] = Ego::Graphics::AnimatedModel::getLegacyNormal(normal, 1);
    nrm[kZ] = Ego::Graphics::AnimatedModel::getLegacyNormal(normal, 2);

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
        if (!collisionWorld().gridIsValid(pchr->getTile())) continue;

        // make sure that the vertices are interpolated
        if (!pchr->getGraphics().updateVertices(-1, -1, true)) {
            retval = gfx_error;
        }

        // the instance has changed, refresh the collision bound
        else {
            pchr->updateCollisionSize(true);
        }

        // do the basic lighting
        pchr->getGraphics().updateLighting();
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
		return EngineContext::get().textureAtlasManager().getSmall(image);
	} else if (1 == size) {
		return EngineContext::get().textureAtlasManager().getBig(image);
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
		EngineContext::get().renderer().getTextureUnit().setActivated(texture.get());
		if (texture && texture->hasAlpha())
		{
			// MH: Enable alpha blending if the texture requires it.
			EngineContext::get().renderer().setBlendingEnabled(true);
			EngineContext::get().renderer().setBlendFunction(idlib::color_blend_parameter::one, idlib::color_blend_parameter::one_minus_source0_alpha);
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

    for (const std::shared_ptr<Ego::Particle> &particle : EngineContext::get().particleHandler().iterator())
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

// GameAppImpl construction/teardown and accessors were relocated to
// game/Graphics/graphic_init.cpp (egolib-game-graphics) alongside the GFX bodies.
