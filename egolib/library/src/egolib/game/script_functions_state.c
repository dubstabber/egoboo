/// @file egolib/game/script_functions_state.c
/// @brief State checks, condition queries, comparisons, and flow control

#include "egolib/game/script_functions_internal.h"

namespace
{
struct SelfStateContext
{
    Object* object = nullptr;
    ObjectProfile* profile = nullptr;
    const ITargetInfo* targetInfo = nullptr;
    const IInventoryHolder* inventory = nullptr;
    const IScriptable* scriptable = nullptr;
    IVisualControl* visual = nullptr;

    bool isResolved() const
    {
        return object != nullptr &&
               profile != nullptr &&
               targetInfo != nullptr &&
               inventory != nullptr &&
               scriptable != nullptr &&
               visual != nullptr;
    }
};

SelfStateContext makeSelfStateContext(const ai_state_t& self)
{
    const ResolvedSelfContext resolvedSelf = resolveSelfContext(self);
    SelfStateContext context;
    context.object = resolvedSelf.object;
    context.profile = resolvedSelf.profile;
    if (!resolvedSelf.isResolved())
    {
        return context;
    }

    context.targetInfo = static_cast<const ITargetInfo*>(resolvedSelf.object);
    context.inventory = static_cast<const IInventoryHolder*>(resolvedSelf.object);
    context.scriptable = static_cast<const IScriptable*>(resolvedSelf.object);
    context.visual = static_cast<IVisualControl*>(resolvedSelf.object);
    return context;
}

ObjectRef heldItemRef(const IInventoryHolder& holder, slot_t slot)
{
    return holder.getHeldObject(slot);
}

bool isUsableRangedWeapon(ObjectRef itemRef)
{
    const IItemInfo* item = tryItemInfo(itemRef);
    const ICharacterState* itemState = tryCharacterState(itemRef);
    return item != nullptr &&
           itemState != nullptr &&
           item->isRangedWeapon() &&
           (0 == itemState->getAmmoMax() || 0 != itemState->getAmmo());
}

bool isMeleeWeapon(ObjectRef itemRef)
{
    const IItemInfo* item = tryItemInfo(itemRef);
    return item != nullptr && item->isMeleeWeapon();
}

bool isShield(ObjectRef itemRef)
{
    const IItemInfo* item = tryItemInfo(itemRef);
    return item != nullptr && item->isShield();
}

bool isLiveStateObjectRef(ObjectRef objectRef)
{
    return tryObject(objectRef) != nullptr;
}

bool trySetResolvedTarget(ai_state_t& self, ObjectRef objectRef)
{
    if (!isLiveStateObjectRef(objectRef))
    {
        return false;
    }

    self.setTarget(objectRef);
    return true;
}

bool hasExistingHolder(const ITargetInfo& objectTargetInfo)
{
    return isLiveStateObjectRef(objectTargetInfo.getHolderRef());
}

bool trySetTargetToHolderLastAttacker(ai_state_t& self, const ITargetInfo& objectTargetInfo)
{
    const IScriptable* holder = tryScriptable(objectTargetInfo.getHolderRef());
    if (holder == nullptr || !HAS_SOME_BITS(holder->getAIAlertBits(), ALERTIF_BLOCKED))
    {
        return false;
    }

    return trySetResolvedTarget(self, holder->getAILastAttacker());
}

bool activeModuleHasIdszWithValidMessage(const ObjectProfile& profile,
                                         int messageId,
                                         IDSZ2 idsz)
{
    if (!profile.isValidMessageID(messageId))
    {
        return false;
    }

    const std::string& moduleName = profile.getMessage(messageId);
    if (moduleName.empty())
    {
        return false;
    }

    try
    {
        return ModuleProfile::moduleHasIDSZ(moduleName, idsz);
    }
    catch (...)
    {
        return false;
    }
}
}

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
    WeatherState& weatherState = GameSessionContext::get().weatherState();
    weatherState.timer_reset = state.argument;
    weatherState.time = state.argument;

    return true;
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
uint8_t scr_IfSitting( script_state_t& state, ai_state_t& self )
{
    // IfSitting()
    /// @author ZZ
    /// @details This function proceeds if the character is riding a mount

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfStateContext selfContext = makeSelfStateContext(self);
    return hasExistingHolder(*selfContext.targetInfo);
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
uint8_t scr_IfGrogged( script_state_t& state, ai_state_t& self )
{
    // IfGrogged()
    /// @author ZZ
    /// @details This function proceeds if the character has been grogged ( a type of
    /// confusion ) this update

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfStateContext selfContext = makeSelfStateContext(self);
    return selfContext.targetInfo->getGrogTimer() > 0 && HAS_SOME_BITS( self.alert, ALERTIF_CONFUSED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfDazed( script_state_t& state, ai_state_t& self )
{
    // IfDazed()
    /// @author ZZ
    /// @details This function proceeds if the character has been dazed ( a type of
    /// confusion ) this update

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfStateContext selfContext = makeSelfStateContext(self);
    return selfContext.targetInfo->getDazeTimer() > 0 && HAS_SOME_BITS( self.alert, ALERTIF_CONFUSED );
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
uint8_t scr_IfYIsLessThanX( script_state_t& state, ai_state_t& self )
{
    // IfYIsLessThanX()
    /// @author ZZ
    /// @details This function proceeds if tmpy is less than tmpx

    if (!resolveSelfContext(self).isResolved()) return false;

    return ( state.y < state.x );
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
    DisplayMsg_printf( "tmpargument %d, selfturn %d", state.argument, int32_t(selfContext.object->getFacingZ()) );

    return true;
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
uint8_t scr_IfNameIsKnown( script_state_t& state, ai_state_t& self )
{
    // IfNameIsKnown()
    /// @author ZZ
    /// @details This function proceeds if the character's name is known

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfStateContext selfContext = makeSelfStateContext(self);
    return selfContext.targetInfo->isNameKnown();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfUsageIsKnown( script_state_t& state, ai_state_t& self )
{
    // IfUsageIsKnown()
    /// @author ZZ
    /// @details This function proceeds if the character's usage is known

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfStateContext selfContext = makeSelfStateContext(self);
    return selfContext.profile->isUsageKnown();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHoldingItemID( script_state_t& state, ai_state_t& self )
{
    // IfHoldingItemID( tmpargument = "idsz" )
    /// @author ZZ
    /// @details This function proceeds if the character is holding a specified item
    /// in hand, setting tmpargument to the latch button to press to use it

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfStateContext selfContext = makeSelfStateContext(self);
    return selfContext.targetInfo->wieldsItemIDSZ(Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHoldingRangedWeapon( script_state_t& state, ai_state_t& self )
{
    // IfHoldingRangedWeapon()
    /// @author ZZ
    /// @details This function passes if the character is holding a ranged weapon, returning
    /// the latch to press to use it.  This also checks ammo.

    if (!resolveSelfContext(self).isResolved()) return false;

    bool hasWeapon = false;
    state.argument = 0;
    const SelfStateContext selfContext = makeSelfStateContext(self);
    const IInventoryHolder& selfInventory = *selfContext.inventory;

    // Check right hand
    const ObjectRef rightHandItem = heldItemRef(selfInventory, SLOT_RIGHT);
    if (isUsableRangedWeapon(rightHandItem))
    {
        state.argument = LATCHBUTTON_RIGHT;
        hasWeapon = true;
    }

    //50% chance to check left hand even though we have already found one in our right hand
    if ( !hasWeapon || Random::nextBool() )
    {
        // Check left hand
        const ObjectRef leftHandItem = heldItemRef(selfInventory, SLOT_LEFT);
        if (isUsableRangedWeapon(leftHandItem))
        {
            state.argument = LATCHBUTTON_LEFT;
            hasWeapon = true;
        }
    }

    return hasWeapon;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHoldingMeleeWeapon( script_state_t& state, ai_state_t& self )
{
    // IfHoldingMeleeWeapon()
    /// @author ZZ
    /// @details This function proceeds if the character is holding a specified item
    /// in hand, setting tmpargument to the latch button to press to use it

    if (!resolveSelfContext(self).isResolved()) return false;

    bool hasWeapon = false;
    state.argument = 0;
    const SelfStateContext selfContext = makeSelfStateContext(self);
    const IInventoryHolder& selfInventory = *selfContext.inventory;

    if ( !hasWeapon )
    {
        // Check right hand
        const ObjectRef rightItem = heldItemRef(selfInventory, SLOT_RIGHT);
        if (isMeleeWeapon(rightItem))
        {
            if ( 0 == state.argument || ( worldUpdateCount() & 1 ) )
            {
                state.argument = LATCHBUTTON_RIGHT;
                hasWeapon = true;
            }
        }
    }

    if ( !hasWeapon )
    {
        // Check left hand
        const ObjectRef leftItem = heldItemRef(selfInventory, SLOT_LEFT);
        if (isMeleeWeapon(leftItem))
        {
            state.argument = LATCHBUTTON_LEFT;
            hasWeapon = true;
        }
    }

    return hasWeapon;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHoldingShield( script_state_t& state, ai_state_t& self )
{
    // IfHoldingShield()
    /// @author ZZ
    /// @details This function proceeds if the character is holding a specified item
    /// in hand, setting tmpargument to the latch button to press to use it. The button will need to be held down.

    if (!resolveSelfContext(self).isResolved()) return false;

    bool hasShield = false;
    state.argument = 0;
    const SelfStateContext selfContext = makeSelfStateContext(self);
    const IInventoryHolder& selfInventory = *selfContext.inventory;

    if ( !hasShield )
    {
        // Check right hand
        const ObjectRef rightItem = heldItemRef(selfInventory, SLOT_RIGHT);
        if (isShield(rightItem))
        {
            state.argument = LATCHBUTTON_RIGHT;
            hasShield = true;
        }
    }

    if ( !hasShield )
    {
        // Check left hand
        const ObjectRef leftItem = heldItemRef(selfInventory, SLOT_LEFT);
        if (isShield(leftItem))
        {
            state.argument = LATCHBUTTON_LEFT;
            hasShield = true;
        }
    }

    return hasShield;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfKursed( script_state_t& state, ai_state_t& self )
{
    // IfKursed()
    /// @author ZZ
    /// @details This function proceeds if the character is kursed

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfStateContext selfContext = makeSelfStateContext(self);
    return selfContext.targetInfo->isKursed();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfOverWater( script_state_t& state, ai_state_t& self )
{
    // IfOverWater()
    /// @author ZZ
    /// @details This function proceeds if the character is on a water tile

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfStateContext selfContext = makeSelfStateContext(self);
    return selfContext.targetInfo->isOnWaterTile();
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
uint8_t scr_IfAmmoOut( script_state_t& state, ai_state_t& self )
{
    // IfAmmoOut()
    /// @author ZZ
    /// @details This function proceeds if the character itself has no ammo left.
    /// This is for crossbows and such, not archers.

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfStateContext selfContext = makeSelfStateContext(self);
    return ( 0 == selfContext.targetInfo->getAmmo() );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIsOdd( script_state_t& state, ai_state_t& self )
{
    // IfStateIsOdd()
    /// @author ZZ
    /// @details This function proceeds if the character's state is 1, 3, 5, 7, etc.

    if (!resolveSelfContext(self).isResolved()) return false;

    return idlib::is_odd(self.state);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHeldInLeftHand( script_state_t& state, ai_state_t& self )
{
    // IfHeldInLeftHand()
    /// @author ZZ
    /// @details This function passes if another character is holding the character in its
    /// left hand.
    /// Usage: Used mostly by enchants that target the item of the other hand

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfStateContext selfContext = makeSelfStateContext(self);
    const ITargetInfo& selfTargetInfo = *selfContext.targetInfo;
    IInventoryHolder* holderInventory = tryInventoryHolder(selfTargetInfo.getHolderRef());
    return holderInventory != nullptr && holderInventory->getHeldObject(SLOT_LEFT) == self.getSelf();
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
uint8_t scr_IfEquipped( script_state_t& state, ai_state_t& self )
{
    // This proceeds if the character is equipped

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfStateContext selfContext = makeSelfStateContext(self);
    return selfContext.targetInfo->isEquipped();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_FlashVariable( script_state_t& state, ai_state_t& self )
{
    // FlashVariable( tmpargument = "amount" )

    /// @author ZZ
    /// @details This function makes the character flash according to tmpargument

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfStateContext selfContext = makeSelfStateContext(self);
    selfContext.visual->flash(state.argument);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_FlashVariableHeight( script_state_t& state, ai_state_t& self )
{
    // FlashVariableHeight( tmpturn = "intensity bottom", tmpx = "bottom", tmpdistance = "intensity top", tmpy = "top" )
    /// @author ZZ
    /// @details This function makes the character flash, feet one color, head another.
    ///          This function sets a character's lighting depending on vertex height...
    ///          Can make feet dark and head light...

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfStateContext selfContext = makeSelfStateContext(self);
    selfContext.visual->flashVariableHeight(Ego::Math::clipBits<16>(state.turn), state.x, state.distance, state.y);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs8( script_state_t& state, ai_state_t& self )
{
    if (!resolveSelfContext(self).isResolved()) return false;

    return ( 8 == self.state );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs9( script_state_t& state, ai_state_t& self )
{
    if (!resolveSelfContext(self).isResolved()) return false;

    return ( 9 == self.state );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs10( script_state_t& state, ai_state_t& self )
{
    if (!resolveSelfContext(self).isResolved()) return false;

    return ( 10 == self.state );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs11( script_state_t& state, ai_state_t& self )
{
    if (!resolveSelfContext(self).isResolved()) return false;

    return ( 11 == self.state );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs12( script_state_t& state, ai_state_t& self )
{
    if (!resolveSelfContext(self).isResolved()) return false;

    return ( 12 == self.state );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs13( script_state_t& state, ai_state_t& self )
{
    if (!resolveSelfContext(self).isResolved()) return false;

    return ( 13 == self.state );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs14( script_state_t& state, ai_state_t& self )
{
    if (!resolveSelfContext(self).isResolved()) return false;

    return ( 14 == self.state );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfStateIs15( script_state_t& state, ai_state_t& self )
{
    if (!resolveSelfContext(self).isResolved()) return false;

    return ( 15 == self.state );
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

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfStateContext selfContext = makeSelfStateContext(self);
    return trySetTargetToHolderLastAttacker(self, *selfContext.targetInfo);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfOperatorIsLinux( script_state_t& state, ai_state_t& self )
{
    // IfOperatorIsLinux()
    /// @author ZF
    /// @details Proceeds if running on linux

    if (!resolveSelfContext(self).isResolved()) return false;

#if defined(ID_LINUX)
    return true;
#else
    return false;
#endif
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


//--------------------------------------------------------------------------------------------
uint8_t scr_IfBackstabbed( script_state_t& state, ai_state_t& self )
{
    // IfBackstabbed()
    /// @author ZF
    /// @details Proceeds if HitFromBehind, target has [STAB] skill and damage dealt is physical
    /// automatically fails if attacker has a code of conduct

    if (!resolveSelfContext(self).isResolved()) return false;

    if (!HAS_SOME_BITS( self.alert, ALERTIF_ATTACKED ))
    {
        return false;
    }

    //Who is the dirty backstabber?
    const SelfStateContext selfContext = makeSelfStateContext(self);
    const ObjectRef lastAttackerRef = selfContext.scriptable->getAILastAttacker();
    const IInventoryHolder* lastAttackerHolder = tryInventoryHolder(lastAttackerRef);
    const ICharacterState* lastAttackerState = tryCharacterState(lastAttackerRef);
    if (lastAttackerHolder == nullptr || lastAttackerState == nullptr || lastAttackerHolder->isTerminated())
    {
        return false;
    }

    //Only if hit from behind
    // (8192 / (2^16-1)) * 360 ~ 45 degrees
    static const Facing tolerance(8192);
    return self.directionlast >= (ATK_BEHIND - tolerance) &&
           self.directionlast < (ATK_BEHIND + tolerance) &&
           lastAttackerState->hasPerk(Ego::Perks::BACKSTAB) &&
           DamageType_isPhysical(self.damagetypelast);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_End( script_state_t& state, ai_state_t& self )
{
    // End()
    /// @author ZZ
    /// @details This Is the last function in a script

    if (!resolveSelfContext(self).isResolved()) return false;

    self.terminate = true;
    return false;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfOperatorIsMacintosh( script_state_t& state, ai_state_t& self )
{
    // IfOperatorIsMacintosh()
    /// @author ZF
    /// @details Proceeds if the current running OS is mac

    if (!resolveSelfContext(self).isResolved()) return false;

#if defined(ID_OSX)
    return true;
#else
    return false;
#endif
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfModuleHasIDSZ( script_state_t& state, ai_state_t& self )
{
    // IfModuleHasIDSZ( tmpargument = "message number with module name", tmpdistance = "idsz" )

    /// @author ZF
    /// @details Proceeds if the specified module has the required IDSZ specified in tmpdistance
    /// The module folder name to be checked is a string from message.txt

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfStateContext selfContext = makeSelfStateContext(self);
    return activeModuleHasIdszWithValidMessage(*selfContext.profile,
                                               static_cast<int>(state.argument),
                                               Ego::Script::Interpreter::safeCast<IDSZ2>(state.distance));
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
uint8_t scr_IfStealthed( script_state_t& state, ai_state_t& self )
{
    // IfStealthed()
    /// @author ZF
    /// @details Returns true if the Object is currently in stealth mode and not detected

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfStateContext selfContext = makeSelfStateContext(self);
    return selfContext.targetInfo->isStealthed();
}
