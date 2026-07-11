/// @file egolib/game/script_functions_enchant.c
/// @brief Enchant/disenchant application and kurse management script functions

#include "egolib/game/script_functions_internal.h"

namespace
{
struct EnchantInvocationContext
{
    IEnchantable* target = nullptr;
    ObjectRef ownerRef = ObjectRef::Invalid;
    ObjectRef spawnerRef = ObjectRef::Invalid;
};

struct TargetCompatibilityContext
{
    ObjectRef targetRef = ObjectRef::Invalid;
    const ITargetInfo* info = nullptr;
    ICharacterState* characterState = nullptr;
    IInventoryHolder* inventory = nullptr;
    ITeamMember* teamMember = nullptr;
    IEnchantable* enchantable = nullptr;
};

struct InventoryCompatibilityContext
{
    const IInventoryHolder* targetInventory = nullptr;
    IInventoryHolder* actorInventory = nullptr;
};

struct TargetStateCompatibilityContext
{
    ICharacterState* characterState = nullptr;
};

struct SelfRoleContextEnchant
{
    IEnchantable* enchantable = nullptr;
};

TargetCompatibilityContext makeTargetCompatibilityContext(const ai_state_t& self)
{
    TargetCompatibilityContext context;
    context.targetRef = self.getTarget();
    context.info = tryTargetInfo(context.targetRef);
    context.characterState = tryCharacterState(context.targetRef);
    context.inventory = tryInventoryHolder(context.targetRef);
    context.teamMember = tryTeamMember(context.targetRef);
    context.enchantable = tryEnchantable(context.targetRef);
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

bool resolveEnchantInvocationContext(const ai_state_t& self,
                                     ObjectRef targetRef,
                                     EnchantInvocationContext& context)
{
    context.target = tryEnchantable(targetRef);
    context.ownerRef = self.owner;
    context.spawnerRef = self.getSelf();
    return context.target != nullptr &&
           hasLiveObjectRef(context.ownerRef) &&
           hasLiveObjectRef(context.spawnerRef);
}

SelfRoleContextEnchant makeSelfRoleContextEnchant(const ai_state_t& self)
{
    SelfRoleContextEnchant context;
    context.enchantable = tryEnchantable(self.getSelf());
    return context;
}

bool setSelfEnchantBoostValues(const script_state_t& state, SelfRoleContextEnchant& selfContext)
{
    if (selfContext.enchantable == nullptr || !selfContext.enchantable->hasActiveEnchants())
    {
        return false;
    }

    const std::shared_ptr<Ego::Enchantment> enchant = selfContext.enchantable->getFirstActiveEnchant();
    if (enchant == nullptr || enchant->isTerminated())
    {
        return false;
    }

    enchant->setBoostValues(FP8_TO_FLOAT(state.argument),
                            FP8_TO_FLOAT(state.distance),
                            FP8_TO_FLOAT(state.x),
                            FP8_TO_FLOAT(state.y));
    return true;
}

bool kurseResolvedTarget(const TargetCompatibilityContext& targetContext)
{
    if (targetContext.inventory == nullptr ||
        targetContext.info == nullptr ||
        targetContext.characterState == nullptr ||
        !targetContext.inventory->isItem() ||
        targetContext.info->isKursed())
    {
        return false;
    }

    targetContext.characterState->setKursed(true);
    return true;
}

bool unkurseResolvedTarget(const TargetCompatibilityContext& targetContext)
{
    if (targetContext.characterState == nullptr)
    {
        return false;
    }

    targetContext.characterState->setKursed(false);
    return true;
}

bool disenchantResolvedTarget(const TargetCompatibilityContext& targetContext)
{
    return targetContext.enchantable != nullptr &&
           targetContext.enchantable->disenchant();
}

template <typename Fn>
void forEachResolvedObjectRef(Fn&& fn)
{
    ObjectHandler* handler = Ego::Entities::tryActiveObjectHandler();
    if (handler == nullptr)
    {
        return;
    }

    for (const ObjectRef& objectRef : handler->objectRefIterator())
    {
        fn(objectRef);
    }
}

void unkurseItemIfPresent(ObjectRef itemRef)
{
    ICharacterState* itemState = tryCharacterState(itemRef);
    if (itemState != nullptr)
    {
        itemState->setKursed(false);
    }
}

void unkurseTargetHeldAndActorPocketItems(const InventoryCompatibilityContext& context)
{
    unkurseItemIfPresent(context.targetInventory->getHeldObject(SLOT_LEFT));
    unkurseItemIfPresent(context.targetInventory->getHeldObject(SLOT_RIGHT));

    for (const ObjectRef& actorPocketItemRef : context.actorInventory->getInventoryItemRefs())
    {
        unkurseItemIfPresent(actorPocketItemRef);
    }
}

ICharacterState* resolveAliveTargetState(const ai_state_t& self)
{
    return tryLivingCharacterState(self.getTarget());
}

bool resolveTargetStateCompatibilityContext(const ai_state_t& self,
                                            TargetStateCompatibilityContext& context)
{
    context.characterState = resolveAliveTargetState(self);
    return context.characterState != nullptr;
}

bool dispelResolvedTargetEnchants(const TargetStateCompatibilityContext& context,
                                  IDSZ2 removedByIDSZ)
{
    if (context.characterState == nullptr)
    {
        return false;
    }

    context.characterState->removeEnchantsWithIDSZ(removedByIDSZ);
    return true;
}

} // namespace


//--------------------------------------------------------------------------------------------
uint8_t scr_EnchantTarget( script_state_t& state, ai_state_t& self )
{
    // EnchantTarget()
    /// @author ZZ
    /// @details This function enchants the target with the enchantment given
    /// in enchant.txt. Make sure you use set_OwnerToTarget before doing this.

    SelfProfileSnapshot selfContext = makeSelfProfileSnapshot(self);
    if (!selfContext.isResolved()) return false;

    EnchantInvocationContext enchantContext;
    if (!resolveEnchantInvocationContext(self, self.getTarget(), enchantContext))
    {
        return false;
    }

    return enchantContext.target->addEnchant(selfContext.enchantRef,
                                             selfContext.profileRef.get(),
                                             enchantContext.ownerRef,
                                             enchantContext.spawnerRef) != nullptr;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EnchantChild( script_state_t& state, ai_state_t& self )
{
    // EnchantChild()
    /// @author ZZ
    /// @details This function can be used with SpawnCharacter to enchant the
    /// newly spawned character with the enchantment
    /// given in enchant.txt. Make sure you use set_OwnerToTarget before doing this.

    SelfProfileSnapshot selfContext = makeSelfProfileSnapshot(self);
    if (!selfContext.isResolved()) return false;

    EnchantInvocationContext enchantContext;
    if (!resolveEnchantInvocationContext(self, self.child, enchantContext))
    {
        return false;
    }

    return enchantContext.target->addEnchant(selfContext.enchantRef,
                                             selfContext.profileRef.get(),
                                             enchantContext.ownerRef,
                                             enchantContext.spawnerRef) != nullptr;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_UndoEnchant( script_state_t& state, ai_state_t& self )
{
    // UndoEnchant()
    /// @author ZZ
    /// @details This function removes the last enchantment spawned by the character,
    /// proceeding if an enchantment was removed

    SelfRoleContextEnchant selfContext = makeSelfRoleContextEnchant(self);
    if (selfContext.enchantable == nullptr) return false;

    std::shared_ptr<Ego::Enchantment> lastEnchant = selfContext.enchantable->getLastEnchantmentSpawned();
    if(lastEnchant == nullptr || lastEnchant->isTerminated()) {
        return false;
    }

    lastEnchant->requestTerminate();
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisenchantTarget( script_state_t& state, ai_state_t& self )
{
    // DisenchantTarget()
    /// @author ZZ
    /// @details This function removes all enchantments on the Target character, proceeding
    /// if there were any, failing if not

    if (!hasLiveSelf(self)) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    return disenchantResolvedTarget(targetContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisenchantAll( script_state_t& state, ai_state_t& self )
{
    // DisenchantAll()
    /// @author ZZ
    /// @details This function removes all enchantments in the game

    if (!hasLiveSelf(self)) return false;

    forEachResolvedObjectRef([](ObjectRef objectRef)
    {
        if (IEnchantable* objectEnchantable = tryEnchantable(objectRef))
        {
            objectEnchantable->disenchant();
        }
    });

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DispelTargetEnchantID( script_state_t& state, ai_state_t& self )
{
    // DispelEnchantID( tmpargument = "idsz" )
    /// @author ZF
    /// @details This function removes all enchants from the target who match the specified RemovedByIDSZ

    if (!hasLiveSelf(self)) return false;
    TargetStateCompatibilityContext targetContext;
    return resolveTargetStateCompatibilityContext(self, targetContext) &&
           dispelResolvedTargetEnchants(targetContext,
                                        Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetEnchantBoostValues( script_state_t& state, ai_state_t& self )
{
    // SetEnchantBoostValues( tmpargument = "owner mana regen", tmpdistance = "owner life regen", tmpx = "target mana regen", tmpy = "target life regen" )
    /// @author ZZ
    /// @details This function sets the mana and life drains for the last enchantment
    /// spawned by this character.
    /// Values are 8.8 fixed point

    if (!hasLiveSelf(self)) return false;

    SelfRoleContextEnchant selfContext = makeSelfRoleContextEnchant(self);
    return setSelfEnchantBoostValues(state, selfContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_KurseTarget( script_state_t& state, ai_state_t& self )
{
    // KurseTarget()
    /// @author ZF
    /// @details This makes the target kursed

    if (!hasLiveSelf(self)) return false;
    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    return kurseResolvedTarget(targetContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_UnkurseTarget( script_state_t& state, ai_state_t& self )
{
    // UnkurseTarget()
    /// @author ZZ
    /// @details This function unkurses the target

    if (!hasLiveSelf(self)) return false;
    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    return unkurseResolvedTarget(targetContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_UnkurseTargetInventory( script_state_t& state, ai_state_t& self )
{
    // UnkurseTargetInventory()
    /// @author ZZ
    /// @details This function preserves the legacy compatibility behavior: unkurse the
    /// target's held items plus the actor's pocket items, but not the target's pockets.

    if (!hasLiveSelf(self)) return false;

    InventoryCompatibilityContext inventoryContext;
    if (!resolveInventoryCompatibilityContext(self, inventoryContext))
    {
        return false;
    }

    unkurseTargetHeldAndActorPocketItems(inventoryContext);

    return true;
}
