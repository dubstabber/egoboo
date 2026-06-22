/// @file egolib/game/script_functions_appearance.c
/// @brief Class change, spell/spellbook transforms, equip, armor, IDSz, and character self-presentation

#include "egolib/game/script_functions_internal.h"

namespace
{
struct SelfRoleContext
{
    IAnimationControl* animation = nullptr;
    IDamageable* damageable = nullptr;
    IEquipmentControl* equipment = nullptr;
    IAppearanceProfile* appearance = nullptr;
    ICharacterState* characterState = nullptr;
    IEnchantable* enchantable = nullptr;
    IMorphControl* morph = nullptr;
    ITeamMember* teamMember = nullptr;
    const ITargetInfo* targetInfo = nullptr;
    IWallet* wallet = nullptr;
};

struct ClassChangeCompatibilityContext
{
    IMorphControl* selfMorph = nullptr;
};

struct InventoryCompatibilityContext
{
    const IInventoryHolder* targetInventory = nullptr;
    IInventoryHolder* actorInventory = nullptr;
};

SelfRoleContext makeSelfRoleContext(const ai_state_t& self)
{
    SelfRoleContext context;
    const ObjectRef selfRef = self.getSelf();
    context.animation = tryAnimationControl(selfRef);
    context.damageable = tryDamageable(selfRef);
    context.equipment = tryEquipmentControl(selfRef);
    context.appearance = tryAppearanceProfile(selfRef);
    context.characterState = tryCharacterState(selfRef);
    context.enchantable = tryEnchantable(selfRef);
    context.morph = tryMorphControl(selfRef);
    context.teamMember = tryTeamMember(selfRef);
    context.targetInfo = tryTargetInfo(selfRef);
    context.wallet = tryWallet(selfRef);
    return context;
}

SelfProfileSnapshot makeRequiredSelfProfileSnapshot(const ai_state_t& self)
{
    return makeSelfProfileSnapshot(self);
}

bool hasRequiredSelfProfileSnapshot(const SelfProfileSnapshot& context)
{
    return context.isResolved();
}

bool setSelfDamageType(SelfRoleContext& selfContext, DamageType damageType)
{
    if (selfContext.damageable == nullptr)
    {
        return false;
    }

    selfContext.damageable->setDamageTargetType(damageType);
    return true;
}

bool markSelfEquipped(SelfRoleContext& selfContext)
{
    if (selfContext.equipment == nullptr)
    {
        return false;
    }

    selfContext.equipment->setEquipped(true);
    return true;
}

bool changeSelfArmor(script_state_t& state, SelfRoleContext& selfContext)
{
    if (selfContext.appearance == nullptr)
    {
        return false;
    }

    const int oldSkin = selfContext.appearance->getSkin();
    state.x = state.argument;
    selfContext.appearance->setSkin(Ego::Script::Interpreter::safeCast<size_t>(state.argument));
    state.x = selfContext.appearance->getSkin();
    state.argument = oldSkin;
    return true;
}

bool setSelfMoney(const script_state_t& state, SelfRoleContext& selfContext)
{
    if (selfContext.wallet == nullptr)
    {
        return false;
    }

    selfContext.wallet->giveMoney(state.argument - selfContext.wallet->getMoney());
    return true;
}

ClassChangeCompatibilityContext makeClassChangeCompatibilityContext(IMorphControl& selfObject)
{
    ClassChangeCompatibilityContext context;
    context.selfMorph = &selfObject;
    return context;
}

bool changeSelfClass(const ClassChangeCompatibilityContext& context, ObjectProfileRef profileID)
{
    if (context.selfMorph == nullptr ||
        !activeProfileSystem().isLoaded(profileID))
    {
        return false;
    }

    context.selfMorph->polymorphObject(profileID, 0);
    context.selfMorph->setBaseModelRef(profileID);
    return true;
}

void becomeSpell(IEnchantable& selfEnchantable,
                 IMorphControl& selfMorph,
                 ObjectProfileRef spellProfile,
                 ai_state_t& self)
{
    selfEnchantable.disenchant();
    selfMorph.polymorphObject(spellProfile, 0);
    self.content = 0;
    self.state = 0;
}

void becomeSpellbook(IEnchantable& selfEnchantable,
                     IMorphControl& selfMorph,
                     IAnimationControl& selfAnimation,
                     ObjectProfileRef oldProfile,
                     SKIN_T spellEffectSkin,
                     ai_state_t& self)
{
    selfEnchantable.disenchant();
    selfMorph.polymorphObject(ObjectProfileRef(SPELLBOOK), spellEffectSkin);
    self.state = 0;
    self.content = REF_TO_INT(oldProfile.get());

    const ModelAction droppedAction = selfAnimation.resolveModelAction(ACTION_JB);
    selfAnimation.startAnimation(droppedAction, false, true);
}

bool itemMatchesType(ObjectRef itemRef, const IDSZ2& idsz)
{
    const IItemInfo* item = tryItemInfo(itemRef);
    return item != nullptr && item->hasTypeIDSZ(idsz);
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

ObjectRef findMatchingTargetHeldOrActorPocketItemRef(const InventoryCompatibilityContext& context,
                                                     const IDSZ2& idsz)
{
    const std::array<slot_t, 2> heldSlots = {SLOT_LEFT, SLOT_RIGHT};
    for (const slot_t heldSlot : heldSlots)
    {
        const ObjectRef heldObjectRef = context.targetInventory->getHeldObject(heldSlot);
        if (itemMatchesType(heldObjectRef, idsz))
        {
            return heldObjectRef;
        }
    }

    for (const ObjectRef& actorPocketItemRef : context.actorInventory->getInventoryItemRefs())
    {
        if (itemMatchesType(actorPocketItemRef, idsz))
        {
            return actorPocketItemRef;
        }
    }

    return ObjectRef::Invalid;
}

void removeActorPocketItemRefIfPresent(IInventoryHolder& actorInventory, ObjectRef itemRef)
{
    for (size_t slot = 0; slot < actorInventory.getInventoryMaxItems(); ++slot)
    {
        if (actorInventory.getInventoryItemRef(slot) == itemRef)
        {
            Inventory::remove_item(actorInventory, slot, true);
            return;
        }
    }
}

bool consumeOrPoofItemWithActorPocketCompatibility(ObjectRef itemRef, IInventoryHolder& actorInventory)
{
    ICharacterState* itemState = tryCharacterState(itemRef);
    if (itemState == nullptr)
    {
        return false;
    }

    if (itemState->getAmmo() > 1)
    {
        itemState->setAmmo(itemState->getAmmo() - 1);
        return true;
    }

    const IInventoryHolder* itemInventory = tryInventoryHolder(itemRef);
    ILifecycleControl* itemLifecycle = tryLifecycleControl(itemRef);
    if (itemInventory == nullptr || itemLifecycle == nullptr)
    {
        return false;
    }

    if (itemInventory->isInsideInventory())
    {
        removeActorPocketItemRefIfPresent(actorInventory, itemRef);
    }
    else
    {
        itemLifecycle->detachFromHolder(true, false);
    }

    itemLifecycle->requestTerminate();
    return true;
}
} // namespace


//--------------------------------------------------------------------------------------------
uint8_t scr_BecomeSpell( script_state_t& state, ai_state_t& self )
{
    // BecomeSpell()
    /// @author ZZ
    /// @details This function turns a spellbook character into a spell based on its
    /// content.
    /// TOO COMPLICATED TO EXPLAIN.  SHOULDN'T EVER BE NEEDED BY YOU.

    SelfRoleContext selfContext = makeSelfRoleContext(self);
    if (selfContext.enchantable == nullptr || selfContext.morph == nullptr) return false;

    becomeSpell(*selfContext.enchantable,
                *selfContext.morph,
                ObjectProfileRef(self.content),
                self);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_BecomeSpellbook( script_state_t& state, ai_state_t& self )
{
    // BecomeSpellbook()
    //
    /// @author ZZ
    /// @details This function turns a spell character into a spellbook and sets the content accordingly.
    /// TOO COMPLICATED TO EXPLAIN. Just copy the spells that already exist, and don't change
    /// them too much

    SelfRoleContext roleContext = makeSelfRoleContext(self);
    if (roleContext.enchantable == nullptr ||
        roleContext.morph == nullptr ||
        roleContext.animation == nullptr) return false;

    SelfProfileSnapshot selfContext = makeRequiredSelfProfileSnapshot(self);
    if (!hasRequiredSelfProfileSnapshot(selfContext)) return false;

    becomeSpellbook(*roleContext.enchantable,
                    *roleContext.morph,
                    *roleContext.animation,
                    selfContext.profileRef,
                    selfContext.spellEffectSkin,
                    self);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ChangeTargetClass( script_state_t& state, ai_state_t& self )
{
    // ChangeTargetClass( tmpargument = "slot" )

    /// @author ZZ
    /// @details This function changes the target character's model slot.
    /// DON'T USE THIS FOR EXPORTABLE ITEMS OR CHARACTERS, AS THE MODEL SLOTS MAY VARY FROM
    /// MODULE TO MODULE.
    /// USAGE: This is intended as a way to incorporate more player classes into the game.

    SelfRoleContext selfContext = makeSelfRoleContext(self);
    if (selfContext.morph == nullptr) return false;

    const auto profileID = ObjectProfileRef(static_cast<PRO_REF>(state.argument));
    const ClassChangeCompatibilityContext classContext = makeClassChangeCompatibilityContext(*selfContext.morph);

    /// @details This function polymorphs a character permanently so that it can be exported properly
    /// A character turned into a frog with this function will also export as a frog!
    return changeSelfClass(classContext, profileID);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_CostTargetItemID( script_state_t& state, ai_state_t& self )
{
    // CostTargetItemID( tmpargument = "idsz" )
    /// @author ZZ
    /// @details This function proceeds if the target has a matching held item or the
    /// actor has a matching pocket item, and poofs that item. This preserves the
    /// legacy actor-pocket compatibility behavior for one-use items such as keys.

    InventoryCompatibilityContext inventoryContext;
    if (!resolveInventoryCompatibilityContext(self, inventoryContext))
    {
        return false;
    }

    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);
    const ObjectRef itemRef = findMatchingTargetHeldOrActorPocketItemRef(inventoryContext, idsz);
    if (itemRef == ObjectRef::Invalid)
    {
        return false;
    }

    return consumeOrPoofItemWithActorPocketCompatibility(itemRef, *inventoryContext.actorInventory);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_Equip( script_state_t& state, ai_state_t& self )
{
    // Equip()
    /// @author ZZ
    /// @details This function flags the character as being equipped.
    /// This is used by equipment items when they are placed in the inventory

    SelfRoleContext selfContext = makeSelfRoleContext(self);
    return markSelfEquipped(selfContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ChangeArmor( script_state_t& state, ai_state_t& self )
{
    // ChangeArmor( tmpargument = "time" )
    /// @author ZZ
    /// @details This function changes the character's armor.
    /// Sets tmpargument as the old type and tmpx as the new type

    SelfRoleContext selfContext = makeSelfRoleContext(self);
    return changeSelfArmor(state, selfContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetDamageType( script_state_t& state, ai_state_t& self )
{
    // SetDamageType( tmpargument = "damage type" )
    /// @author ZZ
    /// @details This function lets a weapon change the type of damage it inflicts

    SelfRoleContext selfContext = makeSelfRoleContext(self);
    return setSelfDamageType(selfContext, static_cast<DamageType>(state.argument % DAMAGE_COUNT));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetMoney( script_state_t& state, ai_state_t& self )
{
    // SetMoney()
    /// @author ZF
    /// @details Permanently sets the money for the character to tmpargument

    SelfRoleContext selfContext = makeSelfRoleContext(self);
    return setSelfMoney(state, selfContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfCharacterWasABook( script_state_t& state, ai_state_t& self )
{
    // IfCharacterWasABook()
    /// @author ZZ
    /// @details This function proceeds if the base model is the same as the current
    /// model or if the base model is SPELLBOOK
    /// USAGE: USED BY THE MORPH SPELL. Not much use elsewhere

    SelfProfileSnapshot selfContext = makeRequiredSelfProfileSnapshot(self);
    if (!hasRequiredSelfProfileSnapshot(selfContext)) return false;

    return selfContext.baseModelIsSpellbook ||
           selfContext.currentProfileMatchesBaseModel;
}
