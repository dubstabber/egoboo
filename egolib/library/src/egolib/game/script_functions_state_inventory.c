/// @file egolib/game/script_functions_state_inventory.c
/// @brief Inventory/equipment/knowledge/world condition queries (split from script_functions_state.c)

#include "egolib/game/script_functions_internal.h"
#include "egolib/game/script_functions_state_internal.h"

namespace
{
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
