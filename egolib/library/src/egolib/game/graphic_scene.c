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

/// @file egolib/game/graphic_scene.c
/// @brief World-render orchestration helpers for the graphics shell

#include "egolib/game/graphic_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

#include "egolib/Entities/_Include.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/Physics/ICollisionWorld.hpp"
#include "egolib/game/graphic_fan.h"
#include "egolib/game/graphic_prt.h"
#include "egolib/game/Graphics/EntityList.hpp"
#include "egolib/game/Graphics/RenderPass.hpp"
#include "egolib/game/Graphics/TileList.hpp"
#include "egolib/game/renderer_3d.h"
#include "egolib/game/Graphics/CameraSystem.hpp"
#include "egolib/game/Graphics/BillboardSystem.hpp"  // Ego::Graphics::BillboardSystem (complete type for render_all)
#include "egolib/game/Module/Passage.hpp"

#include <algorithm>
#include <memory>
#include <vector>

using namespace Ego::Time;

Clock<ClockPolicy::NonRecursive> sortDoListUnreflected_timer("render.sortDoListUnreflected", 512);
Clock<ClockPolicy::NonRecursive> sortDoListReflected_timer("render.sortDoListReflected", 512);
Clock<ClockPolicy::NonRecursive> render_scene_init_timer("render.scene.init", 512);
Clock<ClockPolicy::NonRecursive> render_scene_mesh_timer("render.scene.mesh", 512);
Clock<ClockPolicy::NonRecursive> gfx_make_tileList_timer("gfx.make.tileList", 512);
Clock<ClockPolicy::NonRecursive> gfx_make_entityList_timer("gfx.make.entityList", 512);
Clock<ClockPolicy::NonRecursive> do_grid_lighting_timer("do.grid.lighting", 512);
Clock<ClockPolicy::NonRecursive> light_fans_timer("light.fans", 512);

namespace
{
using namespace gfx_internal;

gfx_rv gfx_make_entityList(Ego::Graphics::EntityList& el, Camera& cam);
gfx_rv gfx_make_tileList(Ego::Graphics::TileList& tl, Camera& cam);
gfx_rv gfx_update_flashing(Ego::Graphics::EntityList& el);

gfx_rv render_scene_init(Ego::Graphics::TileList& tl, Ego::Graphics::EntityList& el, dynalist_t& dyl, Camera& cam)
{
    gfx_rv retval = gfx_success;

    {
        ClockScope<ClockPolicy::NonRecursive> scope(gfx_make_tileList_timer);
        if (gfx_error == gfx_make_tileList(tl, cam))
        {
            retval = gfx_error;
        }
    }

    auto mesh = tl.getMesh();
    if (!mesh)
    {
        throw idlib::runtime_error(__FILE__, __LINE__, "tile list is not attached to a mesh");
    }

    {
        ClockScope<ClockPolicy::NonRecursive> scope(gfx_make_entityList_timer);
        if (gfx_error == gfx_make_entityList(el, cam))
        {
            retval = gfx_error;
        }
    }

    {
        ClockScope<ClockPolicy::NonRecursive> scope(do_grid_lighting_timer);
        if (gfx_error == GridIllumination::do_grid_lighting(tl, dyl, cam))
        {
            retval = gfx_error;
        }
    }

    {
        ClockScope<ClockPolicy::NonRecursive> scope(light_fans_timer);
        GridIllumination::light_fans(tl);
    }

    {
        ClockScope<ClockPolicy::NonRecursive> scope(EngineContext::get().gfx().updateObjectInstancesTimer());
        if (gfx_error == EngineContext::get().gfx().update_object_instances(cam))
        {
            retval = gfx_error;
        }
    }

    {
        ClockScope<ClockPolicy::NonRecursive> scope(EngineContext::get().gfx().updateParticleInstancesTimer());
        if (gfx_error == EngineContext::get().gfx().update_particle_instances(cam))
        {
            retval = gfx_error;
        }
    }

    if (gfx_error == gfx_update_flashing(el))
    {
        retval = gfx_error;
    }

    return retval;
}

gfx_rv render_scene(Camera& cam, Ego::Graphics::TileList& tl, Ego::Graphics::EntityList& el)
{
    gfx_rv retval = gfx_success;
    {
        ClockScope<ClockPolicy::NonRecursive> clockScope(render_scene_init_timer);
        if (gfx_error == render_scene_init(tl, el, EngineContext::get().gfx().getDynalist(), cam))
        {
            retval = gfx_error;
        }
    }
    {
        ClockScope<ClockPolicy::NonRecursive> clockScope(render_scene_mesh_timer);
        {
            ClockScope<ClockPolicy::NonRecursive> clockScope2(sortDoListReflected_timer);
            el.sort(cam, true);
        }
        animate_all_tiles(*tl.getMesh());
        EngineContext::get().gfx().getNonReflective().run(cam, tl, el);
        EngineContext::get().gfx().getReflective0().run(cam, tl, el);
        EngineContext::get().gfx().getEntityReflections().run(cam, tl, el);
        EngineContext::get().gfx().getReflective1().run(cam, tl, el);
        EngineContext::get().gfx().getHeightmap().run(cam, tl, el);
        EngineContext::get().gfx().getEntityShadows().run(cam, tl, el);
    }
    {
        ClockScope<ClockPolicy::NonRecursive> scope(sortDoListUnreflected_timer);
        el.sort(cam, false);
    }

    EngineContext::get().gfx().getOpaqueEntities().run(cam, tl, el);
    EngineContext::get().gfx().getWater().run(cam, tl, el);
    EngineContext::get().gfx().getNonOpaqueEntities().run(cam, tl, el);

    if (EngineContext::get().inputSystem().isKeyDown(SDLK_F8))
    {
        draw_passages(cam);
    }

#if defined(DRAW_PRT_GRIP_ATTACH)
    render_all_prt_attachment();
#endif

#if defined(DRAW_LISTS)
    Renderer3D::lineSegmentList.draw_all(cam);
    Renderer3D::pointList.draw_all(cam);
#endif

#if defined(DRAW_PRT_BBOX)
    ParticleGraphicsRenderer::render_all_prt_bbox();
#endif
    return retval;
}
} // namespace

