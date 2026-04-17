/// @file egolib/game/script_functions_state.c
/// @brief State checks, condition queries, comparisons, and flow control

#include "egolib/game/script_functions_internal.h"


//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
uint8_t scr_IfSpawned( script_state_t& state, ai_state_t& self )
{
    // IfSpawned()
    /// @author ZZ
    /// @details This function proceeds if the character was spawned this update

    SCRIPT_FUNCTION_BEGIN();

    // Proceed only if it's a new character
    returncode = HAS_SOME_BITS( self.alert, ALERTIF_SPAWNED );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTimeOut( script_state_t& state, ai_state_t& self )
{
    // IfTimeOut()
    /// @author ZZ
    /// @details This function proceeds if the character's aitime is 0.  Use
    /// in conjunction with set_Time

    SCRIPT_FUNCTION_BEGIN();

    // Proceed only if time alert is set
    returncode = ( worldUpdateCount() > self.timer );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfAttacked( script_state_t& state, ai_state_t& self )
{
    // IfAttacked()
    /// @author ZZ
    /// @details This function proceeds if the character ( an item ) was put in its
    /// owner's pocket this update

    SCRIPT_FUNCTION_BEGIN();

    // Proceed only if the character was damaged
    returncode = HAS_SOME_BITS( self.alert, ALERTIF_ATTACKED );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfBumped( script_state_t& state, ai_state_t& self )
{
    // IfBumped()
    /// @author ZZ
    /// @details This function proceeds if the character was bumped by another character
    /// this update

    SCRIPT_FUNCTION_BEGIN();

    // Proceed only if the character was bumped
    returncode = HAS_SOME_BITS( self.alert, ALERTIF_BUMPED );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfOrdered( script_state_t& state, ai_state_t& self )
{
    // IfOrdered()
    /// @author ZZ
    /// @details This function proceeds if the character got an order from another
    /// character on its team this update

    SCRIPT_FUNCTION_BEGIN();

    // Proceed only if the character was ordered
    returncode = HAS_SOME_BITS( self.alert, ALERTIF_ORDERED );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfCalledForHelp( script_state_t& state, ai_state_t& self )
{
    // IfCalledForHelp()
    /// @author ZZ
    /// @details This function proceeds if one of the character's teammates was nearly
    /// killed this update

    SCRIPT_FUNCTION_BEGIN();

    // Proceed only if the character was called for help
    returncode = HAS_SOME_BITS( self.alert, ALERTIF_CALLEDFORHELP );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetContent( script_state_t& state, ai_state_t& self )
{
    // SetContent( tmpargument )
    /// @author ZZ
    /// @details This function sets the content variable.  Used in conjunction with
    /// GetContent.  Content is preserved from update to update

    SCRIPT_FUNCTION_BEGIN();

    // Set the content
    self.content = Ego::Script::Interpreter::safeCast<int>(state.argument);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfKilled( script_state_t& state, ai_state_t& self )
{
    // IfKilled()
    /// @author ZZ
    /// @details This function proceeds if the character was killed this update

    SCRIPT_FUNCTION_BEGIN();

    // Proceed only if the character's been killed
    returncode = HAS_SOME_BITS( self.alert, ALERTIF_KILLED );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTime( script_state_t& state, ai_state_t& self )
{
    // SetTime( tmpargument = "time" )
    /// @author ZZ
    /// @details This function sets the character's ai timer.  50 clicks per second.
    /// Used in conjunction with IfTimeOut

    SCRIPT_FUNCTION_BEGIN();

    self.timer = UpdateTime( self.timer, (int)state.argument );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetContent( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetContent()
    /// @author ZZ
    /// @details This function sets tmpargument to the character's content variable.
    /// Used in conjunction with set_Content, or as a NOP to space out an Else

    SCRIPT_FUNCTION_BEGIN();

    // Get the content
    state.argument = self.content;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_Else( script_state_t& state, ai_state_t& self )
{
    // Else
    /// @author ZZ
    /// @details This function fails if the last function was more indented

    SCRIPT_FUNCTION_BEGIN();

    returncode = ( ppro->getAIScript().indent >= ppro->getAIScript().indent_last );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHealed( script_state_t& state, ai_state_t& self )
{
    // IfHealed()
    /// @author ZZ
    /// @details This function proceeds if the character was healed by a healing particle

    SCRIPT_FUNCTION_BEGIN();

    // Proceed only if the character was healed
    returncode = HAS_SOME_BITS( self.alert, ALERTIF_HEALED );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetState( script_state_t& state, ai_state_t& self )
{
    // SetState( tmpargument = "state" )
    /// @author ZZ
    /// @details This function sets the character's state.
    /// VERY IMPORTANT. State is preserved from update to update

    SCRIPT_FUNCTION_BEGIN();

    pchr->ai.state = state.argument;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetState( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetState()
    /// @author ZZ
    /// @details This function reads the character's state variable

    SCRIPT_FUNCTION_BEGIN();

    state.argument = self.state;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs( script_state_t& state, ai_state_t& self )
{
    // IfStateIs( tmpargument = "state" )
    /// @author ZZ
    /// @details This function proceeds if the character's state equals tmpargument

    SCRIPT_FUNCTION_BEGIN();

    returncode = ( state.argument == self.state );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfGrabbed( script_state_t& state, ai_state_t& self )
{
    // IfGrabbed()
    /// @author ZZ
    /// @details This function proceeds if the character was grabbed (picked up) this update.
    /// Used mostly by item characters

    SCRIPT_FUNCTION_BEGIN();

    returncode = HAS_SOME_BITS( self.alert, ALERTIF_GRABBED );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfDropped( script_state_t& state, ai_state_t& self )
{
    // IfDropped()
    /// @author ZZ
    /// @details This function proceeds if the character was dropped this update.
    /// Used mostly by item characters

    SCRIPT_FUNCTION_BEGIN();

    returncode = HAS_SOME_BITS( self.alert, ALERTIF_DROPPED );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfXIsLessThanY( script_state_t& state, ai_state_t& self )
{
    // IfXIsLessThanY( tmpx, tmpy )
    /// @author ZZ
    /// @details This function proceeds if tmpx is less than tmpy.

    SCRIPT_FUNCTION_BEGIN();

    returncode = ( state.x < state.y );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetWeatherTime( script_state_t& state, ai_state_t& self )
{
    // SetWeatherTime( tmpargument = "time" )
    /// @author ZZ
    /// @details This function can be used to slow down or speed up or stop rain and
    /// other weather effects

    SCRIPT_FUNCTION_BEGIN();

    // Set the weather timer
    WeatherState& weatherState = GameSessionContext::get().weatherState();
    weatherState.timer_reset = state.argument;
    weatherState.time = state.argument;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfReaffirmed( script_state_t& state, ai_state_t& self )
{
    // IfReaffirmed()
    /// @author ZZ
    /// @details This function proceeds if the character was damaged by its reaffirm
    /// damage type.  Used to relight the torch.

    SCRIPT_FUNCTION_BEGIN();

    returncode = HAS_SOME_BITS( self.alert, ALERTIF_REAFFIRMED );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfUsed( script_state_t& state, ai_state_t& self )
{
    // IfUsed()
    /// @author ZZ
    /// @details This function proceeds if the character was used by its holder or rider.
    /// Character's cannot be used if their reload time is greater than 0

    SCRIPT_FUNCTION_BEGIN();

    returncode = HAS_SOME_BITS( self.alert, ALERTIF_USED );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfCleanedUp( script_state_t& state, ai_state_t& self )
{
    // IfCleanedUp()
    /// @author ZZ
    /// @details This function proceeds if the character is dead and if the boss told it
    /// to clean itself up

    SCRIPT_FUNCTION_BEGIN();

    returncode = HAS_SOME_BITS( self.alert, ALERTIF_CLEANEDUP );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfSitting( script_state_t& state, ai_state_t& self )
{
    // IfSitting()
    /// @author ZZ
    /// @details This function proceeds if the character is riding a mount

    SCRIPT_FUNCTION_BEGIN();

    returncode = objectHandler().exists( pchr->getHolderRef() );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfScoredAHit( script_state_t& state, ai_state_t& self )
{
    // IfScoredAHit()
    /// @author ZZ
    /// @details This function proceeds if the character damaged another character this
    /// update. If it's a held character it also sets the target to whoever was hit

    SCRIPT_FUNCTION_BEGIN();

    // Proceed only if the character scored a hit
//    if ( !objectHandler().exists( pchr->attachedto ) || objectHandler().get(pchr->attachedto).ismount )
//    {
    returncode = HAS_SOME_BITS( self.alert, ALERTIF_SCOREDAHIT );
//    }

    // Proceed only if the holder scored a hit with the character
    /*    else if ( objectHandler().get(pchr->attachedto).ai.lastitemused == pself->index )
        {
            returncode = HAS_SOME_BITS( objectHandler().get(pchr->attachedto).ai.alert, ALERTIF_SCOREDAHIT );
        }
        else returncode = false;*/

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfDisaffirmed( script_state_t& state, ai_state_t& self )
{
    // IfDisaffirmed()
    /// @author ZZ
    /// @details This function proceeds if the character was disaffirmed.
    /// This doesn't seem useful anymore.

    SCRIPT_FUNCTION_BEGIN();

    returncode = HAS_SOME_BITS( self.alert, ALERTIF_DISAFFIRMED );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfChanged( script_state_t& state, ai_state_t& self )
{
    // IfChanged()
    /// @author ZZ
    /// @details This function proceeds if the character was polymorphed.
    /// Needed for morph spells and such

    SCRIPT_FUNCTION_BEGIN();

    returncode = HAS_SOME_BITS( self.alert, ALERTIF_CHANGED );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfInWater( script_state_t& state, ai_state_t& self )
{
    // IfInWater()
    /// @author ZZ
    /// @details This function proceeds if the character has just entered into some water
    /// this update ( and the water is really water, not fog or another effect )

    SCRIPT_FUNCTION_BEGIN();

    returncode = HAS_SOME_BITS( self.alert, ALERTIF_INWATER );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfBored( script_state_t& state, ai_state_t& self )
{
    // IfBored()
    /// @author ZZ
    /// @details This function proceeds if the character has been standing idle too long

    SCRIPT_FUNCTION_BEGIN();

    returncode = HAS_SOME_BITS( self.alert, ALERTIF_BORED );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTooMuchBaggage( script_state_t& state, ai_state_t& self )
{
    // IfTooMuchBaggage()
    /// @author ZZ
    /// @details This function proceeds if the character tries to put an item in his/her
    /// pockets, but the character already has 6 items in the inventory.
    /// Used to tell the players what's going on.

    SCRIPT_FUNCTION_BEGIN();

    returncode = HAS_SOME_BITS( self.alert, ALERTIF_TOOMUCHBAGGAGE );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfGrogged( script_state_t& state, ai_state_t& self )
{
    // IfGrogged()
    /// @author ZZ
    /// @details This function proceeds if the character has been grogged ( a type of
    /// confusion ) this update

    SCRIPT_FUNCTION_BEGIN();

    returncode = objectHandler().get(self.getSelf())->getGrogTimer() > 0 && HAS_SOME_BITS( self.alert, ALERTIF_CONFUSED );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfDazed( script_state_t& state, ai_state_t& self )
{
    // IfDazed()
    /// @author ZZ
    /// @details This function proceeds if the character has been dazed ( a type of
    /// confusion ) this update

    SCRIPT_FUNCTION_BEGIN();

    returncode = objectHandler().get(self.getSelf())->getDazeTimer() > 0 && HAS_SOME_BITS( self.alert, ALERTIF_CONFUSED );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfInvisible( script_state_t& state, ai_state_t& self )
{
    // IfInvisible()
    /// @author ZZ
    /// @details This function proceeds if the character is invisible

    SCRIPT_FUNCTION_BEGIN();

    returncode = pchr->inst.alpha <= INVISIBLE;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfArmorIs( script_state_t& state, ai_state_t& self )
{
    // IfArmorIs( tmpargument = "skin" )
    /// @author ZZ
    /// @details This function proceeds if the character's skin type equals tmpargument

    int tTmp;

    SCRIPT_FUNCTION_BEGIN();

    tTmp = pchr->skin;
    returncode = ( tTmp == state.argument );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfUnarmed( script_state_t& state, ai_state_t& self )
{
    // IfUnarmed()
    /// @author ZZ
    /// @details This function proceeds if the character is holding no items in hand.

    SCRIPT_FUNCTION_BEGIN();

    returncode = ( !objectHandler().exists( pchr->getHeldObject(SLOT_LEFT) ) && !objectHandler().exists( pchr->getHeldObject(SLOT_RIGHT) ) );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHitFromBehind(script_state_t& state, ai_state_t& self) {
    /// IfHitFromBehind()
    /// @author ZZ
    /// @details This function proceeds if the last attack to the character came
    /// from behind

    SCRIPT_FUNCTION_BEGIN();

    returncode = false;
    // (8192 / (2^16-1)) * 360 ~ 45 degrees
    static const Facing tolerance(8192);
    if (self.directionlast >= (ATK_BEHIND - tolerance) && self.directionlast < (ATK_BEHIND + tolerance)) {
        returncode = true;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHitFromFront(script_state_t& state, ai_state_t& self) {
    /// IfHitFromFront()
    /// @author ZZ
    /// @details This function proceeds if the last attack to the character came
    /// from the front

    SCRIPT_FUNCTION_BEGIN();

    returncode = false;
    // (8192 / (2^16-1)) * 360 ~ 45 degrees
    static const Facing tolerance(8192);
    if (self.directionlast >= (ATK_FRONT - tolerance) && self.directionlast < (ATK_FRONT + tolerance)) {
        returncode = true;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHitFromLeft(script_state_t& state, ai_state_t& self) {
    /// IfHitFromLeft()
    /// @author ZZ
    /// @details This function proceeds if the last attack to the character came
    /// from the left

    SCRIPT_FUNCTION_BEGIN();

    returncode = false;
    // (8192 / (2^16-1)) * 360 ~ 45 degrees
    static const Facing tolerance(8192);
    if (self.directionlast >= (ATK_LEFT - tolerance) && self.directionlast < (ATK_LEFT + tolerance)) {
        returncode = true;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHitFromRight(script_state_t& state, ai_state_t& self) {
    /// IfHitFromRight()
    /// @author ZZ
    /// @details This function proceeds if the last attack to the character came
    /// from the right

    SCRIPT_FUNCTION_BEGIN();

    returncode = false;
    // (8192 / (2^16-1)) * 360 ~ 45 degrees
    static const Facing tolerance(8192);
    if (self.directionlast >= (ATK_RIGHT - tolerance) && self.directionlast < (ATK_RIGHT + tolerance)) {
        returncode = true;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfNotDropped( script_state_t& state, ai_state_t& self )
{
    // IfNotDropped()
    /// @author ZZ
    /// @details This function proceeds if the character is kursed and another character
    /// was holding it and tried to drop it

    SCRIPT_FUNCTION_BEGIN();

    returncode = HAS_SOME_BITS( self.alert, ALERTIF_NOTDROPPED );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfYIsLessThanX( script_state_t& state, ai_state_t& self )
{
    // IfYIsLessThanX()
    /// @author ZZ
    /// @details This function proceeds if tmpy is less than tmpx

    SCRIPT_FUNCTION_BEGIN();

    returncode = ( state.y < state.x );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfBlocked( script_state_t& state, ai_state_t& self )
{
    // IfBlocked()
    /// @author ZZ
    /// @details This function proceeds if the character blocked the attack of another
    /// character this update

    SCRIPT_FUNCTION_BEGIN();

    returncode = HAS_SOME_BITS( self.alert, ALERTIF_BLOCKED );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs0( script_state_t& state, ai_state_t& self )
{
    SCRIPT_FUNCTION_BEGIN();

    returncode = ( 0 == self.state );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs1( script_state_t& state, ai_state_t& self )
{
    SCRIPT_FUNCTION_BEGIN();

    returncode = ( 1 == self.state );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs2( script_state_t& state, ai_state_t& self )
{
    SCRIPT_FUNCTION_BEGIN();

    returncode = ( 2 == self.state );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs3( script_state_t& state, ai_state_t& self )
{
    SCRIPT_FUNCTION_BEGIN();

    returncode = ( 3 == self.state );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs4( script_state_t& state, ai_state_t& self )
{
    SCRIPT_FUNCTION_BEGIN();

    returncode = ( 4 == self.state );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs5( script_state_t& state, ai_state_t& self )
{
    SCRIPT_FUNCTION_BEGIN();

    returncode = ( 5 == self.state );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs6( script_state_t& state, ai_state_t& self )
{
    SCRIPT_FUNCTION_BEGIN();

    returncode = ( 6 == self.state );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs7( script_state_t& state, ai_state_t& self )
{
    SCRIPT_FUNCTION_BEGIN();

    returncode = ( 7 == self.state );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfContentIs( script_state_t& state, ai_state_t& self )
{
    // IfContentIs( tmpargument = "test" )
    /// @author ZZ
    /// @details This function proceeds if the content matches tmpargument

    SCRIPT_FUNCTION_BEGIN();

    returncode = ( state.argument == self.content );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIsNot( script_state_t& state, ai_state_t& self )
{
    // IfStateIsNot( tmpargument = "test" )
    /// @author ZZ
    /// @details This function proceeds if the character's state does not equal tmpargument

    SCRIPT_FUNCTION_BEGIN();

    returncode = ( state.argument != self.state );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfXIsEqualToY( script_state_t& state, ai_state_t& self )
{
    // These functions proceed if tmpx and tmpy are the same

    SCRIPT_FUNCTION_BEGIN();

    returncode = ( state.x == state.y );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DebugMessage( script_state_t& state, ai_state_t& self )
{
    // DebugMessage()
    /// @author ZZ
    /// @details This function spits out some useful numbers

    SCRIPT_FUNCTION_BEGIN();

    DisplayMsg_printf( "aistate %d, aicontent %d, target %" PRIuZ, self.state, self.content, self.getTarget().get() );
    DisplayMsg_printf( "tmpx %d, tmpy %d", state.x, state.y );
    DisplayMsg_printf( "tmpdistance %d, tmpturn %d", state.distance, state.turn );
    DisplayMsg_printf( "tmpargument %d, selfturn %d", state.argument, int32_t(pchr->ori.facing_z) );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHitGround( script_state_t& state, ai_state_t& self )
{
    // IfHitGround()
    /// @author ZZ
    /// @details This function proceeds if a character hit the ground this update.
    /// Used to determine when to play the sound for a dropped item

    SCRIPT_FUNCTION_BEGIN();

    returncode = HAS_SOME_BITS( self.alert, ALERTIF_HITGROUND );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfNameIsKnown( script_state_t& state, ai_state_t& self )
{
    // IfNameIsKnown()
    /// @author ZZ
    /// @details This function proceeds if the character's name is known

    SCRIPT_FUNCTION_BEGIN();

    returncode = pchr->isNameKnown();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfUsageIsKnown( script_state_t& state, ai_state_t& self )
{
    // IfUsageIsKnown()
    /// @author ZZ
    /// @details This function proceeds if the character's usage is known

    SCRIPT_FUNCTION_BEGIN();

    returncode = ppro->isUsageKnown();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHoldingItemID( script_state_t& state, ai_state_t& self )
{
    // IfHoldingItemID( tmpargument = "idsz" )
    /// @author ZZ
    /// @details This function proceeds if the character is holding a specified item
    /// in hand, setting tmpargument to the latch button to press to use it

    SCRIPT_FUNCTION_BEGIN();

    returncode = (pchr->isWieldingItemIDSZ(Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument)) != nullptr);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHoldingRangedWeapon( script_state_t& state, ai_state_t& self )
{
    // IfHoldingRangedWeapon()
    /// @author ZZ
    /// @details This function passes if the character is holding a ranged weapon, returning
    /// the latch to press to use it.  This also checks ammo.

    SCRIPT_FUNCTION_BEGIN();

    returncode = false;
    state.argument = 0;

    // Check right hand
    const std::shared_ptr<Object> &rightHandItem = objectHandler()[pchr->getHeldObject(SLOT_RIGHT)];

    if (rightHandItem)
    {
        if ( rightHandItem->getProfile()->isRangedWeapon() && (0 == rightHandItem->ammomax || (0 != rightHandItem->ammo)))
        {
            state.argument = LATCHBUTTON_RIGHT;
            returncode = true;
        }
    }

    //50% chance to check left hand even though we have already found one in our right hand
    if ( !returncode || Random::nextBool() )
    {
        // Check left hand
        const std::shared_ptr<Object> &leftHandItem = objectHandler()[pchr->getHeldObject(SLOT_LEFT)];
        if (leftHandItem)
        {
            if ( leftHandItem->getProfile()->isRangedWeapon() && (0 == leftHandItem->ammomax || (0 != leftHandItem->ammo)))
            {
                state.argument = LATCHBUTTON_LEFT;
                returncode = true;
            }
        }

    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHoldingMeleeWeapon( script_state_t& state, ai_state_t& self )
{
    // IfHoldingMeleeWeapon()
    /// @author ZZ
    /// @details This function proceeds if the character is holding a specified item
    /// in hand, setting tmpargument to the latch button to press to use it

    SCRIPT_FUNCTION_BEGIN();

    returncode = false;
    state.argument = 0;

    if ( !returncode )
    {
        // Check right hand
        const std::shared_ptr<Object> &rightItem = pchr->getRightHandItem();
        if (rightItem)
        {
            if ( !rightItem->getProfile()->isRangedWeapon() && rightItem->getProfile()->getWeaponAction() != ACTION_PA )
            {
                if ( 0 == state.argument || ( worldUpdateCount() & 1 ) )
                {
                    state.argument = LATCHBUTTON_RIGHT;
                    returncode = true;
                }
            }
        }
    }

    if ( !returncode )
    {
        // Check left hand
        const std::shared_ptr<Object> &leftItem = pchr->getLeftHandItem();
        if (leftItem)
        {
            if ( !leftItem->getProfile()->isRangedWeapon() && leftItem->getProfile()->getWeaponAction() != ACTION_PA )
            {
                state.argument = LATCHBUTTON_LEFT;
                returncode = true;
            }
        }
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHoldingShield( script_state_t& state, ai_state_t& self )
{
    // IfHoldingShield()
    /// @author ZZ
    /// @details This function proceeds if the character is holding a specified item
    /// in hand, setting tmpargument to the latch button to press to use it. The button will need to be held down.

    SCRIPT_FUNCTION_BEGIN();

    returncode = false;
    state.argument = 0;

    if ( !returncode )
    {
        // Check right hand
        const std::shared_ptr<Object> &rightItem = pchr->getRightHandItem();
        if ( rightItem )
        {
            if ( rightItem->getProfile()->getWeaponAction() == ACTION_PA )
            {
                state.argument = LATCHBUTTON_RIGHT;
                returncode = true;
            }
        }
    }

    if ( !returncode )
    {
        // Check left hand
        const std::shared_ptr<Object> &leftItem = pchr->getLeftHandItem();
        if ( leftItem )
        {     
            if ( leftItem->getProfile()->getWeaponAction() == ACTION_PA )
            {
                state.argument = LATCHBUTTON_LEFT;
                returncode = true;
            }
        }
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfKursed( script_state_t& state, ai_state_t& self )
{
    // IfKursed()
    /// @author ZZ
    /// @details This function proceeds if the character is kursed

    SCRIPT_FUNCTION_BEGIN();

    returncode = pchr->isKursed();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfOverWater( script_state_t& state, ai_state_t& self )
{
    // IfOverWater()
    /// @author ZZ
    /// @details This function proceeds if the character is on a water tile

    SCRIPT_FUNCTION_BEGIN();

    returncode = pchr->isOnWaterTile();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfThrown( script_state_t& state, ai_state_t& self )
{
    // IfThrown()
    /// @author ZZ
    /// @details This function proceeds if the character was thrown this update.

    SCRIPT_FUNCTION_BEGIN();

    returncode = HAS_SOME_BITS( self.alert, ALERTIF_THROWN );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfCrushed( script_state_t& state, ai_state_t& self )
{
    // IfCrushed()
    /// @author ZZ
    /// @details This function proceeds if the character was crushed in a passage this
    /// update.

    SCRIPT_FUNCTION_BEGIN();

    returncode = HAS_SOME_BITS( self.alert, ALERTIF_CRUSHED );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfNotPutAway( script_state_t& state, ai_state_t& self )
{
    // IfNotPutAway()
    /// @author ZZ
    /// @details This function proceeds if the character couldn't be put into another
    /// character's pockets for some reason.
    /// It might be kursed or too big or something

    SCRIPT_FUNCTION_BEGIN();

    returncode = HAS_SOME_BITS( self.alert, ALERTIF_NOTPUTAWAY );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTakenOut( script_state_t& state, ai_state_t& self )
{
    // IfTakenOut()
    /// @author ZZ
    /// @details This function proceeds if the character is equiped in another's inventory,
    /// and the holder tried to unequip it ( take it out of pack ), but the
    /// item was kursed and didn't cooperate

    SCRIPT_FUNCTION_BEGIN();

    returncode = HAS_SOME_BITS( self.alert, ALERTIF_TAKENOUT );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfAmmoOut( script_state_t& state, ai_state_t& self )
{
    // IfAmmoOut()
    /// @author ZZ
    /// @details This function proceeds if the character itself has no ammo left.
    /// This is for crossbows and such, not archers.

    SCRIPT_FUNCTION_BEGIN();

    returncode = ( 0 == pchr->ammo );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIsOdd( script_state_t& state, ai_state_t& self )
{
    // IfStateIsOdd()
    /// @author ZZ
    /// @details This function proceeds if the character's state is 1, 3, 5, 7, etc.

    SCRIPT_FUNCTION_BEGIN();

    returncode = idlib::is_odd(self.state);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHeldInLeftHand( script_state_t& state, ai_state_t& self )
{
    // IfHeldInLeftHand()
    /// @author ZZ
    /// @details This function passes if another character is holding the character in its
    /// left hand.
    /// Usage: Used mostly by enchants that target the item of the other hand

    SCRIPT_FUNCTION_BEGIN();

    returncode = false;
    const std::shared_ptr<Object> holder = objectHandler()[pchr->getHolderRef()];
    if (holder)
    {
        returncode = holder->getHeldObject(SLOT_LEFT) == pchr->getObjRef();
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHitVulnerable( script_state_t& state, ai_state_t& self )
{
    // IfHitVulnerable()
    /// @author ZZ
    /// @details This function proceeds if the character was hit by a weapon of its
    /// vulnerability IDSZ.
    /// For example, a werewolf gets hit by a [SILV] bullet.

    SCRIPT_FUNCTION_BEGIN();

    returncode = HAS_SOME_BITS( self.alert, ALERTIF_HITVULNERABLE );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfEquipped( script_state_t& state, ai_state_t& self )
{
    // This proceeds if the character is equipped

    SCRIPT_FUNCTION_BEGIN();

    returncode = pchr->isEquipped();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_FlashVariable( script_state_t& state, ai_state_t& self )
{
    // FlashVariable( tmpargument = "amount" )

    /// @author ZZ
    /// @details This function makes the character flash according to tmpargument

    SCRIPT_FUNCTION_BEGIN();

    FlashObject( pchr, state.argument );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_FlashVariableHeight( script_state_t& state, ai_state_t& self )
{
    // FlashVariableHeight( tmpturn = "intensity bottom", tmpx = "bottom", tmpdistance = "intensity top", tmpy = "top" )
    /// @author ZZ
    /// @details This function makes the character flash, feet one color, head another.
    ///          This function sets a character's lighting depending on vertex height...
    ///          Can make feet dark and head light...

    SCRIPT_FUNCTION_BEGIN();

    pchr->inst.flashVariableHeight(Ego::Math::clipBits<16>(state.turn), state.x, state.distance, state.y);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs8( script_state_t& state, ai_state_t& self )
{
    SCRIPT_FUNCTION_BEGIN();

    returncode = ( 8 == self.state );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs9( script_state_t& state, ai_state_t& self )
{
    SCRIPT_FUNCTION_BEGIN();

    returncode = ( 9 == self.state );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs10( script_state_t& state, ai_state_t& self )
{
    SCRIPT_FUNCTION_BEGIN();

    returncode = ( 10 == self.state );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs11( script_state_t& state, ai_state_t& self )
{
    SCRIPT_FUNCTION_BEGIN();

    returncode = ( 11 == self.state );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs12( script_state_t& state, ai_state_t& self )
{
    SCRIPT_FUNCTION_BEGIN();

    returncode = ( 12 == self.state );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs13( script_state_t& state, ai_state_t& self )
{
    SCRIPT_FUNCTION_BEGIN();

    returncode = ( 13 == self.state );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs14( script_state_t& state, ai_state_t& self )
{
    SCRIPT_FUNCTION_BEGIN();

    returncode = ( 14 == self.state );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs15( script_state_t& state, ai_state_t& self )
{
    SCRIPT_FUNCTION_BEGIN();

    returncode = ( 15 == self.state );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DoNothing( script_state_t& state, ai_state_t& self )
{
    // DoNothing()
    /// @author ZF
    /// @details This function does nothing
    /// Use this for debugging or in combination with a Else function

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHolderBlocked( script_state_t& state, ai_state_t& self )
{
    // IfHolderBlocked()
    /// @author ZF
    /// @details This function passes if the holder blocked an attack

    SCRIPT_FUNCTION_BEGIN();

    ObjectRef iattached = pchr->getHolderRef();

    if ( objectHandler().exists( iattached ) )
    {
        BIT_FIELD bits = objectHandler().get(iattached)->ai.alert;

        if ( HAS_SOME_BITS( bits, ALERTIF_BLOCKED ) )
        {
            auto iLastAttacker = objectHandler().get(iattached)->ai.getLastAttacker();

            if ( objectHandler().exists(iLastAttacker) )
            {
                self.setTarget(iLastAttacker);
            }
            else
            {
                returncode = false;
            }
        }
        else
        {
            returncode = false;
        }
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfOperatorIsLinux( script_state_t& state, ai_state_t& self )
{
    // IfOperatorIsLinux()
    /// @author ZF
    /// @details Proceeds if running on linux

    SCRIPT_FUNCTION_BEGIN();

#if defined(ID_LINUX)
    returncode = true;
#else
    returncode = false;
#endif

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfSomeoneIsStealing( script_state_t& state, ai_state_t& self )
{
    // IfSomeoneIsStealing()
    /// @author ZF
    /// @details This function passes if someone stealed from it's shop

    SCRIPT_FUNCTION_BEGIN();

    returncode = ( self.order_value == Passage::SHOP_STOLEN && self.order_counter == Passage::SHOP_THEFT );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfBackstabbed( script_state_t& state, ai_state_t& self )
{
    // IfBackstabbed()
    /// @author ZF
    /// @details Proceeds if HitFromBehind, target has [STAB] skill and damage dealt is physical
    /// automatically fails if attacker has a code of conduct

    SCRIPT_FUNCTION_BEGIN();

    //Now check if it really was backstabbed
    returncode = false;
    if ( HAS_SOME_BITS( self.alert, ALERTIF_ATTACKED ) )
    {
        //Who is the dirty backstabber?
        Object *pLastAttacker = objectHandler().get( self.getLastAttacker() );
        if (!pLastAttacker || pLastAttacker->isTerminated()) return false;

        //Only if hit from behind
        // (8192 / (2^16-1)) * 360 ~ 45 degrees
        static const Facing tolerance(8192);
        if ( self.directionlast >= (ATK_BEHIND - tolerance) && self.directionlast < (ATK_BEHIND + tolerance) )
        {
            //And require the backstab skill
            if (pLastAttacker->hasPerk(Ego::Perks::BACKSTAB) )
            {
                //Finally we require it to be physical damage!
                if (DamageType_isPhysical(self.damagetypelast))
                {
                    returncode = true;
                }
            }
        }
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_End( script_state_t& state, ai_state_t& self )
{
    // End()
    /// @author ZZ
    /// @details This Is the last function in a script

    SCRIPT_FUNCTION_BEGIN();

    self.terminate = true;
    returncode = false;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfOperatorIsMacintosh( script_state_t& state, ai_state_t& self )
{
    // IfOperatorIsMacintosh()
    /// @author ZF
    /// @details Proceeds if the current running OS is mac

    SCRIPT_FUNCTION_BEGIN();

#if defined(ID_OSX)
    returncode = true;
#else
    returncode = false;
#endif

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfModuleHasIDSZ( script_state_t& state, ai_state_t& self )
{
    // IfModuleHasIDSZ( tmpargument = "message number with module name", tmpdistance = "idsz" )

    /// @author ZF
    /// @details Proceeds if the specified module has the required IDSZ specified in tmpdistance
    /// The module folder name to be checked is a string from message.txt

    SCRIPT_FUNCTION_BEGIN();

    ///use message.txt to send the module name
    if ( !ppro->isValidMessageID((int)state.argument) ) return false;

    returncode = ModuleProfile::moduleHasIDSZ( activeModule().getName(), state.distance);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfLevelUp( script_state_t& state, ai_state_t& self )
{
    // IfLevelUp()
    /// @author ZF
    /// @details This function proceeds if the character gained a new level this update
    SCRIPT_FUNCTION_BEGIN();

    returncode = HAS_SOME_BITS( self.alert, ALERTIF_LEVELUP );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStealthed( script_state_t& state, ai_state_t& self )
{
    // IfStealthed()
    /// @author ZF
    /// @details Returns true if the Object is currently in stealth mode and not detected

    SCRIPT_FUNCTION_BEGIN();

    returncode = pchr->isStealthed();

    SCRIPT_FUNCTION_END();
}
