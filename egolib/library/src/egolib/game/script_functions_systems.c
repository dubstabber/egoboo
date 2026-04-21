/// @file egolib/game/script_functions_systems.c
/// @brief Passages, quests, commerce, teams, combat, enchantment, inventory, stats, and environment

#include "egolib/game/script_functions_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

namespace
{
egoboo_config_t& config()
{
    return EngineContext::get().config();
}

const IDamageable& damageableRole(const Object& object)
{
    return object;
}

IAppearanceProfile& appearanceProfile(Object& object)
{
    return object;
}

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

IWallet& wallet(Object& object)
{
    return object;
}

IMorphControl& morphControl(Object& object)
{
    return object;
}

ObjectRef leaderRef(const ITargetInfo& selfInfo)
{
    return activeModule().getTeamLeaderRef(selfInfo.getTeamRef());
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

ObjectRef findMatchingHeldOrInventoryItemRef(const IInventoryHolder& holder, const IDSZ2& idsz)
{
    const std::array<slot_t, 2> heldSlots = {SLOT_LEFT, SLOT_RIGHT};
    for (const slot_t heldSlot : heldSlots)
    {
        const ObjectRef heldObjectRef = holder.getHeldObject(heldSlot);
        if (itemMatchesType(heldObjectRef, idsz))
        {
            return heldObjectRef;
        }
    }

    for (const ObjectRef inventoryItemRef : holder.getInventoryItemRefs())
    {
        if (itemMatchesType(inventoryItemRef, idsz))
        {
            return inventoryItemRef;
        }
    }

    return ObjectRef::Invalid;
}

void removeInventoryItemRefIfPresent(IInventoryHolder& holder, ObjectRef itemRef)
{
    for (size_t slot = 0; slot < holder.getInventoryMaxItems(); ++slot)
    {
        if (holder.getInventoryItemRef(slot) == itemRef)
        {
            Inventory::remove_item(holder, slot, true);
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

bool consumeOrPoofItemWithLegacyInventoryPath(ObjectRef itemRef, IInventoryHolder& selfInventory)
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
        removeInventoryItemRefIfPresent(selfInventory, itemRef);
    }
    else
    {
        itemLifecycle->detachFromHolder(true, false);
    }

    itemLifecycle->requestTerminate();
    return true;
}

std::shared_ptr<Ego::Player> targetPlayer(const ITargetInfo& target)
{
    if (!target.isPlayer())
    {
        return nullptr;
    }

    const size_t playerIndex = target.getPlayerNumber();
    const auto& playerList = activeModule().getPlayerList();
    if (playerIndex >= playerList.size())
    {
        return nullptr;
    }

    return playerList[playerIndex];
}

std::shared_ptr<Ego::Player> resolvedTargetPlayer(const ai_state_t& self)
{
    const ITargetInfo* target = tryTargetInfo(self.getTarget());
    return target != nullptr ? targetPlayer(*target) : nullptr;
}

Ego::QuestLog* resolvedTargetQuestLog(const ai_state_t& self)
{
    const std::shared_ptr<Ego::Player> player = resolvedTargetPlayer(self);
    return player != nullptr ? &player->getQuestLog() : nullptr;
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

ICharacterState* resolveAliveTargetState(const ai_state_t& self)
{
    const ITargetInfo* resolvedTargetInfo = tryTargetInfo(self.getTarget());
    ICharacterState* resolvedTargetState = tryCharacterState(self.getTarget());
    return resolvedTargetInfo != nullptr &&
           resolvedTargetState != nullptr &&
           resolvedTargetInfo->isAlive() ? resolvedTargetState : nullptr;
}

bool resolveDamageableTarget(const ai_state_t& self,
                             IDamageable*& damageableTarget)
{
    damageableTarget = tryDamageable(self.getTarget());
    return damageableTarget != nullptr;
}

bool resolveSelfDamageable(const ai_state_t& self,
                           IDamageable*& damageableSelf)
{
    damageableSelf = tryDamageable(self.getSelf());
    return damageableSelf != nullptr;
}

bool resolveAliveTargetStateAndDamageable(const ai_state_t& self,
                                          ICharacterState*& resolvedTargetState,
                                          IDamageable*& resolvedDamageable)
{
    resolvedTargetState = resolveAliveTargetState(self);
    resolvedDamageable = tryDamageable(self.getTarget());
    return resolvedTargetState != nullptr && resolvedDamageable != nullptr;
}

bool resolveHealingTarget(const ai_state_t& self,
                          ICharacterState*& targetState,
                          IDamageable*& damageableTarget)
{
    targetState = tryCharacterState(self.getTarget());
    damageableTarget = tryDamageable(self.getTarget());
    return targetState != nullptr && damageableTarget != nullptr;
}

bool resolveRetaliationTarget(const ai_state_t& self,
                              const ITargetInfo*& targetInfo,
                              IDamageable*& damageableSelf)
{
    targetInfo = tryTargetInfo(self.getTarget());
    damageableSelf = tryDamageable(self.getSelf());
    return targetInfo != nullptr && damageableSelf != nullptr;
}

bool resolveEnchantParticipants(const ai_state_t& self,
                                ObjectRef enchantedRef,
                                IEnchantable*& enchantedTarget,
                                ObjectRef& ownerRef)
{
    enchantedTarget = tryEnchantable(enchantedRef);
    ownerRef = self.owner;
    return enchantedTarget != nullptr && tryObject(ownerRef) != nullptr;
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
    for (const std::shared_ptr<Ego::Player>& player : activeModule().getPlayerList())
    {
        if (player != nullptr && fn(player->getQuestLog()))
        {
            updated = true;
        }
    }

    return updated;
}

IAppearanceProfile* resolvedTargetAppearance(const ai_state_t& self)
{
    return tryAppearanceProfile(self.getTarget());
}

struct AppearanceWalletTarget
{
    IAppearanceProfile* appearance;
    IWallet* wallet;
};

AppearanceWalletTarget resolvedTargetAppearanceWallet(const ai_state_t& self)
{
    return {tryAppearanceProfile(self.getTarget()), tryWallet(self.getTarget())};
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
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetTargetArmorPrice( script_state_t& state, ai_state_t& self )
{
    // tmpx = GetTargetArmorPrice( tmpargument = "skin" )
    /// @author ZZ
    /// @details This function returns the cost of the desired skin upgrade, setting
    /// tmpx to the price

    SCRIPT_FUNCTION_BEGIN();

    IAppearanceProfile* targetAppearance = tryAppearanceProfile(self.getTarget());
    if (targetAppearance == nullptr)
    {
        return false;
    }

    int value = targetAppearance->getSkinCost(Ego::Script::Interpreter::safeCast<size_t>(state.argument));

    if ( value > 0 )
    {
        state.x  = value;
        returncode = true;
    }
    else
    {
        state.x  = 0;
        returncode = false;
    }

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

    std::shared_ptr<Passage> passage = activeModule().getPassageByID(state.argument);
    
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

    std::shared_ptr<Passage> passage = activeModule().getPassageByID(state.argument);

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

    std::shared_ptr<Passage> passage = activeModule().getPassageByID(state.argument);

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
    /// @details This function proceeds if the target has a matching item, and poofs
    /// that item.
    /// For one use keys and such

    SCRIPT_FUNCTION_BEGIN();

    const IInventoryHolder* targetInventory = tryInventoryHolder(self.getTarget());
    if (targetInventory == nullptr)
    {
        return false;
    }

    returncode = false;
    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);
    const ObjectRef itemRef = findMatchingHeldOrInventoryItemRef(*targetInventory, idsz);
    if (itemRef != ObjectRef::Invalid)
    {
        returncode = consumeOrPoofItemWithLegacyInventoryPath(itemRef, inventoryHolder(*pchr));
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

    if ( ModuleProfile::moduleAddIDSZ(activeModule().getPath(), Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument)) )
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

    IDamageable* damageableTarget = nullptr;
    if (!resolveDamageableTarget(self, damageableTarget))
    {
        return false;
    }

    tmp_damage.base = state.argument;
    tmp_damage.rand = 1;

    damageableTarget->damage(ATK_FRONT, tmp_damage, damageableRole(*pchr).getDamageTargetType(),
                             targetInfo(*pchr).getTeamRef(), pchr->toSharedPointer(), false, false, true);

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

    int iTmp;

    SCRIPT_FUNCTION_BEGIN();

    IAppearanceProfile* targetAppearance = resolvedTargetAppearance(self);
    if (targetAppearance == nullptr)
    {
        return false;
    }

    iTmp = targetAppearance->getSkin();
    state.x = targetAppearance->setSkin(state.argument);

    state.argument = iTmp;  // The character's old armor

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

    IWallet* targetWallet = tryWallet(self.getTarget());
    IWallet& selfWallet = wallet(*pchr);
    if (targetWallet == nullptr)
    {
        return false;
    }

    //squash out-or-range values
    if(state.argument < 0 && std::abs(state.argument) > targetWallet->getMoney()) {
        state.argument = -targetWallet->getMoney();
    }
    if(state.argument > selfWallet.getMoney()) {
        state.argument = selfWallet.getMoney();
    }

    //Do the transfer
    selfWallet.giveMoney(-state.argument);
    targetWallet->giveMoney(state.argument);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfLeaderIsAlive( script_state_t& state, ai_state_t& self )
{
    // IfLeaderIsAlive()
    /// @author ZZ
    /// @details This function proceeds if the team has a leader

    SCRIPT_FUNCTION_BEGIN();

    returncode = ( leaderRef(targetInfo(*pchr)) != ObjectRef::Invalid );

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

	auto mesh = activeModule().getMeshPointer();
	if (!mesh) {
		throw idlib::argument_null_error(__FILE__, __LINE__, "mesh");
	}
    returncode = mesh->set_texture( pchr->getTile(), state.argument );

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

    wallet(*pchr).dropMoney(state.argument);

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

    becomeSpellbook(enchantable(*pchr),
                    morphControl(*pchr),
                    animationControl(*pchr),
                    pchr->getProfileID(),
                    ppro->getSpellEffectType(),
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

    pchr->setDamageTargetType(static_cast<DamageType>(state.argument % DAMAGE_COUNT));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetWaterLevel( script_state_t& state, ai_state_t& self )
{
    // SetWaterLevel( tmpargument = "level" )
    /// @author ZZ
    /// @details This function raises or lowers the water in the module

    SCRIPT_FUNCTION_BEGIN();

    activeModule().getWater().set_douse_level(state.argument / 10.0f);

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

    IEnchantable* targetEnchantable = nullptr;
    ObjectRef ownerRef = ObjectRef::Invalid;
    if (resolveEnchantParticipants(self, self.getTarget(), targetEnchantable, ownerRef)) {
        const std::shared_ptr<Object> owner = tryObjectShared(ownerRef);
        if (owner != nullptr) {
            const std::shared_ptr<Object> spawner = pchr->toSharedPointer();
            returncode = targetEnchantable->addEnchant(pchr->getProfile()->getEnchantRef(),
                                                       pchr->getProfileID().get(),
                                                       owner,
                                                       spawner) != nullptr;
        }
        else {
            returncode = false;
        }
    }
    else {
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

    IEnchantable* childEnchantable = nullptr;
    ObjectRef ownerRef = ObjectRef::Invalid;
    if (resolveEnchantParticipants(self, self.child, childEnchantable, ownerRef)) {
        const std::shared_ptr<Object> owner = tryObjectShared(ownerRef);
        if (owner != nullptr) {
            const std::shared_ptr<Object> spawner = pchr->toSharedPointer();
            returncode = childEnchantable->addEnchant(pchr->getProfile()->getEnchantRef(),
                                                      pchr->getProfileID().get(),
                                                      owner,
                                                      spawner) != nullptr;
        }
        else {
            returncode = false;
        }
    }
    else {
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

    if(state.distance < XP_COUNT && state.distance >= 0) {
        teamMember(*pchr).giveTeamExperience(state.argument, static_cast<XPType>(state.distance));
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_RestockTargetAmmoIDAll( script_state_t& state, ai_state_t& self )
{
    // RestockTargetAmmoIDAll( tmpargument = "idsz" )
    /// @author ZZ
    /// @details This function restocks the ammo of every item the character is holding,
    /// if the item matches the ID given ( parent or child type )

    SCRIPT_FUNCTION_BEGIN();

    const IInventoryHolder* targetInventory = tryInventoryHolder(self.getTarget());
    if (targetInventory == nullptr)
    {
        return false;
    }

    int iTmp = 0;  // Amount of ammo given
    const IInventoryHolder& selfInventory = inventoryHolder(*pchr);
    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);

    iTmp += restockAmmoIfMatching(targetInventory->getHeldObject(SLOT_LEFT), idsz);
    iTmp += restockAmmoIfMatching(targetInventory->getHeldObject(SLOT_RIGHT), idsz);

    for (const ObjectRef itemRef : selfInventory.getInventoryItemRefs())
    {
        iTmp += restockAmmoIfMatching(itemRef, idsz);
    }

    state.argument = iTmp;
    returncode = ( iTmp != 0 );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_RestockTargetAmmoIDFirst( script_state_t& state, ai_state_t& self )
{
    // RestockTargetAmmoIDFirst( tmpargument = "idsz" )
    /// @author ZZ
    /// @details This function restocks the ammo of the first item the character is holding,
    /// if the item matches the ID given ( parent or child type )

    SCRIPT_FUNCTION_BEGIN();

    const IInventoryHolder* targetInventory = tryInventoryHolder(self.getTarget());
    if (targetInventory == nullptr)
    {
        return false;
    }

    int iTmp = 0;  // Amount of ammo given
    const IInventoryHolder& selfInventory = inventoryHolder(*pchr);
    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);

    iTmp += restockAmmoIfMatching(targetInventory->getHeldObject(SLOT_LEFT), idsz);

    if (iTmp == 0)
    {
        iTmp += restockAmmoIfMatching(targetInventory->getHeldObject(SLOT_RIGHT), idsz);
    }

    if (iTmp == 0)
    {
        for (const ObjectRef itemRef : selfInventory.getInventoryItemRefs())
        {
            iTmp += restockAmmoIfMatching(itemRef, idsz);
            if ( 0 != iTmp ) break;
        }
    }

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

    const ObjectRef killerRef = resolvedKillSourceRef(targetInfo(*pchr), self.getSelf());
    const std::shared_ptr<Object> killer = tryObjectShared(killerRef);
    IDamageable* damageableTarget = tryDamageable(self.getTarget());
    if (killer == nullptr || damageableTarget == nullptr)
    {
        return false;
    }

    damageableTarget->kill(killer, false);

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

    state.argument = activeModule().getWater()._douse_level * 10;

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

    IDamageable* damageableSelf = nullptr;
    if (!resolveSelfDamageable(self, damageableSelf))
    {
        return false;
    }

    damageableSelf->heal(pchr->toSharedPointer(), state.argument, true);

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

    pchr->setEquipped(true);

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

    int iTmp;

    SCRIPT_FUNCTION_BEGIN();

    state.x = state.argument;
    IAppearanceProfile& selfAppearance = appearanceProfile(*pchr);
    iTmp = selfAppearance.getSkin();
    selfAppearance.setSkin(Ego::Script::Interpreter::safeCast<size_t>(state.argument));
    state.x = selfAppearance.getSkin();
    state.argument = iTmp;  // The character's old armor

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
    ICharacterState* resolvedTargetState = nullptr;
    IDamageable* resolvedDamageable = nullptr;
    if (resolveAliveTargetStateAndDamageable(self, resolvedTargetState, resolvedDamageable))
    {
        resolvedTargetState->increaseBaseAttribute(Ego::Attribute::MAX_LIFE, FP8_TO_FLOAT(state.argument));
        resolvedDamageable->heal(pchr->toSharedPointer(), state.argument, true);
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
    if(activePlayingState()->getMiniMap()->isVisible()) returncode = false;

    activePlayingState()->getMiniMap()->setVisible(true);

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

    activePlayingState()->getMiniMap()->setShowPlayerPosition(true);

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
        //activePlayingState()->getMiniMap()->addBlip(state.x, state.y, static_cast<HUDColors>(state.argument % COLOR_MAX));
        activePlayingState()->getMiniMap()->addBlip(state.x, state.y, objectHandler()[pchr->getObjRef()]);
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

    IDamageable* damageableTarget = nullptr;
    ICharacterState* targetState = nullptr;
    if (!resolveHealingTarget(self, targetState, damageableTarget)) {
        return false;
    }

    returncode = false;
    if (damageableTarget->heal(pchr->toSharedPointer(), state.argument, false))
    {
        returncode = true;
        targetState->removeEnchantsWithIDSZ(IDSZ2('H', 'E', 'A', 'L'));
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
    if ( ICharacterState* resolvedTargetState = resolveAliveTargetState(self);
         resolvedTargetState != nullptr && state.argument > 0)
    {
        resolvedTargetState->costMana(-state.argument, pchr->getObjRef());
    }

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

    float fTmp;

    SCRIPT_FUNCTION_BEGIN();

    fog_instance_t& fog = GameSessionContext::get().fog();
    fTmp = ( Ego::Script::Interpreter::safeCast<float>(state.argument) / 10.0f ) - fog._top;
    fog._top += fTmp;
    fog._distance += fTmp;
    fog._on = config().graphic_fog_enable.getValue();
	if (fog._distance < 1.0f)  fog._on = false;

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

    state.argument = GameSessionContext::get().fog()._top * 10;

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

    fog_instance_t& fog = GameSessionContext::get().fog();
	fog._red = Ego::Math::constrain(state.turn, 0, 0xFF);
	fog._grn = Ego::Math::constrain(state.argument, 0, 0xFF);
	fog._blu = Ego::Math::constrain(state.distance, 0, 0xFF);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetFogBottomLevel( script_state_t& state, ai_state_t& self )
{
    // SetFogBottomLevel( tmpargument = "level" )

    /// @author ZZ
    /// @details This function sets the level of the module's fog.
    /// Values are * 10

    float fTmp;

    SCRIPT_FUNCTION_BEGIN();

    fog_instance_t& fog = GameSessionContext::get().fog();
	fTmp = (state.argument / 10.0f) - fog._bottom;
    fog._bottom += fTmp;
    fog._distance -= fTmp;
    fog._on = config().graphic_fog_enable.getValue();
	if (fog._distance < 1.0f)  fog._on = false;

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

    state.argument = GameSessionContext::get().fog()._bottom * 10;

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
    Index1D idx = activeModule().getMeshPointer()->getTileIndex(Ego::Vector2f(float(state.x), float(state.y)));

    const ego_tile_info_t& ptr = activeModule().getMeshPointer()->getTileInfo(idx);
    returncode = true;
    state.argument = ptr._img & TILE_LOWER_MASK;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTileXY( script_state_t& state, ai_state_t& self )
{
    // SetTileXY( tmpargument = "tile type", tmpx = "x", tmpy = "y" )
    /// @author ZZ
    /// @details This function changes the tile type at the specified coordinates

    SCRIPT_FUNCTION_BEGIN();

	auto mesh = activeModule().getMeshPointer();
	if (!mesh) {
		throw idlib::argument_null_error(__FILE__, __LINE__, "mesh");
	}

    Index1D index = mesh->getTileIndex(Ego::Vector2f(float(state.x), float(state.y)));
    returncode = mesh->set_texture( index, state.argument );

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

    returncode = ( pchr->getBaseModelRef() == ObjectProfileRef(SPELLBOOK) ||
                   pchr->getBaseModelRef() == pchr->getProfileID() );

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
    /// @details This function unkurses all items held and in the pockets of the target

    SCRIPT_FUNCTION_BEGIN();

    IInventoryHolder* targetInventory = tryInventoryHolder(self.getTarget());
    if (targetInventory == nullptr)
    {
        return false;
    }

    unkurseItemIfPresent(targetInventory->getHeldObject(SLOT_LEFT));
    unkurseItemIfPresent(targetInventory->getHeldObject(SLOT_RIGHT));

    for (const ObjectRef itemRef : inventoryHolder(*pchr).getInventoryItemRefs())
    {
        unkurseItemIfPresent(itemRef);
    }

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

    std::shared_ptr<Passage> passage = activeModule().getPassageByID(state.argument);
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

    activeModule().beatModule();

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

    activeModule().setExportValid(false);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EnableExport( script_state_t& state, ai_state_t& self )
{
    // EnableExport()
    /// @author ZZ
    /// @details This function turns export on

    SCRIPT_FUNCTION_BEGIN();

    activeModule().setExportValid(true);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DropTargetMoney( script_state_t& state, ai_state_t& self )
{
    // DropTargetMoney( tmpargument = "amount" )
    /// @author ZZ
    /// @details This function drops some of the target's money

    SCRIPT_FUNCTION_BEGIN();

    IWallet* targetWallet = tryWallet(self.getTarget());
    if (targetWallet == nullptr)
    {
        return false;
    }

    targetWallet->dropMoney(state.argument);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinTeam( script_state_t& state, ai_state_t& self )
{
    // JoinTeam( tmpargument = "team" )
    /// @author ZZ
    /// @details This makes the character itself join a specified team (A = 0, B = 1, 23 = Z, etc.)

    SCRIPT_FUNCTION_BEGIN();

    teamMember(*pchr).setTeam(static_cast<TEAM_REF>(state.argument));

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

    g_endText.setText("");

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddEndMessage( script_state_t& state, ai_state_t& self )
{
    // AddEndMessage( tmpargument = "message" )
    /// @author ZZ
    /// @details This function appends a message to the end-module text buffer

    SCRIPT_FUNCTION_BEGIN();

    returncode = ::AddEndMessage( pchr,  state.argument, &state );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddStat( script_state_t& state, ai_state_t& self )
{
    // AddStat()
    /// @author ZZ
    /// @details This function turns on an NPC's status display

    SCRIPT_FUNCTION_BEGIN();

    activePlayingState()->addStatusMonitor( objectHandler()[self.getSelf()] );

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

    for (const std::shared_ptr<Object>& object : objectHandler().iterator()) {
        if (object == nullptr) {
            continue;
        }

        IEnchantable* enchantable = tryEnchantable(object->getObjRef());
        if (enchantable != nullptr) {
            enchantable->disenchant();
        }
    }

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

    std::shared_ptr<Passage> passage = activeModule().getPassageByID(state.argument);
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

    int iTmp;

    SCRIPT_FUNCTION_BEGIN();

    const AppearanceWalletTarget target = resolvedTargetAppearanceWallet(self);
    if (target.appearance == nullptr || target.wallet == nullptr)
    {
        return false;
    }

    iTmp = target.appearance->getSkinCost(static_cast<size_t>(state.argument));
    state.y = iTmp;                                       // Cost of new skin

    iTmp -= target.appearance->getSkinCost(target.appearance->getSkin());     // Refund for old skin

    if ( iTmp > target.wallet->getMoney() )
    {
        // Not enough.
        state.x = iTmp - target.wallet->getMoney();        // Amount needed
        returncode = false;
    }
    else
    {
        // Pay for it.  Cost may be negative after refund.
        target.wallet->giveMoney(-iTmp);
        state.x = 0;
        returncode = true;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinEvilTeam( script_state_t& state, ai_state_t& self )
{
    // JoinEvilTeam()
    /// @author ZZ
    /// @details This function adds the character to the evil Team.

    SCRIPT_FUNCTION_BEGIN();

    teamMember(*pchr).setTeam(static_cast<TEAM_REF>(Team::TEAM_EVIL));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinNullTeam( script_state_t& state, ai_state_t& self )
{
    // JoinNullTeam()
    /// @author ZZ
    /// @details This function adds the character to the null Team.

    SCRIPT_FUNCTION_BEGIN();

    teamMember(*pchr).setTeam(static_cast<TEAM_REF>(Team::TEAM_NULL));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinGoodTeam( script_state_t& state, ai_state_t& self )
{
    // JoinGoodTeam()
    /// @author ZZ
    /// @details This function adds the character to the good Team.

    SCRIPT_FUNCTION_BEGIN();

    teamMember(*pchr).setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));

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

    activeModule().enablePitsKill();

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
        activeModule().giveTeamExperience(static_cast<TEAM_REF>(Team::TEAM_GOOD),
                                          state.argument,
                                          static_cast<XPType>(state.distance));
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

    {
		EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "deprecated script function ", "`", "EnableListenSkill", "`", " by class `", pchr->getProfile()->getClassName(), "`", Log::EndOfEntry);
    }

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

    if ( !ppro->isValidMessageID(state.argument) ) return false;

    returncode = link_follow_modname( ppro->getMessage(state.argument).c_str(), true );
    if ( !returncode )
    {
        DisplayMsg_printf( "That's too scary for %s", pchr->getName().c_str() );
    }

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
        GameSessionContext::get().publishEnemySense(EnemySenseState(targetInfo->getTeamRef(), state.argument));
    }
    else
    {
        GameSessionContext::get().resetEnemySense();
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

    if ( state.x > EDGE && state.y > EDGE && state.x < activeModule().getMeshPointer()->_tmem._edge_x - EDGE && state.y < activeModule().getMeshPointer()->_tmem._edge_y - EDGE )
    {
        activeModule().enablePitsTeleport(Ego::Vector3f(static_cast<float>(state.x), 
                                                         static_cast<float>(state.y), 
                                                         static_cast<float>(state.distance)));
    }
    else
    {
        //make it kill instead
        activeModule().enablePitsKill();
    }

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

    IWallet& selfWallet = wallet(*pchr);
    selfWallet.giveMoney(state.argument - selfWallet.getMoney());

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

    const ITargetInfo* targetInfo = nullptr;
    IDamageable* damageableSelf = nullptr;
    if (!resolveRetaliationTarget(self, targetInfo, damageableSelf))
    {
        return false;
    }

    const std::shared_ptr<Object> target = tryObjectShared(self.getTarget());
    if (target == nullptr)
    {
        return false;
    }

    tmp_damage.base = state.argument;
    tmp_damage.rand = 1;

    damageableSelf->damage(ATK_FRONT, tmp_damage, static_cast<DamageType>(state.distance), targetInfo->getTeamRef(), target, false, false, true);

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
