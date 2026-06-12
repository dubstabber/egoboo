/// @file egolib/game/script_functions_target_identity.c
/// @brief IDSZ-keyed target identity queries. Split off script_functions_target.c
///        on 2026-06-12 (9 entries: HasID, HasItemID, HoldingItemID, HasSkillID,
///        HasVulnerabilityID, HasSpecialID, HasAnyID, HasItemIDEquipped, HasQuest).
/// @details Shared infrastructure (SelfTargetSelectorContext / TargetCompatibilityContext /
///          isFacing / makeSelfTargetSelectorContext / makeTargetCompatibilityContext /
///          tryResolvedTargetInfo and friends) lives in script_functions_target_impl.h.

#include "egolib/game/script_functions_target_impl.h"

//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetHasID( script_state_t& state, ai_state_t& self )
{
    // IfTargetHasID( tmpargument = "idsz" )
    /// @author ZZ
    /// @details This function proceeds if the target has either a parent or type IDSZ
    /// matching tmpargument.

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    return target->hasTypeIDSZ(Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetHasItemID( script_state_t& state, ai_state_t& self )
{
    // IfTargetHasItemID( tmpargument = "idsz" )
    /// @author ZZ
    /// @details This function proceeds if the target has a matching item in his/her
    /// pockets or hands.

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    if (targetContext.info == nullptr || targetContext.inventory == nullptr)
    {
        return false;
    }

    const IDSZ2 itemId = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);

    //Check hands
    if (targetContext.info->wieldsItemIDSZ(itemId)) {
        return true;
    }

    //Check inventory
    return ObjectRef::Invalid != Inventory::findItem(*targetContext.inventory, itemId, false);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetHoldingItemID( script_state_t& state, ai_state_t& self )
{
    // IfTargetHoldingItemID( tmpargument = "idsz" )
    /// @author ZZ
    /// @details This function proceeds if the target has a matching item in his/her
    /// hands.  It also sets tmpargument to the proper latch button to press
    /// to use that item

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    return target->wieldsItemIDSZ(Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetHasSkillID( script_state_t& state, ai_state_t& self )
{
    // IfTargetHasSkillID( tmpargument = "skill idsz" )
    /// @author ZZ
    /// @details This function proceeds if ID matches tmpargument

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    return target->hasSkillIDSZ(Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetHasVulnerabilityID( script_state_t& state, ai_state_t& self )
{
    // IfTargetHasVulnerabilityID( tmpargument = "vulnerability idsz" )
    /// @author ZZ
    /// @details This function proceeds if the target is vulnerable to the given IDSZ.

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    return target->matchesVulnerabilityIDSZ(Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetHasSpecialID( script_state_t& state, ai_state_t& self )
{
    // IfTargetHasSpecialID( tmpargument = "special idsz" )
    /// @author ZZ
    /// @details This function proceeds if the character has a special IDSZ ( in data.txt )

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    return target->matchesSpecialIDSZ(Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetHasAnyID( script_state_t& state, ai_state_t& self )
{
    // IfTargetHasAnyID( tmpargument = "idsz" )
    /// @author ZZ
    /// @details This function proceeds if the target has any IDSZ that matches the given one

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    return target->hasAnyIDSZ(Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetHasItemIDEquipped( script_state_t& state, ai_state_t& self )
{
    // IfTargetHasItemIDEquipped( tmpargument = "item idsz" )
    /// @author ZZ
    /// @details This function proceeds if the target already wearing a matching item

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    if (targetContext.inventory == nullptr)
    {
        return false;
    }

	auto iitem = Inventory::findItem(*targetContext.inventory, Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument), true );

    return isLiveTargetRef(iitem);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetHasQuest( script_state_t& state, ai_state_t& self )
{
    // tmpdistance = IfTargetHasQuest( tmpargument = "quest idsz )
    /// @author ZF
    /// @details This function proceeds if the Target has the unfinIshed quest specified in tmpargument
    /// and sets tmpdistance to the Quest Level of the specified quest.

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    if (targetContext.info == nullptr)
    {
        return false;
    }

    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);
    if(!targetContext.info->isPlayer()) {
        return false;
    }

    const std::shared_ptr<Ego::Player> player = tryPlayer(*targetContext.info);
    if (player == nullptr)
    {
        return false;
    }

    // only find active quests
    if(!player->getQuestLog().hasActiveQuest(idsz)) {
        return false;
    }

    state.distance = player->getQuestLog()[idsz];
    return true;
}


