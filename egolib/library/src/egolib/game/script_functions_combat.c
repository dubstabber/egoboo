/// @file egolib/game/script_functions_combat.c
/// @brief Damage, kill, heal, enchant/disenchant, grog/daze, ammo, and stat gifts

#include "egolib/game/script_functions_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

namespace
{
GameSessionContext& gameSession()
{
    return GameSessionContext::get();
}

ObjectRef selfObjectRef(const ai_state_t& self)
{
    return self.getSelf();
}

Object& resolvedSelfObject(const ai_state_t& self)
{
    return *resolveSelfContext(self).object;
}

IEnchantable& enchantable(Object& object)
{
    return object;
}

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

struct TargetStateCompatibilityContext
{
    ICharacterState* characterState = nullptr;
};

struct EnchantInvocationContext
{
    IEnchantable* target = nullptr;
    OwnedObjectHandle owner;
    OwnedObjectHandle spawner;
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

struct SelfRoleContext
{
    Object* selfObject = nullptr;
    IAppearanceProfile* appearance = nullptr;
    ICharacterState* characterState = nullptr;
    IEnchantable* enchantable = nullptr;
    ITeamMember* teamMember = nullptr;
    const ITargetInfo* targetInfo = nullptr;
    IWallet* wallet = nullptr;
};

struct SelfProfilePolicyData
{
    ObjectProfileRef profileRef = ObjectProfileRef::Invalid;
    EVE_REF enchantRef = INVALID_EVE_REF;
    SKIN_T spellEffectSkin = ObjectProfile::NO_SKIN_OVERRIDE;
};

struct SelfProfileComparisonData
{
    ObjectProfileRef baseModelRef = ObjectProfileRef::Invalid;
    bool baseModelIsSpellbook = false;
    bool currentProfileMatchesBaseModel = false;
};

struct SelfProfilePolicyDataFull
{
    ObjectProfileRef profileRef = ObjectProfileRef::Invalid;
    EVE_REF enchantRef = INVALID_EVE_REF;
    SKIN_T spellEffectSkin = ObjectProfile::NO_SKIN_OVERRIDE;
    SelfProfileComparisonData comparison;
};

struct SelfProfileContext
{
    const ObjectProfile* profile = nullptr;
    std::string selfName;
    std::string className;
    SelfProfilePolicyDataFull policy;
};

struct InventoryCompatibilityContext
{
    const IInventoryHolder* targetInventory = nullptr;
    IInventoryHolder* actorInventory = nullptr;
};

void maybeAddSkillPerk(ICharacterState& targetState, uint32_t skillId);

SelfRoleContext makeSelfRoleContext(const ai_state_t& self)
{
    SelfRoleContext context;
    context.selfObject = tryObject(self.getSelf());
    if (context.selfObject == nullptr)
    {
        return context;
    }

    context.appearance = static_cast<IAppearanceProfile*>(context.selfObject);
    context.characterState = static_cast<ICharacterState*>(context.selfObject);
    context.enchantable = static_cast<IEnchantable*>(context.selfObject);
    context.teamMember = static_cast<ITeamMember*>(context.selfObject);
    context.targetInfo = static_cast<const ITargetInfo*>(context.selfObject);
    context.wallet = static_cast<IWallet*>(context.selfObject);
    return context;
}

SelfProfileContext makeSelfProfileContext(const ai_state_t& self)
{
    SelfProfileContext context;
    Object* selfObject = tryObject(self.getSelf());
    if (selfObject == nullptr)
    {
        return context;
    }

    const std::shared_ptr<ObjectProfile>& selfProfile = selfObject->getProfile();
    if (selfProfile == nullptr)
    {
        return context;
    }

    context.profile = selfProfile.get();
    context.selfName = selfObject->getName();
    context.className = selfProfile->getClassName();
    context.policy.profileRef = selfObject->getProfileID();
    context.policy.enchantRef = selfProfile->getEnchantRef();
    context.policy.spellEffectSkin = selfProfile->getSpellEffectType();
    context.policy.comparison.baseModelRef = selfObject->getBaseModelRef();
    context.policy.comparison.baseModelIsSpellbook = context.policy.comparison.baseModelRef == ObjectProfileRef(SPELLBOOK);
    context.policy.comparison.currentProfileMatchesBaseModel =
        context.policy.comparison.baseModelRef == context.policy.profileRef;
    return context;
}

bool resolveOwnedObjectHandle(ObjectRef objectRef, OwnedObjectHandle& handle)
{
    handle.ref = objectRef;
    handle.object = tryObjectShared(objectRef);
    return handle.object != nullptr;
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

bool setSelfEnchantBoostValues(const script_state_t& state, SelfRoleContext& selfContext)
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

bool resolveTargetStateCompatibilityContext(const ai_state_t& self,
                                            TargetStateCompatibilityContext& context)
{
    context.characterState = resolveAliveTargetState(self);
    return context.characterState != nullptr;
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

bool resolveAliveTargetHealingContext(const ai_state_t& self,
                                      HealingInvocationContext& context)
{
    const ITargetInfo* resolvedTargetInfo = tryTargetInfo(self.getTarget());
    context.targetState = tryCharacterState(self.getTarget());
    context.damageable = tryDamageable(self.getTarget());
    return resolvedTargetInfo != nullptr &&
           context.targetState != nullptr &&
           context.damageable != nullptr &&
           resolvedTargetInfo->isAlive() &&
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

void applyResolvedTargetBaseAttribute(const TargetStateCompatibilityContext& context,
                                      Ego::Attribute::AttributeType attribute,
                                      float value)
{
    if (context.characterState != nullptr)
    {
        context.characterState->increaseBaseAttribute(attribute, value);
    }
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

bool resolveEnchantInvocationContext(const ai_state_t& self,
                                     ObjectRef targetRef,
                                     EnchantInvocationContext& context)
{
    context.target = tryEnchantable(targetRef);
    return context.target != nullptr &&
           resolveOwnedObjectHandle(self.owner, context.owner) &&
           resolveOwnedObjectHandle(self.getSelf(), context.spawner);
}

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

bool giveResolvedTargetExperience(const TargetCompatibilityContext& targetContext,
                                  int amount,
                                  XPType type)
{
    if (targetContext.characterState == nullptr)
    {
        return false;
    }

    targetContext.characterState->giveExperience(amount, type, false);
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

bool giveResolvedTargetSkill(const TargetCompatibilityContext& targetContext, uint32_t skillId)
{
    if (targetContext.characterState == nullptr)
    {
        return false;
    }

    maybeAddSkillPerk(*targetContext.characterState, skillId);
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
    ObjectHandler* handler = gameSession().tryObjectHandler();
    if (handler == nullptr)
    {
        return;
    }

    for (const ObjectRef& objectRef : handler->objectRefIterator())
    {
        fn(objectRef);
    }
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

bool itemMatchesType(ObjectRef itemRef, const IDSZ2& idsz)
{
    const IItemInfo* item = tryItemInfo(itemRef);
    return item != nullptr && item->hasTypeIDSZ(idsz);
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

void maybeAddSkillPerk(ICharacterState& targetState, uint32_t skillId)
{
    switch(skillId)
    {
        case IDSZ2::caseLabel( 'A', 'W', 'E', 'P' ): targetState.addPerk(Ego::Perks::WEAPON_PROFICIENCY); break;
        case IDSZ2::caseLabel( 'P', 'O', 'I', 'S' ): targetState.addPerk(Ego::Perks::POISONRY); break;
        case IDSZ2::caseLabel( 'C', 'K', 'U', 'R' ): targetState.addPerk(Ego::Perks::SENSE_KURSES); break;
        case IDSZ2::caseLabel( 'R', 'E', 'A', 'D' ): targetState.addPerk(Ego::Perks::LITERACY); break;
        case IDSZ2::caseLabel( 'W', 'M', 'A', 'G' ): targetState.addPerk(Ego::Perks::ARCANE_MAGIC); break;
        case IDSZ2::caseLabel( 'H', 'M', 'A', 'G' ): targetState.addPerk(Ego::Perks::DIVINE_MAGIC); break;
        case IDSZ2::caseLabel( 'T', 'E', 'C', 'H' ): targetState.addPerk(Ego::Perks::USE_TECHNOLOGICAL_ITEMS); break;
        case IDSZ2::caseLabel( 'D', 'I', 'S', 'A' ): targetState.addPerk(Ego::Perks::TRAP_LORE); break;
        case IDSZ2::caseLabel( 'S', 'T', 'A', 'B' ): targetState.addPerk(Ego::Perks::BACKSTAB); break;
        case IDSZ2::caseLabel( 'D', 'A', 'R', 'K' ): targetState.addPerk(Ego::Perks::NIGHT_VISION); break;
        default: break;
    }
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
uint8_t scr_EnchantTarget( script_state_t& state, ai_state_t& self )
{
    // EnchantTarget()
    /// @author ZZ
    /// @details This function enchants the target with the enchantment given
    /// in enchant.txt. Make sure you use set_OwnerToTarget before doing this.

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfProfileContext selfContext = makeSelfProfileContext(self);

    EnchantInvocationContext enchantContext;
    if (!resolveEnchantInvocationContext(self, self.getTarget(), enchantContext))
    {
        return false;
    }

    return enchantContext.target->addEnchant(selfContext.policy.enchantRef,
                                             selfContext.policy.profileRef.get(),
                                             enchantContext.owner.object,
                                             enchantContext.spawner.object) != nullptr;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EnchantChild( script_state_t& state, ai_state_t& self )
{
    // EnchantChild()
    /// @author ZZ
    /// @details This function can be used with SpawnCharacter to enchant the
    /// newly spawned character with the enchantment
    /// given in enchant.txt. Make sure you use set_OwnerToTarget before doing this.

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfProfileContext selfContext = makeSelfProfileContext(self);

    EnchantInvocationContext enchantContext;
    if (!resolveEnchantInvocationContext(self, self.child, enchantContext))
    {
        return false;
    }

    return enchantContext.target->addEnchant(selfContext.policy.enchantRef,
                                             selfContext.policy.profileRef.get(),
                                             enchantContext.owner.object,
                                             enchantContext.spawner.object) != nullptr;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_UndoEnchant( script_state_t& state, ai_state_t& self )
{
    // UndoEnchant()
    /// @author ZZ
    /// @details This function removes the last enchantment spawned by the character,
    /// proceeding if an enchantment was removed

    if (!resolveSelfContext(self).isResolved()) return false;

    std::shared_ptr<Ego::Enchantment> lastEnchant = enchantable(resolvedSelfObject(self)).getLastEnchantmentSpawned();
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

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    return disenchantResolvedTarget(targetContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisenchantAll( script_state_t& state, ai_state_t& self )
{
    // DisenchantAll()
    /// @author ZZ
    /// @details This function removes all enchantments in the game

    if (!resolveSelfContext(self).isResolved()) return false;

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

    if (!resolveSelfContext(self).isResolved()) return false;
    TargetStateCompatibilityContext targetContext;
    return resolveTargetStateCompatibilityContext(self, targetContext) &&
           dispelResolvedTargetEnchants(targetContext,
                                        Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveExperienceToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveExperienceToTarget( tmpargument = "amount", tmpdistance = "type" )
    /// @author ZZ
    /// @details This function gives the target some experience, xptype from distance,
    /// amount from argument.

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    return giveResolvedTargetExperience(targetContext,
                                        state.argument,
                                        static_cast<XPType>(state.distance));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveStrengthToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveStrengthToTarget(argument = "amount")
    // Permanently boost the target's strength

    if (!resolveSelfContext(self).isResolved()) return false;
    if ( ICharacterState* resolvedTargetState = resolveAliveTargetState(self) )
    {
        resolvedTargetState->increaseBaseAttribute(Ego::Attribute::MIGHT, FP8_TO_FLOAT(state.argument));
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveIntelligenceToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveIntelligenceToTarget(tmpargument = "amount")
    // Permanently boost the target's intelligence

    if (!resolveSelfContext(self).isResolved()) return false;
    if ( ICharacterState* resolvedTargetState = resolveAliveTargetState(self) )
    {
        resolvedTargetState->increaseBaseAttribute(Ego::Attribute::INTELLECT, FP8_TO_FLOAT(state.argument));
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveDexterityToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveDexterityToTarget(tmpargument = "amount")
    // Permanently boost the target's dexterity

    if (!resolveSelfContext(self).isResolved()) return false;
    if ( ICharacterState* resolvedTargetState = resolveAliveTargetState(self) )
    {
        resolvedTargetState->increaseBaseAttribute(Ego::Attribute::AGILITY, FP8_TO_FLOAT(state.argument));
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveLifeToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveLifeToTarget(tmpargument = "amount")
    /// @author ZZ
    /// @details Permanently boost the target's life

    if (!resolveSelfContext(self).isResolved()) return false;
    HealingInvocationContext healingContext;
    if (resolveAliveTargetHealingContext(self, healingContext))
    {
        healingContext.targetState->increaseBaseAttribute(Ego::Attribute::MAX_LIFE,
                                                          FP8_TO_FLOAT(state.argument));
        healingContext.damageable->heal(healingContext.healer.object, state.argument, true);
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveManaToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveManaToTarget(tmpargument = "amount")
    /// @author ZZ
    /// @details Permanently boost the target's mana

    if (!resolveSelfContext(self).isResolved()) return false;
    if ( ICharacterState* resolvedTargetState = resolveAliveTargetState(self) )
    {
        resolvedTargetState->increaseBaseAttribute(Ego::Attribute::MAX_MANA, FP8_TO_FLOAT(state.argument));
        resolvedTargetState->costMana(-state.argument, ObjectRef::Invalid);
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveManaFlowToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveManaFlowToTarget()
    /// @author ZF
    /// @details Permanently boost the target's mana flow

    if (!resolveSelfContext(self).isResolved()) return false;
    TargetStateCompatibilityContext targetContext;
    resolveTargetStateCompatibilityContext(self, targetContext);
    applyResolvedTargetBaseAttribute(targetContext,
                                     Ego::Attribute::SPELL_POWER,
                                     FP8_TO_FLOAT(state.argument));

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveManaReturnToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveManaReturnToTarget()
    /// @author ZF
    /// @details Permanently boost the target's mana return

    if (!resolveSelfContext(self).isResolved()) return false;
    TargetStateCompatibilityContext targetContext;
    resolveTargetStateCompatibilityContext(self, targetContext);
    applyResolvedTargetBaseAttribute(targetContext,
                                     Ego::Attribute::MANA_REGEN,
                                     FP8_TO_FLOAT(state.argument));

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
uint8_t scr_GiveSkillToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveSkillToTarget( tmpargument = "skill_IDSZ" )
    /// @author ZF
    /// @details This function permanently gives the target character a Perk

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    return giveResolvedTargetSkill(targetContext, state.argument);
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
uint8_t scr_KurseTarget( script_state_t& state, ai_state_t& self )
{
    // KurseTarget()
    /// @author ZF
    /// @details This makes the target kursed

    if (!resolveSelfContext(self).isResolved()) return false;
    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    return kurseResolvedTarget(targetContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_UnkurseTarget( script_state_t& state, ai_state_t& self )
{
    // UnkurseTarget()
    /// @author ZZ
    /// @details This function unkurses the target

    if (!resolveSelfContext(self).isResolved()) return false;
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

    if (!resolveSelfContext(self).isResolved()) return false;

    InventoryCompatibilityContext inventoryContext;
    if (!resolveInventoryCompatibilityContext(self, inventoryContext))
    {
        return false;
    }

    unkurseTargetHeldAndActorPocketItems(inventoryContext);

    return true;
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


//--------------------------------------------------------------------------------------------
uint8_t scr_SetEnchantBoostValues( script_state_t& state, ai_state_t& self )
{
    // SetEnchantBoostValues( tmpargument = "owner mana regen", tmpdistance = "owner life regen", tmpx = "target mana regen", tmpy = "target life regen" )
    /// @author ZZ
    /// @details This function sets the mana and life drains for the last enchantment
    /// spawned by this character.
    /// Values are 8.8 fixed point

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfRoleContext selfContext = makeSelfRoleContext(self);
    return setSelfEnchantBoostValues(state, selfContext);
}
