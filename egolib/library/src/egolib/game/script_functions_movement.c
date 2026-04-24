/// @file egolib/game/script_functions_movement.c
/// @brief Movement, pathfinding, position, and physics property functions

#include "egolib/game/script_functions_internal.h"

namespace
{
struct SelfMovementContext
{
    Object* object = nullptr;
    ObjectProfile* profile = nullptr;
    IMovementControl* movement = nullptr;
    const IPhysical* physical = nullptr;

    bool isResolved() const
    {
        return object != nullptr &&
               profile != nullptr &&
               movement != nullptr &&
               physical != nullptr;
    }
};

SelfMovementContext makeSelfMovementContext(const ai_state_t& self)
{
    const ResolvedSelfContext resolvedSelf = resolveSelfContext(self);
    SelfMovementContext context;
    context.object = resolvedSelf.object;
    context.profile = resolvedSelf.profile;
    if (!resolvedSelf.isResolved())
    {
        return context;
    }

    context.movement = static_cast<IMovementControl*>(resolvedSelf.object);
    context.physical = static_cast<const IPhysical*>(resolvedSelf.object);
    return context;
}

bool hasResolvedSelf(const ai_state_t& self)
{
    return resolveSelfContext(self).isResolved();
}

bool setEncodedFrame(Object& object, int encodedFrame)
{
    const uint16_t interpolationStep = encodedFrame & 3;
    const int frameAlong = encodedFrame >> 2;

    const ModelAction action = object.getProfile()->getModel()->getAction(ACTION_DA);
    if (!object.setAction(action, true, true))
    {
        return false;
    }

    return object.setFrameFull(frameAlong, interpolationStep);
}
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfAtWaypoint( script_state_t& state, ai_state_t& self )
{
    // IfAtWaypoint()
    /// @author ZZ
    /// @details This function proceeds if the character reached its waypoint this
    /// update

    if (!hasResolvedSelf(self)) return false;

    // Proceed only if the character reached a waypoint
    return HAS_SOME_BITS( self.alert, ALERTIF_ATWAYPOINT );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfAtLastWaypoint( script_state_t& state, ai_state_t& self )
{
    // IfAtLastWaypoint()
    /// @author ZZ
    /// @details This function proceeds if the character reached its last waypoint this
    /// update

    if (!hasResolvedSelf(self)) return false;

    // Proceed only if the character reached its last waypoint
    return HAS_SOME_BITS( self.alert, ALERTIF_ATLASTWAYPOINT );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ClearWaypoints( script_state_t& state, ai_state_t& self )
{
    // ClearWaypoints()
    /// @author ZZ
    /// @details This function is used to move a character around.  Do this before
    /// AddWaypoint

    if (!hasResolvedSelf(self)) return false;

	waypoint_list_t::clear(self.wp_lst);
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddWaypoint( script_state_t& state, ai_state_t& self )
{
    // AddWaypoint( tmpx = "x position", tmpy = "y position" )
    /// @author ZZ
    /// @details This function tells the character where to move next

    if (!hasResolvedSelf(self)) return false;

    const bool added = ::AddWaypoint( self.wp_lst, self.getSelf(), Ego::Script::Interpreter::safeCast<float>(state.x),
                                      Ego::Script::Interpreter::safeCast<float>(state.y));

    if ( added )
    {
        // make sure we update the waypoint, since the list changed
        ai_state_t::get_wp( self );
    }

    return added;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_FindPath( script_state_t& state, ai_state_t& self )
{
    // FindPath
    /// @author ZF
    /// @details Ported the A* path finding algorithm by birdsey and heavily modified it
    /// This function adds enough waypoints to get from one point to another

    bool used_astar;
    const SelfMovementContext selfContext = makeSelfMovementContext(self);
    if (!selfContext.isResolved()) return false;

    //Too soon since last try?
    if ( self.astar_timer > worldUpdateCount() ) return true;

    const bool foundPath = ::FindPath( self.wp_lst,
                                       *selfContext.physical,
                                       selfContext.object->getStoppedByMask(),
                                       Ego::Script::Interpreter::safeCast<float>(state.x),
                                       Ego::Script::Interpreter::safeCast<float>(state.y), &used_astar );

    if ( used_astar )
    {
        // limit the rate of AStar calculations to be once every half second.
        self.astar_timer = worldUpdateCount() + ( ONESECOND / 2 );
    }

    //Make sure the waypoint list is updated
	ai_state_t::get_wp( self );

    return foundPath;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_Compass( script_state_t& state, ai_state_t& self )
{
    // Compass( tmpturn = "rotation", tmpdistance = "radius" )
    /// @author ZZ
    /// @details This function modifies tmpx and tmpy, depending on the setting of
    /// tmpdistance and tmpturn.  It acts like one of those Compass thing
    /// with the two little needle legs

    if (!hasResolvedSelf(self)) return false;

    Ego::Vector2f loc_pos = Ego::Vector2f(Ego::Script::Interpreter::safeCast<float>(state.x),
                                          Ego::Script::Interpreter::safeCast<float>(state.y));

    const bool resolved = ::Compass( loc_pos, state.turn, state.distance );

    // update the position
    if ( resolved )
    {
        state.x = loc_pos[XX];
        state.y = loc_pos[YY];
    }

    return resolved;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTurnModeToVelocity( script_state_t& state, ai_state_t& self )
{
    // SetTurnModeToVelocity()
    /// @author ZZ
    /// @details This function sets the character's movement mode to the default

    const SelfMovementContext selfContext = makeSelfMovementContext(self);
    if (!selfContext.isResolved()) return false;

    selfContext.movement->setTurnMode(TURNMODE_VELOCITY);
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTurnModeToWatch( script_state_t& state, ai_state_t& self )
{
    // SetTurnModeToWatch()
    /// @author ZZ
    /// @details This function makes the character look at its next waypoint, usually
    /// used with close waypoints or the Stop function

    const SelfMovementContext selfContext = makeSelfMovementContext(self);
    if (!selfContext.isResolved()) return false;

    selfContext.movement->setTurnMode(TURNMODE_WATCH);
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTurnModeToSpin( script_state_t& state, ai_state_t& self )
{
    // SetTurnModeToSpin()
    /// @author ZZ
    /// @details This function makes the character spin around in a circle, usually
    /// used for magical items and such

    const SelfMovementContext selfContext = makeSelfMovementContext(self);
    if (!selfContext.isResolved()) return false;

    selfContext.movement->setTurnMode(TURNMODE_SPIN);
    return true;
}


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
uint8_t scr_SetTurnModeToWatchTarget( script_state_t& state, ai_state_t& self )
{
    // SetTurnModeToWatchTarget()
    /// @author ZZ
    /// @details This function makes the character face its target, no matter what
    /// direction it is moving in.  Undo this with set_TurnModeToVelocity

    const SelfMovementContext selfContext = makeSelfMovementContext(self);
    if (!selfContext.isResolved()) return false;

    selfContext.movement->setTurnMode(TURNMODE_WATCHTARGET);
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

    return setEncodedFrame(*selfContext.object, state.argument);
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
