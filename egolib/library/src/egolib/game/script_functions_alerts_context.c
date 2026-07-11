/// @file egolib/game/script_functions_alerts_context.c
/// @brief Role-dependent alert-check script functions.

#include "egolib/game/script_functions_internal.h"

namespace
{
struct SelfStateContext
{
    const ITargetInfo* targetInfo = nullptr;
    const IInventoryHolder* inventory = nullptr;
    const IScriptable* scriptable = nullptr;
    IVisualControl* visual = nullptr;

    bool isResolved() const
    {
        return targetInfo != nullptr &&
               inventory != nullptr &&
               scriptable != nullptr &&
               visual != nullptr;
    }
};

SelfStateContext makeSelfStateContext(const ai_state_t& self)
{
    SelfStateContext context;
    const ObjectRef selfRef = self.getSelf();
    context.targetInfo = tryTargetInfo(selfRef);
    context.inventory = tryInventoryHolder(selfRef);
    context.scriptable = tryScriptable(selfRef);
    context.visual = tryVisualControl(selfRef);
    return context;
}

bool isLiveAlertObjectRef(ObjectRef objectRef)
{
    return hasLiveObjectRef(objectRef);
}

bool hasExistingHolder(const ITargetInfo& objectTargetInfo)
{
    return isLiveAlertObjectRef(objectTargetInfo.getHolderRef());
}

bool trySetAlertResolvedTarget(ai_state_t& self, ObjectRef objectRef)
{
    if (!isLiveAlertObjectRef(objectRef))
    {
        return false;
    }

    self.setTarget(objectRef);
    return true;
}

bool trySetTargetToHolderLastAttacker(ai_state_t& self, const ITargetInfo& objectTargetInfo)
{
    const IScriptable* holder = tryScriptable(objectTargetInfo.getHolderRef());
    if (holder == nullptr || !HAS_SOME_BITS(holder->getAIAlertBits(), ALERTIF_BLOCKED))
    {
        return false;
    }

    return trySetAlertResolvedTarget(self, holder->getAILastAttacker());
}
}

//--------------------------------------------------------------------------------------------
uint8_t scr_IfSitting( script_state_t& state, ai_state_t& self )
{
    // IfSitting()
    /// @author ZZ
    /// @details This function proceeds if the character is riding a mount

    const SelfStateContext selfContext = makeSelfStateContext(self);
    if (!selfContext.isResolved()) return false;
    return hasExistingHolder(*selfContext.targetInfo);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfGrogged( script_state_t& state, ai_state_t& self )
{
    // IfGrogged()
    /// @author ZZ
    /// @details This function proceeds if the character has been grogged ( a type of
    /// confusion ) this update

    const SelfStateContext selfContext = makeSelfStateContext(self);
    if (!selfContext.isResolved()) return false;
    return selfContext.targetInfo->getGrogTimer() > 0 && HAS_SOME_BITS( self.alert, ALERTIF_CONFUSED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfDazed( script_state_t& state, ai_state_t& self )
{
    // IfDazed()
    /// @author ZZ
    /// @details This function proceeds if the character has been dazed ( a type of
    /// confusion ) this update

    const SelfStateContext selfContext = makeSelfStateContext(self);
    if (!selfContext.isResolved()) return false;
    return selfContext.targetInfo->getDazeTimer() > 0 && HAS_SOME_BITS( self.alert, ALERTIF_CONFUSED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfBackstabbed( script_state_t& state, ai_state_t& self )
{
    // IfBackstabbed()
    /// @author ZF
    /// @details Proceeds if HitFromBehind, target has [STAB] skill and damage dealt is physical
    /// automatically fails if attacker has a code of conduct

    if (!HAS_SOME_BITS( self.alert, ALERTIF_ATTACKED ))
    {
        return false;
    }

    //Who is the dirty backstabber?
    const SelfStateContext selfContext = makeSelfStateContext(self);
    if (!selfContext.isResolved()) return false;
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
uint8_t scr_IfHolderBlocked( script_state_t& state, ai_state_t& self )
{
    // IfHolderBlocked()
    /// @author ZF
    /// @details This function passes if the holder blocked an attack

    const SelfStateContext selfContext = makeSelfStateContext(self);
    if (!selfContext.isResolved()) return false;
    return trySetTargetToHolderLastAttacker(self, *selfContext.targetInfo);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfHeldInLeftHand( script_state_t& state, ai_state_t& self )
{
    // IfHeldInLeftHand()
    /// @author ZZ
    /// @details This function passes if another character is holding the character in its
    /// left hand.
    /// Usage: Used mostly by enchants that target the item of the other hand

    const SelfStateContext selfContext = makeSelfStateContext(self);
    if (!selfContext.isResolved()) return false;
    const ITargetInfo& selfTargetInfo = *selfContext.targetInfo;
    IInventoryHolder* holderInventory = tryInventoryHolder(selfTargetInfo.getHolderRef());
    return holderInventory != nullptr && holderInventory->getHeldObject(SLOT_LEFT) == self.getSelf();
}
