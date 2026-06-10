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

bool isLiveStateObjectRef(ObjectRef objectRef)
{
    return tryObject(objectRef) != nullptr;
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
    WeatherState& weatherState = GameSessionContext::get().weatherState();
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
    DisplayMsg_printf( "tmpargument %d, selfturn %d", state.argument, int32_t(selfContext.object->getFacingZ()) );

    return true;
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
uint8_t scr_IfStealthed( script_state_t& state, ai_state_t& self )
{
    // IfStealthed()
    /// @author ZF
    /// @details Returns true if the Object is currently in stealth mode and not detected

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfStateContext selfContext = makeSelfStateContext(self);
    return selfContext.targetInfo->isStealthed();
}
