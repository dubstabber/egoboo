#include "egolib/game/Graphics/RenderPasses/NonOpaqueEntitiesRenderPass.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/graphic_mad.h"
#include "egolib/game/graphic_prt.h"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Renderer/Renderer.hpp"
#include "egolib/game/Core/EngineContext.hpp"

namespace Ego {
namespace Graphics {

NonOpaqueEntitiesRenderPass::NonOpaqueEntitiesRenderPass() :
    RenderPass("non opaque entities")
{}

void NonOpaqueEntitiesRenderPass::doRun(::Camera& camera, const TileList& tl, const EntityList& el)
{
    auto& objectHandler = GameSessionContext::get().objectHandler();
    auto& renderer = EngineContext::get().renderer();
    // Set projection matrix.
    renderer.setProjectionMatrix(camera.getProjectionMatrix());
    // Set view matrix.
    renderer.setViewMatrix(camera.getViewMatrix());
    // Set world matrix.
    renderer.setWorldMatrix(idlib::identity<Matrix4f4f>());
    OpenGL::PushAttrib pa(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT);
    {
        auto& renderer = EngineContext::get().renderer();
        //---- set the the transparency parameters

        // don't write into the depth buffer (disable glDepthMask for transparent objects)
        renderer.setDepthWriteEnabled(false);

        // do not draw hidden surfaces
        renderer.setDepthTestEnabled(true);
        renderer.setDepthFunction(idlib::compare_function::less_or_equal);

        // Now render all transparent and light objects
        for (size_t i = el.getSize(); i > 0; --i)
        {
            size_t j = i - 1;
            // A character.
            if (ParticleRef::Invalid == el.get(j).iprt && ObjectRef::Invalid != el.get(j).iobj)
            {
                const auto& object = objectHandler[el.get(j).iobj];
                if (object)
                {
                    ObjectGraphicsRenderer::render_trans(camera, *object);
                }
            }
            // A particle.
            else if (ObjectRef::Invalid == el.get(j).iobj && ParticleRef::Invalid != el.get(j).iprt)
            {
                ParticleGraphicsRenderer::render_one_prt_trans(el.get(j).iprt);
            }
        }
    }
}

} // namespace Graphics
} // namespace Ego
