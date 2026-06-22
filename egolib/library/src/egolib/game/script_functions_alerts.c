/// @file egolib/game/script_functions_alerts.c
/// @brief Alert-check functions: test whether specific events have happened to the AI character

#include "egolib/game/script_functions_internal.h"

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
uint8_t scr_IfSpawned( script_state_t& state, ai_state_t& self )
{
    // IfSpawned()
    /// @author ZZ
    /// @details This function proceeds if the character was spawned this update

    if (!resolveSelfContext(self).isResolved()) return false;

    // Proceed only if it's a new character
    return HAS_SOME_BITS( self.alert, ALERTIF_SPAWNED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfAttacked( script_state_t& state, ai_state_t& self )
{
    // IfAttacked()
    /// @author ZZ
    /// @details This function proceeds if the character ( an item ) was put in its
    /// owner's pocket this update

    if (!resolveSelfContext(self).isResolved()) return false;

    // Proceed only if the character was damaged
    return HAS_SOME_BITS( self.alert, ALERTIF_ATTACKED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfBumped( script_state_t& state, ai_state_t& self )
{
    // IfBumped()
    /// @author ZZ
    /// @details This function proceeds if the character was bumped by another character
    /// this update

    if (!resolveSelfContext(self).isResolved()) return false;

    // Proceed only if the character was bumped
    return HAS_SOME_BITS( self.alert, ALERTIF_BUMPED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfOrdered( script_state_t& state, ai_state_t& self )
{
    // IfOrdered()
    /// @author ZZ
    /// @details This function proceeds if the character got an order from another
    /// character on its team this update

    if (!resolveSelfContext(self).isResolved()) return false;

    // Proceed only if the character was ordered
    return HAS_SOME_BITS( self.alert, ALERTIF_ORDERED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfCalledForHelp( script_state_t& state, ai_state_t& self )
{
    // IfCalledForHelp()
    /// @author ZZ
    /// @details This function proceeds if one of the character's teammates was nearly
    /// killed this update

    if (!resolveSelfContext(self).isResolved()) return false;

    // Proceed only if the character was called for help
    return HAS_SOME_BITS( self.alert, ALERTIF_CALLEDFORHELP );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfKilled( script_state_t& state, ai_state_t& self )
{
    // IfKilled()
    /// @author ZZ
    /// @details This function proceeds if the character was killed this update

    if (!resolveSelfContext(self).isResolved()) return false;

    // Proceed only if the character's been killed
    return HAS_SOME_BITS( self.alert, ALERTIF_KILLED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHealed( script_state_t& state, ai_state_t& self )
{
    // IfHealed()
    /// @author ZZ
    /// @details This function proceeds if the character was healed by a healing particle

    if (!resolveSelfContext(self).isResolved()) return false;

    // Proceed only if the character was healed
    return HAS_SOME_BITS( self.alert, ALERTIF_HEALED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfGrabbed( script_state_t& state, ai_state_t& self )
{
    // IfGrabbed()
    /// @author ZZ
    /// @details This function proceeds if the character was grabbed (picked up) this update.
    /// Used mostly by item characters

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_GRABBED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfDropped( script_state_t& state, ai_state_t& self )
{
    // IfDropped()
    /// @author ZZ
    /// @details This function proceeds if the character was dropped this update.
    /// Used mostly by item characters

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_DROPPED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfReaffirmed( script_state_t& state, ai_state_t& self )
{
    // IfReaffirmed()
    /// @author ZZ
    /// @details This function proceeds if the character was damaged by its reaffirm
    /// damage type.  Used to relight the torch.

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_REAFFIRMED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfUsed( script_state_t& state, ai_state_t& self )
{
    // IfUsed()
    /// @author ZZ
    /// @details This function proceeds if the character was used by its holder or rider.
    /// Character's cannot be used if their reload time is greater than 0

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_USED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfCleanedUp( script_state_t& state, ai_state_t& self )
{
    // IfCleanedUp()
    /// @author ZZ
    /// @details This function proceeds if the character is dead and if the boss told it
    /// to clean itself up

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_CLEANEDUP );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfScoredAHit( script_state_t& state, ai_state_t& self )
{
    // IfScoredAHit()
    /// @author ZZ
    /// @details This function proceeds if the character damaged another character this
    /// update. If it's a held character it also sets the target to whoever was hit

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_SCOREDAHIT );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfDisaffirmed( script_state_t& state, ai_state_t& self )
{
    // IfDisaffirmed()
    /// @author ZZ
    /// @details This function proceeds if the character was disaffirmed.
    /// This doesn't seem useful anymore.

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_DISAFFIRMED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfChanged( script_state_t& state, ai_state_t& self )
{
    // IfChanged()
    /// @author ZZ
    /// @details This function proceeds if the character was polymorphed.
    /// Needed for morph spells and such

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_CHANGED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfInWater( script_state_t& state, ai_state_t& self )
{
    // IfInWater()
    /// @author ZZ
    /// @details This function proceeds if the character has just entered into some water
    /// this update ( and the water is really water, not fog or another effect )

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_INWATER );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfBored( script_state_t& state, ai_state_t& self )
{
    // IfBored()
    /// @author ZZ
    /// @details This function proceeds if the character has been standing idle too long

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_BORED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTooMuchBaggage( script_state_t& state, ai_state_t& self )
{
    // IfTooMuchBaggage()
    /// @author ZZ
    /// @details This function proceeds if the character tries to put an item in his/her
    /// pockets, but the character already has 6 items in the inventory.
    /// Used to tell the players what's going on.

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_TOOMUCHBAGGAGE );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHitFromBehind(script_state_t& state, ai_state_t& self) {
    /// IfHitFromBehind()
    /// @author ZZ
    /// @details This function proceeds if the last attack to the character came
    /// from behind

    if (!resolveSelfContext(self).isResolved()) return false;

    // (8192 / (2^16-1)) * 360 ~ 45 degrees
    static const Facing tolerance(8192);
    return self.directionlast >= (ATK_BEHIND - tolerance) && self.directionlast < (ATK_BEHIND + tolerance);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHitFromFront(script_state_t& state, ai_state_t& self) {
    /// IfHitFromFront()
    /// @author ZZ
    /// @details This function proceeds if the last attack to the character came
    /// from the front

    if (!resolveSelfContext(self).isResolved()) return false;

    // (8192 / (2^16-1)) * 360 ~ 45 degrees
    static const Facing tolerance(8192);
    return self.directionlast >= (ATK_FRONT - tolerance) && self.directionlast < (ATK_FRONT + tolerance);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHitFromLeft(script_state_t& state, ai_state_t& self) {
    /// IfHitFromLeft()
    /// @author ZZ
    /// @details This function proceeds if the last attack to the character came
    /// from the left

    if (!resolveSelfContext(self).isResolved()) return false;

    // (8192 / (2^16-1)) * 360 ~ 45 degrees
    static const Facing tolerance(8192);
    return self.directionlast >= (ATK_LEFT - tolerance) && self.directionlast < (ATK_LEFT + tolerance);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHitFromRight(script_state_t& state, ai_state_t& self) {
    /// IfHitFromRight()
    /// @author ZZ
    /// @details This function proceeds if the last attack to the character came
    /// from the right

    if (!resolveSelfContext(self).isResolved()) return false;

    // (8192 / (2^16-1)) * 360 ~ 45 degrees
    static const Facing tolerance(8192);
    return self.directionlast >= (ATK_RIGHT - tolerance) && self.directionlast < (ATK_RIGHT + tolerance);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfNotDropped( script_state_t& state, ai_state_t& self )
{
    // IfNotDropped()
    /// @author ZZ
    /// @details This function proceeds if the character is kursed and another character
    /// was holding it and tried to drop it

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_NOTDROPPED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfBlocked( script_state_t& state, ai_state_t& self )
{
    // IfBlocked()
    /// @author ZZ
    /// @details This function proceeds if the character blocked the attack of another
    /// character this update

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_BLOCKED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHitGround( script_state_t& state, ai_state_t& self )
{
    // IfHitGround()
    /// @author ZZ
    /// @details This function proceeds if a character hit the ground this update.
    /// Used to determine when to play the sound for a dropped item

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_HITGROUND );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHitVulnerable( script_state_t& state, ai_state_t& self )
{
    // IfHitVulnerable()
    /// @author ZZ
    /// @details This function proceeds if the character was hit by a weapon of its
    /// vulnerability IDSZ.
    /// For example, a werewolf gets hit by a [SILV] bullet.

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_HITVULNERABLE );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfThrown( script_state_t& state, ai_state_t& self )
{
    // IfThrown()
    /// @author ZZ
    /// @details This function proceeds if the character was thrown this update.

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_THROWN );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfCrushed( script_state_t& state, ai_state_t& self )
{
    // IfCrushed()
    /// @author ZZ
    /// @details This function proceeds if the character was crushed in a passage this
    /// update.

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_CRUSHED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfNotPutAway( script_state_t& state, ai_state_t& self )
{
    // IfNotPutAway()
    /// @author ZZ
    /// @details This function proceeds if the character couldn't be put into another
    /// character's pockets for some reason.
    /// It might be kursed or too big or something

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_NOTPUTAWAY );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTakenOut( script_state_t& state, ai_state_t& self )
{
    // IfTakenOut()
    /// @author ZZ
    /// @details This function proceeds if the character is equiped in another's inventory,
    /// and the holder tried to unequip it ( take it out of pack ), but the
    /// item was kursed and didn't cooperate

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_TAKENOUT );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfLevelUp( script_state_t& state, ai_state_t& self )
{
    // IfLevelUp()
    /// @author ZF
    /// @details This function proceeds if the character gained a new level this update
    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_LEVELUP );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfSomeoneIsStealing( script_state_t& state, ai_state_t& self )
{
    // IfSomeoneIsStealing()
    /// @author ZF
    /// @details This function passes if someone stealed from it's shop

    if (!resolveSelfContext(self).isResolved()) return false;

    return ( self.order_value == Passage::SHOP_STOLEN && self.order_counter == Passage::SHOP_THEFT );
}
