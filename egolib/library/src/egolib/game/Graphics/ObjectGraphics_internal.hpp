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

/// @file egolib/game/Graphics/ObjectGraphics_internal.hpp
/// @brief File-local helpers shared by the two ObjectGraphics TUs (ObjectGraphics.cpp +
///   ObjectGraphics_animation.cpp). These were a single anonymous namespace before the split;
///   they are now `inline` functions in an Ego::Graphics::detail namespace so both TUs can use
///   them (the vertex/lighting half needs the tint helpers; the animation half needs the
///   locomotion/attachment/config helpers). Internal to ObjectGraphics — not a public API.

#pragma once

#include "egolib/game/Graphics/ObjectGraphics.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/graphic.h"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Module/Module.hpp"

namespace Ego
{
namespace Graphics
{
namespace detail
{

inline egoboo_config_t& config()
{
    return EngineContext::get().config();
}

inline IAudioSystem& audioSystem()
{
    return EngineContext::get().audioSystem();
}

inline const IMovementControl& movementControl(const Object& object)
{
    return object;
}

inline const std::shared_ptr<Object>& attachmentObject(ObjectRef objectRef)
{
    return GameSessionContext::get().activeModule().getObjectHandler()[objectRef];
}

inline const std::shared_ptr<Object>& heldItem(const IInventoryHolder& object, slot_t slot)
{
    return attachmentObject(object.getHeldObject(slot));
}

struct TintRenderState
{
    int alpha;
    int light;
    int sheen;
    colorshift_t colorShift;
};

struct LocomotionAnimationDecision
{
    bool shouldApply = false;
    float animationRate = 1.0f;
    ModelAction action = ACTION_DA;
    int lip = 0;
};

inline bool isWalkTypeAnimation(ModelAction action)
{
    return action < ACTION_DD || ACTION_IS_TYPE(action, W);
}

inline LocomotionAnimationDecision makeLocomotionAnimationDecision(Object& object,
                                                            const ModelDescriptor& modelDescriptor,
                                                            ModelAction currentAnimation)
{
    LocomotionAnimationDecision decision;
    if (!isWalkTypeAnimation(currentAnimation))
    {
        return decision;
    }

    if (!object.isTouchingGround() && !object.isFlying())
    {
        return decision;
    }

    decision.shouldApply = true;

    float speed = 0.0f;
    if (object.isFlying())
    {
        speed = idlib::euclidean_norm(object.getVelocity());
    }
    else
    {
        speed = std::max(idlib::euclidean_norm(xy(object.getVelocity())),
                         idlib::euclidean_norm(movementControl(object).getDesiredVelocity()));
        if (object.floorIsSlippy())
        {
            decision.animationRate = 2.0f;
            speed *= 2.0f;
        }
    }

    if (object.getFat() > 0.0f)
    {
        speed /= object.getFat();
    }

    if (speed <= 1.0f)
    {
        decision.action = ACTION_DA;
    }
    else if (object.isStealthed() && modelDescriptor.isActionValid(ACTION_WA))
    {
        decision.action = ACTION_WA;
        decision.lip = LIPWA;
    }
    else if (speed <= 4.0f && modelDescriptor.isActionValid(ACTION_WB))
    {
        decision.action = ACTION_WB;
        decision.lip = LIPWB;
    }
    else
    {
        decision.action = ACTION_WC;
        decision.lip = LIPWC;
    }

    if (object.isFlying())
    {
        switch (decision.action)
        {
            case ACTION_DA: decision.action = ACTION_WC; break;
            case ACTION_WA: decision.action = ACTION_WB; break;
            case ACTION_WB: decision.action = ACTION_WA; break;
            case ACTION_WC: decision.action = ACTION_DA; break;
            default: break;
        }
    }

    return decision;
}

inline uint8_t computeReflectionAlpha(const IPhysical& object, uint8_t alpha)
{
    const float altitudeAboveGround = std::max(0.0f, object.getPosZ() - object.getFloorElevation());
    float alphaFade = (255.0f - altitudeAboveGround) * 0.5f;
    alphaFade = Ego::Math::constrain(alphaFade, 0.0f, 255.0f);

    return alpha * alphaFade * idlib::fraction<float, 1, 255>();
}

inline TintRenderState makeTintRenderState(const IPhysical& object,
                                    uint8_t alpha,
                                    uint8_t light,
                                    uint8_t sheen,
                                    const colorshift_t& colorShift,
                                    bool reflection)
{
    if (!reflection)
    {
        return TintRenderState{alpha, light, sheen, colorShift};
    }

    const uint8_t reflectionAlpha = computeReflectionAlpha(object, alpha);
    const int reflectionLight = (light == 0xFF)
                              ? 0xFF
                              : light * reflectionAlpha * idlib::fraction<float, 1, 255>();

    return TintRenderState{
        reflectionAlpha,
        reflectionLight,
        sheen / 2,
        colorshift_t(static_cast<uint8_t>(colorShift.red + 1),
                     static_cast<uint8_t>(colorShift.green + 1),
                     static_cast<uint8_t>(colorShift.blue + 1))
    };
}

inline void applyLocalPlayerPerception(TintRenderState& state, const LocalPlayerPerceptionState& localPlayerPerception)
{
    if (localPlayerPerception.seeInvisibleLevel > 0.0f)
    {
        state.alpha = std::max(state.alpha, static_cast<int>(SEEINVISIBLE));
    }

    state.light = get_light(state.light, localPlayerPerception.seeDarkMagnitude);
}

inline void encodeTint(GLXvector4f tint, const TintRenderState& state, int type)
{
    tint[RR] = 1.0f / (1 << state.colorShift.red);
    tint[GG] = 1.0f / (1 << state.colorShift.green);
    tint[BB] = 1.0f / (1 << state.colorShift.blue);
    tint[AA] = 1.0f;

    switch (type)
    {
        case CHR_LIGHT:
        case CHR_ALPHA:
            tint[AA] = state.alpha * idlib::fraction<float, 1, 255>();
            tint[RR] = state.light * idlib::fraction<float, 1, 255>() / (1 << state.colorShift.red);
            tint[GG] = state.light * idlib::fraction<float, 1, 255>() / (1 << state.colorShift.green);
            tint[BB] = state.light * idlib::fraction<float, 1, 255>() / (1 << state.colorShift.blue);
            break;

        case CHR_PHONG:
        {
            const float amount = (Ego::Math::constrain(state.sheen, 0, 15) << 4) / 240.0f;

            tint[RR] += tint[RR] * 0.5f + amount;
            tint[GG] += tint[GG] * 0.5f + amount;
            tint[BB] += tint[BB] * 0.5f + amount;

            tint[RR] /= 2.0f;
            tint[GG] /= 2.0f;
            tint[BB] /= 2.0f;
            break;
        }

        case CHR_SOLID:
        case CHR_REFLECT:
        case CHR_UNKNOWN:
        default:
            break;
    }
}

} // namespace detail
} // namespace Graphics
} // namespace Ego
