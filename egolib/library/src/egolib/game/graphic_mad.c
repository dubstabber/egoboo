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

/// @file egolib/game/graphic_mad.c
/// @brief Character model drawing code.
/// @details

#include "egolib/game/graphic_mad.h"
#include "egolib/game/Core/EngineContext.hpp"

#include "egolib/game/renderer_3d.h"
#include "egolib/game/lighting.h"
#include "egolib/game/graphic.h"
#include "egolib/game/Graphics/CameraSystem.hpp"
#include "egolib/Entities/IObjectWorld.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Renderer/Renderer.hpp"

namespace
{
[[maybe_unused]]
egoboo_config_t& config()
{
    return EngineContext::get().config();
}
}

gfx_rv ObjectGraphicsRenderer::render(Camera& cam, const IRenderable& object, GLXvector4f tint, const BIT_FIELD bits, Ego::Renderer& renderer)
{
    /// @author ZZ
    /// @details This function picks the actual function to use

    gfx_rv retval;

    //Not visible?
    if (object.isHidden() || tint[AA] <= 0.0f || object.isInsideInventory()) {
        return gfx_fail;
    }

    if (object.isPhongMapped() || HAS_SOME_BITS(bits, CHR_PHONG))
    {
        retval = render_enviro(cam, object, tint, bits, renderer);
    }
    else
    {
        retval = render_tex(cam, object, tint, bits, renderer);
    }

#if defined(_DEBUG)
    // Debug bbox/grip helpers still require the concrete Object surface and remain out of scope
    // for the read-only IRenderable seam introduced by this pass.
#endif

    return retval;
}

gfx_rv ObjectGraphicsRenderer::render_ref(Camera& cam, const IRenderable& object, Ego::Renderer& renderer)
{
    //Does this object have a reflection?
    if (!object.hasReflection()) {
        return gfx_fail;
    }

    if (object.isHidden()) return gfx_fail;

    // assume the best
	gfx_rv retval = gfx_success;

    {
        Ego::OpenGL::PushAttrib pa(GL_ENABLE_BIT | GL_POLYGON_BIT | GL_COLOR_BUFFER_BIT);
        {
            // cull backward facing polygons
            // use couter-clockwise orientation to determine backfaces
            renderer.setCullingMode(idlib::culling_mode::back);
            renderer.setWindingMode(MAD_REF_CULL);
            Ego::OpenGL::Utilities::isError();

            //Transparent
            if (object.getReflectionAlpha() != 0xFF && object.getLight() == 0xFF) {
                renderer.setBlendingEnabled(true);
                renderer.setBlendFunction(idlib::color_blend_parameter::source0_alpha, idlib::color_blend_parameter::one_minus_source0_alpha);

                GLXvector4f tint;
                object.getTint(tint, true, CHR_ALPHA);

                if (gfx_error == render(cam, object, tint, CHR_ALPHA | CHR_REFLECT, renderer)) {
                    retval = gfx_error;
                }
            }

            //Glowing
            if (object.getLight() != 0xFF) {
                renderer.setBlendingEnabled(true);
                renderer.setBlendFunction(idlib::color_blend_parameter::one, idlib::color_blend_parameter::one);

                GLXvector4f tint;
                object.getTint(tint, true, CHR_LIGHT);

                if (gfx_error == ObjectGraphicsRenderer::render(cam, object, tint, CHR_LIGHT, renderer)) {
                    retval = gfx_error;
                }
                Ego::OpenGL::Utilities::isError();
            }

            //Render shining effect on top of model
            if (object.getReflectionAlpha() == 0xFF && gfx.phongon && object.getSheen() > 0) {
                renderer.setBlendingEnabled(true);
                renderer.setBlendFunction(idlib::color_blend_parameter::one, idlib::color_blend_parameter::one);

                GLXvector4f tint;
                object.getTint(tint, true, CHR_PHONG);

                if (gfx_error == ObjectGraphicsRenderer::render(cam, object, tint, CHR_PHONG, renderer)) {
                    retval = gfx_error;
                }
                Ego::OpenGL::Utilities::isError();
            }
        }
    }

    return retval;
}