void gfx_system_render_world(std::shared_ptr<Camera> camera, std::shared_ptr<Ego::Graphics::TileList> tileList, std::shared_ptr<Ego::Graphics::EntityList> entityList)
{
    if (!camera)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "camera");
    }
    if (!tileList)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "tileList");
    }
    if (!entityList)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "entityList");
    }

    Renderer3D::begin3D(*camera);
    {
        EngineContext::get().gfx().getBackground().run(*camera, *tileList, *entityList);
        render_scene(*camera, *tileList, *entityList);
        EngineContext::get().gfx().getForeground().run(*camera, *tileList, *entityList);

        if (camera->getMotionBlur() > 0)
        {
            if (camera->getMotionBlurOld() < 0.001f)
            {
                GL_DEBUG(glAccum)(GL_LOAD, 1);
            }
            if (true /*currentState != playingState*/)
            {
                GL_DEBUG(glAccum)(GL_MULT, camera->getMotionBlur());
                GL_DEBUG(glAccum)(GL_ACCUM, 1.0f - camera->getMotionBlur());
            }
            GL_DEBUG(glAccum)(GL_RETURN, 1.0f);
        }
    }
    Renderer3D::end3D();

    GFX::get().getBillboardSystem().render_all(*camera);
}

void draw_passages(Camera& cam)
{
    EngineContext::get().renderer().setProjectionMatrix(cam.getProjectionMatrix());
    EngineContext::get().renderer().setViewMatrix(cam.getViewMatrix());
    EngineContext::get().renderer().setWorldMatrix(idlib::identity<Ego::Matrix4f4f>());

    for (int i = 0; i < activeModule().getPassageCount(); ++i)
    {
        const Ego::AxisAlignedBox2f& passageABB = activeModule().getPassageByID(i)->getAxisAlignedBox2f();

        oct_bb_t bb;
        bb._mins[OCT_X] = passageABB.get_min().x();
        bb._maxs[OCT_X] = passageABB.get_max().x();
        bb._mins[OCT_Y] = passageABB.get_min().y();
        bb._maxs[OCT_Y] = passageABB.get_max().y();

        bb._mins[OCT_XY] = bb._mins[OCT_X];
        bb._maxs[OCT_XY] = bb._maxs[OCT_X];
        bb._mins[OCT_YX] = bb._mins[OCT_Y];
        bb._maxs[OCT_YX] = bb._maxs[OCT_Y];

        // TODO: should be mesh highest elevation at OCT_X and OCT_Y
        bb._mins[OCT_Z] = -100.0f;
        bb._maxs[OCT_Z] = 100.0f;

        Renderer3D::renderOctBB(bb, true, false);
    }
}

gfx_rv gfx_make_dynalist(dynalist_t& dyl, Camera& cam)
{
    using namespace gfx_internal;

    size_t tnc;
    Ego::Vector3f vdist;

    float distance = 0.0f;
    dynalight_data_t* plight = NULL;

    float distance_max = 0.0f;
    dynalight_data_t* plight_max = NULL;

    if ((uint32_t)(dyl.frame + 30) >= renderedFrameCount())
    {
        dyl.frame = -1;
    }

    if (dyl.frame >= 0 && (uint32_t)dyl.frame >= renderedFrameCount())
    {
        return gfx_success;
    }

    dynalist_t::init(dyl);

    for (const std::shared_ptr<Ego::Particle>& particle : EngineContext::get().particleHandler().iterator())
    {
        if (particle->isTerminated()) continue;

        dynalight_info_t& pprt_dyna = particle->dynalight;

        if (!pprt_dyna.on || 0.0f == pprt_dyna.level) continue;

        plight = NULL;
        vdist = particle->getPosition() - cam.getTrackPosition();
        distance = idlib::squared_euclidean_norm(vdist);

        if (dyl.size < gfx.dynalist_max && dyl.size < TOTAL_MAX_DYNA)
        {
            if (0 == dyl.size)
            {
                distance_max = distance;
            }
            else
            {
                distance_max = std::max(distance_max, distance);
            }

            plight = dyl.lst + dyl.size;
            dyl.size++;

            if (distance_max == distance)
            {
                plight_max = plight;
            }
        }
        else if (distance < distance_max)
        {
            plight = plight_max;

            distance_max = dyl.lst[0].distance;
            plight_max = dyl.lst + 0;
            for (tnc = 1; tnc < gfx.dynalist_max; tnc++)
            {
                if (dyl.lst[tnc].distance > distance_max)
                {
                    plight_max = dyl.lst + tnc;
                    distance_max = plight_max->distance;
                }
            }
        }

        if (NULL != plight)
        {
            plight->distance = distance;
            plight->pos = particle->getPosition();
            plight->level = pprt_dyna.level;
            plight->falloff = pprt_dyna.falloff;
        }
    }

    dyl.frame = renderedFrameCount();

    return gfx_success;
}

