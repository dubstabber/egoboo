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
#include "egolib/game/graphic_fan.h"
#include "egolib/Script/script.h"
#include "egolib/game/script_compile.h"
#include "egolib/FileFormats/Globals.hpp"
#include "egolib/game/game.h"
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
#include "egolib/game/mesh.h"
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

namespace
{
GameEngine& engine()
{
    return EngineContext::get().engine();
}

Ego::GUI::UIManager& uiManager()
{
    return *engine().getUIManager();
}

GameSessionContext& gameSession()
{
    return GameSessionContext::get();
}

GameModule& activeModule()
{
    return gameSession().activeModule();
}

uint32_t renderedFrameCount()
{
    return engine().getNumberOfFramesRendered();
}

uint32_t& worldUpdateCount()
{
    return gameSession().worldUpdateCount();
}
}

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

static bool sum_global_lighting(std::array<float, LIGHTING_VEC_SIZE> &lighting);

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
// grid_lighting FUNCTIONS
//--------------------------------------------------------------------------------------------
float GridIllumination::grid_lighting_test(const ego_mesh_t& mesh, GLXvector3f pos, float& low_diff, float& hgh_diff)
{
    const lighting_cache_t *cache_list[4];

    int ix = std::floor(pos[XX] / Info<float>::Grid::Size());
    int iy = std::floor(pos[YY] / Info<float>::Grid::Size());

    Index1D fan[4];
    fan[0] = mesh.getTileIndex(Index2D(ix, iy));
    fan[1] = mesh.getTileIndex(Index2D(ix + 1, iy));
    fan[2] = mesh.getTileIndex(Index2D(ix, iy + 1));
    fan[3] = mesh.getTileIndex(Index2D(ix + 1, iy + 1));

    for (size_t cnt = 0; cnt < 4; cnt++)
    {
        cache_list[cnt] = nullptr;
		if (fan[cnt] == Index1D::Invalid) {
			cache_list[cnt] = nullptr;
		} else {
			cache_list[cnt] = &(mesh.getTileInfo(fan[cnt])._cache);
		}
    }

    float u = pos[XX] / Info<float>::Grid::Size() - ix;
    float v = pos[YY] / Info<float>::Grid::Size() - iy;

    return lighting_cache_test(cache_list, u, v, low_diff, hgh_diff);
}

//--------------------------------------------------------------------------------------------
float GridIllumination::light_corners(ego_mesh_t& mesh, ego_tile_info_t& tile, bool reflective, float mesh_lighting_keep)
{
	// if no update is requested, return an "error value"
	if (!tile._lightingCache.getNeedUpdate())
	{
		return -1.0f;
	}

	// has the lighting already been calculated this frame?
	if (tile._lightingCache.isValid(renderedFrameCount()))
	{
		return -1.0f;
	}

	// get the normal and lighting cache for this tile
	tile_mem_t& ptmem = mesh._tmem;
	normal_cache_t& ncache = tile._ncache;
	light_cache_t& lcache = tile._lightingCache._contents;
	light_cache_t& d1_cache = tile._vertexLightingCache._d1_cache;
	light_cache_t& d2_cache = tile._vertexLightingCache._d2_cache;

	float max_delta = 0.0f;
	for (size_t corner = 0; corner < 4; corner++)
	{
		GLXvector3f& pnrm = ncache[corner];
		float& plight = lcache[corner];
		float& pdelta1 = d1_cache[corner];
		float& pdelta2 = d2_cache[corner];
		GLXvector3f& ppos = ptmem._plst[tile._vrtstart + corner];

		float light_old, delta, light_tmp;
		float light_new = 0.0f;
		light_one_corner(mesh, tile, reflective, 
			             Ego::Vector3f(ppos[0], ppos[1], ppos[2]),
			             Ego::Vector3f(pnrm[0], pnrm[1], pnrm[2]),
			             light_new);

		if (plight != light_new)
		{
			light_old = plight;
			plight = light_old * mesh_lighting_keep + light_new * (1.0f - mesh_lighting_keep);

			// measure the actual delta
			delta = std::abs(light_old - plight);

			// measure the relative change of the lighting
			light_tmp = 0.5f * (std::abs(plight) + std::abs(light_old));
			if (0.0f == light_tmp)
			{
				delta = 10.0f;
			}
			else
			{
				delta /= light_tmp;
				delta = Ego::Math::constrain(delta, 0.0f, 10.0f);
			}

			// add in the actual change this update
			pdelta2 += std::abs(delta);

			// update the estimate to match the actual change
			pdelta1 = pdelta2;
		}

		max_delta = std::max(max_delta, pdelta1);
	}

	// un-mark the lcache
	tile._lightingCache.setNeedUpdate(false);
	tile._lightingCache.setLastFrame(renderedFrameCount());

	return max_delta;
}

