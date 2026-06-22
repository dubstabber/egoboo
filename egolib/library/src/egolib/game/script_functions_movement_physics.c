/// @file egolib/game/script_functions_movement_physics.c
/// @brief Physics: position, acceleration, frame, reload, and shadow functions

#include "egolib/game/script_functions_internal.h"
#include "egolib/game/script_functions_movement_internal.h"

namespace
{
bool setEncodedFrame(Object& object, int encodedFrame)
{
    const uint16_t interpolationStep = encodedFrame & 3;
    const int frameAlong = encodedFrame >> 2;

    const ModelAction action = object.getProfile()->getModel()->getAction(ACTION_DA);
    if (!object.getGraphics().setAction(action, true, true))
    {
        return false;
    }

    return object.getGraphics().setFrameFull(frameAlong, interpolationStep);
}
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetXY( script_state_t& state, ai_state_t& self )
{
    // SetXY( tmpargument = "index", tmpx = "x", tmpy = "y" )
    /// @author ZZ
    /// @details This function sets one of the 8 permanent storage variable slots
    /// ( each of which holds an x,y pair )

    if (!hasResolvedSelf(self)) return false;

    self.x[state.argument & STOR_AND] = state.x;
    self.y[state.argument & STOR_AND] = state.y;
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetXY( script_state_t& state, ai_state_t& self )
{
    // tmpx,tmpy = GetXY( tmpargument = "index" )
    /// @author ZZ
    /// @details This function reads one of the 8 permanent storage variable slots,
    /// setting tmpx and tmpy accordingly

    if (!hasResolvedSelf(self)) return false;

    state.x = self.x[state.argument & STOR_AND];
    state.y = self.y[state.argument & STOR_AND];
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddXY( script_state_t& state, ai_state_t& self )
{
    // AddXY( tmpargument = "index", tmpx = "x", tmpy = "y" )
    /// @author ZZ
    /// @details This function alters the contents of one of the 8 permanent storage
    /// slots

    if (!hasResolvedSelf(self)) return false;

    self.x[state.argument & STOR_AND] += state.x;
    self.y[state.argument & STOR_AND] += state.y;
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AccelerateTarget( script_state_t& state, ai_state_t& self )
{
    // AccelerateTarget( tmpx = "acc x", tmpy = "acc y" )
    /// @author ZZ
    /// @details This function changes the x and y speeds of the target

    if (!hasResolvedSelf(self)) return false;

    IMovementControl* targetMovement = tryMovementControl(self.getTarget());
    const IPhysical* targetPhysical = tryPhysical(self.getTarget());
    if (targetMovement == nullptr || targetPhysical == nullptr)
    {
        return false;
    }

    targetMovement->setVelocity(targetPhysical->getVelocity() +
                                Ego::Vector3f(state.x, state.y, 0.0f));
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetFrame( script_state_t& state, ai_state_t& self )
{
    // SetFrame( tmpargument = "frame" )
    /// @author ZZ
    /// @details This function sets the current .MD2 frame for the character.  Values are * 4

    const SelfMovementContext selfContext = makeSelfMovementContext(self);
    if (!selfContext.isResolved()) return false;

    Object* selfObject = tryObject(self.getSelf());
    return selfObject != nullptr &&
           setEncodedFrame(*selfObject, state.argument);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetReloadTime( script_state_t& state, ai_state_t& self )
{
    // SetReloadTime( tmpargument = "time" )
    /// @author ZZ
    /// @details This function stops a character from being used for a while.  Used
    /// by weapons to slow down their attack rate.  50 clicks per second.

    const SelfMovementContext selfContext = makeSelfMovementContext(self);
    if (!selfContext.isResolved()) return false;

    selfContext.movement->setReloadTimer(static_cast<uint16_t>(std::max(0, state.argument)));
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetSpeedPercent( script_state_t& state, ai_state_t& self )
{
    // SetSpeedPercent( tmpargument = "percent" )
    /// @author ZZ
    /// @details This function acts like Run or Walk, except it allows the explicit
    /// setting of the speed

    if (!hasResolvedSelf(self)) return false;

    self.maxSpeed = std::max(0.0f, state.argument / 100.0f);
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_Teleport( script_state_t& state, ai_state_t& self )
{
    // Teleport( tmpx = "x", tmpy = "y" )
    /// @author ZZ
    /// @details This function teleports the character to a new location, failing if
    /// the location is blocked or off the map

    const SelfMovementContext selfContext = makeSelfMovementContext(self);
    if (!selfContext.isResolved()) return false;

    const IPhysical& selfPhysical = *selfContext.physical;
    auto location = Ego::Vector3f(Ego::Script::Interpreter::safeCast<float>(state.x),
                                  Ego::Script::Interpreter::safeCast<float>(state.y),
                                  selfPhysical.getPosZ());
    return selfContext.movement->teleport(location, selfPhysical.getFacingZ());
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetReloadTime( script_state_t& state, ai_state_t& self )
{
    // SetTargetReloadTime( tmpargument = "time" )

    /// @author ZZ
    /// @details This function sets the target's reload time
    /// This function stops the target from attacking for a while.

    if (!hasResolvedSelf(self)) return false;

    IMovementControl* targetMovement = tryMovementControl(self.getTarget());
    if (targetMovement == nullptr)
    {
        return false;
    }

    if ( state.argument > 0 )
    {
        targetMovement->setReloadTimer(static_cast<uint16_t>(Ego::Math::constrain(state.argument, 0, 0xFFFF)));
    }
    else
    {
        targetMovement->setReloadTimer(0);
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetShadowSize( script_state_t& state, ai_state_t& self )
{
    // SetShadowSize( tmpargument = "size" )
    /// @author ZZ
    /// @details This function makes the character's shadow bigger or smaller

    const SelfMovementContext selfContext = makeSelfMovementContext(self);
    if (!selfContext.isResolved()) return false;

    IMovementControl& selfMovement = *selfContext.movement;
    selfMovement.setShadowSize(state.argument * selfMovement.getFat());
    selfMovement.setSavedShadowSize(state.argument);
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AccelerateUp( script_state_t& state, ai_state_t& self )
{
    // AccelerateUp( tmpargument = "acc z" )
    /// @author ZZ
    /// @details This function makes the character accelerate up and down

    const SelfMovementContext selfContext = makeSelfMovementContext(self);
    if (!selfContext.isResolved()) return false;

    const IPhysical& selfPhysical = *selfContext.physical;
    selfContext.movement->setVelocity(selfPhysical.getVelocity() +
                                      Ego::Vector3f(0.0f, 0.0f, state.argument / 100.0f));
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AccelerateTargetUp( script_state_t& state, ai_state_t& self )
{
    // AccelerateTargetUp( tmpargument = "acc z" )
    /// @author ZF
    /// @details This function makes the target accelerate up and down

    if (!hasResolvedSelf(self)) return false;

    IMovementControl* targetMovement = tryMovementControl(self.getTarget());
    const IPhysical* targetPhysical = tryPhysical(self.getTarget());
    if (targetMovement == nullptr || targetPhysical == nullptr)
    {
        return false;
    }

    targetMovement->setVelocity(targetPhysical->getVelocity() +
                                Ego::Vector3f(0.0f, 0.0f, state.argument / 100.0f));
    return true;
}
