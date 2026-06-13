/// @file egolib/game/script_functions_movement.c
/// @brief Navigation: waypoint, pathfinding, and turn-mode functions

#include "egolib/game/script_functions_internal.h"
#include "egolib/game/script_functions_movement_internal.h"

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