bool GridIllumination::grid_lighting_interpolate(const ego_mesh_t& mesh, lighting_cache_t& dst, const Ego::Vector2f& pos)
{
    // grab this tile's coordinates
    int ix = std::floor(pos[XX] / Info<float>::Grid::Size()),
        iy = std::floor(pos[YY] / Info<float>::Grid::Size());

    // find the tile id for the surrounding tiles
	Index1D fan[4];
    fan[0] = mesh.getTileIndex(Index2D(ix, iy));
    fan[1] = mesh.getTileIndex(Index2D(ix + 1, iy));
    fan[2] = mesh.getTileIndex(Index2D(ix, iy + 1));
    fan[3] = mesh.getTileIndex(Index2D(ix + 1, iy + 1));

	std::array<const lighting_cache_t *,4> cache_list;
    for (size_t cnt = 0; cnt < 4; cnt++)
    {
		if (fan[cnt] == Index1D::Invalid) {
			cache_list[cnt] = nullptr;
		} else {
			cache_list[cnt] = &(mesh.getTileInfo(fan[cnt])._cache);
		}
    }

    // grab the coordinates relative to the parent tile
	float u = pos[XX] / Info<float>::Grid::Size() - ix,
		  v = pos[YY] / Info<float>::Grid::Size() - iy;

    return lighting_cache_t::lighting_cache_interpolate(dst, cache_list, u, v);
}

void GridIllumination::test_one_corner(const ego_mesh_t& mesh, GLXvector3f pos, float& pdelta)
{
	// interpolate the lighting for the given corner of the mesh
	float low_delta, hgh_delta;
	pdelta = grid_lighting_test(mesh, pos, low_delta, hgh_delta);

	// determine the weighting
	float hgh_wt, low_wt;
	hgh_wt = (pos[ZZ] - mesh._tmem._bbox.get_min()[kZ]) / (mesh._tmem._bbox.get_max()[kZ] - mesh._tmem._bbox.get_min()[kZ]);
	hgh_wt = Ego::Math::constrain(hgh_wt, 0.0f, 1.0f);
	low_wt = 1.0f - hgh_wt;

	pdelta = low_wt * low_delta + hgh_wt * hgh_delta;
}

bool GridIllumination::test_corners(const ego_mesh_t& mesh, ego_tile_info_t& tile, float threshold)
{
	if (threshold < 0.0f) threshold = 0.0f;

	// get the lighting and per-vertex lighting cache for this tile
	light_cache_t& lcache = tile._lightingCache._contents;
	light_cache_t& d1_cache = tile._vertexLightingCache._d1_cache;

	bool retval = false;
	for (size_t corner = 0; corner < 4; corner++)
	{
		float& pdelta = d1_cache[corner];
		float& plight = lcache[corner];
		GLXvector3f& ppos = mesh._tmem._plst[tile._vrtstart + corner];

		float delta;
		test_one_corner(mesh, ppos, delta);

		if (0.0f == plight)
		{
			delta = 10.0f;
		}
		else
		{
			delta /= plight;
			delta = Ego::Math::constrain(delta, 0.0f, 10.0f);
		}

		pdelta += delta;

		if (pdelta > threshold)
		{
			retval = true;
		}
	}

	return retval;
}

