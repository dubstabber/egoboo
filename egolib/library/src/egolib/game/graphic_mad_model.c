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

/// @file egolib/game/graphic_mad_model.c
/// @brief Character model drawing code.
/// @details

#include "egolib/game/graphic_mad.h"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"

#include "egolib/game/renderer_3d.h"
#include "egolib/game/lighting.h"
#include "egolib/game/graphic.h"
#include "egolib/game/Graphics/CameraSystem.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/Graphics/DefaultMd2ModelRenderer.hpp"
#include "egolib/Renderer/Renderer.hpp"

struct Md2VertexBuffer {
    static void render(GLenum mode, size_t start, size_t length) {
        auto& md2ModelRenderer = EngineContext::get().gfx().getMd2ModelRenderer();
        glBegin(mode); {
            auto *vertex = (Ego::Graphics::DefaultMd2ModelRenderer::Vertex *)md2ModelRenderer.lock();
            for (size_t vertexIndex = start; vertexIndex < start + length; ++vertexIndex) {
                const auto& v = vertex[vertexIndex];
                glColor4f(v.colour.r, v.colour.g, v.colour.b, v.colour.a);
                glNormal3f(v.normal.x, v.normal.y, v.normal.z);
                glTexCoord2f(v.texture.s, v.texture.t);
                glVertex3f(v.position.x, v.position.y, v.position.z);
            }
        }
        glEnd();
    }
};

gfx_rv ObjectGraphicsRenderer::render_enviro(Camera& cam, const IRenderable& object, GLXvector4f tint, const BIT_FIELD bits)
{
    if (!object.hasModelDescriptor())
    {
        Log::Entry e(Log::Level::Error, __FILE__, __LINE__);
        e << "invalid mad `"<< object.getObjRef() << "`" << Log::EndOfEntry;
        EngineContext::get().logTarget() << e;
        return gfx_error;
    }
    const auto& pmd2 = object.getModelDescriptor()->getMD2();
    auto& renderer = EngineContext::get().renderer();
    auto& textureManager = EngineContext::get().textureManager();
    auto& md2ModelRenderer = EngineContext::get().gfx().getMd2ModelRenderer();

    std::shared_ptr<const Ego::Texture> ptex = nullptr;
	if (HAS_SOME_BITS(bits, CHR_PHONG))
	{
		ptex = textureManager.getTexture("mp_data/phong");
	}

	if (!GL_DEBUG(glIsEnabled)(GL_BLEND))
	{
		return gfx_fail;
	}

	if (nullptr == ptex)
	{
		ptex = object.getSkinTexture();
	}

    float uoffset = object.getUOffset() - float(cam.getTurnZ_turns());

	if (HAS_SOME_BITS(bits, CHR_REFLECT))
	{
        renderer.setWorldMatrix(object.getReflectionMatrix());
	}
	else
	{
		renderer.setWorldMatrix(object.getMatrix());
	}

    // Choose texture and matrix
	renderer.getTextureUnit().setActivated(ptex.get());

    {
        Ego::OpenGL::PushAttrib pa(GL_CURRENT_BIT);
        {
            // Get the maximum number of vertices per command.
            size_t vertexBufferCapacity = md2ModelRenderer.getRequiredVertexBufferCapacity(*pmd2);
            // Allocate a vertex buffer.
            md2ModelRenderer.ensureSize(vertexBufferCapacity);
            // Render each command
            for (const auto& glcommand : pmd2->getGLCommands()) {
                // Pre-render this command.
                size_t vertexBufferSize = 0;
                auto *targetVertex = (Ego::Graphics::DefaultMd2ModelRenderer::Vertex *)md2ModelRenderer.lock();
                for (const id_glcmd_packed_t& cmd : glcommand.data) {
                    uint16_t vertexIndex = cmd.index;
                    if (vertexIndex >= object.getVertexCount()) continue;
                    const GLvertex& pvrt = object.getVertex(vertexIndex);
                    targetVertex->position.x = pvrt.pos[XX];
                    targetVertex->position.y = pvrt.pos[YY];
                    targetVertex->position.z = pvrt.pos[ZZ];
                    targetVertex->normal.x = pvrt.nrm[XX];
                    targetVertex->normal.y = pvrt.nrm[YY];
                    targetVertex->normal.z = pvrt.nrm[ZZ];

                    // normalize the color so it can be modulated by the phong/environment map
                    targetVertex->colour.r = pvrt.color_dir * idlib::fraction<float, 1, 255>();
                    targetVertex->colour.g = pvrt.color_dir * idlib::fraction<float, 1, 255>();
                    targetVertex->colour.b = pvrt.color_dir * idlib::fraction<float, 1, 255>();
                    targetVertex->colour.a = 1.0f;

                    float cmax = std::max({targetVertex->colour.r, targetVertex->colour.g, targetVertex->colour.b});

                    if (cmax != 0.0f) {
                        targetVertex->colour.r /= cmax;
                        targetVertex->colour.g /= cmax;
                        targetVertex->colour.b /= cmax;
                    }

                    // apply the tint
                    targetVertex->colour.r *= tint[RR];
                    targetVertex->colour.g *= tint[GG];
                    targetVertex->colour.b *= tint[BB];
                    targetVertex->colour.a *= tint[AA];

                    targetVertex->texture.s = pvrt.env[XX] + uoffset;
                    targetVertex->texture.t = Ego::Math::constrain(cmax, 0.0f, 1.0f);

                    if (0 != (bits & CHR_PHONG)) {
                        // determine the phong texture coordinates
                        // the default phong is bright in both the forward and back directions...
                        targetVertex->texture.t = targetVertex->texture.t * 0.5f + 0.5f;
                    }

                    vertexBufferSize++;
                    targetVertex++;
                }
                // Render this command.
                Md2VertexBuffer::render(glcommand.glMode, 0, vertexBufferSize);
            }
        }
    }
    return gfx_success;
}