gfx_rv ObjectGraphicsRenderer::render_trans(Camera& cam, const IRenderable& object, Ego::Renderer& renderer)
{
    if (object.isHidden()) return gfx_fail;

    // there is an outside chance the object will not be rendered
    bool rendered = false;

    {
        Ego::OpenGL::PushAttrib pa(GL_ENABLE_BIT | GL_POLYGON_BIT | GL_COLOR_BUFFER_BIT);
        {
            if (object.getAlpha() < 0xFF) {
                // most alpha effects will be messed up by
                // skipping backface culling, so don't

                // cull backward facing polygons
                // use clockwise orientation to determine backfaces
                renderer.setCullingMode(idlib::culling_mode::back);
                renderer.setWindingMode(MAD_NRM_CULL);

                // get a speed-up by not displaying completely transparent portions of the skin
                renderer.setAlphaTestEnabled(true);
                renderer.setAlphaFunction(idlib::compare_function::greater, 0.0f);

                renderer.setBlendingEnabled(true);
                renderer.setBlendFunction(idlib::color_blend_parameter::source0_alpha, idlib::color_blend_parameter::one);

                GLXvector4f tint;
                object.getTint(tint, false, CHR_ALPHA);

                if (render(cam, object, tint, CHR_ALPHA, renderer)) {
                    rendered = true;
                }
            }

            else if (object.getLight() < 0xFF) {
                // light effects should show through transparent objects
                renderer.setCullingMode(idlib::culling_mode::none);

                // the alpha test can only mess us up here
                renderer.setAlphaTestEnabled(false);

                renderer.setBlendingEnabled(true);
                renderer.setBlendFunction(idlib::color_blend_parameter::one, idlib::color_blend_parameter::one);

                GLXvector4f tint;
                object.getTint(tint, false, CHR_LIGHT);

                if (render(cam, object, tint, CHR_LIGHT, renderer)) {
                    rendered = true;
                }
            }

            // Render shining effect on top of model
            if (object.getReflectionAlpha() == 0xFF && gfx.phongon && object.getSheen() > 0) {
                renderer.setBlendingEnabled(true);
                renderer.setBlendFunction(idlib::color_blend_parameter::one, idlib::color_blend_parameter::one);

                GLXvector4f tint;
                object.getTint(tint, false, CHR_PHONG);

                if (render(cam, object, tint, CHR_PHONG, renderer)) {
                    rendered = true;
                }
            }
        }
    }

    return rendered ? gfx_success : gfx_fail;
}

gfx_rv ObjectGraphicsRenderer::render_solid(Camera& cam, const IRenderable& object, Ego::Renderer& renderer)
{
    if (object.isHidden()) return gfx_fail;

    //Only proceed if we are truly fully solid
    if (0xFF != object.getAlpha() || 0xFF != object.getLight()) {
        return gfx_success;
    }

    // assume the best
	gfx_rv retval = gfx_success;

    {
        Ego::OpenGL::PushAttrib pa(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_POLYGON_BIT);
        {
            // do not display the completely transparent portion
            // this allows characters that have "holes" in their
            // textures to display the solid portions properly
            //
            // Objects with partially transparent skins should enable the [MODL] parameter "T"
            // which will enable the display of the partially transparent portion of the skin

            renderer.setAlphaTestEnabled(true);
            renderer.setAlphaFunction(idlib::compare_function::equal, 1.0f);

            // can I turn this off?
            renderer.setBlendingEnabled(true);
            renderer.setBlendFunction(idlib::color_blend_parameter::source0_alpha, idlib::color_blend_parameter::one_minus_source0_alpha);

            // allow the dont_cull_backfaces to keep solid objects from culling backfaces
            if (object.isDontCullBackfaces()) {
                // stop culling backward facing polugons
                renderer.setCullingMode(idlib::culling_mode::none);
            } else {
                // cull backward facing polygons
                // use couter-clockwise orientation to determine backfaces
                renderer.setCullingMode(idlib::culling_mode::back);
                renderer.setWindingMode(MAD_NRM_CULL);
            }

            GLXvector4f tint;
            object.getTint(tint, false, CHR_SOLID);

            if (gfx_error == render(cam, object, tint, CHR_SOLID, renderer)) {
                retval = gfx_error;
            }
        }
    }

    return retval;
}

