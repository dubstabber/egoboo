/// @file egolib/game/script_functions_state.c
/// @brief State checks, condition queries, comparisons, and flow control

#include "egolib/game/script_functions_internal.h"
#include "egolib/game/script_functions_state_internal.h"   // SelfStateContext / makeSelfStateContext (shared with script_functions_state_inventory.c)

namespace
{
bool isLiveStateObjectRef(ObjectRef objectRef)
{
    return tryObject(objectRef) != nullptr;
}
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
uint8_t scr_IfTimeOut( script_state_t& state, ai_state_t& self )
{
    // IfTimeOut()
    /// @author ZZ
    /// @details This function proceeds if the character's aitime is 0.  Use
    /// in conjunction with set_Time

    if (!resolveSelfContext(self).isResolved()) return false;

    // Proceed only if time alert is set
    return ( worldUpdateCount() > self.timer );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetContent( script_state_t& state, ai_state_t& self )
{
    // SetContent( tmpargument )
    /// @author ZZ
    /// @details This function sets the content variable.  Used in conjunction with
    /// GetContent.  Content is preserved from update to update

    if (!resolveSelfContext(self).isResolved()) return false;

    // Set the content
    self.content = Ego::Script::Interpreter::safeCast<int>(state.argument);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTime( script_state_t& state, ai_state_t& self )
{
    // SetTime( tmpargument = "time" )
    /// @author ZZ
    /// @details This function sets the character's ai timer.  50 clicks per second.
    /// Used in conjunction with IfTimeOut

    if (!resolveSelfContext(self).isResolved()) return false;

    self.timer = UpdateTime( self.timer, (int)state.argument );

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetContent( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetContent()
    /// @author ZZ
    /// @details This function sets tmpargument to the character's content variable.
    /// Used in conjunction with set_Content, or as a NOP to space out an Else

    if (!resolveSelfContext(self).isResolved()) return false;

    // Get the content
    state.argument = self.content;

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_Else( script_state_t& state, ai_state_t& self )
{
    // Else
    /// @author ZZ
    /// @details This function fails if the last function was more indented

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfStateContext selfContext = makeSelfStateContext(self);
    return ( selfContext.profile->getAIScript().indent >= selfContext.profile->getAIScript().indent_last );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetState( script_state_t& state, ai_state_t& self )
{
    // SetState( tmpargument = "state" )
    /// @author ZZ
    /// @details This function sets the character's state.
    /// VERY IMPORTANT. State is preserved from update to update

    if (!resolveSelfContext(self).isResolved()) return false;

    IScriptable* selfScriptable = tryScriptable(self.getSelf());
    if (selfScriptable == nullptr)
    {
        return false;
    }
    selfScriptable->setAIStateValue(state.argument);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetState( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetState()
    /// @author ZZ
    /// @details This function reads the character's state variable

    if (!resolveSelfContext(self).isResolved()) return false;

    state.argument = self.state;

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs( script_state_t& state, ai_state_t& self )
{
    // IfStateIs( tmpargument = "state" )
    /// @author ZZ
    /// @details This function proceeds if the character's state equals tmpargument

    if (!resolveSelfContext(self).isResolved()) return false;

    return ( state.argument == self.state );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfXIsLessThanY( script_state_t& state, ai_state_t& self )
{
    // IfXIsLessThanY( tmpx, tmpy )
    /// @author ZZ
    /// @details This function proceeds if tmpx is less than tmpy.

    if (!resolveSelfContext(self).isResolved()) return false;

    return ( state.x < state.y );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetWeatherTime( script_state_t& state, ai_state_t& self )
{
    // SetWeatherTime( tmpargument = "time" )
    /// @author ZZ
    /// @details This function can be used to slow down or speed up or stop rain and
    /// other weather effects

    if (!resolveSelfContext(self).isResolved()) return false;

    // Set the weather timer
    WeatherState& weatherState = activeModuleEnvironment().weatherState();
    weatherState.timer_reset = state.argument;
    weatherState.time = state.argument;

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfInvisible( script_state_t& state, ai_state_t& self )
{
    // IfInvisible()
    /// @author ZZ
    /// @details This function proceeds if the character is invisible

    if (!resolveSelfContext(self).isResolved()) return false;

    IRenderable* selfRenderable = tryRenderable(self.getSelf());
    return selfRenderable != nullptr && selfRenderable->getAlpha() <= INVISIBLE;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfArmorIs( script_state_t& state, ai_state_t& self )
{
    // IfArmorIs( tmpargument = "skin" )
    /// @author ZZ
    /// @details This function proceeds if the character's skin type equals tmpargument

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfStateContext selfContext = makeSelfStateContext(self);
    return selfContext.targetInfo->getSkin() == state.argument;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfUnarmed( script_state_t& state, ai_state_t& self )
{
    // IfUnarmed()
    /// @author ZZ
    /// @details This function proceeds if the character is holding no items in hand.

    if (!resolveSelfContext(self).isResolved()) return false;

    IInventoryHolder* selfInventory = tryInventoryHolder(self.getSelf());
    if (selfInventory == nullptr)
    {
        return false;
    }

    return !isLiveStateObjectRef(selfInventory->getHeldObject(SLOT_LEFT))
        && !isLiveStateObjectRef(selfInventory->getHeldObject(SLOT_RIGHT));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfYIsLessThanX( script_state_t& state, ai_state_t& self )
{
    // IfYIsLessThanX()
    /// @author ZZ
    /// @details This function proceeds if tmpy is less than tmpx

    if (!resolveSelfContext(self).isResolved()) return false;

    return ( state.y < state.x );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs0( script_state_t& state, ai_state_t& self )
{
    if (!resolveSelfContext(self).isResolved()) return false;

    return ( 0 == self.state );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs1( script_state_t& state, ai_state_t& self )
{
    if (!resolveSelfContext(self).isResolved()) return false;

    return ( 1 == self.state );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs2( script_state_t& state, ai_state_t& self )
{
    if (!resolveSelfContext(self).isResolved()) return false;

    return ( 2 == self.state );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs3( script_state_t& state, ai_state_t& self )
{
    if (!resolveSelfContext(self).isResolved()) return false;

    return ( 3 == self.state );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs4( script_state_t& state, ai_state_t& self )
{
    if (!resolveSelfContext(self).isResolved()) return false;

    return ( 4 == self.state );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs5( script_state_t& state, ai_state_t& self )
{
    if (!resolveSelfContext(self).isResolved()) return false;

    return ( 5 == self.state );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs6( script_state_t& state, ai_state_t& self )
{
    if (!resolveSelfContext(self).isResolved()) return false;

    return ( 6 == self.state );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs7( script_state_t& state, ai_state_t& self )
{
    if (!resolveSelfContext(self).isResolved()) return false;

    return ( 7 == self.state );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfContentIs( script_state_t& state, ai_state_t& self )
{
    // IfContentIs( tmpargument = "test" )
    /// @author ZZ
    /// @details This function proceeds if the content matches tmpargument

    if (!resolveSelfContext(self).isResolved()) return false;

    return ( state.argument == self.content );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIsNot( script_state_t& state, ai_state_t& self )
{
    // IfStateIsNot( tmpargument = "test" )
    /// @author ZZ
    /// @details This function proceeds if the character's state does not equal tmpargument

    if (!resolveSelfContext(self).isResolved()) return false;

    return ( state.argument != self.state );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfXIsEqualToY( script_state_t& state, ai_state_t& self )
{
    // These functions proceed if tmpx and tmpy are the same

    if (!resolveSelfContext(self).isResolved()) return false;

    return ( state.x == state.y );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DebugMessage( script_state_t& state, ai_state_t& self )
{
    // DebugMessage()
    /// @author ZZ
    /// @details This function spits out some useful numbers

    if (!resolveSelfContext(self).isResolved()) return false;

    DisplayMsg_printf( "aistate %d, aicontent %d, target %" PRIuZ, self.state, self.content, self.getTarget().get() );
    DisplayMsg_printf( "tmpx %d, tmpy %d", state.x, state.y );
    DisplayMsg_printf( "tmpdistance %d, tmpturn %d", state.distance, state.turn );
    const SelfStateContext selfContext = makeSelfStateContext(self);
    DisplayMsg_printf( "tmpargument %d, selfturn %d", state.argument, int32_t(selfContext.physical->getFacingZ()) );

    return true;
}
