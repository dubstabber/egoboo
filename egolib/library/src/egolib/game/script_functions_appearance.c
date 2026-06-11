/// @file egolib/game/script_functions_appearance.c
/// @brief Class change, spell/spellbook transforms, equip, armor, IDSz, and character self-presentation

#include "egolib/game/script_functions_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

namespace
{
IAnimationControl& animationControl(Object& object)
{
    return object;
}

IEnchantable& enchantable(Object& object)
{
    return object;
}

Object& resolvedSelfObject(const ai_state_t& self)
{
    return *resolveSelfContext(self).object;
}

IMorphControl& morphControl(Object& object)
{
    return object;
}

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

struct SelfProfileComparisonData
{
    ObjectProfileRef baseModelRef = ObjectProfileRef::Invalid;
    bool baseModelIsSpellbook = false;
    bool currentProfileMatchesBaseModel = false;
};

struct SelfProfilePolicyData
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
    SelfProfilePolicyData policy;
};

struct ClassChangeCompatibilityContext
{
    IMorphControl* selfMorph = nullptr;
};

struct PresentationEffectsContext
{
    ObjectRef selfRef = ObjectRef::Invalid;
    std::shared_ptr<IPlayingStateController> playingState;
    std::shared_ptr<Ego::GUI::MiniMap> minimap;
};

struct SelfPresentationCompatibilityContext
{
    SelfRoleContext selfRole;
    PresentationEffectsContext presentation;
};

struct InventoryCompatibilityContext
{
    const IInventoryHolder* targetInventory = nullptr;
    IInventoryHolder* actorInventory = nullptr;
};

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

PresentationEffectsContext makePresentationEffectsContext(const ai_state_t& self)
{
    PresentationEffectsContext context;
    context.selfRef = self.getSelf();
    context.playingState = EngineContext::get().tryActivePlayingState();
    context.minimap = context.playingState ? context.playingState->getMiniMap() : nullptr;
    return context;
}

SelfPresentationCompatibilityContext makeSelfPresentationCompatibilityContext(const ai_state_t& self)
{
    SelfPresentationCompatibilityContext context;
    context.selfRole = makeSelfRoleContext(self);
    context.presentation = makePresentationEffectsContext(self);
    return context;
}

bool setSelfDamageType(SelfRoleContext& selfContext, DamageType damageType)
{
    if (selfContext.selfObject == nullptr)
    {
        return false;
    }

    selfContext.selfObject->setDamageTargetType(damageType);
    return true;
}

bool markSelfEquipped(SelfRoleContext& selfContext)
{
    if (selfContext.selfObject == nullptr)
    {
        return false;
    }

    selfContext.selfObject->setEquipped(true);
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

bool applySelfDamageType(SelfPresentationCompatibilityContext& context, DamageType damageType)
{
    return setSelfDamageType(context.selfRole, damageType);
}

bool markSelfAsEquipped(SelfPresentationCompatibilityContext& context)
{
    return markSelfEquipped(context.selfRole);
}

bool applySelfArmorChange(script_state_t& state, SelfPresentationCompatibilityContext& context)
{
    return changeSelfArmor(state, context.selfRole);
}

bool applySelfMoney(const script_state_t& state, SelfPresentationCompatibilityContext& context)
{
    return setSelfMoney(state, context.selfRole);
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
        !EngineContext::get().profileSystem().isLoaded(profileID))
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

    if (!resolveSelfContext(self).isResolved()) return false;

    becomeSpell(enchantable(resolvedSelfObject(self)),
                morphControl(resolvedSelfObject(self)),
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

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfProfileContext selfContext = makeSelfProfileContext(self);

    becomeSpellbook(enchantable(resolvedSelfObject(self)),
                    morphControl(resolvedSelfObject(self)),
                    animationControl(resolvedSelfObject(self)),
                    selfContext.policy.profileRef,
                    selfContext.policy.spellEffectSkin,
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

    if (!resolveSelfContext(self).isResolved()) return false;

    const auto profileID = ObjectProfileRef(static_cast<PRO_REF>(state.argument));
    const ClassChangeCompatibilityContext classContext = makeClassChangeCompatibilityContext(resolvedSelfObject(self));

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

    if (!resolveSelfContext(self).isResolved()) return false;

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

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
    return markSelfAsEquipped(selfContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ChangeArmor( script_state_t& state, ai_state_t& self )
{
    // ChangeArmor( tmpargument = "time" )
    /// @author ZZ
    /// @details This function changes the character's armor.
    /// Sets tmpargument as the old type and tmpx as the new type

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
    return applySelfArmorChange(state, selfContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetDamageType( script_state_t& state, ai_state_t& self )
{
    // SetDamageType( tmpargument = "damage type" )
    /// @author ZZ
    /// @details This function lets a weapon change the type of damage it inflicts

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
    return applySelfDamageType(selfContext, static_cast<DamageType>(state.argument % DAMAGE_COUNT));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetMoney( script_state_t& state, ai_state_t& self )
{
    // SetMoney()
    /// @author ZF
    /// @details Permanently sets the money for the character to tmpargument

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
    return applySelfMoney(state, selfContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfCharacterWasABook( script_state_t& state, ai_state_t& self )
{
    // IfCharacterWasABook()
    /// @author ZZ
    /// @details This function proceeds if the base model is the same as the current
    /// model or if the base model is SPELLBOOK
    /// USAGE: USED BY THE MORPH SPELL. Not much use elsewhere

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfProfileContext selfContext = makeSelfProfileContext(self);

    return selfContext.policy.comparison.baseModelIsSpellbook ||
           selfContext.policy.comparison.currentProfileMatchesBaseModel;
}
