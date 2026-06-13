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

/// @file egolib/game/graphic_lighting.c
/// @brief Grid illumination and dynamic-light accumulation helpers.

#include "egolib/game/graphic.h"

#include "egolib/game/graphic_internal.h"
#include "egolib/game/graphic_fan.h"
#include "egolib/game/Graphics/Camera.hpp"
#include "egolib/game/Graphics/TileList.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/FileFormats/Globals.hpp"
#include "egolib/game/game.h"
#include "egolib/game/mesh.h"

//--------------------------------------------------------------------------------------------

using namespace Ego::Time;
using namespace gfx_internal;

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

gfx_rv GridIllumination::light_fans_throttle_update(ego_mesh_t * mesh, ego_tile_info_t& tile, const Index1D& tileIndex, float threshold)
{
    bool retval = false;

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

void GridIllumination::light_fans(Ego::Graphics::TileList& tl)
{
    light_fans_update_lcache(tl);
    light_fans_update_clst(tl);
}
