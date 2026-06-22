/// @file egolib/game/script_functions_combat.c
/// @brief Damage, kill, heal, grog/daze, and ammo economy script functions

#include "egolib/game/script_functions_internal.h"

namespace
{

struct OwnedObjectHandle
{
    ObjectRef ref = ObjectRef::Invalid;
    std::shared_ptr<Object> object;
};

struct DamageInvocationContext
{
    IDamageable* damageable = nullptr;
    TEAM_REF teamRef = static_cast<TEAM_REF>(Team::TEAM_MAX);
    DamageType damageType = DamageType::DAMAGE_DIRECT;
    OwnedObjectHandle source;
};

struct HealingInvocationContext
{
    ICharacterState* targetState = nullptr;
    IDamageable* damageable = nullptr;
    OwnedObjectHandle healer;
};

struct TargetCompatibilityContext
{
    ObjectRef targetRef = ObjectRef::Invalid;
    const ITargetInfo* info = nullptr;
    ICharacterState* characterState = nullptr;
};

struct InventoryCompatibilityContext
{
    const IInventoryHolder* targetInventory = nullptr;
    IInventoryHolder* actorInventory = nullptr;
};

struct SelfRoleContext
{
    ICharacterState* characterState = nullptr;
};

TargetCompatibilityContext makeTargetCompatibilityContext(const ai_state_t& self)
{
    TargetCompatibilityContext context;
    context.targetRef = self.getTarget();
    context.info = tryTargetInfo(context.targetRef);
    context.characterState = tryCharacterState(context.targetRef);
    return context;
}

bool resolveInventoryCompatibilityContext(ObjectRef actorRef,
                                          ObjectRef targetRef,
                                          InventoryCompatibilityContext& context)
{
    context.targetInventory = tryInventoryHolder(targetRef);
    context.actorInventory = tryInventoryHolder(actorRef);
    return context.targetInventory != nullptr &&
           context.actorInventory != nullptr;
}

bool resolveInventoryCompatibilityContext(const ai_state_t& self,
                                          InventoryCompatibilityContext& context)
{
    return resolveInventoryCompatibilityContext(self.getSelf(), self.getTarget(), context);
}

ObjectRef selfObjectRef(const ai_state_t& self)
{
    return self.getSelf();
}

bool resolveOwnedObjectHandle(ObjectRef objectRef, OwnedObjectHandle& handle)
{
    handle.ref = objectRef;
    handle.object = tryObjectShared(objectRef);
    return handle.object != nullptr;
}

SelfRoleContext makeSelfRoleContext(const ai_state_t& self)
{
    SelfRoleContext context;
    context.characterState = tryCharacterState(self.getSelf());
    return context;
}

bool increaseSelfAmmo(SelfRoleContext& selfContext)
{
    if (selfContext.characterState == nullptr)
    {
        return false;
    }

    if (selfContext.characterState->getAmmo() < selfContext.characterState->getAmmoMax())
    {
        selfContext.characterState->setAmmo(selfContext.characterState->getAmmo() + 1);
    }

    return true;
}

bool costSelfAmmo(SelfRoleContext& selfContext)
{
    if (selfContext.characterState == nullptr)
    {
        return false;
    }

    if (selfContext.characterState->getAmmo() > 0)
    {
        selfContext.characterState->setAmmo(selfContext.characterState->getAmmo() - 1);
    }

    return true;
}

ObjectRef resolvedKillSourceRef(const ITargetInfo& selfInfo, ObjectRef selfRef)
{
    const ObjectRef holderRef = selfInfo.getHolderRef();
    const ITargetInfo* holderInfo = tryTargetInfo(holderRef);
    if (holderInfo != nullptr && !holderInfo->isMount())
    {
        return holderRef;
    }

    return selfRef;
}

bool resolveSelfAttributedDamageContext(const ai_state_t& self,
                                        DamageInvocationContext& context)
{
    context.damageable = tryDamageable(self.getTarget());
    const IDamageable* selfDamageable = tryDamageable(self.getSelf());
    const ITargetInfo* selfInfo = tryTargetInfo(self.getSelf());
    if (context.damageable == nullptr ||
        selfDamageable == nullptr ||
        selfInfo == nullptr ||
        !resolveOwnedObjectHandle(self.getSelf(), context.source))
    {
        return false;
    }

    context.damageType = selfDamageable->getDamageTargetType();
    context.teamRef = selfInfo->getTeamRef();
    return true;
}

ICharacterState* resolveAliveTargetState(const ai_state_t& self)
{
    const ITargetInfo* resolvedTargetInfo = tryTargetInfo(self.getTarget());
    ICharacterState* resolvedTargetState = tryCharacterState(self.getTarget());
    return resolvedTargetInfo != nullptr &&
           resolvedTargetState != nullptr &&
           resolvedTargetInfo->isAlive() ? resolvedTargetState : nullptr;
}

bool resolveKillDamageContext(const ai_state_t& self,
                              DamageInvocationContext& context)
{
    context.damageable = tryDamageable(self.getTarget());
    const ITargetInfo* selfInfo = tryTargetInfo(self.getSelf());
    if (context.damageable == nullptr || selfInfo == nullptr)
    {
        return false;
    }

    return resolveOwnedObjectHandle(resolvedKillSourceRef(*selfInfo, self.getSelf()), context.source);
}

bool resolveSelfHealingContext(const ai_state_t& self,
                               HealingInvocationContext& context)
{
    context.damageable = tryDamageable(self.getSelf());
    return context.damageable != nullptr &&
           resolveOwnedObjectHandle(self.getSelf(), context.healer);
}

bool resolveHealingTargetContext(const ai_state_t& self,
                                 HealingInvocationContext& context)
{
    context.targetState = tryCharacterState(self.getTarget());
    context.damageable = tryDamageable(self.getTarget());
    return context.targetState != nullptr &&
           context.damageable != nullptr &&
           resolveOwnedObjectHandle(self.getSelf(), context.healer);
}

bool pumpTargetManaFromSelf(const ai_state_t& self, int amount)
{
    if (amount <= 0)
    {
        return false;
    }

    ICharacterState* resolvedTargetState = resolveAliveTargetState(self);
    if (resolvedTargetState == nullptr)
    {
        return false;
    }

    return resolvedTargetState->costMana(-amount, selfObjectRef(self));
}

bool resolveRetaliationDamageContext(const ai_state_t& self,
                                     DamageInvocationContext& context)
{
    context.damageable = tryDamageable(self.getSelf());
    const ITargetInfo* retaliationInfo = tryTargetInfo(self.getTarget());
    if (context.damageable == nullptr ||
        retaliationInfo == nullptr ||
        !resolveOwnedObjectHandle(self.getTarget(), context.source))
    {
        return false;
    }

    context.teamRef = retaliationInfo->getTeamRef();
    return true;
}

void applyRetaliationDamage(const DamageInvocationContext& context,
                            int amount,
                            DamageType damageType)
{
    IPair damage;
    damage.base = amount;
    damage.rand = 1;

    context.damageable->damage(ATK_FRONT, damage, damageType,
                               context.teamRef, context.source.object,
                               false, false, true);
}

bool grogResolvedTarget(const TargetCompatibilityContext& targetContext, int amount)
{
    if (targetContext.info == nullptr ||
        targetContext.characterState == nullptr ||
        !targetContext.info->canBeGrogged())
    {
        return false;
    }

    const int timerValue = targetContext.characterState->getGrogTimer() + amount;
    targetContext.characterState->setGrogTimer(std::max(0, timerValue));
    return true;
}

bool dazeResolvedTarget(const TargetCompatibilityContext& targetContext,
                        int amount,
                        ObjectRef selfRef)
{
    if (targetContext.info == nullptr || targetContext.characterState == nullptr)
    {
        return false;
    }

    if (!targetContext.info->canBeDazed() && selfRef != targetContext.targetRef)
    {
        return false;
    }

    const int timerValue = targetContext.characterState->getDazeTimer() + amount;
    targetContext.characterState->setDazeTimer(std::max(0, timerValue));
    return true;
}

bool costResolvedTargetMana(const TargetCompatibilityContext& targetContext,
                            int amount,
                            ObjectRef sourceRef)
{
    return targetContext.characterState != nullptr &&
           targetContext.characterState->costMana(amount, sourceRef);
}

bool setResolvedTargetAmmo(const TargetCompatibilityContext& targetContext, int amount)
{
    if (targetContext.characterState == nullptr)
    {
        return false;
    }

    targetContext.characterState->setAmmo(std::min(amount, static_cast<int>(targetContext.characterState->getAmmoMax())));
    return true;
}

int restockAmmoIfMatching(ObjectRef itemRef, const IDSZ2& idsz)
{
    const IItemInfo* item = tryItemInfo(itemRef);
    ICharacterState* itemState = tryCharacterState(itemRef);
    if (item == nullptr || itemState == nullptr || !item->hasTypeIDSZ(idsz))
    {
        return 0;
    }

    if (itemState->getAmmo() >= itemState->getAmmoMax())
    {
        return 0;
    }

    const int amount = itemState->getAmmoMax() - itemState->getAmmo();
    itemState->setAmmo(itemState->getAmmoMax());
    return amount;
}

int restockMatchingTargetHeldAndActorPocketAmmo(const InventoryCompatibilityContext& context,
                                                const IDSZ2& idsz,
                                                bool stopAfterFirst)
{
    int ammoGiven = 0;
    const std::array<slot_t, 2> heldSlots = {SLOT_LEFT, SLOT_RIGHT};
    for (const slot_t heldSlot : heldSlots)
    {
        ammoGiven += restockAmmoIfMatching(context.targetInventory->getHeldObject(heldSlot), idsz);
        if (stopAfterFirst && ammoGiven != 0)
        {
            return ammoGiven;
        }
    }

    for (const ObjectRef& actorPocketItemRef : context.actorInventory->getInventoryItemRefs())
    {
        ammoGiven += restockAmmoIfMatching(actorPocketItemRef, idsz);
        if (stopAfterFirst && ammoGiven != 0)
        {
            return ammoGiven;
        }
    }

    return ammoGiven;
}

} // namespace


