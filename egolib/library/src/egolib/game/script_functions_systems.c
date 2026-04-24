/// @file egolib/game/script_functions_systems.c
/// @brief Passages, quests, commerce, teams, combat, enchantment, inventory, stats, and environment

#include "egolib/game/script_functions_internal.h"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/GUI/MessageLog.hpp"

namespace
{
using FollowLinkByModuleNameFn = bool (*)(const std::string&, bool);

GameSessionContext& gameSession()
{
    return GameSessionContext::get();
}

egoboo_config_t& config()
{
    return EngineContext::get().config();
}

FollowLinkByModuleNameFn g_followLinkByModuleName = &link_follow_modname;

IAnimationControl& animationControl(Object& object)
{
    return object;
}

IEnchantable& enchantable(Object& object)
{
    return object;
}

ObjectRef selfObjectRef(const ai_state_t& self)
{
    return self.getSelf();
}

Object& resolvedSelfObject(const ai_state_t& self)
{
    return *resolveSelfContext(self).object;
}

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

struct FollowLinkRequest
{
    std::string selfName;
    std::string moduleName;
};

struct PassageCompatibilityContext
{
    std::shared_ptr<Passage> passage;
    ObjectRef selfRef = ObjectRef::Invalid;
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

struct SelfProfileContext
{
    const ObjectProfile* profile = nullptr;
    std::string selfName;
    std::string className;
    SelfProfilePolicyData policy;
};

struct ModuleEffectsContext
{
    ObjectRef selfRef = ObjectRef::Invalid;
    GameModule* module = nullptr;
};

struct PresentationEffectsContext
{
    ObjectRef selfRef = ObjectRef::Invalid;
    std::shared_ptr<PlayingState> playingState;
    std::shared_ptr<Ego::GUI::MiniMap> minimap;
};

struct SelfPresentationCompatibilityContext
{
    SelfRoleContext selfRole;
    PresentationEffectsContext presentation;
};

struct TargetEconomyCompatibilityContext
{
    IAppearanceProfile* targetAppearance = nullptr;
    IWallet* selfWallet = nullptr;
    IWallet* targetWallet = nullptr;
};

struct ArmorCostPolicy
{
    int requestedSkinCost = 0;
    int currentSkinRefund = 0;
    int netCost = 0;
};

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

struct QuestCompatibilityContext
{
    Ego::QuestLog* targetQuestLog = nullptr;
};

struct ClassChangeCompatibilityContext
{
    IMorphControl* selfMorph = nullptr;
};

void maybeAddSkillPerk(ICharacterState& targetState, uint32_t skillId);
void publishEnemySense(const EnemySenseState& state);
void resetEnemySense();

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

ModuleEffectsContext makeModuleEffectsContext(const ai_state_t& self)
{
    ModuleEffectsContext context;
    context.selfRef = self.getSelf();
    context.module = gameSession().tryActiveModule();
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

bool giveSelfTeamExperience(const script_state_t& state, SelfRoleContext& selfContext)
{
    if (state.distance < 0 || state.distance >= XP_COUNT)
    {
        return true;
    }

    if (selfContext.teamMember == nullptr)
    {
        return false;
    }

    selfContext.teamMember->giveTeamExperience(state.argument, static_cast<XPType>(state.distance));
    return true;
}

bool setSelfTeam(SelfRoleContext& selfContext, TEAM_REF teamRef)
{
    if (selfContext.teamMember == nullptr)
    {
        return false;
    }

    selfContext.teamMember->setTeam(teamRef);
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

bool joinSelfTeamToResolvedTarget(const TargetCompatibilityContext& targetContext,
                                  SelfRoleContext& selfContext)
{
    if (targetContext.info == nullptr || selfContext.teamMember == nullptr)
    {
        return false;
    }

    selfContext.teamMember->setTeam(targetContext.info->getTeamRef());
    return true;
}

bool becomeSelfLeader(SelfRoleContext& selfContext)
{
    if (selfContext.teamMember == nullptr)
    {
        return false;
    }

    selfContext.teamMember->becomeTeamLeader();
    return true;
}

bool isSelfLeaderAlive(const SelfRoleContext& selfContext)
{
    return selfContext.targetInfo != nullptr &&
           teamLeaderRef(*selfContext.targetInfo) != ObjectRef::Invalid;
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

bool dropMoney(const script_state_t& state, IWallet* targetWallet)
{
    if (targetWallet == nullptr)
    {
        return false;
    }

    targetWallet->dropMoney(state.argument);
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

bool applySelfTeam(SelfPresentationCompatibilityContext& context, TEAM_REF teamRef)
{
    return setSelfTeam(context.selfRole, teamRef);
}

IMorphControl& morphControl(Object& object)
{
    return object;
}

GameModule& compatibleModule(const ModuleEffectsContext& context)
{
    if (context.module != nullptr)
    {
        return *context.module;
    }

    return activeModule();
}

water_instance_t& moduleWater(const ModuleEffectsContext& context)
{
    return compatibleModule(context).getWater();
}

fog_instance_t& moduleFog(const ModuleEffectsContext&)
{
    return gameSession().fog();
}

void setModuleWaterLevel(const ModuleEffectsContext& context, int waterLevelTimesTen)
{
    moduleWater(context).set_douse_level(waterLevelTimesTen / 10.0f);
}

int getModuleWaterLevelTimesTen(const ModuleEffectsContext& context)
{
    return moduleWater(context)._douse_level * 10;
}

void setModuleFogTopLevel(const ModuleEffectsContext& context, int fogLevelTimesTen)
{
    fog_instance_t& fog = moduleFog(context);
    const float delta = (Ego::Script::Interpreter::safeCast<float>(fogLevelTimesTen) / 10.0f) - fog._top;
    fog._top += delta;
    fog._distance += delta;
    fog._on = config().graphic_fog_enable.getValue();
    if (fog._distance < 1.0f)
    {
        fog._on = false;
    }
}

int getModuleFogTopLevelTimesTen(const ModuleEffectsContext& context)
{
    return moduleFog(context)._top * 10;
}

void setModuleFogColor(const ModuleEffectsContext& context, int red, int green, int blue)
{
    fog_instance_t& fog = moduleFog(context);
    fog._red = Ego::Math::constrain(red, 0, 0xFF);
    fog._grn = Ego::Math::constrain(green, 0, 0xFF);
    fog._blu = Ego::Math::constrain(blue, 0, 0xFF);
}

void setModuleFogBottomLevel(const ModuleEffectsContext& context, int fogLevelTimesTen)
{
    fog_instance_t& fog = moduleFog(context);
    const float delta = (fogLevelTimesTen / 10.0f) - fog._bottom;
    fog._bottom += delta;
    fog._distance -= delta;
    fog._on = config().graphic_fog_enable.getValue();
    if (fog._distance < 1.0f)
    {
        fog._on = false;
    }
}

int getModuleFogBottomLevelTimesTen(const ModuleEffectsContext& context)
{
    return moduleFog(context)._bottom * 10;
}

bool tryGetModuleTileTypeAtPosition(const ModuleEffectsContext& context,
                                    const Ego::Vector2f& position,
                                    uint16_t& tileType)
{
    return compatibleModule(context).tryGetTileTypeAtPosition(position, tileType);
}

bool setModuleTileTypeAtPosition(const ModuleEffectsContext& context,
                                 const Ego::Vector2f& position,
                                 uint16_t tileType)
{
    return compatibleModule(context).setTileTypeAtPosition(position, tileType);
}

void markActiveModuleBeaten(const ModuleEffectsContext& context)
{
    compatibleModule(context).beatModule();
}

void setActiveModuleExportValid(const ModuleEffectsContext& context, bool valid)
{
    compatibleModule(context).setExportValid(valid);
}

void enableActiveModulePitsKill(const ModuleEffectsContext& context)
{
    compatibleModule(context).enablePitsKill();
}

void enableActiveModulePitsTeleport(const ModuleEffectsContext& context, const Ego::Vector3f& location)
{
    compatibleModule(context).enablePitsTeleport(location);
}

void giveGoodTeamExperience(const ModuleEffectsContext& context, int amount, XPType type)
{
    compatibleModule(context).giveTeamExperience(static_cast<TEAM_REF>(Team::TEAM_GOOD), amount, type);
}

bool tryAddActiveModuleIdsz(const ModuleEffectsContext& context, const IDSZ2& idsz)
{
    return ModuleProfile::moduleAddIDSZ(compatibleModule(context).getPath(), idsz);
}

bool setActorTileType(const ModuleEffectsContext& context, uint16_t tileType)
{
    Object* selfObject = tryObject(context.selfRef);
    return selfObject != nullptr &&
           compatibleModule(context).setTileType(selfObject->getTile(), tileType);
}

bool showMiniMap(const PresentationEffectsContext& context)
{
    if (!context.minimap)
    {
        return false;
    }

    const bool wasHidden = !context.minimap->isVisible();
    context.minimap->setVisible(true);
    return wasHidden;
}

void showMiniMapPlayerPosition(const PresentationEffectsContext& context)
{
    if (context.minimap)
    {
        context.minimap->setShowPlayerPosition(true);
    }
}

const Object* tryUiObject(ObjectRef objectRef)
{
    return tryObject(objectRef);
}

void addMiniMapBlip(const PresentationEffectsContext& context, float x, float y, ObjectRef objectRef)
{
    const Object* object = tryUiObject(objectRef);
    if (!context.minimap || !object)
    {
        return;
    }

    context.minimap->addBlip(x, y, object->getIcon());
}

void addSelfMiniMapBlip(const PresentationEffectsContext& context, float x, float y)
{
    addMiniMapBlip(context, x, y, context.selfRef);
}

void addSelfStatusMonitor(const PresentationEffectsContext& context)
{
    if (!context.playingState)
    {
        return;
    }

    context.playingState->addStatusMonitor(context.selfRef);
}

void clearEndMessageText()
{
    g_endText.setText("");
}

bool addEndMessageText(Object& object, int messageIndex, script_state_t& state)
{
    return ::AddEndMessage(&object, messageIndex, &state);
}

bool addSelfEndMessageText(const PresentationEffectsContext& context, int messageIndex, script_state_t& state)
{
    Object* selfObject = tryObject(context.selfRef);
    return selfObject != nullptr &&
           addEndMessageText(*selfObject, messageIndex, state);
}

void logDeprecatedScriptFunctionUse(const std::string& functionName,
                                    const std::string& className)
{
    EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning,
                                                           __FILE__,
                                                           __LINE__,
                                                           "deprecated script function ",
                                                           "`",
                                                           functionName,
                                                           "`",
                                                           " by class `",
                                                           className,
                                                           "`",
                                                           Log::EndOfEntry);
}

void publishDeprecatedEnableListenSkillWarning(const SelfProfileContext& context)
{
    logDeprecatedScriptFunctionUse("EnableListenSkill", context.className);
}

bool resolveFollowLinkRequest(const SelfProfileContext& context,
                              const int messageId,
                              FollowLinkRequest& request)
{
    if (!context.profile->isValidMessageID(messageId))
    {
        return false;
    }

    request.selfName = context.selfName;
    request.moduleName = context.profile->getMessage(messageId);
    return true;
}

void publishFollowLinkFailureMessage(const PresentationEffectsContext& context,
                                     const std::string& selfName)
{
    const std::string text = "That's too scary for " + selfName;
    if (context.playingState)
    {
        context.playingState->getMessageLog()->addMessage(text);
        return;
    }

    DisplayMsg_printf("%s", text.c_str());
}

bool tryFollowLink(const PresentationEffectsContext& context,
                   const FollowLinkRequest& request)
{
    const bool followed = g_followLinkByModuleName(request.moduleName, true);
    if (!followed)
    {
        publishFollowLinkFailureMessage(context, request.selfName);
    }

    return followed;
}

bool followLinkFromMessageId(const SelfProfileContext& context,
                             const PresentationEffectsContext& presentationContext,
                             const int messageId)
{
    FollowLinkRequest request;
    return resolveFollowLinkRequest(context, messageId, request) &&
           tryFollowLink(presentationContext, request);
}

bool resolvePassageCompatibilityContext(const ai_state_t& self,
                                        int passageId,
                                        PassageCompatibilityContext& context)
{
    context.selfRef = self.getSelf();
    context.passage = tryPassage(passageId);
    return context.passage != nullptr;
}

bool openResolvedPassage(const PassageCompatibilityContext& context)
{
    if (!context.passage)
    {
        return false;
    }

    context.passage->open();
    return true;
}

bool closeResolvedPassage(const PassageCompatibilityContext& context)
{
    return context.passage != nullptr && context.passage->close();
}

bool isResolvedPassageOpen(const PassageCompatibilityContext& context)
{
    return context.passage != nullptr && context.passage->isOpen();
}

void flashResolvedPassage(const PassageCompatibilityContext& context, uint8_t flashColor)
{
    if (context.passage)
    {
        context.passage->flashColor(flashColor);
    }
}

bool addResolvedShopPassage(const PassageCompatibilityContext& context)
{
    if (!context.passage)
    {
        return false;
    }

    context.passage->makeShop(context.selfRef);
    return true;
}

bool resolveOwnedObjectHandle(ObjectRef objectRef, OwnedObjectHandle& handle)
{
    handle.ref = objectRef;
    handle.object = tryObjectShared(objectRef);
    return handle.object != nullptr;
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

struct InventoryCompatibilityContext
{
    const IInventoryHolder* targetInventory = nullptr;
    IInventoryHolder* actorInventory = nullptr;
};

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

void unkurseItemIfPresent(ObjectRef itemRef)
{
    ICharacterState* itemState = tryCharacterState(itemRef);
    if (itemState != nullptr)
    {
        itemState->setKursed(false);
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

int restockAmmoIfMatching(ObjectRef itemRef, const IDSZ2& idsz);

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

void unkurseTargetHeldAndActorPocketItems(const InventoryCompatibilityContext& context)
{
    unkurseItemIfPresent(context.targetInventory->getHeldObject(SLOT_LEFT));
    unkurseItemIfPresent(context.targetInventory->getHeldObject(SLOT_RIGHT));

    for (const ObjectRef& actorPocketItemRef : context.actorInventory->getInventoryItemRefs())
    {
        unkurseItemIfPresent(actorPocketItemRef);
    }
}

Ego::QuestLog* resolvedTargetQuestLog(const ai_state_t& self)
{
    const ITargetInfo* target = tryTargetInfo(self.getTarget());
    return target != nullptr ? tryQuestLog(*target) : nullptr;
}

QuestCompatibilityContext makeQuestCompatibilityContext(const ai_state_t& self)
{
    QuestCompatibilityContext context;
    context.targetQuestLog = resolvedTargetQuestLog(self);
    return context;
}

ClassChangeCompatibilityContext makeClassChangeCompatibilityContext(Object& selfObject)
{
    ClassChangeCompatibilityContext context;
    context.selfMorph = static_cast<IMorphControl*>(&selfObject);
    return context;
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

bool setResolvedTargetTeam(const TargetCompatibilityContext& targetContext, TEAM_REF teamRef)
{
    if (targetContext.teamMember == nullptr)
    {
        return false;
    }

    targetContext.teamMember->setTeam(teamRef);
    return true;
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

void publishEnemySenseFromResolvedTarget(const TargetCompatibilityContext& targetContext,
                                         uint32_t idsz)
{
    if (targetContext.info != nullptr)
    {
        publishEnemySense(EnemySenseState(targetContext.info->getTeamRef(), idsz));
        return;
    }

    resetEnemySense();
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

bool addQuestIfMissing(Ego::QuestLog& questLog, const IDSZ2& idsz, int progress)
{
    if (questLog.hasActiveQuest(idsz) || questLog.isBeaten(idsz))
    {
        return false;
    }

    questLog.setQuestProgress(idsz, std::max(progress, 0));
    return true;
}

bool adjustActiveQuestLevel(Ego::QuestLog& questLog, const IDSZ2& idsz, int delta)
{
    if (delta == 0 || !questLog.hasActiveQuest(idsz))
    {
        return false;
    }

    questLog.setQuestProgress(idsz, questLog[idsz] + delta);
    return true;
}

bool beatActiveQuest(Ego::QuestLog& questLog, const IDSZ2& idsz)
{
    if (!questLog.hasActiveQuest(idsz))
    {
        return false;
    }

    questLog.setQuestProgress(idsz, Ego::QuestLog::QUEST_BEATEN);
    return true;
}

bool raiseQuestLevelIfHigher(Ego::QuestLog& questLog, const IDSZ2& idsz, int progress)
{
    if (progress <= 0 || questLog.isBeaten(idsz) || progress <= questLog[idsz])
    {
        return false;
    }

    questLog.setQuestProgress(idsz, progress);
    return true;
}

template <typename Fn>
bool updatePlayerQuestLogs(Fn&& fn)
{
    bool updated = false;
    for (const std::shared_ptr<Ego::Player>& player : gameSession().playerList())
    {
        if (player != nullptr && fn(player->getQuestLog()))
        {
            updated = true;
        }
    }

    return updated;
}

bool addResolvedQuest(const QuestCompatibilityContext& context, const IDSZ2& idsz, int progress)
{
    return context.targetQuestLog != nullptr &&
           addQuestIfMissing(*context.targetQuestLog, idsz, progress);
}

bool adjustResolvedQuestLevel(const QuestCompatibilityContext& context, const IDSZ2& idsz, int delta)
{
    return context.targetQuestLog != nullptr &&
           adjustActiveQuestLevel(*context.targetQuestLog, idsz, delta);
}

bool beatQuestForAllPlayers(const IDSZ2& idsz)
{
    return updatePlayerQuestLogs([&](Ego::QuestLog& questLog) { return beatActiveQuest(questLog, idsz); });
}

bool raiseQuestForAllPlayers(const IDSZ2& idsz, int progress)
{
    return updatePlayerQuestLogs([&](Ego::QuestLog& questLog)
    {
        return raiseQuestLevelIfHigher(questLog, idsz, progress);
    });
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

TargetEconomyCompatibilityContext makeTargetEconomyCompatibilityContext(const ai_state_t& self)
{
    TargetEconomyCompatibilityContext context;
    context.targetAppearance = tryAppearanceProfile(self.getTarget());
    context.selfWallet = tryWallet(self.getSelf());
    context.targetWallet = tryWallet(self.getTarget());
    return context;
}

bool setTargetArmorPrice(script_state_t& state, const TargetEconomyCompatibilityContext& context)
{
    if (context.targetAppearance == nullptr)
    {
        return false;
    }

    const int value = context.targetAppearance->getSkinCost(Ego::Script::Interpreter::safeCast<size_t>(state.argument));
    if (value <= 0)
    {
        state.x = 0;
        return false;
    }

    state.x = value;
    return true;
}

ArmorCostPolicy makeArmorCostPolicy(const IAppearanceProfile& appearance,
                                    const size_t requestedSkin)
{
    ArmorCostPolicy policy;
    policy.requestedSkinCost = appearance.getSkinCost(requestedSkin);
    policy.currentSkinRefund = appearance.getSkinCost(appearance.getSkin());
    policy.netCost = policy.requestedSkinCost - policy.currentSkinRefund;
    return policy;
}

bool changeTargetArmor(script_state_t& state, const TargetEconomyCompatibilityContext& context)
{
    if (context.targetAppearance == nullptr)
    {
        return false;
    }

    const int oldSkin = context.targetAppearance->getSkin();
    state.x = context.targetAppearance->setSkin(Ego::Script::Interpreter::safeCast<size_t>(state.argument));
    state.argument = oldSkin;
    return true;
}

void clampTransferredMoney(script_state_t& state, const TargetEconomyCompatibilityContext& context)
{
    if (context.selfWallet == nullptr || context.targetWallet == nullptr)
    {
        return;
    }

    if (state.argument < 0 && std::abs(state.argument) > context.targetWallet->getMoney())
    {
        state.argument = -context.targetWallet->getMoney();
    }
    if (state.argument > context.selfWallet->getMoney())
    {
        state.argument = context.selfWallet->getMoney();
    }
}

bool giveMoneyToTarget(script_state_t& state, const TargetEconomyCompatibilityContext& context)
{
    if (context.selfWallet == nullptr || context.targetWallet == nullptr)
    {
        return false;
    }

    clampTransferredMoney(state, context);
    context.selfWallet->giveMoney(-state.argument);
    context.targetWallet->giveMoney(state.argument);
    return true;
}

bool chargeTargetArmor(script_state_t& state, const TargetEconomyCompatibilityContext& context)
{
    if (context.targetAppearance == nullptr || context.targetWallet == nullptr)
    {
        return false;
    }

    const ArmorCostPolicy armorCost = makeArmorCostPolicy(*context.targetAppearance,
                                                          Ego::Script::Interpreter::safeCast<size_t>(state.argument));
    state.y = armorCost.requestedSkinCost;

    if (armorCost.netCost > context.targetWallet->getMoney())
    {
        state.x = armorCost.netCost - context.targetWallet->getMoney();
        return false;
    }

    context.targetWallet->giveMoney(-armorCost.netCost);
    state.x = 0;
    return true;
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

void publishEnemySense(const EnemySenseState& state)
{
    gameSession().publishEnemySense(state);
}

void resetEnemySense()
{
    gameSession().resetEnemySense();
}

void configurePitFall(const ModuleEffectsContext& context, const Ego::Vector3f& location)
{
    if (compatibleModule(context).isInsidePitBounds(location.x(), location.y()))
    {
        enableActiveModulePitsTeleport(context, location);
        return;
    }

    enableActiveModulePitsKill(context);
}

void pushModuleEndVictoryScreen()
{
    engine().pushGameState(std::make_shared<VictoryScreen>(nullptr, true));
}
}

void scr_systems_set_follow_link_by_modname_for_test(bool (*fn)(const std::string&, bool))
{
    g_followLinkByModuleName = fn ? fn : &link_follow_modname;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetTargetArmorPrice( script_state_t& state, ai_state_t& self )
{
    // tmpx = GetTargetArmorPrice( tmpargument = "skin" )
    /// @author ZZ
    /// @details This function returns the cost of the desired skin upgrade, setting
    /// tmpx to the price

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetEconomyCompatibilityContext targetContext = makeTargetEconomyCompatibilityContext(self);
    return setTargetArmorPrice(state, targetContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinTargetTeam( script_state_t& state, ai_state_t& self )
{
    // JoinTargetTeam()
    /// @author ZZ
    /// @details This function lets a character join a different team.  Used
    /// mostly for pets

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    SelfRoleContext selfContext = makeSelfRoleContext(self);
    return joinSelfTeamToResolvedTarget(targetContext, selfContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_OpenPassage( script_state_t& state, ai_state_t& self )
{
    // OpenPassage( tmpargument = "passage" )

    /// @author ZZ
    /// @details This function opens the passage specified by tmpargument, failing if the
    /// passage was already open.
    /// Passage areas are defined in passage.txt and set in spawn.txt for the given character

    if (!resolveSelfContext(self).isResolved()) return false;

    PassageCompatibilityContext passageContext;
    return resolvePassageCompatibilityContext(self, state.argument, passageContext) &&
           openResolvedPassage(passageContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ClosePassage( script_state_t& state, ai_state_t& self )
{
    // ClosePassage( tmpargument = "passage" )
    /// @author ZZ
    /// @details This function closes the passage specified by tmpargument, proceeding
    /// if the passage isn't blocked.  Crushable characters within the passage
    /// are crushed.

    if (!resolveSelfContext(self).isResolved()) return false;

    PassageCompatibilityContext passageContext;
    return resolvePassageCompatibilityContext(self, state.argument, passageContext) &&
           closeResolvedPassage(passageContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfPassageOpen( script_state_t& state, ai_state_t& self )
{
    // IfPassageOpen( tmpargument = "passage" )
    /// @author ZZ
    /// @details This function proceeds if the given passage is valid and open to movement
    /// Used mostly by door characters to tell them when to run their open animation.

    if (!resolveSelfContext(self).isResolved()) return false;

    PassageCompatibilityContext passageContext;
    return resolvePassageCompatibilityContext(self, state.argument, passageContext) &&
           isResolvedPassageOpen(passageContext);
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
uint8_t scr_AddIDSZ( script_state_t& state, ai_state_t& self )
{
    // AddIDSZ( tmpargument = "idsz" )
    /// @author ZZ
    /// @details This function slaps an expansion IDSZ onto the menu.txt file.
    /// Used to show completion of special quests for a given module

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    if (tryAddActiveModuleIdsz(moduleContext, Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument)))
    {
        // invalidate any module list so that we will reload them
        //module_list_valid = false;
    }

    return true;
}


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
uint8_t scr_IfLeaderKilled( script_state_t& state, ai_state_t& self )
{
    // IfLeaderKilled()
    /// @author ZZ
    /// @details This function proceeds if the team's leader died this update

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_LEADERKILLED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_BecomeLeader( script_state_t& state, ai_state_t& self )
{
    // BecomeLeader()
    /// @author ZZ
    /// @details This function makes the character the leader of the team

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfRoleContext selfContext = makeSelfRoleContext(self);
    return becomeSelfLeader(selfContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ChangeTargetArmor( script_state_t& state, ai_state_t& self )
{
    // ChangeTargetArmor( tmpargument = "armor" )

    /// @author ZZ
    /// @details This function sets the target's armor type and returns the old type
    /// as tmpargument and the new type as tmpx

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetEconomyCompatibilityContext targetContext = makeTargetEconomyCompatibilityContext(self);
    return changeTargetArmor(state, targetContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveMoneyToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveMoneyToTarget( tmpargument = "money" )
    /// @author ZZ
    /// @details This function increases the target's money, while decreasing the
    /// character's own money.  tmpargument is set to the amount transferred

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetEconomyCompatibilityContext targetContext = makeTargetEconomyCompatibilityContext(self);
    return giveMoneyToTarget(state, targetContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfLeaderIsAlive( script_state_t& state, ai_state_t& self )
{
    // IfLeaderIsAlive()
    /// @author ZZ
    /// @details This function proceeds if the team has a leader

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfRoleContext selfContext = makeSelfRoleContext(self);
    return isSelfLeaderAlive(selfContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ChangeTile( script_state_t& state, ai_state_t& self )
{
    // ChangeTile( tmpargument = "tile type")
    /// @author ZZ
    /// @details This function changes the tile under the character to the new tile type,
    /// which is highly module dependent

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    return setActorTileType(moduleContext, state.argument);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DropMoney( script_state_t& state, ai_state_t& self )
{
    // DropMoney( tmpargument = "money" )
    /// @author ZZ
    /// @details This function drops a certain amount of money, if the character has that
    /// much

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfRoleContext selfContext = makeSelfRoleContext(self);
    return dropMoney(state, selfContext.wallet);
}


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
uint8_t scr_SetWaterLevel( script_state_t& state, ai_state_t& self )
{
    // SetWaterLevel( tmpargument = "level" )
    /// @author ZZ
    /// @details This function raises or lowers the water in the module

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    setModuleWaterLevel(moduleContext, state.argument);

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
uint8_t scr_GiveExperienceToTargetTeam( script_state_t& state, ai_state_t& self )
{
    // GiveExperienceToTargetTeam( tmpargument = "amount", tmpdistance = "type" )
    /// @author ZZ
    /// @details This function gives experience to everyone on the target's team

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfRoleContext selfContext = makeSelfRoleContext(self);
    return giveSelfTeamExperience(state, selfContext);
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
uint8_t scr_GetWaterLevel( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetWaterLevel()
    /// @author ZZ
    /// @details This function sets tmpargument to the current douse level for the water * 10.
    /// A waterlevel in wawalight of 85 would set tmpargument to 850

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    state.argument = getModuleWaterLevelTimesTen(moduleContext);

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
uint8_t scr_BreakPassage( script_state_t& state, ai_state_t& self )
{
    // BreakPassage( tmpargument = "passage", tmpturn = "tile type", tmpdistance = "number of frames", tmpx = "borken tile", tmpy = "tile fx bits" )

    /// @author ZZ
    /// @details This function makes the tiles fall away ( turns into damage terrain )
    /// This function causes the tiles of a passage to increment if stepped on.
    /// tmpx and tmpy are both set to the location of whoever broke the tile if
    /// the function passed.

    if (!resolveSelfContext(self).isResolved()) return false;

    return ::BreakPassage( state.y, state.x, state.distance, state.turn, ( PASS_REF )state.argument, &( state.x ), &( state.y ) );
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
uint8_t scr_ShowMap( script_state_t& state, ai_state_t& self )
{
    // ShowMap()
    /// @author ZZ
    /// @details This function shows the module's map.
    /// Fails if map already visible

    if (!resolveSelfContext(self).isResolved()) return false;
    const SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
    return showMiniMap(selfContext.presentation);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ShowYouAreHere( script_state_t& state, ai_state_t& self )
{
    // ShowYouAreHere()
    /// @author ZZ
    /// @details This function shows the blinking white blip on the map that represents the
    /// camera location

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
    showMiniMapPlayerPosition(selfContext.presentation);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ShowBlipXY( script_state_t& state, ai_state_t& self )
{
    // ShowBlipXY( tmpx = "x", tmpy = "y", tmpargument = "color" )

    /// @author ZZ
    /// @details This function draws a blip on the map, and must be done each update
    if (!resolveSelfContext(self).isResolved()) return false;

    // Add a blip
    if ( state.argument >= 0 )
    {
        const SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
        addSelfMiniMapBlip(selfContext.presentation, state.x, state.y);
    }

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
uint8_t scr_SetFogLevel( script_state_t& state, ai_state_t& self )
{
    // SetFogLevel( tmpargument = "level" )
    /// @author ZZ
    /// @details This function sets the level of the module's fog.
    /// Values are * 10
    /// !!BAD!! DOESN'T WORK !!BAD!!

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    setModuleFogTopLevel(moduleContext, state.argument);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetFogLevel( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetFogLevel()
    /// @author ZZ
    /// @details This function sets tmpargument to the level of the module's fog.
    /// Values are * 10

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    state.argument = getModuleFogTopLevelTimesTen(moduleContext);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetFogTAD( script_state_t& state, ai_state_t& self )
{
    /// @author ZZ
    /// @details This function sets the color of the module's fog.
    /// TAD stands for <turn, argument, distance> == <red, green, blue>.
    /// Makes sense, huh?
    /// !!BAD!! DOESN'T WORK !!BAD!!

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    setModuleFogColor(moduleContext, state.turn, state.argument, state.distance);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetFogBottomLevel( script_state_t& state, ai_state_t& self )
{
    // SetFogBottomLevel( tmpargument = "level" )

    /// @author ZZ
    /// @details This function sets the level of the module's fog.
    /// Values are * 10

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    setModuleFogBottomLevel(moduleContext, state.argument);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetFogBottomLevel( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetFogBottomLevel()

    /// @author ZZ
    /// @details This function sets tmpargument to the level of the module's fog.
    /// Values are * 10

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    state.argument = getModuleFogBottomLevelTimesTen(moduleContext);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetTileXY( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetTileXY( tmpx = "x", tmpy = "y" )
    /// @author ZZ
    /// @details This function sets tmpargument to the tile type at the specified
    /// coordinates

    if (!resolveSelfContext(self).isResolved()) return false;

    uint16_t tileType = 0;
    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    if (!tryGetModuleTileTypeAtPosition(moduleContext,
                                        Ego::Vector2f(float(state.x), float(state.y)),
                                        tileType))
    {
        return false;
    }

    state.argument = tileType;
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTileXY( script_state_t& state, ai_state_t& self )
{
    // SetTileXY( tmpargument = "tile type", tmpx = "x", tmpy = "y" )
    /// @author ZZ
    /// @details This function changes the tile type at the specified coordinates

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    return setModuleTileTypeAtPosition(moduleContext,
                                       Ego::Vector2f(float(state.x), float(state.y)),
                                       state.argument);
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
uint8_t scr_FlashPassage( script_state_t& state, ai_state_t& self )
{
    // FlashPassage( tmpargument = "passage", tmpdistance = "color" )

    /// @author ZZ
    /// @details This function makes the given passage light or dark.
    /// Usage: For debug purposes

    if (!resolveSelfContext(self).isResolved()) return false;

    PassageCompatibilityContext passageContext;
    resolvePassageCompatibilityContext(self, state.argument, passageContext);
    flashResolvedPassage(passageContext, state.distance);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_FindTileInPassage( script_state_t& state, ai_state_t& self )
{
    // tmpx, tmpy = FindTileInPassage( tmpargument = "passage", tmpdistance = "tile type", tmpx, tmpy )

    /// @author ZZ
    /// @details This function finds all tiles of the specified type that lie within the
    /// given passage.  Call multiple times to find multiple tiles.  tmpx and
    /// tmpy will be set to the middle of the found tile if one is found, or
    /// both will be set to 0 if no tile is found.
    /// tmpx and tmpy are required and set on return

    if (!resolveSelfContext(self).isResolved()) return false;

    return ::FindTileInPassage( state.x, state.y, state.distance, static_cast<PASS_REF>(state.argument), &(state.x), &(state.y) );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_BeatModule( script_state_t& state, ai_state_t& self )
{
    // BeatModule()
    /// @author ZZ
    /// @details This function displays the Module Ended message

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    markActiveModuleBeaten(moduleContext);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EndModule( script_state_t& state, ai_state_t& self )
{
    // EndModule()
    /// @author ZZ
    /// @details This function presses the Escape key

    if (!resolveSelfContext(self).isResolved()) return false;

    pushModuleEndVictoryScreen();

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisableExport( script_state_t& state, ai_state_t& self )
{
    // DisableExport()
    /// @author ZZ
    /// @details This function turns export off

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    setActiveModuleExportValid(moduleContext, false);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EnableExport( script_state_t& state, ai_state_t& self )
{
    // EnableExport()
    /// @author ZZ
    /// @details This function turns export on

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    setActiveModuleExportValid(moduleContext, true);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DropTargetMoney( script_state_t& state, ai_state_t& self )
{
    // DropTargetMoney( tmpargument = "amount" )
    /// @author ZZ
    /// @details This function drops some of the target's money

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetEconomyCompatibilityContext targetContext = makeTargetEconomyCompatibilityContext(self);
    return dropMoney(state, targetContext.targetWallet);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinTeam( script_state_t& state, ai_state_t& self )
{
    // JoinTeam( tmpargument = "team" )
    /// @author ZZ
    /// @details This makes the character itself join a specified team (A = 0, B = 1, 23 = Z, etc.)

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
    return applySelfTeam(selfContext, static_cast<TEAM_REF>(state.argument));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_TargetJoinTeam( script_state_t& state, ai_state_t& self )
{
    // TargetJoinTeam( tmpargument = "team" )
    /// @author ZZ
    /// @details This makes the Target join a Team specified in tmpargument (A = 0, 25 = Z, etc.)

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    return setResolvedTargetTeam(targetContext, static_cast<TEAM_REF>(state.argument));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ClearEndMessage( script_state_t& state, ai_state_t& self )
{
    // ClearEndMessage()
    /// @author ZZ
    /// @details This function empties the end-module text buffer

    if (!resolveSelfContext(self).isResolved()) return false;

    clearEndMessageText();

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddEndMessage( script_state_t& state, ai_state_t& self )
{
    // AddEndMessage( tmpargument = "message" )
    /// @author ZZ
    /// @details This function appends a message to the end-module text buffer

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
    return addSelfEndMessageText(selfContext.presentation, state.argument, state);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddStat( script_state_t& state, ai_state_t& self )
{
    // AddStat()
    /// @author ZZ
    /// @details This function turns on an NPC's status display

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
    addSelfStatusMonitor(selfContext.presentation);

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
uint8_t scr_AddShopPassage( script_state_t& state, ai_state_t& self )
{
    // AddShopPassage( tmpargument = "passage" )
    /// @author ZZ
    /// @details This function makes a passage behave as a shop area, as long as the
    /// character is alive.

    if (!resolveSelfContext(self).isResolved()) return false;

    PassageCompatibilityContext passageContext;
    return resolvePassageCompatibilityContext(self, state.argument, passageContext) &&
           addResolvedShopPassage(passageContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_TargetPayForArmor( script_state_t& state, ai_state_t& self )
{
    // tmpx, tmpy = TargetPayForArmor( tmpargument = "skin" )

    /// @author ZZ
    /// @details This function costs the Target the appropriate amount of money for the
    /// given armor type.  Passes if the character has enough, and fails if not.
    /// Does trade-in bonus automatically.  tmpy is always set to cost of requested
    /// skin tmpx is set to amount needed after trade-in ( 0 for pass ).

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetEconomyCompatibilityContext targetContext = makeTargetEconomyCompatibilityContext(self);
    return chargeTargetArmor(state, targetContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinEvilTeam( script_state_t& state, ai_state_t& self )
{
    // JoinEvilTeam()
    /// @author ZZ
    /// @details This function adds the character to the evil Team.

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
    return applySelfTeam(selfContext, static_cast<TEAM_REF>(Team::TEAM_EVIL));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinNullTeam( script_state_t& state, ai_state_t& self )
{
    // JoinNullTeam()
    /// @author ZZ
    /// @details This function adds the character to the null Team.

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
    return applySelfTeam(selfContext, static_cast<TEAM_REF>(Team::TEAM_NULL));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinGoodTeam( script_state_t& state, ai_state_t& self )
{
    // JoinGoodTeam()
    /// @author ZZ
    /// @details This function adds the character to the good Team.

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
    return applySelfTeam(selfContext, static_cast<TEAM_REF>(Team::TEAM_GOOD));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PitsKill( script_state_t& state, ai_state_t& self )
{
    // PitsKill()
    /// @author ZZ
    /// @details This function activates pit deaths for when characters fall below a
    /// certain altitude.

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    enableActiveModulePitsKill(moduleContext);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveExperienceToGoodTeam( script_state_t& state, ai_state_t& self )
{
    // GiveExperienceToGoodTeam(  tmpargument = "amount", tmpdistance = "type" )
    /// @author ZZ
    /// @details This function gives experience to everyone on the G Team

    if (!resolveSelfContext(self).isResolved()) return false;

    if(state.distance < XP_COUNT)
    {
        const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
        giveGoodTeamExperience(moduleContext, state.argument, static_cast<XPType>(state.distance));
    }


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
uint8_t scr_EnableListenSkill( script_state_t& state, ai_state_t& self )
{
    // EnableListenSkill()
    /// @author ZF
    /// @details This function increases range from which sound can be heard by 33%

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfProfileContext selfContext = makeSelfProfileContext(self);

    publishDeprecatedEnableListenSkillWarning(selfContext);
    return false;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_FollowLink( script_state_t& state, ai_state_t& self )
{
    // FollowLink( tmpargument = "index of next module name" )
    /// @author BB
    /// @details Skips to the next module!

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfProfileContext selfContext = makeSelfProfileContext(self);
    const PresentationEffectsContext presentationContext = makePresentationEffectsContext(self);

    return followLinkFromMessageId(selfContext, presentationContext, state.argument);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddQuest( script_state_t& state, ai_state_t& self )
{
    // AddQuest( tmpargument = "quest idsz" )
    /// @author ZF
    /// @details This function adds a quest idsz set in tmpargument into the targets quest.txt to 0

    if (!resolveSelfContext(self).isResolved()) return false;

    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);
    const QuestCompatibilityContext questContext = makeQuestCompatibilityContext(self);
    return addResolvedQuest(questContext, idsz, state.distance);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_BeatQuestAllPlayers( script_state_t& state, ai_state_t& self )
{
    // BeatQuestAllPlayers()
    /// @author ZF
    /// @details This function marks a IDSZ in the targets quest.txt as beaten
    ///               returns true if at least one quest got marked as beaten.

    if (!resolveSelfContext(self).isResolved()) return false;

    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);
    return beatQuestForAllPlayers(idsz);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetQuestLevel( script_state_t& state, ai_state_t& self )
{
    // SetQuestLevel( tmpargument = "idsz", distance = "adjustment" )
    /// @author ZF
    /// @details This function modifies the quest level for a specific quest IDSZ
    /// tmpargument specifies quest idsz (tmpargument) and the adjustment (tmpdistance, which may be negative)

    if (!resolveSelfContext(self).isResolved()) return false;

    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);
    const QuestCompatibilityContext questContext = makeQuestCompatibilityContext(self);
    return adjustResolvedQuestLevel(questContext, idsz, state.distance);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddQuestAllPlayers( script_state_t& state, ai_state_t& self )
{
    // AddQuestAllPlayers( tmpargument = "quest idsz" )
    /// @author ZF
    /// @details This function adds a quest idsz set in tmpargument into all local player's quest logs
    /// The quest level Is set to tmpdistance if the level Is not already higher

    if (!resolveSelfContext(self).isResolved()) return false;

    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);
    return raiseQuestForAllPlayers(idsz, state.distance);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddBlipAllEnemies( script_state_t& state, ai_state_t& self )
{
    // AddBlipAllEnemies()
    /// @author ZF
    /// @details show all enemies on the minimap who match the IDSZ given in tmpargument
    /// it show only the enemies of the AI Target

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    publishEnemySenseFromResolvedTarget(targetContext, state.argument);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PitsFall( script_state_t& state, ai_state_t& self )
{
    // PitsFall( tmpx = "teleprt x", tmpy = "teleprt y", tmpdistance = "teleprt z" )
    /// @author ZF
    /// @details This function activates pit teleportation.

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    configurePitFall(moduleContext,
                     Ego::Vector3f(static_cast<float>(state.x),
                                   static_cast<float>(state.y),
                                   static_cast<float>(state.distance)));

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
uint8_t scr_GiveSkillToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveSkillToTarget( tmpargument = "skill_IDSZ" )
    /// @author ZF
    /// @details This function permanently gives the target character a Perk

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    return giveResolvedTargetSkill(targetContext, state.argument);
}
