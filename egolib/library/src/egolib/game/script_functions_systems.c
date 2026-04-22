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

IInventoryHolder& inventoryHolder(Object& object)
{
    return object;
}

IAnimationControl& animationControl(Object& object)
{
    return object;
}

ICharacterState& characterState(Object& object)
{
    return object;
}

IEnchantable& enchantable(Object& object)
{
    return object;
}

ITeamMember& teamMember(Object& object)
{
    return object;
}

const ITargetInfo& targetInfo(const Object& object)
{
    return object;
}

ObjectRef selfObjectRef(const ai_state_t& self)
{
    return self.getSelf();
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

struct SelfCompatibilityContext
{
    Object* selfObject = nullptr;
    IAppearanceProfile* appearance = nullptr;
    ITeamMember* teamMember = nullptr;
    IWallet* wallet = nullptr;
    const ObjectProfile* profile = nullptr;
    std::string selfName;
    std::string className;
    SelfProfilePolicyData policy;
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

struct EnchantInvocationContext
{
    IEnchantable* target = nullptr;
    OwnedObjectHandle owner;
    OwnedObjectHandle spawner;
};

SelfCompatibilityContext makeSelfCompatibilityContext(Object& selfObject)
{
    SelfCompatibilityContext context;
    context.selfObject = &selfObject;
    context.appearance = static_cast<IAppearanceProfile*>(&selfObject);
    context.teamMember = static_cast<ITeamMember*>(&selfObject);
    context.wallet = static_cast<IWallet*>(&selfObject);
    return context;
}

SelfCompatibilityContext makeSelfCompatibilityProfileContext(Object& selfObject, const ObjectProfile& selfProfile)
{
    SelfCompatibilityContext context = makeSelfCompatibilityContext(selfObject);
    context.profile = &selfProfile;
    context.selfName = selfObject.getName();
    context.className = selfProfile.getClassName();
    context.policy.profileRef = selfObject.getProfileID();
    context.policy.enchantRef = selfProfile.getEnchantRef();
    context.policy.spellEffectSkin = selfProfile.getSpellEffectType();
    context.policy.comparison.baseModelRef = selfObject.getBaseModelRef();
    context.policy.comparison.baseModelIsSpellbook = context.policy.comparison.baseModelRef == ObjectProfileRef(SPELLBOOK);
    context.policy.comparison.currentProfileMatchesBaseModel =
        context.policy.comparison.baseModelRef == context.policy.profileRef;
    return context;
}

bool setSelfDamageType(SelfCompatibilityContext& selfContext, DamageType damageType)
{
    if (selfContext.selfObject == nullptr)
    {
        return false;
    }

    selfContext.selfObject->setDamageTargetType(damageType);
    return true;
}

bool markSelfEquipped(SelfCompatibilityContext& selfContext)
{
    if (selfContext.selfObject == nullptr)
    {
        return false;
    }

    selfContext.selfObject->setEquipped(true);
    return true;
}

bool changeSelfArmor(script_state_t& state, SelfCompatibilityContext& selfContext)
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

bool giveSelfTeamExperience(const script_state_t& state, SelfCompatibilityContext& selfContext)
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

bool setSelfTeam(SelfCompatibilityContext& selfContext, TEAM_REF teamRef)
{
    if (selfContext.teamMember == nullptr)
    {
        return false;
    }

    selfContext.teamMember->setTeam(teamRef);
    return true;
}

bool setSelfMoney(const script_state_t& state, SelfCompatibilityContext& selfContext)
{
    if (selfContext.wallet == nullptr)
    {
        return false;
    }

    selfContext.wallet->giveMoney(state.argument - selfContext.wallet->getMoney());
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

IMorphControl& morphControl(Object& object)
{
    return object;
}

water_instance_t& moduleWater()
{
    return activeModule().getWater();
}

fog_instance_t& moduleFog()
{
    return gameSession().fog();
}

void setModuleWaterLevel(int waterLevelTimesTen)
{
    moduleWater().set_douse_level(waterLevelTimesTen / 10.0f);
}

int getModuleWaterLevelTimesTen()
{
    return moduleWater()._douse_level * 10;
}

void setModuleFogTopLevel(int fogLevelTimesTen)
{
    fog_instance_t& fog = moduleFog();
    const float delta = (Ego::Script::Interpreter::safeCast<float>(fogLevelTimesTen) / 10.0f) - fog._top;
    fog._top += delta;
    fog._distance += delta;
    fog._on = config().graphic_fog_enable.getValue();
    if (fog._distance < 1.0f)
    {
        fog._on = false;
    }
}

int getModuleFogTopLevelTimesTen()
{
    return moduleFog()._top * 10;
}

void setModuleFogColor(int red, int green, int blue)
{
    fog_instance_t& fog = moduleFog();
    fog._red = Ego::Math::constrain(red, 0, 0xFF);
    fog._grn = Ego::Math::constrain(green, 0, 0xFF);
    fog._blu = Ego::Math::constrain(blue, 0, 0xFF);
}

void setModuleFogBottomLevel(int fogLevelTimesTen)
{
    fog_instance_t& fog = moduleFog();
    const float delta = (fogLevelTimesTen / 10.0f) - fog._bottom;
    fog._bottom += delta;
    fog._distance -= delta;
    fog._on = config().graphic_fog_enable.getValue();
    if (fog._distance < 1.0f)
    {
        fog._on = false;
    }
}

int getModuleFogBottomLevelTimesTen()
{
    return moduleFog()._bottom * 10;
}

bool tryGetModuleTileTypeAtPosition(const Ego::Vector2f& position, uint16_t& tileType)
{
    return activeModule().tryGetTileTypeAtPosition(position, tileType);
}

bool setModuleTileTypeAtPosition(const Ego::Vector2f& position, uint16_t tileType)
{
    return activeModule().setTileTypeAtPosition(position, tileType);
}

void markActiveModuleBeaten()
{
    activeModule().beatModule();
}

void setActiveModuleExportValid(bool valid)
{
    activeModule().setExportValid(valid);
}

void enableActiveModulePitsKill()
{
    activeModule().enablePitsKill();
}

void enableActiveModulePitsTeleport(const Ego::Vector3f& location)
{
    activeModule().enablePitsTeleport(location);
}

void giveGoodTeamExperience(int amount, XPType type)
{
    activeModule().giveTeamExperience(static_cast<TEAM_REF>(Team::TEAM_GOOD), amount, type);
}

bool tryAddActiveModuleIdsz(const IDSZ2& idsz)
{
    return ModuleProfile::moduleAddIDSZ(activeModule().getPath(), idsz);
}

bool setActorTileType(const Object& actor, uint16_t tileType)
{
    return activeModule().setTileType(actor.getTile(), tileType);
}

std::shared_ptr<PlayingState> tryPlayingState()
{
    return EngineContext::get().tryActivePlayingState();
}

std::shared_ptr<Ego::GUI::MiniMap> tryMiniMap()
{
    const std::shared_ptr<PlayingState> state = tryPlayingState();
    return state ? state->getMiniMap() : nullptr;
}

bool showMiniMap()
{
    const std::shared_ptr<Ego::GUI::MiniMap> minimap = tryMiniMap();
    if (!minimap)
    {
        return false;
    }

    const bool wasHidden = !minimap->isVisible();
    minimap->setVisible(true);
    return wasHidden;
}

void showMiniMapPlayerPosition()
{
    if (const std::shared_ptr<Ego::GUI::MiniMap> minimap = tryMiniMap())
    {
        minimap->setShowPlayerPosition(true);
    }
}

std::shared_ptr<Object> tryUiObject(ObjectRef objectRef)
{
    return tryObjectShared(objectRef);
}

void addMiniMapBlip(float x, float y, ObjectRef objectRef)
{
    const std::shared_ptr<Ego::GUI::MiniMap> minimap = tryMiniMap();
    const std::shared_ptr<Object> object = tryUiObject(objectRef);
    if (!minimap || !object)
    {
        return;
    }

    minimap->addBlip(x, y, object);
}

void addSelfMiniMapBlip(const ai_state_t& self, float x, float y)
{
    addMiniMapBlip(x, y, selfObjectRef(self));
}

void addSelfStatusMonitor(ObjectRef objectRef)
{
    const std::shared_ptr<PlayingState> state = tryPlayingState();
    const std::shared_ptr<Object> object = tryUiObject(objectRef);
    if (!state || !object)
    {
        return;
    }

    state->addStatusMonitor(object);
}

void clearEndMessageText()
{
    g_endText.setText("");
}

bool addEndMessageText(Object& object, int messageIndex, script_state_t& state)
{
    return ::AddEndMessage(&object, messageIndex, &state);
}

bool addSelfEndMessageText(const SelfCompatibilityContext& selfContext, int messageIndex, script_state_t& state)
{
    return selfContext.selfObject != nullptr &&
           addEndMessageText(*selfContext.selfObject, messageIndex, state);
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

void publishDeprecatedEnableListenSkillWarning(const SelfCompatibilityContext& context)
{
    logDeprecatedScriptFunctionUse("EnableListenSkill", context.className);
}

bool resolveFollowLinkRequest(const SelfCompatibilityContext& context,
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

void publishFollowLinkFailureMessage(const std::string& selfName)
{
    const std::string text = "That's too scary for " + selfName;
    if (const std::shared_ptr<PlayingState> playingState = EngineContext::get().tryActivePlayingState())
    {
        playingState->getMessageLog()->addMessage(text);
        return;
    }

    DisplayMsg_printf("%s", text.c_str());
}

bool tryFollowLink(const FollowLinkRequest& request)
{
    const bool followed = g_followLinkByModuleName(request.moduleName, true);
    if (!followed)
    {
        publishFollowLinkFailureMessage(request.selfName);
    }

    return followed;
}

bool followLinkFromMessageId(const SelfCompatibilityContext& context,
                             const int messageId)
{
    FollowLinkRequest request;
    return resolveFollowLinkRequest(context, messageId, request) &&
           tryFollowLink(request);
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

bool resolveInventoryCompatibilityContext(const ai_state_t& self,
                                          IInventoryHolder& actorInventory,
                                          InventoryCompatibilityContext& context)
{
    context.targetInventory = tryInventoryHolder(self.getTarget());
    context.actorInventory = &actorInventory;
    return context.targetInventory != nullptr;
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

bool resolveEnchantInvocationContext(const ai_state_t& self,
                                     ObjectRef targetRef,
                                     EnchantInvocationContext& context)
{
    context.target = tryEnchantable(targetRef);
    return context.target != nullptr &&
           resolveOwnedObjectHandle(self.owner, context.owner) &&
           resolveOwnedObjectHandle(self.getSelf(), context.spawner);
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

template <typename Fn>
void forEachResolvedObjectRef(Fn&& fn)
{
    ObjectHandler* handler = gameSession().tryObjectHandler();
    if (handler == nullptr)
    {
        return;
    }

    for (const std::shared_ptr<Object>& object : handler->iterator())
    {
        if (object != nullptr)
        {
            fn(object->getObjRef());
        }
    }
}

TargetEconomyCompatibilityContext makeTargetEconomyCompatibilityContext(Object& selfObject, const ai_state_t& self)
{
    TargetEconomyCompatibilityContext context;
    context.targetAppearance = tryAppearanceProfile(self.getTarget());
    context.selfWallet = static_cast<IWallet*>(&selfObject);
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

void configurePitFall(const Ego::Vector3f& location)
{
    if (activeModule().isInsidePitBounds(location.x(), location.y()))
    {
        enableActiveModulePitsTeleport(location);
        return;
    }

    enableActiveModulePitsKill();
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

    SCRIPT_FUNCTION_BEGIN();

    const TargetEconomyCompatibilityContext targetContext = makeTargetEconomyCompatibilityContext(*pchr, self);
    returncode = setTargetArmorPrice(state, targetContext);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinTargetTeam( script_state_t& state, ai_state_t& self )
{
    // JoinTargetTeam()
    /// @author ZZ
    /// @details This function lets a character join a different team.  Used
    /// mostly for pets

    SCRIPT_FUNCTION_BEGIN();

    returncode = false;
    const ITargetInfo* targetTeamInfo = tryTargetInfo(self.getTarget());
    ITeamMember& selfTeamMember = teamMember(*pchr);
    if ( targetTeamInfo != nullptr )
    {
        selfTeamMember.setTeam(targetTeamInfo->getTeamRef());
        returncode = true;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_OpenPassage( script_state_t& state, ai_state_t& self )
{
    // OpenPassage( tmpargument = "passage" )

    /// @author ZZ
    /// @details This function opens the passage specified by tmpargument, failing if the
    /// passage was already open.
    /// Passage areas are defined in passage.txt and set in spawn.txt for the given character

    SCRIPT_FUNCTION_BEGIN();

    const std::shared_ptr<Passage> passage = tryPassage(state.argument);
    
    returncode = false;
    if(passage) {
        returncode = true;
        passage->open();
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ClosePassage( script_state_t& state, ai_state_t& self )
{
    // ClosePassage( tmpargument = "passage" )
    /// @author ZZ
    /// @details This function closes the passage specified by tmpargument, proceeding
    /// if the passage isn't blocked.  Crushable characters within the passage
    /// are crushed.

    SCRIPT_FUNCTION_BEGIN();

    const std::shared_ptr<Passage> passage = tryPassage(state.argument);

    returncode = false;
    if(passage) {
        returncode = passage->close();
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfPassageOpen( script_state_t& state, ai_state_t& self )
{
    // IfPassageOpen( tmpargument = "passage" )
    /// @author ZZ
    /// @details This function proceeds if the given passage is valid and open to movement
    /// Used mostly by door characters to tell them when to run their open animation.

    SCRIPT_FUNCTION_BEGIN();

    const std::shared_ptr<Passage> passage = tryPassage(state.argument);

    returncode = false;
    if(passage) {
        returncode = passage->isOpen();
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_CostTargetItemID( script_state_t& state, ai_state_t& self )
{
    // CostTargetItemID( tmpargument = "idsz" )
    /// @author ZZ
    /// @details This function proceeds if the target has a matching held item or the
    /// actor has a matching pocket item, and poofs that item. This preserves the
    /// legacy actor-pocket compatibility behavior for one-use items such as keys.

    SCRIPT_FUNCTION_BEGIN();

    InventoryCompatibilityContext inventoryContext;
    IInventoryHolder& actorInventory = inventoryHolder(*pchr);
    if (!resolveInventoryCompatibilityContext(self, actorInventory, inventoryContext))
    {
        return false;
    }

    returncode = false;
    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);
    const ObjectRef itemRef = findMatchingTargetHeldOrActorPocketItemRef(inventoryContext, idsz);
    if (itemRef != ObjectRef::Invalid)
    {
        returncode = consumeOrPoofItemWithActorPocketCompatibility(itemRef, actorInventory);
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddIDSZ( script_state_t& state, ai_state_t& self )
{
    // AddIDSZ( tmpargument = "idsz" )
    /// @author ZZ
    /// @details This function slaps an expansion IDSZ onto the menu.txt file.
    /// Used to show completion of special quests for a given module

    SCRIPT_FUNCTION_BEGIN();

    if (tryAddActiveModuleIdsz(Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument)))
    {
        // invalidate any module list so that we will reload them
        //module_list_valid = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DamageTarget( script_state_t& state, ai_state_t& self )
{
    // DamageTarget( tmpargument = "damage" )
    /// @author ZZ
    /// @details This function applies little bit of love to the character's target.
    /// The amount is set in tmpargument

    IPair tmp_damage;

    SCRIPT_FUNCTION_BEGIN();

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

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfLeaderKilled( script_state_t& state, ai_state_t& self )
{
    // IfLeaderKilled()
    /// @author ZZ
    /// @details This function proceeds if the team's leader died this update

    SCRIPT_FUNCTION_BEGIN();

    returncode = HAS_SOME_BITS( self.alert, ALERTIF_LEADERKILLED );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_BecomeLeader( script_state_t& state, ai_state_t& self )
{
    // BecomeLeader()
    /// @author ZZ
    /// @details This function makes the character the leader of the team

    SCRIPT_FUNCTION_BEGIN();

    teamMember(*pchr).becomeTeamLeader();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ChangeTargetArmor( script_state_t& state, ai_state_t& self )
{
    // ChangeTargetArmor( tmpargument = "armor" )

    /// @author ZZ
    /// @details This function sets the target's armor type and returns the old type
    /// as tmpargument and the new type as tmpx

    SCRIPT_FUNCTION_BEGIN();

    const TargetEconomyCompatibilityContext targetContext = makeTargetEconomyCompatibilityContext(*pchr, self);
    returncode = changeTargetArmor(state, targetContext);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveMoneyToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveMoneyToTarget( tmpargument = "money" )
    /// @author ZZ
    /// @details This function increases the target's money, while decreasing the
    /// character's own money.  tmpargument is set to the amount transferred

    SCRIPT_FUNCTION_BEGIN();

    const TargetEconomyCompatibilityContext targetContext = makeTargetEconomyCompatibilityContext(*pchr, self);
    returncode = giveMoneyToTarget(state, targetContext);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfLeaderIsAlive( script_state_t& state, ai_state_t& self )
{
    // IfLeaderIsAlive()
    /// @author ZZ
    /// @details This function proceeds if the team has a leader

    SCRIPT_FUNCTION_BEGIN();

    returncode = ( teamLeaderRef(targetInfo(*pchr)) != ObjectRef::Invalid );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ChangeTile( script_state_t& state, ai_state_t& self )
{
    // ChangeTile( tmpargument = "tile type")
    /// @author ZZ
    /// @details This function changes the tile under the character to the new tile type,
    /// which is highly module dependent

    SCRIPT_FUNCTION_BEGIN();

    returncode = setActorTileType(*pchr, state.argument);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DropMoney( script_state_t& state, ai_state_t& self )
{
    // DropMoney( tmpargument = "money" )
    /// @author ZZ
    /// @details This function drops a certain amount of money, if the character has that
    /// much

    SCRIPT_FUNCTION_BEGIN();

    SelfCompatibilityContext selfContext = makeSelfCompatibilityContext(*pchr);
    returncode = dropMoney(state, selfContext.wallet);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_BecomeSpell( script_state_t& state, ai_state_t& self )
{
    // BecomeSpell()
    /// @author ZZ
    /// @details This function turns a spellbook character into a spell based on its
    /// content.
    /// TOO COMPLICATED TO EXPLAIN.  SHOULDN'T EVER BE NEEDED BY YOU.

    SCRIPT_FUNCTION_BEGIN();

    becomeSpell(enchantable(*pchr), morphControl(*pchr), ObjectProfileRef(self.content), self);

    SCRIPT_FUNCTION_END();
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

    SCRIPT_FUNCTION_BEGIN();

    SelfCompatibilityContext selfContext = makeSelfCompatibilityProfileContext(*pchr, *ppro);

    becomeSpellbook(enchantable(*pchr),
                    morphControl(*pchr),
                    animationControl(*pchr),
                    selfContext.policy.profileRef,
                    selfContext.policy.spellEffectSkin,
                    self);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetDamageType( script_state_t& state, ai_state_t& self )
{
    // SetDamageType( tmpargument = "damage type" )
    /// @author ZZ
    /// @details This function lets a weapon change the type of damage it inflicts

    SCRIPT_FUNCTION_BEGIN();

    SelfCompatibilityContext selfContext = makeSelfCompatibilityContext(*pchr);
    returncode = setSelfDamageType(selfContext, static_cast<DamageType>(state.argument % DAMAGE_COUNT));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetWaterLevel( script_state_t& state, ai_state_t& self )
{
    // SetWaterLevel( tmpargument = "level" )
    /// @author ZZ
    /// @details This function raises or lowers the water in the module

    SCRIPT_FUNCTION_BEGIN();

    setModuleWaterLevel(state.argument);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EnchantTarget( script_state_t& state, ai_state_t& self )
{
    // EnchantTarget()
    /// @author ZZ
    /// @details This function enchants the target with the enchantment given
    /// in enchant.txt. Make sure you use set_OwnerToTarget before doing this.

    SCRIPT_FUNCTION_BEGIN();

    SelfCompatibilityContext selfContext = makeSelfCompatibilityProfileContext(*pchr, *ppro);

    EnchantInvocationContext enchantContext;
    if (resolveEnchantInvocationContext(self, self.getTarget(), enchantContext))
    {
        returncode = enchantContext.target->addEnchant(selfContext.policy.enchantRef,
                                                       selfContext.policy.profileRef.get(),
                                                       enchantContext.owner.object,
                                                       enchantContext.spawner.object) != nullptr;
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EnchantChild( script_state_t& state, ai_state_t& self )
{
    // EnchantChild()
    /// @author ZZ
    /// @details This function can be used with SpawnCharacter to enchant the
    /// newly spawned character with the enchantment
    /// given in enchant.txt. Make sure you use set_OwnerToTarget before doing this.

    SCRIPT_FUNCTION_BEGIN();

    SelfCompatibilityContext selfContext = makeSelfCompatibilityProfileContext(*pchr, *ppro);

    EnchantInvocationContext enchantContext;
    if (resolveEnchantInvocationContext(self, self.child, enchantContext))
    {
        returncode = enchantContext.target->addEnchant(selfContext.policy.enchantRef,
                                                       selfContext.policy.profileRef.get(),
                                                       enchantContext.owner.object,
                                                       enchantContext.spawner.object) != nullptr;
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveExperienceToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveExperienceToTarget( tmpargument = "amount", tmpdistance = "type" )
    /// @author ZZ
    /// @details This function gives the target some experience, xptype from distance,
    /// amount from argument.

    SCRIPT_FUNCTION_BEGIN();

    ICharacterState* targetState = tryCharacterState(self.getTarget());
    if(targetState == nullptr) {
        return false;
    }

    targetState->giveExperience(state.argument, static_cast<XPType>(state.distance), false);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IncreaseAmmo( script_state_t& state, ai_state_t& self )
{
    // IncreaseAmmo()
    /// @author ZZ
    /// @details This function increases the character's ammo by 1

    SCRIPT_FUNCTION_BEGIN();
    ICharacterState& selfState = characterState(*pchr);
    if ( selfState.getAmmo() < selfState.getAmmoMax() )
    {
        selfState.setAmmo(selfState.getAmmo() + 1);
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_UnkurseTarget( script_state_t& state, ai_state_t& self )
{
    // UnkurseTarget()
    /// @author ZZ
    /// @details This function unkurses the target

    SCRIPT_FUNCTION_BEGIN();
    ICharacterState* targetState = tryCharacterState(self.getTarget());
    if (targetState == nullptr)
    {
        return false;
    }

    targetState->setKursed(false);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveExperienceToTargetTeam( script_state_t& state, ai_state_t& self )
{
    // GiveExperienceToTargetTeam( tmpargument = "amount", tmpdistance = "type" )
    /// @author ZZ
    /// @details This function gives experience to everyone on the target's team

    SCRIPT_FUNCTION_BEGIN();

    SelfCompatibilityContext selfContext = makeSelfCompatibilityContext(*pchr);
    returncode = giveSelfTeamExperience(state, selfContext);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_RestockTargetAmmoIDAll( script_state_t& state, ai_state_t& self )
{
    // RestockTargetAmmoIDAll( tmpargument = "idsz" )
    /// @author ZZ
    /// @details This function restocks matching ammo on the target's held items and
    /// the actor's pocket items, preserving the legacy target-held plus actor-pocket
    /// compatibility traversal.

    SCRIPT_FUNCTION_BEGIN();

    InventoryCompatibilityContext inventoryContext;
    if (!resolveInventoryCompatibilityContext(self, inventoryHolder(*pchr), inventoryContext))
    {
        return false;
    }

    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);
    const int iTmp = restockMatchingTargetHeldAndActorPocketAmmo(inventoryContext, idsz, false);

    state.argument = iTmp;
    returncode = ( iTmp != 0 );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_RestockTargetAmmoIDFirst( script_state_t& state, ai_state_t& self )
{
    // RestockTargetAmmoIDFirst( tmpargument = "idsz" )
    /// @author ZZ
    /// @details This function restocks the first matching item in the legacy target-held
    /// then actor-pocket traversal order.

    SCRIPT_FUNCTION_BEGIN();

    InventoryCompatibilityContext inventoryContext;
    if (!resolveInventoryCompatibilityContext(self, inventoryHolder(*pchr), inventoryContext))
    {
        return false;
    }

    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);
    const int iTmp = restockMatchingTargetHeldAndActorPocketAmmo(inventoryContext, idsz, true);

    state.argument = iTmp;
    returncode = ( iTmp != 0 );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_KillTarget( script_state_t& state, ai_state_t& self )
{
    // KillTarget()
    /// @author ZZ
    /// @details This function kills the target

    SCRIPT_FUNCTION_BEGIN();

    DamageInvocationContext damageContext;
    if (!resolveKillDamageContext(self, damageContext))
    {
        return false;
    }

    damageContext.damageable->kill(damageContext.source.object, false);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_UndoEnchant( script_state_t& state, ai_state_t& self )
{
    // UndoEnchant()
    /// @author ZZ
    /// @details This function removes the last enchantment spawned by the character,
    /// proceeding if an enchantment was removed

    SCRIPT_FUNCTION_BEGIN();

    std::shared_ptr<Ego::Enchantment> lastEnchant = enchantable(*pchr).getLastEnchantmentSpawned();
    if(lastEnchant == nullptr || lastEnchant->isTerminated()) {
        returncode = false;
    }
    else {
        returncode = true;
        lastEnchant->requestTerminate();
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetWaterLevel( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetWaterLevel()
    /// @author ZZ
    /// @details This function sets tmpargument to the current douse level for the water * 10.
    /// A waterlevel in wawalight of 85 would set tmpargument to 850

    SCRIPT_FUNCTION_BEGIN();

    state.argument = getModuleWaterLevelTimesTen();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_CostTargetMana( script_state_t& state, ai_state_t& self )
{
    // CostTargetMana( tmpargument = "amount" )
    /// @author ZZ
    /// @details This function costs the target a specific amount of mana, proceeding
    /// if the target was able to pay the price.  The amounts are 8.8 fixed point

    SCRIPT_FUNCTION_BEGIN();

    ICharacterState* targetState = tryCharacterState(self.getTarget());
    returncode = targetState ? targetState->costMana(state.argument, self.getSelf()) : false;

    SCRIPT_FUNCTION_END();
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

    SCRIPT_FUNCTION_BEGIN();

    HealingInvocationContext healingContext;
    if (!resolveSelfHealingContext(self, healingContext))
    {
        return false;
    }

    healingContext.damageable->heal(healingContext.healer.object, state.argument, true);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_Equip( script_state_t& state, ai_state_t& self )
{
    // Equip()
    /// @author ZZ
    /// @details This function flags the character as being equipped.
    /// This is used by equipment items when they are placed in the inventory

    SCRIPT_FUNCTION_BEGIN();

    SelfCompatibilityContext selfContext = makeSelfCompatibilityContext(*pchr);
    returncode = markSelfEquipped(selfContext);

    SCRIPT_FUNCTION_END();
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

    SCRIPT_FUNCTION_BEGIN();

    returncode = ::BreakPassage( state.y, state.x, state.distance, state.turn, ( PASS_REF )state.argument, &( state.x ), &( state.y ) );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ChangeArmor( script_state_t& state, ai_state_t& self )
{
    // ChangeArmor( tmpargument = "time" )
    /// @author ZZ
    /// @details This function changes the character's armor.
    /// Sets tmpargument as the old type and tmpx as the new type

    SCRIPT_FUNCTION_BEGIN();

    SelfCompatibilityContext selfContext = makeSelfCompatibilityContext(*pchr);
    returncode = changeSelfArmor(state, selfContext);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveStrengthToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveStrengthToTarget(argument = "amount")
    // Permanently boost the target's strength

    SCRIPT_FUNCTION_BEGIN();
    if ( ICharacterState* resolvedTargetState = resolveAliveTargetState(self) )
    {
        resolvedTargetState->increaseBaseAttribute(Ego::Attribute::MIGHT, FP8_TO_FLOAT(state.argument));
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveIntelligenceToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveIntelligenceToTarget(tmpargument = "amount")
    // Permanently boost the target's intelligence

    SCRIPT_FUNCTION_BEGIN();
    if ( ICharacterState* resolvedTargetState = resolveAliveTargetState(self) )
    {
        resolvedTargetState->increaseBaseAttribute(Ego::Attribute::INTELLECT, FP8_TO_FLOAT(state.argument));
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveDexterityToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveDexterityToTarget(tmpargument = "amount")
    // Permanently boost the target's dexterity

    SCRIPT_FUNCTION_BEGIN();
    if ( ICharacterState* resolvedTargetState = resolveAliveTargetState(self) )
    {
        resolvedTargetState->increaseBaseAttribute(Ego::Attribute::AGILITY, FP8_TO_FLOAT(state.argument));
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveLifeToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveLifeToTarget(tmpargument = "amount")
    /// @author ZZ
    /// @details Permanently boost the target's life

    SCRIPT_FUNCTION_BEGIN();
    HealingInvocationContext healingContext;
    if (resolveAliveTargetHealingContext(self, healingContext))
    {
        healingContext.targetState->increaseBaseAttribute(Ego::Attribute::MAX_LIFE,
                                                          FP8_TO_FLOAT(state.argument));
        healingContext.damageable->heal(healingContext.healer.object, state.argument, true);
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveManaToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveManaToTarget(tmpargument = "amount")
    /// @author ZZ
    /// @details Permanently boost the target's mana

    SCRIPT_FUNCTION_BEGIN();
    if ( ICharacterState* resolvedTargetState = resolveAliveTargetState(self) )
    {
        resolvedTargetState->increaseBaseAttribute(Ego::Attribute::MAX_MANA, FP8_TO_FLOAT(state.argument));
        resolvedTargetState->costMana(-state.argument, ObjectRef::Invalid);
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ShowMap( script_state_t& state, ai_state_t& self )
{
    // ShowMap()
    /// @author ZZ
    /// @details This function shows the module's map.
    /// Fails if map already visible

    SCRIPT_FUNCTION_BEGIN();
    returncode = showMiniMap();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ShowYouAreHere( script_state_t& state, ai_state_t& self )
{
    // ShowYouAreHere()
    /// @author ZZ
    /// @details This function shows the blinking white blip on the map that represents the
    /// camera location

    SCRIPT_FUNCTION_BEGIN();

    showMiniMapPlayerPosition();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ShowBlipXY( script_state_t& state, ai_state_t& self )
{
    // ShowBlipXY( tmpx = "x", tmpy = "y", tmpargument = "color" )

    /// @author ZZ
    /// @details This function draws a blip on the map, and must be done each update
    SCRIPT_FUNCTION_BEGIN();

    // Add a blip
    if ( state.argument >= 0 )
    {
        addSelfMiniMapBlip(self, state.x, state.y);
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_HealTarget( script_state_t& state, ai_state_t& self )
{
    // HealTarget( tmpargument = "amount" )
    /// @author ZZ
    /// @details This function gives some life back to the target.
    /// Values are 8.8 fixed point. Any enchantments that are removed by [HEAL], like poison, go away

    SCRIPT_FUNCTION_BEGIN();

    HealingInvocationContext healingContext;
    if (!resolveHealingTargetContext(self, healingContext))
    {
        return false;
    }

    returncode = false;
    if (healingContext.damageable->heal(healingContext.healer.object, state.argument, false))
    {
        returncode = true;
        healingContext.targetState->removeEnchantsWithIDSZ(IDSZ2('H', 'E', 'A', 'L'));
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PumpTarget( script_state_t& state, ai_state_t& self )
{
    // PumpTarget( tmpargument = "amount" )
    /// @author ZZ
    /// @details This function gives some mana back to the target.
    /// Values are 8.8 fixed point

    SCRIPT_FUNCTION_BEGIN();
    pumpTargetManaFromSelf(self, state.argument);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_CostAmmo( script_state_t& state, ai_state_t& self )
{
    // CostAmmo()
    /// @author ZZ
    /// @details This function costs the character 1 point of ammo

    SCRIPT_FUNCTION_BEGIN();
    ICharacterState& selfState = characterState(*pchr);
    if ( selfState.getAmmo() > 0 )
    {
        selfState.setAmmo(selfState.getAmmo() - 1);
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetFogLevel( script_state_t& state, ai_state_t& self )
{
    // SetFogLevel( tmpargument = "level" )
    /// @author ZZ
    /// @details This function sets the level of the module's fog.
    /// Values are * 10
    /// !!BAD!! DOESN'T WORK !!BAD!!

    SCRIPT_FUNCTION_BEGIN();

    setModuleFogTopLevel(state.argument);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetFogLevel( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetFogLevel()
    /// @author ZZ
    /// @details This function sets tmpargument to the level of the module's fog.
    /// Values are * 10

    SCRIPT_FUNCTION_BEGIN();

    state.argument = getModuleFogTopLevelTimesTen();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetFogTAD( script_state_t& state, ai_state_t& self )
{
    /// @author ZZ
    /// @details This function sets the color of the module's fog.
    /// TAD stands for <turn, argument, distance> == <red, green, blue>.
    /// Makes sense, huh?
    /// !!BAD!! DOESN'T WORK !!BAD!!

    SCRIPT_FUNCTION_BEGIN();

    setModuleFogColor(state.turn, state.argument, state.distance);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetFogBottomLevel( script_state_t& state, ai_state_t& self )
{
    // SetFogBottomLevel( tmpargument = "level" )

    /// @author ZZ
    /// @details This function sets the level of the module's fog.
    /// Values are * 10

    SCRIPT_FUNCTION_BEGIN();

    setModuleFogBottomLevel(state.argument);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetFogBottomLevel( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetFogBottomLevel()

    /// @author ZZ
    /// @details This function sets tmpargument to the level of the module's fog.
    /// Values are * 10

    SCRIPT_FUNCTION_BEGIN();

    state.argument = getModuleFogBottomLevelTimesTen();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetTileXY( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetTileXY( tmpx = "x", tmpy = "y" )
    /// @author ZZ
    /// @details This function sets tmpargument to the tile type at the specified
    /// coordinates

    SCRIPT_FUNCTION_BEGIN();

    returncode = false;
    uint16_t tileType = 0;
    if (tryGetModuleTileTypeAtPosition(Ego::Vector2f(float(state.x), float(state.y)), tileType))
    {
        returncode = true;
        state.argument = tileType;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTileXY( script_state_t& state, ai_state_t& self )
{
    // SetTileXY( tmpargument = "tile type", tmpx = "x", tmpy = "y" )
    /// @author ZZ
    /// @details This function changes the tile type at the specified coordinates

    SCRIPT_FUNCTION_BEGIN();

    returncode = setModuleTileTypeAtPosition(Ego::Vector2f(float(state.x), float(state.y)),
                                             state.argument);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfCharacterWasABook( script_state_t& state, ai_state_t& self )
{
    // IfCharacterWasABook()
    /// @author ZZ
    /// @details This function proceeds if the base model is the same as the current
    /// model or if the base model is SPELLBOOK
    /// USAGE: USED BY THE MORPH SPELL. Not much use elsewhere

    SCRIPT_FUNCTION_BEGIN();

    SelfCompatibilityContext selfContext = makeSelfCompatibilityProfileContext(*pchr, *ppro);

    returncode = ( selfContext.policy.comparison.baseModelIsSpellbook ||
                   selfContext.policy.comparison.currentProfileMatchesBaseModel );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetEnchantBoostValues( script_state_t& state, ai_state_t& self )
{
    // SetEnchantBoostValues( tmpargument = "owner mana regen", tmpdistance = "owner life regen", tmpx = "target mana regen", tmpy = "target life regen" )
    /// @author ZZ
    /// @details This function sets the mana and life drains for the last enchantment
    /// spawned by this character.
    /// Values are 8.8 fixed point

    SCRIPT_FUNCTION_BEGIN();

    returncode = false;
    if (enchantable(*pchr).hasActiveEnchants()) {
        const std::shared_ptr<Ego::Enchantment> enchant = enchantable(*pchr).getFirstActiveEnchant();
        if(enchant != nullptr && !enchant->isTerminated()) {
            enchant->setBoostValues(FP8_TO_FLOAT(state.argument), FP8_TO_FLOAT(state.distance), FP8_TO_FLOAT(state.x), FP8_TO_FLOAT(state.y));
            returncode = true;            
        }
    }

    SCRIPT_FUNCTION_END();
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

    SCRIPT_FUNCTION_BEGIN();

    const auto profileID = ObjectProfileRef(static_cast<PRO_REF>(state.argument));

    /// @details This function polymorphs a character permanently so that it can be exported properly
    /// A character turned into a frog with this function will also export as a frog!
    if(EngineContext::get().profileSystem().isLoaded(profileID)) 
    {
        IMorphControl& targetMorph = morphControl(*pchr);

        //Change the object
        targetMorph.polymorphObject(ObjectProfileRef(profileID), 0);

        // set the base model to the new model, too
        targetMorph.setBaseModelRef(profileID);

        returncode = true;
    }
    else {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_UnkurseTargetInventory( script_state_t& state, ai_state_t& self )
{
    // UnkurseTargetInventory()
    /// @author ZZ
    /// @details This function preserves the legacy compatibility behavior: unkurse the
    /// target's held items plus the actor's pocket items, but not the target's pockets.

    SCRIPT_FUNCTION_BEGIN();

    InventoryCompatibilityContext inventoryContext;
    if (!resolveInventoryCompatibilityContext(self, inventoryHolder(*pchr), inventoryContext))
    {
        return false;
    }

    unkurseTargetHeldAndActorPocketItems(inventoryContext);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_FlashPassage( script_state_t& state, ai_state_t& self )
{
    // FlashPassage( tmpargument = "passage", tmpdistance = "color" )

    /// @author ZZ
    /// @details This function makes the given passage light or dark.
    /// Usage: For debug purposes

    SCRIPT_FUNCTION_BEGIN();

    const std::shared_ptr<Passage> passage = tryPassage(state.argument);
    if(passage) {
        passage->flashColor(state.distance);
    }

    SCRIPT_FUNCTION_END();
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

    SCRIPT_FUNCTION_BEGIN();

    returncode = ::FindTileInPassage( state.x, state.y, state.distance, static_cast<PASS_REF>(state.argument), &(state.x), &(state.y) );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_BeatModule( script_state_t& state, ai_state_t& self )
{
    // BeatModule()
    /// @author ZZ
    /// @details This function displays the Module Ended message

    SCRIPT_FUNCTION_BEGIN();

    markActiveModuleBeaten();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EndModule( script_state_t& state, ai_state_t& self )
{
    // EndModule()
    /// @author ZZ
    /// @details This function presses the Escape key

    SCRIPT_FUNCTION_BEGIN();

    // This tells the game to quit
    engine().pushGameState(std::make_shared<VictoryScreen>(nullptr, true));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisableExport( script_state_t& state, ai_state_t& self )
{
    // DisableExport()
    /// @author ZZ
    /// @details This function turns export off

    SCRIPT_FUNCTION_BEGIN();

    setActiveModuleExportValid(false);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EnableExport( script_state_t& state, ai_state_t& self )
{
    // EnableExport()
    /// @author ZZ
    /// @details This function turns export on

    SCRIPT_FUNCTION_BEGIN();

    setActiveModuleExportValid(true);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DropTargetMoney( script_state_t& state, ai_state_t& self )
{
    // DropTargetMoney( tmpargument = "amount" )
    /// @author ZZ
    /// @details This function drops some of the target's money

    SCRIPT_FUNCTION_BEGIN();

    const TargetEconomyCompatibilityContext targetContext = makeTargetEconomyCompatibilityContext(*pchr, self);
    returncode = dropMoney(state, targetContext.targetWallet);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinTeam( script_state_t& state, ai_state_t& self )
{
    // JoinTeam( tmpargument = "team" )
    /// @author ZZ
    /// @details This makes the character itself join a specified team (A = 0, B = 1, 23 = Z, etc.)

    SCRIPT_FUNCTION_BEGIN();

    SelfCompatibilityContext selfContext = makeSelfCompatibilityContext(*pchr);
    returncode = setSelfTeam(selfContext, static_cast<TEAM_REF>(state.argument));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_TargetJoinTeam( script_state_t& state, ai_state_t& self )
{
    // TargetJoinTeam( tmpargument = "team" )
    /// @author ZZ
    /// @details This makes the Target join a Team specified in tmpargument (A = 0, 25 = Z, etc.)

    SCRIPT_FUNCTION_BEGIN();

    ITeamMember* targetTeamMember = tryTeamMember(self.getTarget());
    if(targetTeamMember) {
        targetTeamMember->setTeam(static_cast<TEAM_REF>(state.argument));
        returncode = true;
    }
    else {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ClearEndMessage( script_state_t& state, ai_state_t& self )
{
    // ClearEndMessage()
    /// @author ZZ
    /// @details This function empties the end-module text buffer

    SCRIPT_FUNCTION_BEGIN();

    clearEndMessageText();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddEndMessage( script_state_t& state, ai_state_t& self )
{
    // AddEndMessage( tmpargument = "message" )
    /// @author ZZ
    /// @details This function appends a message to the end-module text buffer

    SCRIPT_FUNCTION_BEGIN();

    SelfCompatibilityContext selfContext = makeSelfCompatibilityContext(*pchr);
    returncode = addSelfEndMessageText(selfContext, state.argument, state);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddStat( script_state_t& state, ai_state_t& self )
{
    // AddStat()
    /// @author ZZ
    /// @details This function turns on an NPC's status display

    SCRIPT_FUNCTION_BEGIN();

    addSelfStatusMonitor(self.getSelf());

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisenchantTarget( script_state_t& state, ai_state_t& self )
{
    // DisenchantTarget()
    /// @author ZZ
    /// @details This function removes all enchantments on the Target character, proceeding
    /// if there were any, failing if not

    SCRIPT_FUNCTION_BEGIN();

    IEnchantable* targetEnchantable = tryEnchantable(self.getTarget());
    returncode = targetEnchantable ? targetEnchantable->disenchant() : false;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisenchantAll( script_state_t& state, ai_state_t& self )
{
    // DisenchantAll()
    /// @author ZZ
    /// @details This function removes all enchantments in the game

    SCRIPT_FUNCTION_BEGIN();

    forEachResolvedObjectRef([](ObjectRef objectRef)
    {
        if (IEnchantable* objectEnchantable = tryEnchantable(objectRef))
        {
            objectEnchantable->disenchant();
        }
    });

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddShopPassage( script_state_t& state, ai_state_t& self )
{
    // AddShopPassage( tmpargument = "passage" )
    /// @author ZZ
    /// @details This function makes a passage behave as a shop area, as long as the
    /// character is alive.

    SCRIPT_FUNCTION_BEGIN();

    const std::shared_ptr<Passage> passage = tryPassage(state.argument);
    if(passage) {
        passage->makeShop(self.getSelf());
        returncode = true;
    }
    else {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
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

    SCRIPT_FUNCTION_BEGIN();

    const TargetEconomyCompatibilityContext targetContext = makeTargetEconomyCompatibilityContext(*pchr, self);
    returncode = chargeTargetArmor(state, targetContext);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinEvilTeam( script_state_t& state, ai_state_t& self )
{
    // JoinEvilTeam()
    /// @author ZZ
    /// @details This function adds the character to the evil Team.

    SCRIPT_FUNCTION_BEGIN();

    SelfCompatibilityContext selfContext = makeSelfCompatibilityContext(*pchr);
    returncode = setSelfTeam(selfContext, static_cast<TEAM_REF>(Team::TEAM_EVIL));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinNullTeam( script_state_t& state, ai_state_t& self )
{
    // JoinNullTeam()
    /// @author ZZ
    /// @details This function adds the character to the null Team.

    SCRIPT_FUNCTION_BEGIN();

    SelfCompatibilityContext selfContext = makeSelfCompatibilityContext(*pchr);
    returncode = setSelfTeam(selfContext, static_cast<TEAM_REF>(Team::TEAM_NULL));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinGoodTeam( script_state_t& state, ai_state_t& self )
{
    // JoinGoodTeam()
    /// @author ZZ
    /// @details This function adds the character to the good Team.

    SCRIPT_FUNCTION_BEGIN();

    SelfCompatibilityContext selfContext = makeSelfCompatibilityContext(*pchr);
    returncode = setSelfTeam(selfContext, static_cast<TEAM_REF>(Team::TEAM_GOOD));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PitsKill( script_state_t& state, ai_state_t& self )
{
    // PitsKill()
    /// @author ZZ
    /// @details This function activates pit deaths for when characters fall below a
    /// certain altitude.

    SCRIPT_FUNCTION_BEGIN();

    enableActiveModulePitsKill();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveExperienceToGoodTeam( script_state_t& state, ai_state_t& self )
{
    // GiveExperienceToGoodTeam(  tmpargument = "amount", tmpdistance = "type" )
    /// @author ZZ
    /// @details This function gives experience to everyone on the G Team

    SCRIPT_FUNCTION_BEGIN();

    if(state.distance < XP_COUNT)
    {
        giveGoodTeamExperience(state.argument, static_cast<XPType>(state.distance));
    }


    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GrogTarget( script_state_t& state, ai_state_t& self )
{
    // GrogTarget( tmpargument = "amount" )
    /// @author ZF
    /// @details This function grogs the Target for a duration equal to tmpargument

    SCRIPT_FUNCTION_BEGIN();
    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    ICharacterState* targetState = tryCharacterState(self.getTarget());
    if (targetInfo == nullptr || targetState == nullptr)
    {
        return false;
    }

    returncode = false;
    if ( targetInfo->canBeGrogged() )
    {
        int timer_val = targetState->getGrogTimer() + state.argument;
        targetState->setGrogTimer(std::max(0, timer_val));
        returncode = true;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DazeTarget( script_state_t& state, ai_state_t& self )
{
    // DazeTarget( tmpargument = "amount" )
    /// @author ZF
    /// @details This function dazes the Target for a duration equal to tmpargument

    SCRIPT_FUNCTION_BEGIN();
    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    ICharacterState* targetState = tryCharacterState(self.getTarget());
    if (targetInfo == nullptr || targetState == nullptr)
    {
        return false;
    }

    // Characters who manage to daze themselves are to ignore their daze immunity
    returncode = false;
    if ( targetInfo->canBeDazed() || self.getSelf() == self.getTarget() )
    {
        int timer_val = targetState->getDazeTimer() + state.argument;
        targetState->setDazeTimer(std::max(0, timer_val));

        returncode = true;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EnableListenSkill( script_state_t& state, ai_state_t& self )
{
    // EnableListenSkill()
    /// @author ZF
    /// @details This function increases range from which sound can be heard by 33%

    SCRIPT_FUNCTION_BEGIN();

    SelfCompatibilityContext selfContext = makeSelfCompatibilityProfileContext(*pchr, *ppro);

    publishDeprecatedEnableListenSkillWarning(selfContext);
    returncode = false;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_FollowLink( script_state_t& state, ai_state_t& self )
{
    // FollowLink( tmpargument = "index of next module name" )
    /// @author BB
    /// @details Skips to the next module!

    SCRIPT_FUNCTION_BEGIN();

    SelfCompatibilityContext selfContext = makeSelfCompatibilityProfileContext(*pchr, *ppro);

    returncode = followLinkFromMessageId(selfContext, state.argument);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddQuest( script_state_t& state, ai_state_t& self )
{
    // AddQuest( tmpargument = "quest idsz" )
    /// @author ZF
    /// @details This function adds a quest idsz set in tmpargument into the targets quest.txt to 0

    SCRIPT_FUNCTION_BEGIN();

    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);
    Ego::QuestLog* questLog = resolvedTargetQuestLog(self);
    returncode = questLog != nullptr && addQuestIfMissing(*questLog, idsz, state.distance);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_BeatQuestAllPlayers( script_state_t& state, ai_state_t& self )
{
    // BeatQuestAllPlayers()
    /// @author ZF
    /// @details This function marks a IDSZ in the targets quest.txt as beaten
    ///               returns true if at least one quest got marked as beaten.

    SCRIPT_FUNCTION_BEGIN();

    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);

    returncode = updatePlayerQuestLogs([&](Ego::QuestLog& questLog) { return beatActiveQuest(questLog, idsz); });

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetQuestLevel( script_state_t& state, ai_state_t& self )
{
    // SetQuestLevel( tmpargument = "idsz", distance = "adjustment" )
    /// @author ZF
    /// @details This function modifies the quest level for a specific quest IDSZ
    /// tmpargument specifies quest idsz (tmpargument) and the adjustment (tmpdistance, which may be negative)

    SCRIPT_FUNCTION_BEGIN();

    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);
    Ego::QuestLog* questLog = resolvedTargetQuestLog(self);
    returncode = questLog != nullptr && adjustActiveQuestLevel(*questLog, idsz, state.distance);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddQuestAllPlayers( script_state_t& state, ai_state_t& self )
{
    // AddQuestAllPlayers( tmpargument = "quest idsz" )
    /// @author ZF
    /// @details This function adds a quest idsz set in tmpargument into all local player's quest logs
    /// The quest level Is set to tmpdistance if the level Is not already higher

    SCRIPT_FUNCTION_BEGIN();

    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);
    returncode = updatePlayerQuestLogs([&](Ego::QuestLog& questLog)
    {
        return raiseQuestLevelIfHigher(questLog, idsz, state.distance);
    });

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddBlipAllEnemies( script_state_t& state, ai_state_t& self )
{
    // AddBlipAllEnemies()
    /// @author ZF
    /// @details show all enemies on the minimap who match the IDSZ given in tmpargument
    /// it show only the enemies of the AI Target

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo != nullptr)
    {
        publishEnemySense(EnemySenseState(targetInfo->getTeamRef(), state.argument));
    }
    else
    {
        resetEnemySense();
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PitsFall( script_state_t& state, ai_state_t& self )
{
    // PitsFall( tmpx = "teleprt x", tmpy = "teleprt y", tmpdistance = "teleprt z" )
    /// @author ZF
    /// @details This function activates pit teleportation.

    SCRIPT_FUNCTION_BEGIN();

    configurePitFall(Ego::Vector3f(static_cast<float>(state.x),
                                   static_cast<float>(state.y),
                                   static_cast<float>(state.distance)));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveManaFlowToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveManaFlowToTarget()
    /// @author ZF
    /// @details Permanently boost the target's mana flow

    SCRIPT_FUNCTION_BEGIN();
    if ( ICharacterState* resolvedTargetState = resolveAliveTargetState(self) )
    {
        resolvedTargetState->increaseBaseAttribute(Ego::Attribute::SPELL_POWER, FP8_TO_FLOAT(state.argument));
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveManaReturnToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveManaReturnToTarget()
    /// @author ZF
    /// @details Permanently boost the target's mana return

    SCRIPT_FUNCTION_BEGIN();
    if ( ICharacterState* resolvedTargetState = resolveAliveTargetState(self) )
    {
        resolvedTargetState->increaseBaseAttribute(Ego::Attribute::MANA_REGEN, FP8_TO_FLOAT(state.argument));
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetMoney( script_state_t& state, ai_state_t& self )
{
    // SetMoney()
    /// @author ZF
    /// @details Permanently sets the money for the character to tmpargument

    SCRIPT_FUNCTION_BEGIN();

    SelfCompatibilityContext selfContext = makeSelfCompatibilityContext(*pchr);
    returncode = setSelfMoney(state, selfContext);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DispelTargetEnchantID( script_state_t& state, ai_state_t& self )
{
    // DispelEnchantID( tmpargument = "idsz" )
    /// @author ZF
    /// @details This function removes all enchants from the target who match the specified RemovedByIDSZ

    SCRIPT_FUNCTION_BEGIN();
    returncode = false;
    if ( ICharacterState* resolvedTargetState = resolveAliveTargetState(self) )
    {
        // Check all enchants to see if they are removed
        resolvedTargetState->removeEnchantsWithIDSZ(Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument));
        returncode = true;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_KurseTarget( script_state_t& state, ai_state_t& self )
{
    // KurseTarget()
    /// @author ZF
    /// @details This makes the target kursed

    SCRIPT_FUNCTION_BEGIN();
    IInventoryHolder* targetInventory = tryInventoryHolder(self.getTarget());
    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    ICharacterState* targetState = tryCharacterState(self.getTarget());
    returncode = false;
    if ( targetInventory != nullptr && targetInfo != nullptr && targetState != nullptr &&
         targetInventory->isItem() && !targetInfo->isKursed() )
    {
        targetState->setKursed(true);
        returncode = true;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetAmmo( script_state_t& state, ai_state_t& self )
{
    // SetTargetAmmo( tmpargument = "ammo" )
    /// @author ZF
    /// @details This function sets the ammo of the character's current AI target

    SCRIPT_FUNCTION_BEGIN();
    ICharacterState* targetState = tryCharacterState(self.getTarget());
    if (targetState == nullptr)
    {
        return false;
    }

    targetState->setAmmo(std::min( state.argument, (int)targetState->getAmmoMax() ));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_TargetDamageSelf( script_state_t& state, ai_state_t& self )
{
    // TargetDamageSelf( tmpargument = "damage" )
    /// @author ZF
    /// @details This function applies little bit of hate from the character's target to
    /// the character itself. The amount is set in tmpargument

    IPair tmp_damage;

    SCRIPT_FUNCTION_BEGIN();

    DamageInvocationContext damageContext;
    if (!resolveRetaliationDamageContext(self, damageContext))
    {
        return false;
    }

    tmp_damage.base = state.argument;
    tmp_damage.rand = 1;

    damageContext.damageable->damage(ATK_FRONT, tmp_damage,
                                     static_cast<DamageType>(state.distance),
                                     damageContext.teamRef, damageContext.source.object,
                                     false, false, true);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveSkillToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveSkillToTarget( tmpargument = "skill_IDSZ" )
    /// @author ZF
    /// @details This function permanently gives the target character a Perk

    SCRIPT_FUNCTION_BEGIN();

    ICharacterState* targetState = tryCharacterState(self.getTarget());
    if (targetState == nullptr)
    {
        return false;
    }

    maybeAddSkillPerk(*targetState, state.argument);

    SCRIPT_FUNCTION_END();
}