// Do fog...
/*
if(fogon && pchr->inst->light==255)
{
    // The full fog value
    alpha = 0xff000000 | (fogred<<16) | (foggrn<<8) | (fogblu);

    for (cnt = 0; cnt < pmad->transvertices; cnt++)
    {
        // Figure out the z position of the vertex...  Not totally accurate
        z = (pchr->inst->_vertexList[cnt].pos[ZZ]) + pchr->matrix(3,2);

        // Figure out the fog coloring
        if(z < fogtop)
        {
            if(z < fogbottom)
            {
                pchr->inst->_vertexList[cnt].specular = alpha;
            }
            else
            {
                z = 1.0f - ((z - fogbottom)/fogdistance);  // 0.0f to 1.0f...  Amount of fog to keep
                red = fogred * z;
                grn = foggrn * z;
                blu = fogblu * z;
                fogspec = 0xff000000 | (red<<16) | (grn<<8) | (blu);
                pchr->inst->_vertexList[cnt].specular = fogspec;
            }
        }
        else
        {
            pchr->inst->_vertexList[cnt].specular = 0;
        }
    }
}

else
{
    for (cnt = 0; cnt < pmad->transvertices; cnt++)
        pchr->inst->_vertexList[cnt].specular = 0;
}

*/

gfx_rv ObjectGraphicsRenderer::render_tex(Camera& camera, const IRenderable& object, GLXvector4f tint, const BIT_FIELD bits)
{
    if (!object.hasModelDescriptor())
    {
        Log::Entry e(Log::Level::Error, __FILE__, __LINE__);
        e << "invalid mad `" << object.getObjRef() << "`" << Log::EndOfEntry;
        EngineContext::get().logTarget() << e;
        return gfx_error;
    }

    auto& renderer = EngineContext::get().renderer();
    auto& md2ModelRenderer = EngineContext::get().gfx().getMd2ModelRenderer();
    const std::shared_ptr<MD2Model> &pmd2 = object.getModelDescriptor()->getMD2();

    // To make life easier
    std::shared_ptr<const Ego::Texture> ptex = object.getSkinTexture();

    float uoffset = object.getUOffset() * idlib::fraction<float, 1, 65535>();
    float voffset = object.getVOffset() * idlib::fraction<float, 1, 65535>();

    float base_amb = 0.0f;
    if (0 == (bits & CHR_LIGHT))
    {
        // Convert the "light" parameter to self-lighting for
        // every object that is not being rendered using CHR_LIGHT.
        base_amb = (0xFF == object.getLight()) ? 0 : (object.getLight() * idlib::fraction<float, 1, 255>());
    }

    // Get the maximum number of vertices per command.
    size_t vertexBufferCapacity = md2ModelRenderer.getRequiredVertexBufferCapacity(*pmd2);
    // Allocate a vertex buffer.
    md2ModelRenderer.ensureSize(vertexBufferCapacity);

    if (0 != (bits & CHR_REFLECT))
    {
        renderer.setWorldMatrix(object.getReflectionMatrix());
    }
    else
    {
        renderer.setWorldMatrix(object.getMatrix());
    }

    // Choose texture.
	renderer.getTextureUnit().setActivated(ptex.get());

    {
        Ego::OpenGL::PushAttrib pa(GL_CURRENT_BIT);
        {
            // Render each command
            for (const MD2_GLCommand& glcommand : pmd2->getGLCommands()) {
                // Pre-render this command.
                size_t vertexBufferSize = 0;
                auto* targetVertex = (Ego::Graphics::DefaultMd2ModelRenderer::Vertex *)md2ModelRenderer.lock();
                for (const id_glcmd_packed_t &cmd : glcommand.data) {
                    uint16_t vertexIndex = cmd.index;
                    if (vertexIndex >= object.getVertexCount()) {
                        continue;
                    }
                    const GLvertex& pvrt = object.getVertex(vertexIndex);
                    targetVertex->position.x = pvrt.pos[XX];
                    targetVertex->position.y = pvrt.pos[YY];
                    targetVertex->position.z = pvrt.pos[ZZ];
                    targetVertex->normal.x = pvrt.nrm[XX];
                    targetVertex->normal.y = pvrt.nrm[YY];
                    targetVertex->normal.z = pvrt.nrm[ZZ];

                    // Determine the texture coordinates.
                    targetVertex->texture.s = cmd.s + uoffset;
                    targetVertex->texture.t = cmd.t + voffset;

                    // Perform lighting.
                    if (HAS_NO_BITS(bits, CHR_LIGHT) && HAS_NO_BITS(bits, CHR_ALPHA)) {
                        // The directional lighting.
                        float fcol = pvrt.color_dir * idlib::fraction<float, 1, 255>();

                        targetVertex->colour.r = fcol;
                        targetVertex->colour.g = fcol;
                        targetVertex->colour.b = fcol;
                        targetVertex->colour.a = 1.0f;

                        // Ambient lighting.
                        if (HAS_NO_BITS(bits, CHR_PHONG)) {
                            // Convert the "light" parameter to self-lighting for
                            // every object that is not being rendered using CHR_LIGHT.

                            float acol = base_amb + object.getAmbientColour() * idlib::fraction<float, 1, 255>();

                            targetVertex->colour.r += acol;
                            targetVertex->colour.g += acol;
                            targetVertex->colour.b += acol;
                        }

                        // clip the colors
                        targetVertex->colour.r = Ego::Math::constrain(targetVertex->colour.r, 0.0f, 1.0f);
                        targetVertex->colour.g = Ego::Math::constrain(targetVertex->colour.g, 0.0f, 1.0f);
                        targetVertex->colour.b = Ego::Math::constrain(targetVertex->colour.b, 0.0f, 1.0f);

                        // tint the object
                        targetVertex->colour.r *= tint[RR];
                        targetVertex->colour.g *= tint[GG];
                        targetVertex->colour.b *= tint[BB];
                    } else {
                        // Set the basic tint.
                        targetVertex->colour.r = tint[RR];
                        targetVertex->colour.g = tint[GG];
                        targetVertex->colour.b = tint[BB];
                        targetVertex->colour.a = tint[AA];
                    }
                    vertexBufferSize++;
                    targetVertex++;
                }
                // Render this command.
                Md2VertexBuffer::render(glcommand.glMode, 0, vertexBufferSize);
            }
        }
    }
    return gfx_success;
}