#if _DEBUG
void ObjectGraphicsRenderer::draw_chr_bbox(const std::shared_ptr<Object>& pchr)
{
    static constexpr bool drawLeftSlot = false;
    static constexpr bool drawRightSlot = false;
    static constexpr bool drawCharacter = true;
    
    // Draw the object bounding box as a part of the graphics debug mode F7.
    if (config().debug_developerMode_enable.getValue() && EngineContext::get().inputSystem().isKeyDown(SDLK_F7))
    {
        EngineContext::get().renderer().setWorldMatrix(idlib::identity<Ego::Matrix4f4f>());

        if (drawLeftSlot)
        {
            auto bb = idlib::translate(pchr->getSlotCollisionVolume(SLOT_LEFT), pchr->getPosition());
            Renderer3D::renderOctBB(bb, true, true);
        }
        if (drawRightSlot)
        {
            auto bb = idlib::translate(pchr->getSlotCollisionVolume(SLOT_RIGHT), pchr->getPosition());
            Renderer3D::renderOctBB(bb, true, true);
        }
        if (drawCharacter)
        {
            auto bb = idlib::translate(pchr->getMinCollisionVolume(), pchr->getPosition());
            Renderer3D::renderOctBB(bb, true, true);
        }
    }

    //// The grips and vertrices of all objects.
    if (EngineContext::get().inputSystem().isKeyDown(SDLK_F6))
    {
        draw_chr_attached_grip( pchr );

        // Draw all the vertices of an object
        GL_DEBUG(glPointSize(5));
        draw_chr_verts(pchr, 0, pchr->getVertexCount());
    }
}
#endif

#if _DEBUG
void ObjectGraphicsRenderer::draw_chr_verts(const std::shared_ptr<Object>& pchr, int vrt_offset, int verts )
{
    /// @author BB
    /// @details a function that will draw some of the vertices of the given character.
    ///     The original idea was to use this to debug the grip for attached items.

    int vmin, vmax, cnt;

    vmin = vrt_offset;
    vmax = vmin + verts;

    if ( vmin < 0 || ( size_t )vmin > pchr->getVertexCount() ) return;
    if ( vmax < 0 || ( size_t )vmax > pchr->getVertexCount() ) return;

    // disable the texturing so all the points will be white,
    // not the texture color of the last vertex we drawn
    EngineContext::get().renderer().getTextureUnit().setActivated(nullptr);

	EngineContext::get().renderer().setWorldMatrix(pchr->getMatrix());
    GL_DEBUG( glBegin( GL_POINTS ) );
    {
        for ( cnt = vmin; cnt < vmax; cnt++ )
        {
            GL_DEBUG( glVertex3fv )( pchr->getVertex(cnt).pos );
        }
    }
    GL_DEBUG_END();
}
#endif

#if _DEBUG
void ObjectGraphicsRenderer::draw_one_grip(const Object& object, int slot)
{
    // disable the texturing so all the points will be white,
    // not the texture color of the last vertex we drawn
    EngineContext::get().renderer().getTextureUnit().setActivated(nullptr);

    EngineContext::get().renderer().setWorldMatrix(object.getMatrix());

    _draw_one_grip_raw(object, slot);
}