namespace
{
using namespace gfx_internal;

gfx_rv gfx_make_tileList(Ego::Graphics::TileList& tl, Camera& cam)
{
    static const bool clippingEnabled = true;

    if (1 != (renderedFrameCount() & 3))
    {
        return gfx_success;
    }

    tl.reset();

    int startX, startY, endX, endY;
    if (clippingEnabled)
    {
        static const float offset = 10;
        float centerX = cam.getTrackPosition()[kX] / Info<float>::Grid::Size();
        float centerY = cam.getTrackPosition()[kY] / Info<float>::Grid::Size();
        auto& cw = Ego::Physics::activeCollisionWorld();
        startX = Ego::Math::constrain<int>(centerX - offset, 0, cw.getTileCountX());
        startY = Ego::Math::constrain<int>(centerY - offset, 0, cw.getTileCountY());
        endX = Ego::Math::constrain<int>(centerX + offset, 0, cw.getTileCountX());
        endY = Ego::Math::constrain<int>(centerY + offset, 0, cw.getTileCountY());
    }
    else
    {
        startX = 0;
        startY = 0;
        endX = Ego::Physics::activeCollisionWorld().getTileCountX();
        endY = Ego::Physics::activeCollisionWorld().getTileCountY();
    }
    size_t tileCountX = Ego::Physics::activeCollisionWorld().getTileCountX();
    for (size_t x = startX; x < endX; ++x)
    {
        for (size_t y = startY; y < endY; ++y)
        {
            if (gfx_error == tl.add(x + y * tileCountX, cam))
            {
                return gfx_error;
            }
        }
    }

    return gfx_success;
}

gfx_rv gfx_make_entityList(Ego::Graphics::EntityList& el, Camera& cam)
{
    el.clear();

    std::vector<ObjectRef> visibleObjectRefs;
    activeModule().getObjectHandler().findObjectRefs(
        cam.getCenter()[kX],
        cam.getCenter()[kY],
        Info<float>::Grid::Size() * 10,
        visibleObjectRefs,
        true);

    for (const ObjectRef& objectRef : visibleObjectRefs)
    {
        Object* object = activeModule().getObjectHandler().get(objectRef);
        if (object == nullptr || object->isTerminated())
        {
            continue;
        }

        el.add(cam, *object);
    }

    for (const std::shared_ptr<Ego::Particle>& particle : EngineContext::get().particleHandler().iterator())
    {
        el.add(cam, *particle.get());
    }

    return gfx_success;
}

gfx_rv gfx_update_flashing(Ego::Graphics::EntityList& el)
{
    gfx_rv retval;

    if (el.getSize() >= Ego::Graphics::EntityList::CAPACITY)
    {
        Log::Entry e(Log::Level::Error, __FILE__, __LINE__);
        e << "invalid entity list size" << Log::EndOfEntry;
        EngineContext::get().logTarget() << e;
        return gfx_error;
    }

    const LocalPlayerPerceptionState& localPlayerPerception = GameSessionContext::get().localPlayerPerception();
    retval = gfx_success;
    for (size_t i = 0, n = el.getSize(); i < n; ++i)
    {
        float tmp_seekurse_level;

        ObjectRef iobj = el.get(i).iobj;

        const std::shared_ptr<Object>& object = activeModule().getObjectHandler()[iobj];
        if (!object) continue;

        if (DONTFLASH != object->getProfile()->getFlashAND())
        {
            if (HAS_NO_BITS(renderedFrameCount(), object->getProfile()->getFlashAND()))
            {
                object->flash(255);
            }
        }

        tmp_seekurse_level = std::min(localPlayerPerception.seeKurseLevel, 1.0f);
        if ((localPlayerPerception.seeKurseLevel > 0.0f) && object->isKursed() && 1.0f != tmp_seekurse_level)
        {
            if (HAS_NO_BITS(renderedFrameCount(), SEEKURSEAND))
            {
                object->flash(255.0f * (1.0f - tmp_seekurse_level));
            }
        }
    }

    return retval;
}
} // namespace