/*
    // Do fog...
    if(fogon && pchr->inst->light==255)
    {
        // The full fog value
*        alpha = 0xff000000 | (fogred<<16) | (foggrn<<8) | (fogblu);

        for (cnt = 0; cnt < pmad->transvertices; cnt++)
        {
            // Figure out the z position of the vertex...  Not totally accurate
            z = (pchr->inst->_vertexList[cnt].pos[ZZ]) + pchr->matrix(3,2);

            // Figure out the fog coloring
            if(z < fogtop)
            {
                if(z < fogbottom)
                {
                    pchr->inst->_vertexList[cnt].specular = alpha;
                }
                else
                {
                    spek = pchr->inst->_vertexList[cnt].specular & 255;
                    z = (z - fogbottom)/fogdistance;  // 0.0f to 1.0f...  Amount of old to keep
                    fogtokeep = 1.0f-z;  // 0.0f to 1.0f...  Amount of fog to keep
                    spek = spek * z;
                    red = (fogred * fogtokeep) + spek;
                    grn = (foggrn * fogtokeep) + spek;
                    blu = (fogblu * fogtokeep) + spek;
                    fogspec = 0xff000000 | (red<<16) | (grn<<8) | (blu);
                    pchr->inst->_vertexList[cnt].specular = fogspec;
                }
            }
        }
    }
*/