void GridIllumination::light_one_corner(ego_mesh_t& mesh, ego_tile_info_t& tile, const bool reflective, const Ego::Vector3f& pos, const Ego::Vector3f& nrm, float& plight)
{
	// interpolate the lighting for the given corner of the mesh
	lighting_cache_t grid_light;
	grid_lighting_interpolate(mesh, grid_light, Ego::Vector2f(pos[kX], pos[kY]));

	if (reflective) {
		float light_dir, light_amb;
		lighting_cache_t::lighting_evaluate_cache(grid_light, nrm, pos[ZZ], mesh._tmem._bbox, &light_amb, &light_dir);

		// make ambient light only illuminate 1/2
		plight = light_amb + 0.5f * light_dir;
	} else {
		plight = lighting_cache_t::lighting_evaluate_cache(grid_light, nrm, pos[ZZ], mesh._tmem._bbox, NULL, NULL);
	}
}

bool GridIllumination::light_corner(ego_mesh_t& mesh, const Index1D& fan, float height, float nrm[], float& plight)
{
	ego_tile_info_t& ptile = mesh.getTileInfo(fan);

	// get the grid lighting
	const lighting_cache_t& lighting = ptile._cache;

	bool reflective = (0 != ptile.testFX(MAPFX_REFLECTIVE));

	// evaluate the grid lighting at this node
	if (reflective)
	{
		float light_dir, light_amb;

		lighting_cache_t::lighting_evaluate_cache(lighting, Ego::Vector3f(nrm[0], nrm[1], nrm[2]), height, mesh._tmem._bbox, &light_amb, &light_dir);

		// make ambient light only illuminate 1/2
		plight = light_amb + 0.5f * light_dir;
	} else {
		plight = lighting_cache_t::lighting_evaluate_cache(lighting, Ego::Vector3f(nrm[0], nrm[1], nrm[2]), height, mesh._tmem._bbox, NULL, NULL);
	}

	// clip the light to a reasonable value
	plight = Ego::Math::constrain(plight, 0.0f, 255.0f);

	return true;
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

//--------------------------------------------------------------------------------------------
// LIGHTING FUNCTIONS
//--------------------------------------------------------------------------------------------
gfx_rv GridIllumination::light_fans_throttle_update(ego_mesh_t * mesh, ego_tile_info_t& tile, const Index1D& tileIndex, float threshold)
{
    bool       retval = false;

    if (!mesh)
    {
		throw idlib::argument_null_error(__FILE__, __LINE__, "mesh");
    }
	tile_mem_t& tmem = mesh->_tmem;

#if defined(CLIP_LIGHT_FANS) && !defined(CLIP_ALL_LIGHT_FANS)

    // visible fans based on the update "need"
    retval = test_corners(*mesh, tile, threshold);

    // update every 4 fans even if there is no need
    if (!retval)
    {
        // use a kind of checkerboard pattern
        auto i2 = Grid::map<int>(tileIndex, (int)tmem.getInfo().getTileCountX());
        if (0 != (((i2.x() ^ i2.y()) + renderedFrameCount()) & 0x03))
        {
            retval = true;
        }
    }

#else
    retval = true;
#endif

    return retval ? gfx_success : gfx_fail;
}

//--------------------------------------------------------------------------------------------
void GridIllumination::light_fans_update_lcache(Ego::Graphics::TileList& tl)
{
	const int frame_skip = 1 << 2; // 1 << 2 ~ 2^2 ~ 4. 
#if defined(CLIP_ALL_LIGHT_FANS)
	const int frame_mask = frame_skip - 1; // 4 - 1 = 3 = binary(11).
#endif

	/// @note we are measuring the change in the intensity at the corner of a tile (the "delta") as
	/// a fraction of the current intensity. This is because your eye is much more sensitive to
	/// intensity differences when the intensity is low.
	///
	/// @note it is normally assumed that 64 colors of gray can make a smoothly colored black and white picture
	/// which means that the threshold could be set as low as 1/64 = 0.015625.
	const float delta_threshold = 0.05f;
	
	bool is_valid;

	auto mesh = tl.getMesh();
	if (!mesh)
	{
		throw idlib::runtime_error(__FILE__, __LINE__, "tile list not attached to a mesh");
	}

#if defined(CLIP_ALL_LIGHT_FANS)
	// Update all visible fans once every 4 frames.
	if (0 != (renderedFrameCount() & frame_mask)) {
		return;
}
#endif

#if !defined(CLIP_LIGHT_FANS)
    // update only every frame
	float local_mesh_lighting_keep = 0.9f;
#else
    // update only every 4 frames
	float local_mesh_lighting_keep = std::pow(0.9f, frame_skip);
#endif

    // cache the grid lighting
    for (size_t entry = 0; entry < tl._all.size(); entry++)
    {
        // which tile?
        Index1D fan = tl._all[entry].getIndex();

        // grab a pointer to the tile
		ego_tile_info_t& ptile = mesh->getTileInfo(fan);

        // Test to see whether the lcache was already updated
        // - ptile->_lcache_frame < 0 means that the cache value is invalid.
        // - ptile->_lcache_frame is updated inside ego_mesh_light_corners()
#if defined(CLIP_LIGHT_FANS)
        // clip the updated on each individual tile
        is_valid = ptile._lightingCache.isValid(renderedFrameCount(), frame_skip);
#else
        // let the function clip all tile updates
        is_valid = ptile._lightingCache.isValid(renderedFrameCount());
#endif
	if (is_valid)
        {
            continue;
        }

        // If no update was requested ...
        if (!ptile._lightingCache.getNeedUpdate())
        {
			// ... do we need one?
            gfx_rv light_fans_rv = light_fans_throttle_update(mesh.get(), ptile, fan, delta_threshold);
            ptile._lightingCache.setNeedUpdate(gfx_success == light_fans_rv);
        }

		// If there is still no need for an update, go to the next tile.
		if (!ptile._lightingCache.getNeedUpdate()) {
			continue;
		}

        // is the tile reflective?
        bool reflective = (0 != ptile.testFX(MAPFX_REFLECTIVE));

        // light the corners of this tile
        float delta = GridIllumination::light_corners(*mesh, ptile, reflective, local_mesh_lighting_keep);

#if defined(CLIP_LIGHT_FANS)
        // Use the actual maximum change in the intensity at a tile corner to
        // signal whether we need to calculate the next stage.
        ptile._vertexLightingCache.setNeedUpdate(delta > delta_threshold);
#else
        // make sure that ego_mesh_light_corners() did not return an "error value"
        ptile._vertexLightingCache.setNeedUpdate(delta > 0.0f);
#endif
    }
}

//--------------------------------------------------------------------------------------------

float GridIllumination::grid_get_mix(float u0, float u, float v0, float v) {
    // Get the distance of u and v from u0 and v0.
    float du = u - u0,
        dv = v - v0;

    // If the absolute distance du or dv is greater than 1,
    // return 0.
    if (std::abs(du) > 1.0f || std::abs(dv) > 1.0f) {
        return 0.0f;
    }
    // The distances are within the bounds of [-1,+1] at this point.
    // The original formulas are
    // wt_u = (1.0f - du)*(1.0f + du)
    // wt_v = (1.0f - dv)*(1.0f + dv)
    // However, a term of the form
    // y = (1 - x) * (1 + x)
    // can be simplified to
    // y = (1 - x) * 1 + (1 - x) * x
    //   = (1 - x) + (1 - x) * x
    //   = 1 - x + x - x^2
    //   = 1 - x^2
    // Hence the original formulas become
    // wt_u = 1.0f - du * du)
    // wt_v = 1.0f - dv * dv
    float wt_u = 1.0f - du * du,
        wt_v = 1.0f - dv * dv;

    return wt_u * wt_v;
}

float GridIllumination::ego_mesh_interpolate_vertex(const ego_tile_info_t& info, const GLXvector3f& position) {
    const oct_bb_t& boundingBox = info._oct;
    const light_cache_t& lightCache = info._lightingCache._contents;

    // Set the lighting to 0.
    float light = 0.0f;

    // Determine texture coordinates of the specified point.
    float u = (position[XX] - boundingBox._mins[OCT_X]) / (boundingBox._maxs[OCT_X] - boundingBox._mins[OCT_X]);
    float v = (position[YY] - boundingBox._mins[OCT_Y]) / (boundingBox._maxs[OCT_Y] - boundingBox._mins[OCT_Y]);

    // Interpolate the lighting at the four vertices of the tile.
    // to determine the final lighting at the specified point.
    float weightedSum = 0.0f;
    for (size_t i = 0; i < 4; ++i) {
        // Mix the u, v coordinate pairs (0,0),
        // (1,0), (1,1), and (0,1) using the
        // texture coordinates of the specified
        // point.
        static const float ix_off[4] = {0.0f, 1.0f, 1.0f, 0.0f},
            iy_off[4] = {0.0f, 0.0f, 1.0f, 1.0f};
        float mix = grid_get_mix(ix_off[i], u, iy_off[i], v);

        weightedSum += mix;
        light += mix * lightCache[i];
    }

    // Normalize to the weighted sum.
    if (light > 0.0f && weightedSum > 0.0f) {
        light /= weightedSum;
        light = Ego::Math::constrain(light, 0.0f, 255.0f);
    } else {
        light = 0.0f;
    }
    return light;
}

void GridIllumination::light_fans_update_clst(Ego::Graphics::TileList& tl)
{
    /// @author BB
    /// @details update the tile's color list, if needed
    auto mesh = tl.getMesh();
    if (!mesh)
    {
		throw idlib::runtime_error(__FILE__, __LINE__, "tile list is not attached to a mesh");
    }

    // alias the tile memory
	tile_mem_t& ptmem = mesh->_tmem;

    // use the grid to light the tiles
    for (size_t entry = 0; entry < tl._all.size(); entry++)
    {
        Index1D fan = tl._all[entry].getIndex();
        if (Index1D::Invalid == fan) continue;

        // valid tile?
		ego_tile_info_t& ptile = mesh->getTileInfo(fan);

        // Do nothing if this tile does not need an update.
        if (!ptile._vertexLightingCache.getNeedUpdate()) {
            continue;
        }

        // Do nothing if the update was performed in this frame.
        if (ptile._vertexLightingCache.isValid(renderedFrameCount())) {
            continue;
        }

		size_t numberOfVertices;
		tile_definition_t *pdef = tile_dict.get(ptile._type);
        if (nullptr != pdef) {
			numberOfVertices = pdef->numvertices;
        } else {
			numberOfVertices = 4;
        }

		size_t index, vertex;
        // copy the 1st 4 vertices
        for (index = 0, vertex = ptile._vrtstart; index < 4; index++, vertex++)
        {
            GLXvector3f& color = ptmem._clst[vertex];
            float light = ptile._lightingCache._contents[index];
			color[RR] = color[GG] = color[BB] 
				= idlib::fraction<float, 1, 255>() * Ego::Math::constrain(light, 0.0f, 255.0f);
        }

        for ( /* Intentionall left empty. */; index < numberOfVertices; index++, vertex++)
        {
			GLXvector3f& color = ptmem._clst[vertex];
			const GLXvector3f& position = ptmem._plst[vertex];
			float light = ego_mesh_interpolate_vertex(ptile, position);
			color[RR] = color[GG] = color[BB] 
				= idlib::fraction<float, 1, 255>() * Ego::Math::constrain(light, 0.0f, 255.0f);
        }

        // clear out the deltas
        ptile._vertexLightingCache._d1_cache.fill(0.0f);
        ptile._vertexLightingCache._d2_cache.fill(0.0f);

        // This tile was updated this frame and does not require an update (for some time).
		ptile._vertexLightingCache.setNeedUpdate(false);
		ptile._vertexLightingCache._lastFrame = renderedFrameCount();
    }
}

//--------------------------------------------------------------------------------------------
void GridIllumination::light_fans(Ego::Graphics::TileList& tl)
{
	light_fans_update_lcache(tl);
	light_fans_update_clst(tl);
}

//--------------------------------------------------------------------------------------------
float get_ambient_level()
{
    /// @author BB
    /// @details get the actual global ambient level
    float glob_amb = 0.0f;
    float min_amb = 0.0f;
    if (gfx.usefaredge)
    {
        // for outside modules, max light_a means bright sunlight
        // this should be handled with directional lighting, so ambient light is 0
        glob_amb = light_a * 255.0f;
        //glob_amb = 0;
    }
    else
    {
        // for inside modules, max light_a means dingy dungeon lighting
        glob_amb = light_a * 64.0f;
    }

    // determine the minimum ambient, based on darkvision
    min_amb = INVISIBLE / 4;
    if (local_stats.seedark_mag > 0.0f)
    {
        // give a iny boost in the case of no light
        // start with the global light
        min_amb = std::max(glob_amb, min_amb) + 1.0f;

        // light_a can be quite dark, so we need a large magnification
        min_amb *= std::pow(local_stats.seedark_mag, 5);
    }

    return std::max(glob_amb, min_amb);
}

//--------------------------------------------------------------------------------------------
bool sum_global_lighting(std::array<float, LIGHTING_VEC_SIZE> &lighting)
{
    /// @author BB
    /// @details do ambient lighting. if the module is inside, the ambient lighting
    /// is reduced by up to a factor of 8. It is still kept just high enough
    /// so that ordnary objects will not be made invisible. This was breaking some of the AIs

    int cnt;
    float glob_amb;

    glob_amb = get_ambient_level();

    for (cnt = 0; cnt < LVEC_AMB; cnt++)
    {
        lighting[cnt] = 0.0f;
    }
    lighting[LVEC_AMB] = glob_amb;

    if (!gfx.usefaredge) return true;

    // do "outside" directional lighting (i.e. sunlight)
    lighting_vector_sum(lighting, light_nrm, light_d * 255, 0.0f);

    return true;
}

dynalist_t::dynalist_t()
    : frame(-1), size(0), lst{}
{}

void dynalist_t::init(dynalist_t& self) {
    self.size = 0;
}

//--------------------------------------------------------------------------------------------
gfx_rv GridIllumination::do_grid_lighting(Ego::Graphics::TileList& tl, dynalist_t& dyl, Camera& cam)
{
    /// @author ZZ
    /// @details Do all tile lighting, dynamic and global

    size_t cnt;

    int    tnc;

    float x0, y0, local_keep;
    bool needs_dynalight;

    std::array<float, LIGHTING_VEC_SIZE> global_lighting = {0};

    size_t               reg_count = 0;
    dynalight_registry_t reg[TOTAL_MAX_DYNA];

    ego_frect_t mesh_bound, light_bound;
    dynalight_data_t fake_dynalight;

	auto mesh = tl.getMesh();
    if (!mesh)
    {
		throw idlib::runtime_error(__FILE__, __LINE__, "tile list not attached to a mesh");
    }

	Ego::MeshInfo& pinfo = mesh->_info;
	tile_mem_t& tmem = mesh->_tmem;

    // find a bounding box for the "frustum"
    mesh_bound.xmin = tmem._edge_x;
    mesh_bound.xmax = 0;
    mesh_bound.ymin = tmem._edge_y;
    mesh_bound.ymax = 0;
    for (size_t entry = 0; entry < tl._all.size(); entry++)
    {
        Index1D fan = tl._all[entry].getIndex();
        if (fan.i() >= pinfo.getTileCount()) continue;

		const oct_bb_t& poct = tmem.get(fan)._oct;

        mesh_bound.xmin = std::min(mesh_bound.xmin, poct._mins[OCT_X]);
        mesh_bound.xmax = std::max(mesh_bound.xmax, poct._maxs[OCT_X]);
        mesh_bound.ymin = std::min(mesh_bound.ymin, poct._mins[OCT_Y]);
        mesh_bound.ymax = std::max(mesh_bound.ymax, poct._maxs[OCT_Y]);
    }

    // is the visible mesh list empty?
    if (mesh_bound.xmin >= mesh_bound.xmax || mesh_bound.ymin >= mesh_bound.ymax)
        return gfx_success;

    // clear out the dynalight registry
    reg_count = 0;

    // refresh the dynamic light list
    gfx_make_dynalist(dyl, cam);

    // assume no dynamic lighting
    needs_dynalight = false;

    // assume no "extra help" for systems with only flat lighting
    dynalight_data_t::init(fake_dynalight);

    // initialize the light_bound
    light_bound.xmin = tmem._edge_x;
    light_bound.xmax = 0;
    light_bound.ymin = tmem._edge_y;
    light_bound.ymax = 0;

    // make bounding boxes for each dynamic light
    if (gfx.gouraudShading_enable)
    {
        for (cnt = 0; cnt < dyl.size; cnt++)
        {
            float radius;
            ego_frect_t ftmp;

            dynalight_data_t& pdyna = dyl.lst[cnt];

            if (pdyna.falloff <= 0.0f || 0.0f == pdyna.level) continue;

            radius = std::sqrt(pdyna.falloff * 765.0f * 0.5f);

            // find the intersection with the frustum boundary
            ftmp.xmin = std::max(pdyna.pos[kX] - radius, mesh_bound.xmin);
            ftmp.xmax = std::min(pdyna.pos[kX] + radius, mesh_bound.xmax);
            ftmp.ymin = std::max(pdyna.pos[kY] - radius, mesh_bound.ymin);
            ftmp.ymax = std::min(pdyna.pos[kY] + radius, mesh_bound.ymax);

            // check to see if it intersects the "frustum"
            if (ftmp.xmin >= ftmp.xmax || ftmp.ymin >= ftmp.ymax) continue;

            reg[reg_count].bound = ftmp;
            reg[reg_count].reference = cnt;
            reg_count++;

            // determine the maxumum bounding box that encloses all valid lights
            light_bound.xmin = std::min(light_bound.xmin, ftmp.xmin);
            light_bound.xmax = std::max(light_bound.xmax, ftmp.xmax);
            light_bound.ymin = std::min(light_bound.ymin, ftmp.ymin);
            light_bound.ymax = std::max(light_bound.ymax, ftmp.ymax);
        }

        // are there any dynalights visible?
        if (reg_count > 0 && light_bound.xmax >= light_bound.xmin && light_bound.ymax >= light_bound.ymin)
        {
            needs_dynalight = true;
        }
    }
    else
    {
        float dyna_weight = 0.0f;
        float dyna_weight_sum = 0.0f;

        // evaluate all the lights at the camera position
        for (cnt = 0; cnt < dyl.size; cnt++)
        {
			dynalight_data_t& pdyna = dyl.lst[cnt];

            // evaluate the intensity at the camera
			Ego::Vector3f diff = pdyna.pos - cam.getCenter() - Ego::Vector3f(0.0f, 0.0f, 90.0f); // evaluate at the "head height" of a character

            dyna_weight = std::abs(dyna_lighting_intensity(&pdyna, diff));

            fake_dynalight.distance += dyna_weight * pdyna.distance;
            fake_dynalight.falloff += dyna_weight * pdyna.falloff;
            fake_dynalight.level += dyna_weight * pdyna.level;
            fake_dynalight.pos += (pdyna.pos - cam.getCenter()) * dyna_weight;

            dyna_weight_sum += dyna_weight;
        }

        // use a single dynalight to represent the sum of all dynalights
        if (dyna_weight_sum > 0.0f)
        {
            float radius;
            ego_frect_t ftmp;

            fake_dynalight.distance /= dyna_weight_sum;
            fake_dynalight.falloff /= dyna_weight_sum;
            fake_dynalight.level /= dyna_weight_sum;
            fake_dynalight.pos = (fake_dynalight.pos * (1.0/dyna_weight_sum)) + cam.getCenter();

            radius = std::sqrt(fake_dynalight.falloff * 765.0f * 0.5f);

            // find the intersection with the frustum boundary
            ftmp.xmin = std::max(fake_dynalight.pos[kX] - radius, mesh_bound.xmin);
            ftmp.xmax = std::min(fake_dynalight.pos[kX] + radius, mesh_bound.xmax);
            ftmp.ymin = std::max(fake_dynalight.pos[kY] - radius, mesh_bound.ymin);
            ftmp.ymax = std::min(fake_dynalight.pos[kY] + radius, mesh_bound.ymax);

            // make a fake light bound
            light_bound = ftmp;

            // register the fake dynalight
            reg[reg_count].bound = ftmp;
            reg[reg_count].reference = -1;
            reg_count++;

            // let the downstream calc know we are coming
            needs_dynalight = true;
        }
    }

    // sum up the lighting from global sources
    sum_global_lighting(global_lighting);

    // make the grids update their lighting every 4 frames
    local_keep = 0.0f; //std::pow(DYNALIGHT_KEEP, 4); //const static float DYNALIGHT_KEEP = 0.9f;

    // Add to base light level in normal mode
    for (size_t entry = 0; entry < tl._all.size(); entry++)
    {
        bool resist_lighting_calculation = true;

        // grab each grid box in the "frustum"
        Index1D fan = tl._all[entry].getIndex();

        // a valid tile?
        ego_tile_info_t& ptile = mesh->getTileInfo(fan);

        // do not update this more than once a frame
        if (ptile._cache_frame >= 0 && (uint32_t)ptile._cache_frame >= renderedFrameCount()) continue;
        auto i2 = Grid::map<int>(fan, pinfo.getTileCountX());
        // Resist the lighting calculation?
        // This is a speedup for lighting calculations so that
        // not every light-tile calculation is done every single frame
        resist_lighting_calculation = (0 != (((i2.x() + i2.y()) ^ renderedFrameCount()) & 0x03));

        if (resist_lighting_calculation) continue;

        // this is not a "bad" grid box, so grab the lighting info
        lighting_cache_t& pcache_old = ptile._cache;

        lighting_cache_t cache_new;
        cache_new.init(); /// @todo Not needed because of constructor.

        // copy the global lighting
        for (tnc = 0; tnc < LIGHTING_VEC_SIZE; tnc++)
        {
            cache_new.low._lighting[tnc] = global_lighting[tnc];
            cache_new.hgh._lighting[tnc] = global_lighting[tnc];
        };

        // do we need any dynamic lighting at all?
        if (needs_dynalight)
        {
            // calculate the local lighting

            ego_frect_t fgrid_rect;

            x0 = i2.x() * Info<float>::Grid::Size();
            y0 = i2.y() * Info<float>::Grid::Size();

            // check this grid vertex relative to the measured light_bound
            fgrid_rect.xmin = x0 - Info<float>::Grid::Size() * 0.5f;
            fgrid_rect.xmax = x0 + Info<float>::Grid::Size() * 0.5f;
            fgrid_rect.ymin = y0 - Info<float>::Grid::Size() * 0.5f;
            fgrid_rect.ymax = y0 + Info<float>::Grid::Size() * 0.5f;

            // check the bounding box of this grid vs. the bounding box of the lighting
            if (fgrid_rect.xmin <= light_bound.xmax && fgrid_rect.xmax >= light_bound.xmin)
            {
                if (fgrid_rect.ymin <= light_bound.ymax && fgrid_rect.ymax >= light_bound.ymin)
                {
                    // this grid has dynamic lighting. add it.
                    for (cnt = 0; cnt < reg_count; cnt++)
                    {
						Ego::Vector3f nrm;
                        dynalight_data_t *pdyna;

                        // does this dynamic light intersects this grid?
                        if (fgrid_rect.xmin > reg[cnt].bound.xmax || fgrid_rect.xmax < reg[cnt].bound.xmin) continue;
                        if (fgrid_rect.ymin > reg[cnt].bound.ymax || fgrid_rect.ymax < reg[cnt].bound.ymin) continue;

                        // this should be a valid intersection, so proceed
                        tnc = reg[cnt].reference;
                        if (tnc < 0)
                        {
                            pdyna = &fake_dynalight;
                        }
                        else
                        {
                            pdyna = dyl.lst + tnc;
                        }

                        nrm[kX] = pdyna->pos[kX] - x0;
                        nrm[kY] = pdyna->pos[kY] - y0;
                        nrm[kZ] = pdyna->pos[kZ] - tmem._bbox.get_min()[ZZ];
                        sum_dyna_lighting(pdyna, cache_new.low._lighting, nrm);

                        nrm[kZ] = pdyna->pos[kZ] - tmem._bbox.get_max()[ZZ];
                        sum_dyna_lighting(pdyna, cache_new.hgh._lighting, nrm);
                    }
                }
            }
        }
        else if (!gfx.gouraudShading_enable)
        {
            // evaluate the intensity at the camera
        }

        // blend in the global lighting every single time
        // average this in with the existing lighting
        lighting_cache_t::blend(pcache_old, cache_new, local_keep);

        // find the max intensity
        pcache_old.max_light();

        ptile._cache_frame = renderedFrameCount();
    }

    return gfx_success;
}
//--------------------------------------------------------------------------------------------
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
        if(pchr->inst.updateVertices(-1, -1, true) == gfx_error) {
            retval = gfx_error;
        }

        // the instance has changed, refresh the collision bound
        else {
            pchr->getObjectPhysics().updateCollisionSize(true);            
        }

        // do the basic lighting
        pchr->inst.updateLighting();
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
