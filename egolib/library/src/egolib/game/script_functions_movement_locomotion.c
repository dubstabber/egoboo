/// @file egolib/game/script_functions_movement_locomotion.c
/// @brief Active movement: locomotion, latch buttons, bump/fly properties, and target interaction

#include "egolib/game/script_functions_internal.h"
#include "egolib/game/script_functions_movement_internal.h"

//--------------------------------------------------------------------------------------------
uint8_t scr_SetBumpHeight( script_state_t& state, ai_state_t& self )
{
    // SetBumpHeight( tmpargument = "height" )
    /// @author ZZ
    /// @details This function makes the character taller or shorter, usually used when
    /// the character dies

    const SelfMovementContext selfContext = makeSelfMovementContext(self);
    if (!selfContext.isResolved()) return false;

    selfContext.movement->setBumpHeight(Ego::Script::Interpreter::safeCast<float>(state.argument));
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_Run( script_state_t& state, ai_state_t& self )
{
    // Run()
    /// @author ZZ
    /// @details This function sets the character's maximum acceleration to its
    /// actual maximum

    if (!hasResolvedSelf(self)) return false;

    self.maxSpeed = 1.0f;
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_Walk( script_state_t& state, ai_state_t& self )
{
    // Walk()
    /// @author ZZ
    /// @details This function sets the character's maximum acceleration to 66%
    /// of its actual maximum

    if (!hasResolvedSelf(self)) return false;

    self.maxSpeed = 0.66f;
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_Sneak( script_state_t& state, ai_state_t& self )
{
    // Sneak()
    /// @author ZZ
    /// @details This function sets the character's maximum acceleration to 33%
    /// of its actual maximum

    if (!hasResolvedSelf(self)) return false;

    self.maxSpeed = 0.33f;
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetBumpHeight( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetBumpHeight()
    /// @author ZZ
    /// @details This function sets tmpargument to the character's height

    const SelfMovementContext selfContext = makeSelfMovementContext(self);
    if (!selfContext.isResolved()) return false;

    state.argument = selfContext.physical->getCurrentBump().height;
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PressLatchButton( script_state_t& state, ai_state_t& self )
{
    // PressLatchButton( tmpargument = "latch bits" )
    /// @author ZZ
    /// @details This function sets the character latch buttons

    const SelfMovementContext selfContext = makeSelfMovementContext(self);
    if (!selfContext.isResolved()) return false;

    if(state.argument >= LATCHBUTTON_LEFT && state.argument < LATCHBUTTON_RESPAWN)
    {
        selfContext.movement->setLatchButton(static_cast<LatchButton>(state.argument), true);
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_Stop( script_state_t& state, ai_state_t& self )
{
    // Stop()
    /// @author ZZ
    /// @details This function sets the character's maximum acceleration to 0.  Used
    /// along with Walk and Run and Sneak

    if (!hasResolvedSelf(self)) return false;

    self.maxSpeed = 0.0f;
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PressTargetLatchButton( script_state_t& state, ai_state_t& self )
{
    // PressTargetLatchButton( tmpargument = "latch bits" )
    /// @author ZZ
    /// @details This function mimics joystick button presses for the target.
    /// For making items force their own usage and such

    if (!hasResolvedSelf(self)) return false;

    IMovementControl* targetMovement = tryMovementControl(self.getTarget());
    if (targetMovement == nullptr)
    {
        return false;
    }

    if(state.argument >= LATCHBUTTON_LEFT && state.argument < LATCHBUTTON_RESPAWN)
    {
        targetMovement->setLatchButton(static_cast<LatchButton>(state.argument), true);
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_TeleportTarget( script_state_t& state, ai_state_t& self )
{
    // TeleportTarget( tmpx = "x", tmpy = "y" )
    /// @author ZZ
    /// @details This function teleports the target to the X, Y location, failing if the
    /// location is off the map or blocked

    if (!hasResolvedSelf(self)) return false;

    IMovementControl* targetMovement = tryMovementControl(self.getTarget());
    if (targetMovement == nullptr)
    {
        return false;
    }

    return targetMovement->teleport(Ego::Vector3f(float(state.x), float(state.y), float(state.distance)),
                                    Facing(state.turn));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetBumpSize( script_state_t& state, ai_state_t& self )
{
    // SetBumpSize( tmpargument = "size" )
    /// @author ZZ
    /// @details This function sets the how wide the character is

    const SelfMovementContext selfContext = makeSelfMovementContext(self);
    if (!selfContext.isResolved()) return false;

    selfContext.movement->setBumpWidth(Ego::Script::Interpreter::safeCast<float>(state.argument));
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetFlyHeight( script_state_t& state, ai_state_t& self )
{
    // SetFlyHeight( tmpargument = "height" )
    /// @author ZZ
    /// @details This function makes the character fly ( or fall to ground if 0 )

    const SelfMovementContext selfContext = makeSelfMovementContext(self);
    if (!selfContext.isResolved()) return false;

    selfContext.movement->setFlyHeight(Ego::Script::Interpreter::safeCast<float>(state.argument));
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_StopTargetMovement( script_state_t& state, ai_state_t& self )
{
    // StopTargetMovement()
    /// @author ZZ
    /// @details This function makes the target stop moving temporarily
    /// Sets the target's x and y velocities to 0, and
    /// sets the z velocity to 0 if the character is moving upwards.
    /// This is a special function for the IronBall object

    if (!hasResolvedSelf(self)) return false;

    IMovementControl* targetMovement = tryMovementControl(self.getTarget());
    const IPhysical* targetPhysical = tryPhysical(self.getTarget());
    if (targetMovement == nullptr || targetPhysical == nullptr)
    {
        return false;
    }

    targetMovement->setVelocity({0.0f, 0.0f, targetPhysical->getVelocity().z()});
    if (targetPhysical->getVelocity().z() > 0)
    {
        targetMovement->setVelocity({targetPhysical->getVelocity().x(),
                                   targetPhysical->getVelocity().y(),
                                   Ego::Physics::g_environment.gravity});
    }
    return true;
}
