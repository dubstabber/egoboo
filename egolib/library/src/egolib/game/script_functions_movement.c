/// @file egolib/game/script_functions_movement.c
/// @brief Movement, pathfinding, position, and physics property functions

#include "egolib/game/script_functions_internal.h"


//--------------------------------------------------------------------------------------------
uint8_t scr_IfAtWaypoint( script_state_t& state, ai_state_t& self )
{
    // IfAtWaypoint()
    /// @author ZZ
    /// @details This function proceeds if the character reached its waypoint this
    /// update

    SCRIPT_FUNCTION_BEGIN();

    // Proceed only if the character reached a waypoint
    returncode = HAS_SOME_BITS( self.alert, ALERTIF_ATWAYPOINT );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfAtLastWaypoint( script_state_t& state, ai_state_t& self )
{
    // IfAtLastWaypoint()
    /// @author ZZ
    /// @details This function proceeds if the character reached its last waypoint this
    /// update

    SCRIPT_FUNCTION_BEGIN();

    // Proceed only if the character reached its last waypoint
    returncode = HAS_SOME_BITS( self.alert, ALERTIF_ATLASTWAYPOINT );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ClearWaypoints( script_state_t& state, ai_state_t& self )
{
    // ClearWaypoints()
    /// @author ZZ
    /// @details This function is used to move a character around.  Do this before
    /// AddWaypoint

    SCRIPT_FUNCTION_BEGIN();

	returncode = true;
	waypoint_list_t::clear(self.wp_lst);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddWaypoint( script_state_t& state, ai_state_t& self )
{
    // AddWaypoint( tmpx = "x position", tmpy = "y position" )
    /// @author ZZ
    /// @details This function tells the character where to move next

    SCRIPT_FUNCTION_BEGIN();

    returncode = ::AddWaypoint( self.wp_lst, self.getSelf(), Ego::Script::Interpreter::safeCast<float>(state.x),
                                Ego::Script::Interpreter::safeCast<float>(state.y));

    if ( returncode )
    {
        // make sure we update the waypoint, since the list changed
        ai_state_t::get_wp( self );
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_FindPath( script_state_t& state, ai_state_t& self )
{
    // FindPath
    /// @author ZF
    /// @details Ported the A* path finding algorithm by birdsey and heavily modified it
    /// This function adds enough waypoints to get from one point to another

    bool used_astar;

    SCRIPT_FUNCTION_BEGIN();

    //Too soon since last try?
    if ( self.astar_timer > worldUpdateCount() ) return true;

    returncode = ::FindPath( self.wp_lst, pchr, Ego::Script::Interpreter::safeCast<float>(state.x),
                             Ego::Script::Interpreter::safeCast<float>(state.y), &used_astar );

    if ( used_astar )
    {
        // limit the rate of AStar calculations to be once every half second.
        self.astar_timer = worldUpdateCount() + ( ONESECOND / 2 );
    }

    //Make sure the waypoint list is updated
	ai_state_t::get_wp( self );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_Compass( script_state_t& state, ai_state_t& self )
{
    // Compass( tmpturn = "rotation", tmpdistance = "radius" )
    /// @author ZZ
    /// @details This function modifies tmpx and tmpy, depending on the setting of
    /// tmpdistance and tmpturn.  It acts like one of those Compass thing
    /// with the two little needle legs

    SCRIPT_FUNCTION_BEGIN();

    Ego::Vector2f loc_pos = Ego::Vector2f(Ego::Script::Interpreter::safeCast<float>(state.x),
                                          Ego::Script::Interpreter::safeCast<float>(state.y));

    returncode = ::Compass( loc_pos, state.turn, state.distance );

    // update the position
    if ( returncode )
    {
        state.x = loc_pos[XX];
        state.y = loc_pos[YY];
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTurnModeToVelocity( script_state_t& state, ai_state_t& self )
{
    // SetTurnModeToVelocity()
    /// @author ZZ
    /// @details This function sets the character's movement mode to the default

    SCRIPT_FUNCTION_BEGIN();

    pchr->turnmode = TURNMODE_VELOCITY;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTurnModeToWatch( script_state_t& state, ai_state_t& self )
{
    // SetTurnModeToWatch()
    /// @author ZZ
    /// @details This function makes the character look at its next waypoint, usually
    /// used with close waypoints or the Stop function

    SCRIPT_FUNCTION_BEGIN();

    pchr->turnmode = TURNMODE_WATCH;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTurnModeToSpin( script_state_t& state, ai_state_t& self )
{
    // SetTurnModeToSpin()
    /// @author ZZ
    /// @details This function makes the character spin around in a circle, usually
    /// used for magical items and such

    SCRIPT_FUNCTION_BEGIN();

    pchr->turnmode = TURNMODE_SPIN;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetBumpHeight( script_state_t& state, ai_state_t& self )
{
    // SetBumpHeight( tmpargument = "height" )
    /// @author ZZ
    /// @details This function makes the character taller or shorter, usually used when
    /// the character dies

    SCRIPT_FUNCTION_BEGIN();

    pchr->setBumpHeight(Ego::Script::Interpreter::safeCast<float>(state.argument));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_Run( script_state_t& state, ai_state_t& self )
{
    // Run()
    /// @author ZZ
    /// @details This function sets the character's maximum acceleration to its
    /// actual maximum

    SCRIPT_FUNCTION_BEGIN();

    self.maxSpeed = 1.0f;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_Walk( script_state_t& state, ai_state_t& self )
{
    // Walk()
    /// @author ZZ
    /// @details This function sets the character's maximum acceleration to 66%
    /// of its actual maximum

    SCRIPT_FUNCTION_BEGIN();

    self.maxSpeed = 0.66f;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_Sneak( script_state_t& state, ai_state_t& self )
{
    // Sneak()
    /// @author ZZ
    /// @details This function sets the character's maximum acceleration to 33%
    /// of its actual maximum

    SCRIPT_FUNCTION_BEGIN();

    self.maxSpeed = 0.33f;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetBumpHeight( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetBumpHeight()
    /// @author ZZ
    /// @details This function sets tmpargument to the character's height

    SCRIPT_FUNCTION_BEGIN();

    state.argument = pchr->bump.height;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PressLatchButton( script_state_t& state, ai_state_t& self )
{
    // PressLatchButton( tmpargument = "latch bits" )
    /// @author ZZ
    /// @details This function sets the character latch buttons

    SCRIPT_FUNCTION_BEGIN();

    if(state.argument >= LATCHBUTTON_LEFT && state.argument < LATCHBUTTON_RESPAWN)
    {
        pchr->setLatchButton(static_cast<LatchButton>(state.argument), true);
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_Stop( script_state_t& state, ai_state_t& self )
{
    // Stop()
    /// @author ZZ
    /// @details This function sets the character's maximum acceleration to 0.  Used
    /// along with Walk and Run and Sneak

    SCRIPT_FUNCTION_BEGIN();

    self.maxSpeed = 0.0f;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PressTargetLatchButton( script_state_t& state, ai_state_t& self )
{
    // PressTargetLatchButton( tmpargument = "latch bits" )
    /// @author ZZ
    /// @details This function mimics joystick button presses for the target.
    /// For making items force their own usage and such

    Object * pself_target;

    SCRIPT_FUNCTION_BEGIN();

    SCRIPT_REQUIRE_TARGET( pself_target );

    if(state.argument >= LATCHBUTTON_LEFT && state.argument < LATCHBUTTON_RESPAWN)
    {
        pself_target->setLatchButton(static_cast<LatchButton>(state.argument), true);
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_TeleportTarget( script_state_t& state, ai_state_t& self )
{
    // TeleportTarget( tmpx = "x", tmpy = "y" )
    /// @author ZZ
    /// @details This function teleports the target to the X, Y location, failing if the
    /// location is off the map or blocked

    SCRIPT_FUNCTION_BEGIN();

    const std::shared_ptr<Object> &target = objectHandler()[self.getTarget()];
    if(!target) {
        return false;
    }

    returncode = target->teleport(Ego::Vector3f(float(state.x), float(state.y), float(state.distance)), Facing(state.turn));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetBumpSize( script_state_t& state, ai_state_t& self )
{
    // SetBumpSize( tmpargument = "size" )
    /// @author ZZ
    /// @details This function sets the how wide the character is

    SCRIPT_FUNCTION_BEGIN();

    pchr->setBumpWidth(Ego::Script::Interpreter::safeCast<float>(state.argument));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetFlyHeight( script_state_t& state, ai_state_t& self )
{
    // SetFlyHeight( tmpargument = "height" )
    /// @author ZZ
    /// @details This function makes the character fly ( or fall to ground if 0 )

    SCRIPT_FUNCTION_BEGIN();

    pchr->setBaseAttribute(Ego::Attribute::FLY_TO_HEIGHT, std::max(0, state.argument));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTurnModeToWatchTarget( script_state_t& state, ai_state_t& self )
{
    // SetTurnModeToWatchTarget()
    /// @author ZZ
    /// @details This function makes the character face its target, no matter what
    /// direction it is moving in.  Undo this with set_TurnModeToVelocity

    SCRIPT_FUNCTION_BEGIN();

    pchr->turnmode = TURNMODE_WATCHTARGET;

    SCRIPT_FUNCTION_END();
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

    Object * pself_target;

    SCRIPT_FUNCTION_BEGIN();

    SCRIPT_REQUIRE_TARGET( pself_target );

    pself_target->setVelocity({0.0f, 0.0f, pself_target->getVelocity().z()});
    if (pself_target->getVelocity().z() > 0)
    {
        pself_target->setVelocity({pself_target->getVelocity().x(),
                                   pself_target->getVelocity().y(),
                                   Ego::Physics::g_environment.gravity});
    }
    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetXY( script_state_t& state, ai_state_t& self )
{
    // SetXY( tmpargument = "index", tmpx = "x", tmpy = "y" )
    /// @author ZZ
    /// @details This function sets one of the 8 permanent storage variable slots
    /// ( each of which holds an x,y pair )

    SCRIPT_FUNCTION_BEGIN();

    self.x[state.argument & STOR_AND] = state.x;
    self.y[state.argument & STOR_AND] = state.y;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetXY( script_state_t& state, ai_state_t& self )
{
    // tmpx,tmpy = GetXY( tmpargument = "index" )
    /// @author ZZ
    /// @details This function reads one of the 8 permanent storage variable slots,
    /// setting tmpx and tmpy accordingly

    SCRIPT_FUNCTION_BEGIN();

    state.x = self.x[state.argument & STOR_AND];
    state.y = self.y[state.argument & STOR_AND];

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddXY( script_state_t& state, ai_state_t& self )
{
    // AddXY( tmpargument = "index", tmpx = "x", tmpy = "y" )
    /// @author ZZ
    /// @details This function alters the contents of one of the 8 permanent storage
    /// slots

    SCRIPT_FUNCTION_BEGIN();

    self.x[state.argument & STOR_AND] += state.x;
    self.y[state.argument & STOR_AND] += state.y;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AccelerateTarget( script_state_t& state, ai_state_t& self )
{
    // AccelerateTarget( tmpx = "acc x", tmpy = "acc y" )
    /// @author ZZ
    /// @details This function changes the x and y speeds of the target

    Object * pself_target;

    SCRIPT_FUNCTION_BEGIN();

    SCRIPT_REQUIRE_TARGET( pself_target );

    pself_target->setVelocity(pself_target->getVelocity() +
                              Ego::Vector3f(state.x, state.y, 0.0f));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetFrame( script_state_t& state, ai_state_t& self )
{
    // SetFrame( tmpargument = "frame" )
    /// @author ZZ
    /// @details This function sets the current .MD2 frame for the character.  Values are * 4

    SCRIPT_FUNCTION_BEGIN();

    uint16_t ilip   = state.argument & 3;
    int frame_along = state.argument >> 2;

    // resolve the requested action to a action that is valid for this model (if possible)
    const ModelAction action = pchr->getProfile()->getModel()->getAction(ACTION_DA);

    // set the action
    if(pchr->inst.setAction(action, true, true)) {
        
        // the action is set. now set the frame info.
        returncode = pchr->inst.setFrameFull(frame_along, ilip);
    }
    else {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetReloadTime( script_state_t& state, ai_state_t& self )
{
    // SetReloadTime( tmpargument = "time" )
    /// @author ZZ
    /// @details This function stops a character from being used for a while.  Used
    /// by weapons to slow down their attack rate.  50 clicks per second.

    SCRIPT_FUNCTION_BEGIN();

    pchr->reload_timer = std::max( 0, state.argument );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetSpeedPercent( script_state_t& state, ai_state_t& self )
{
    // SetSpeedPercent( tmpargument = "percent" )
    /// @author ZZ
    /// @details This function acts like Run or Walk, except it allows the explicit
    /// setting of the speed

    SCRIPT_FUNCTION_BEGIN();

    self.maxSpeed = std::max(0.0f, state.argument / 100.0f);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_Teleport( script_state_t& state, ai_state_t& self )
{
    // Teleport( tmpx = "x", tmpy = "y" )
    /// @author ZZ
    /// @details This function teleports the character to a new location, failing if
    /// the location is blocked or off the map

    SCRIPT_FUNCTION_BEGIN();

    auto location = Ego::Vector3f(Ego::Script::Interpreter::safeCast<float>(state.x),
                                  Ego::Script::Interpreter::safeCast<float>(state.y),
                                  pchr->getPosZ());
    returncode = pchr->teleport(location, Facing(pchr->ori.facing_z));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetReloadTime( script_state_t& state, ai_state_t& self )
{
    // SetTargetReloadTime( tmpargument = "time" )

    /// @author ZZ
    /// @details This function sets the target's reload time
    /// This function stops the target from attacking for a while.

    Object * pself_target;

    SCRIPT_FUNCTION_BEGIN();

    SCRIPT_REQUIRE_TARGET( pself_target );

    if ( state.argument > 0 )
    {
        pself_target->reload_timer = Ego::Math::constrain( state.argument, 0, 0xFFFF );
    }
    else
    {
        pself_target->reload_timer = 0;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetShadowSize( script_state_t& state, ai_state_t& self )
{
    // SetShadowSize( tmpargument = "size" )
    /// @author ZZ
    /// @details This function makes the character's shadow bigger or smaller

    SCRIPT_FUNCTION_BEGIN();

    pchr->shadow_size     = state.argument * pchr->getFat();
    pchr->shadow_size_save = state.argument;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AccelerateUp( script_state_t& state, ai_state_t& self )
{
    // AccelerateUp( tmpargument = "acc z" )
    /// @author ZZ
    /// @details This function makes the character accelerate up and down

    SCRIPT_FUNCTION_BEGIN();

    pchr->setVelocity(pchr->getVelocity() +
                      Ego::Vector3f(0.0f, 0.0f, state.argument / 100.0f));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AccelerateTargetUp( script_state_t& state, ai_state_t& self )
{
    // AccelerateTargetUp( tmpargument = "acc z" )
    /// @author ZF
    /// @details This function makes the target accelerate up and down

    Object * pself_target;

    SCRIPT_FUNCTION_BEGIN();

    SCRIPT_REQUIRE_TARGET( pself_target );

    pself_target->setVelocity(pself_target->getVelocity() +
                              Ego::Vector3f(0.0f, 0.0f, state.argument / 100.0f));

    SCRIPT_FUNCTION_END();
}