void ObjectGraphicsRenderer::_draw_one_grip_raw(const Object& object, int slot)
{
    int vmin, vmax, cnt;

    float red[4] = {1, 0, 0, 1};
    float grn[4] = {0, 1, 0, 1};
    float blu[4] = {0, 0, 1, 1};
    float * col_ary[3];

    col_ary[0] = red;
    col_ary[1] = grn;
    col_ary[2] = blu;

    vmin = ( int )object.getVertexCount() - ( int )slot_to_grip_offset(( slot_t )slot );
    vmax = vmin + GRIP_VERTS;

    if ( vmin >= 0 && vmax >= 0 && ( size_t )vmax <= object.getVertexCount() )
    {
		Ego::Vector3f src, dst, diff;

        GL_DEBUG( glBegin )( GL_LINES );
        {
            for ( cnt = 1; cnt < GRIP_VERTS; cnt++ )
            {
                src[kX] = object.getVertex(vmin).pos[XX];
                src[kY] = object.getVertex(vmin).pos[YY];
                src[kZ] = object.getVertex(vmin).pos[ZZ];

                diff[kX] = object.getVertex(vmin+cnt).pos[XX] - src[kX];
                diff[kY] = object.getVertex(vmin+cnt).pos[YY] - src[kY];
                diff[kZ] = object.getVertex(vmin+cnt).pos[ZZ] - src[kZ];

                dst[kX] = src[kX] + 3 * diff[kX];
                dst[kY] = src[kY] + 3 * diff[kY];
                dst[kZ] = src[kZ] + 3 * diff[kZ];

                GL_DEBUG( glColor4fv )( col_ary[cnt-1] );

                GL_DEBUG( glVertex3f )( src[kX], src[kY], src[kZ] );
                GL_DEBUG( glVertex3f )( dst[kX], dst[kY], dst[kZ] );
            }
        }
        GL_DEBUG_END();
    }

	EngineContext::get().renderer().setColour(Ego::Colour4f::white());
}

void ObjectGraphicsRenderer::draw_chr_attached_grip(const std::shared_ptr<Object>& pchr)
{
    const Object* pholder = Ego::Entities::tryActiveConstObject(pchr->getHolderRef());
    if (!pholder || pholder->isTerminated()) return;

    draw_one_grip(*pholder, pchr->getAttachmentSlot());
}
#endif

#if 0
void ObjectGraphicsRenderer::draw_chr_grips( Object * pchr )
{
    mad_t * pmad;

    GLint matrix_mode[1];

    if ( !ACTIVE_PCHR( pchr ) ) return;

    const std::shared_ptr<ObjectProfile> &profile = EngineContext::get().profileSystem().getProfile(pchr->profile_ref);

    pmad = chr_get_pmad( GET_INDEX_PCHR( pchr ) );
    if ( NULL == pmad ) return;

    // disable the texturing so all the points will be white,
    // not the texture color of the last vertex we drawn
    Ego::Renderer::getTextureUnit().setActivated(nullptr);

    // save the matrix mode
    GL_DEBUG( glGetIntegerv )( GL_MATRIX_MODE, matrix_mode );

    // store the GL_MODELVIEW matrix (this stack has a finite depth, minimum of 32)
    GL_DEBUG( glMatrixMode )( GL_MODELVIEW );
    GL_DEBUG( glPushMatrix )();
	EngineContext::get().renderer().multiplyMatrix(pchr->inst.matrix);

    if ( profile->isSlotValid(SLOT_LEFT) )
    {
        _draw_one_grip_raw( &( pchr->inst ), pmad, SLOT_LEFT );
    }

    if ( profile->isSlotValid(SLOT_RIGHT) )
    {
        _draw_one_grip_raw( &( pchr->inst ), pmad, SLOT_RIGHT );
    }

    // Restore the GL_MODELVIEW matrix
    GL_DEBUG( glMatrixMode )( GL_MODELVIEW );
    GL_DEBUG( glPopMatrix )();

    // restore the matrix mode
    GL_DEBUG( glMatrixMode )( matrix_mode[0] );
}
#endif