//--------------------------------------------------------------------------------------------
uint8_t scr_DamageTarget( script_state_t& state, ai_state_t& self )
{
    // DamageTarget( tmpargument = "damage" )
    /// @author ZZ
    /// @details This function applies little bit of love to the character's target.
    /// The amount is set in tmpargument

    IPair tmp_damage;

    if (!resolveSelfContext(self).isResolved()) return false;

    DamageInvocationContext damageContext;
    if (!resolveSelfAttributedDamageContext(self, damageContext))
    {
        return false;
    }

    tmp_damage.base = state.argument;
    tmp_damage.rand = 1;

    damageContext.damageable->damage(ATK_FRONT, tmp_damage, damageContext.damageType,
                                     damageContext.teamRef, damageContext.source.object,
                                     false, false, true);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_KillTarget( script_state_t& state, ai_state_t& self )
{
    // KillTarget()
    /// @author ZZ
    /// @details This function kills the target

    if (!resolveSelfContext(self).isResolved()) return false;

    DamageInvocationContext damageContext;
    if (!resolveKillDamageContext(self, damageContext))
    {
        return false;
    }

    damageContext.damageable->kill(damageContext.source.object, false);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_HealSelf( script_state_t& state, ai_state_t& self )
{
    // HealSelf()
    /// @author ZZ
    /// @details This function gives life back to the character.
    /// Values given as 8.8 fixed point
    /// This does NOT remove [HEAL] enchants ( poisons )
    /// This does not set the ALERTIF_HEALED alert

    if (!resolveSelfContext(self).isResolved()) return false;

    HealingInvocationContext healingContext;
    if (!resolveSelfHealingContext(self, healingContext))
    {
        return false;
    }

    healingContext.damageable->heal(healingContext.healer.object, state.argument, true);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_HealTarget( script_state_t& state, ai_state_t& self )
{
    // HealTarget( tmpargument = "amount" )
    /// @author ZZ
    /// @details This function gives some life back to the target.
    /// Values are 8.8 fixed point. Any enchantments that are removed by [HEAL], like poison, go away

    if (!resolveSelfContext(self).isResolved()) return false;

    HealingInvocationContext healingContext;
    if (!resolveHealingTargetContext(self, healingContext))
    {
        return false;
    }

    if (healingContext.damageable->heal(healingContext.healer.object, state.argument, false))
    {
        healingContext.targetState->removeEnchantsWithIDSZ(IDSZ2('H', 'E', 'A', 'L'));
        return true;
    }

    return false;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PumpTarget( script_state_t& state, ai_state_t& self )
{
    // PumpTarget( tmpargument = "amount" )
    /// @author ZZ
    /// @details This function gives some mana back to the target.
    /// Values are 8.8 fixed point

    if (!resolveSelfContext(self).isResolved()) return false;
    pumpTargetManaFromSelf(self, state.argument);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_TargetDamageSelf( script_state_t& state, ai_state_t& self )
{
    // TargetDamageSelf( tmpargument = "damage" )
    /// @author ZF
    /// @details This function applies little bit of hate from the character's target to
    /// the character itself. The amount is set in tmpargument

    if (!resolveSelfContext(self).isResolved()) return false;

    DamageInvocationContext damageContext;
    if (!resolveRetaliationDamageContext(self, damageContext))
    {
        return false;
    }

    applyRetaliationDamage(damageContext,
                           state.argument,
                           static_cast<DamageType>(state.distance));

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GrogTarget( script_state_t& state, ai_state_t& self )
{
    // GrogTarget( tmpargument = "amount" )
    /// @author ZF
    /// @details This function grogs the Target for a duration equal to tmpargument

    if (!resolveSelfContext(self).isResolved()) return false;
    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    return grogResolvedTarget(targetContext, state.argument);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DazeTarget( script_state_t& state, ai_state_t& self )
{
    // DazeTarget( tmpargument = "amount" )
    /// @author ZF
    /// @details This function dazes the Target for a duration equal to tmpargument

    if (!resolveSelfContext(self).isResolved()) return false;
    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    return dazeResolvedTarget(targetContext, state.argument, self.getSelf());
}


//--------------------------------------------------------------------------------------------
uint8_t scr_CostTargetMana( script_state_t& state, ai_state_t& self )
{
    // CostTargetMana( tmpargument = "amount" )
    /// @author ZZ
    /// @details This function costs the target a specific amount of mana, proceeding
    /// if the target was able to pay the price.  The amounts are 8.8 fixed point

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    return costResolvedTargetMana(targetContext, state.argument, self.getSelf());
}


//--------------------------------------------------------------------------------------------
uint8_t scr_CostAmmo( script_state_t& state, ai_state_t& self )
{
    // CostAmmo()
    /// @author ZZ
    /// @details This function costs the character 1 point of ammo

    if (!resolveSelfContext(self).isResolved()) return false;
    SelfRoleContext selfContext = makeSelfRoleContext(self);
    return costSelfAmmo(selfContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IncreaseAmmo( script_state_t& state, ai_state_t& self )
{
    // IncreaseAmmo()
    /// @author ZZ
    /// @details This function increases the character's ammo by 1

    if (!resolveSelfContext(self).isResolved()) return false;
    SelfRoleContext selfContext = makeSelfRoleContext(self);
    return increaseSelfAmmo(selfContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetAmmo( script_state_t& state, ai_state_t& self )
{
    // SetTargetAmmo( tmpargument = "ammo" )
    /// @author ZF
    /// @details This function sets the ammo of the character's current AI target

    if (!resolveSelfContext(self).isResolved()) return false;
    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    return setResolvedTargetAmmo(targetContext, state.argument);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_RestockTargetAmmoIDAll( script_state_t& state, ai_state_t& self )
{
    // RestockTargetAmmoIDAll( tmpargument = "idsz" )
    /// @author ZZ
    /// @details This function restocks matching ammo on the target's held items and
    /// the actor's pocket items, preserving the legacy target-held plus actor-pocket
    /// compatibility traversal.

    if (!resolveSelfContext(self).isResolved()) return false;

    InventoryCompatibilityContext inventoryContext;
    if (!resolveInventoryCompatibilityContext(self, inventoryContext))
    {
        return false;
    }

    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);
    const int iTmp = restockMatchingTargetHeldAndActorPocketAmmo(inventoryContext, idsz, false);

    state.argument = iTmp;
    return ( iTmp != 0 );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_RestockTargetAmmoIDFirst( script_state_t& state, ai_state_t& self )
{
    // RestockTargetAmmoIDFirst( tmpargument = "idsz" )
    /// @author ZZ
    /// @details This function restocks the first matching item in the legacy target-held
    /// then actor-pocket traversal order.

    if (!resolveSelfContext(self).isResolved()) return false;

    InventoryCompatibilityContext inventoryContext;
    if (!resolveInventoryCompatibilityContext(self, inventoryContext))
    {
        return false;
    }

    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);
    const int iTmp = restockMatchingTargetHeldAndActorPocketAmmo(inventoryContext, idsz, true);

    state.argument = iTmp;
    return ( iTmp != 0 );
}
