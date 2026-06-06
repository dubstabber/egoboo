#pragma once

#include "egolib/game/egoboo.h"   // for gfx_rv
#include "egolib/Clock.hpp"        // for Ego::Time::Clock / ClockPolicy

class Camera;
struct dynalist_t;

namespace Ego {
namespace Graphics {
class Md2ModelRenderer;
} // namespace Graphics
} // namespace Ego

/// @brief Service interface for the GFX graphics god-object, decoupling callers
///        from the concrete GFX singleton. Published through EngineContext.
///
/// This is the first ("sub-pass A") slice of the GFX surface: the per-frame
/// instance-update timers and methods, the dynamic-light list, and the MD2
/// model renderer. The render-pass accessors are added in a follow-on pass.
class IGFX
{
public:
    virtual ~IGFX() = default;

    /// @brief Timer for the object-instance update phase.
    virtual Ego::Time::Clock<Ego::Time::ClockPolicy::NonRecursive>& updateObjectInstancesTimer() = 0;

    /// @brief Timer for the particle-instance update phase.
    virtual Ego::Time::Clock<Ego::Time::ClockPolicy::NonRecursive>& updateParticleInstancesTimer() = 0;

    /// @brief Update the object render instances for the given camera.
    virtual gfx_rv update_object_instances(Camera& cam) = 0;

    /// @brief Update the particle render instances for the given camera.
    virtual gfx_rv update_particle_instances(Camera& cam) = 0;

    /// @brief The active dynamic-light list.
    virtual dynalist_t& getDynalist() = 0;

    /// @brief The MD2 model renderer.
    virtual Ego::Graphics::Md2ModelRenderer& getMd2ModelRenderer() const = 0;
};
