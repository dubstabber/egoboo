/// @file egolib/game/script_functions_target_select.c
/// @brief Target SELECTOR functions — all scr_SetTarget* that change the AI's current target

#include "egolib/game/script_functions_target_impl.h"

//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToNearbyEnemy( script_state_t& state, ai_state_t& self )
{
    // SetTargetToNearbyEnemy()
    /// @author ZZ
    /// @details This function sets the target to a nearby enemy, failing if there are none

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    return trySetResolvedTarget(self, findTargetForSelf(selfContext, NEARBY, IDSZ2::None, TARGET_ENEMIES));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToTargetLeftHand( script_state_t& state, ai_state_t& self )
{
    // SetTargetToTargetLeftHand()
    /// @author ZZ
    /// @details This function sets the target to the item in the target's left hand,
    /// failing if the target has no left hand item

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    if (targetContext.inventory == nullptr)
    {
        return false;
    }

    return trySetTargetFromHeldObject(self, *targetContext.inventory, SLOT_LEFT);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToTargetRightHand( script_state_t& state, ai_state_t& self )
{
    // SetTargetToTargetRightHand()
    /// @author ZZ
    /// @details This function sets the target to the item in the target's right hand,
    /// failing if the target has no right hand item

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    if (targetContext.inventory == nullptr)
    {
        return false;
    }

    return trySetTargetFromHeldObject(self, *targetContext.inventory, SLOT_RIGHT);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToWhoeverAttacked( script_state_t& state, ai_state_t& self )
{
    // SetTargetToWhoeverAttacked()
    /// @author ZZ
    /// @details This function sets the target to whoever attacked the character last, failing for damage tiles

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    return trySetResolvedTarget(self, selfLastAttackerRef(selfContext));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToWhoeverBumped( script_state_t& state, ai_state_t& self )
{
    // SetTargetToWhoeverBumped()
    /// @author ZZ
    /// @details This function sets the target to whoever bumped the character last. It never fails

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    return trySetResolvedTarget(self, selfBumpedRef(selfContext));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToWhoeverCalledForHelp( script_state_t& state, ai_state_t& self )
{
    // SetTargetToWhoeverCalledForHelp()
    /// @author ZZ
    /// @details This function sets the target to whoever called for help last.

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    return trySetResolvedTarget(self, selfTeamCallerForHelpRef(selfContext));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToOldTarget( script_state_t& state, ai_state_t& self )
{
    // SetTargetToOldTarget()
    /// @author ZZ
    /// @details This function sets the target to the target from last update, used to
    /// undo other set_Target functions

    if (!resolveSelfContext(self).isResolved()) return false;

    return trySetResolvedTarget(self, self.getOldTarget());
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToWhoeverIsHolding( script_state_t& state, ai_state_t& self )
{
    // SetTargetToWhoeverIsHolding()
    /// @author ZZ
    /// @details This function sets the target to the character's holder or mount,
    /// failing if the character has no mount or holder

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    return trySetResolvedTarget(self, selfHolderRef(selfContext));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToTargetOfLeader( script_state_t& state, ai_state_t& self )
{
    // SetTargetToTargetOfLeader()
    /// @author ZZ
    /// @details This function sets the character's target to the target of its leader,
    /// or it fails with no change if the leader is dead

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    return trySetTargetFromScriptableTarget(self, tryScriptable(selfTeamLeaderRef(selfContext)));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToLeader( script_state_t& state, ai_state_t& self )
{
    // SetTargetToLeader()
    /// @author ZZ
    /// @details This function sets the target to the leader, proceeding if their is
    /// a valid leader for the character's team

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    return trySetResolvedTarget(self, selfTeamLeaderRef(selfContext));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToSelf( script_state_t& state, ai_state_t& self )
{
    // SetTargetToSelf()
    /// @author ZZ
    /// @details This function sets the target to the character itself

    if (!resolveSelfContext(self).isResolved()) return false;

    self.setTarget(self.getSelf());

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToRider( script_state_t& state, ai_state_t& self )
{
    // SetTargetToRider()
    /// @author ZZ
    /// @details This function sets the target to whoever is riding the character (left/only grip),
    /// failing if there is no rider

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    if (selfContext.inventory == nullptr)
    {
        return false;
    }

    return trySetTargetFromHeldObject(self, *selfContext.inventory, SLOT_LEFT);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToWhoeverWasHit( script_state_t& state, ai_state_t& self )
{
    // SetTargetToWhoeverWasHit()
    /// @author ZZ
    /// @details This function sets the target to whoever was hit by the character last

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    return trySetResolvedTarget(self, selfLastHitRef(selfContext));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToWideEnemy( script_state_t& state, ai_state_t& self )
{
    // SetTargetToWideEnemy()
    /// @author ZZ
    /// @details This function sets the target to an enemy in the vicinity around the
    /// character, failing if there are none

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    // WIDE_CLASSIC (not WIDE) -- see script.h: brackets 2.6.8's get_wide_target() block-search
    // reach so "wide" enemy acquisition isn't narrower than the classic engine.
    return trySetResolvedTarget(self, findTargetForSelf(selfContext, WIDE_CLASSIC, IDSZ2::None, TARGET_ENEMIES));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToLowestTarget( script_state_t& state, ai_state_t& self )
{
    // SetTargetToLowestTarget()
    /// @author ZZ
    /// @details This function sets the target to the absolute bottom character.
    /// The holder of the target, or the holder of the holder of the target, or
    /// the holder of the holder of ther holder of the target, etc.   This function never fails

    if (!resolveSelfContext(self).isResolved()) return false;

	auto itarget = chr_get_lowest_attachment( self.getTarget(), false );
    return trySetResolvedTarget(self, itarget);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetOwnerToTarget( script_state_t& state, ai_state_t& self )
{
    // SetOwnerToTarget()
    /// @author ZZ
    /// @details This function must be called before enchanting anything.
    /// The owner is the character that pays the sustain costs and such for the enchantment

    if (!resolveSelfContext(self).isResolved()) return false;

    self.owner = self.getTarget();

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToOwner( script_state_t& state, ai_state_t& self )
{
    // SetTargetToOwner()
    /// @author ZZ
    /// @details This function sets the target to whoever was previously declared as the
    /// owner.

    if (!resolveSelfContext(self).isResolved()) return false;

    return trySetResolvedTarget(self, self.owner);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToWideBlahID( script_state_t& state, ai_state_t& self )
{
    // SetTargetToWideBlahID( tmpargument = "idsz", tmpdistance = "blah bits" )
    /// @author ZZ
    /// @details This function sets the target to a character that matches the description,
    /// and who is located in the general vicinity of the character

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    // Try to find one
    // WIDE_CLASSIC (not WIDE) -- see script.h: SetTargetToWideEnemy's classic-engine sibling
    // (both compiled to get_wide_target() in 2.6.8), so both widen together.
    const auto ichr = findTargetForSelf(selfContext,
                                        WIDE_CLASSIC,
                                        IDSZ2(state.argument),
                                        state.distance);
    return trySetResolvedTarget(self, ichr);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToDistantEnemy( script_state_t& state, ai_state_t& self )
{
    // SetTargetToDistantEnemy( tmpdistance = "distance" )
    /// @author ZZ
    /// @details This function finds a character within a certain distance of the
    /// character, failing if there are none

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    const auto ichr = findTargetForSelf(selfContext, state.distance, IDSZ2::None, TARGET_ENEMIES);
    return trySetResolvedTarget(self, ichr);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToWhoeverIsInPassage( script_state_t& state, ai_state_t& self )
{
    // SetTargetToWhoeverIsInPassage()
    /// @author ZZ
    /// @details This function sets the target to whoever is blocking the given passage
    /// This function lets passage rectangles be used as event triggers

    if (!resolveSelfContext(self).isResolved()) return false;

    return trySetTargetFromPassageOccupant(self,
                                           state.argument,
                                           IDSZ2::None,
                                           TARGET_SELF | TARGET_FRIENDS | TARGET_ENEMIES,
                                           IDSZ2::None);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToNearestBlahID( script_state_t& state, ai_state_t& self )
{
    // SetTargetToNearestBlahID( tmpargument = "idsz", tmpdistance = "blah bits" )

    /// @author ZZ
    /// @details This function finds the NEAREST ( exact ) character that fits the given
    /// parameters, failing if it finds none

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    // Try to find one
    const auto ichr = findTargetForSelf(selfContext,
                                        NEAREST,
                                        IDSZ2(state.argument),
                                        state.distance);
    return trySetResolvedTarget(self, ichr);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToNearestEnemy( script_state_t& state, ai_state_t& self )
{
    // SetTargetToNearestEnemy()
    /// @author ZZ
    /// @details This function finds the NEAREST ( exact ) enemy, failing if it finds none

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    const auto ichr = findTargetForSelf(selfContext, NEAREST, IDSZ2::None, TARGET_ENEMIES);
    return trySetResolvedTarget(self, ichr);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToNearestFriend( script_state_t& state, ai_state_t& self )
{
    // SetTargetToNearestFriend()
    /// @author ZZ
    /// @details This function finds the NEAREST ( exact ) friend, failing if it finds none

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    const auto ichr = findTargetForSelf(selfContext, NEAREST, IDSZ2::None, TARGET_FRIENDS);
    return trySetResolvedTarget(self, ichr);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToNearestLifeform( script_state_t& state, ai_state_t& self )
{
    // SetTargetToNearestLifeform()

    /// @author ZZ
    /// @details This function finds the NEAREST ( exact ) friend or enemy, failing if it
    /// finds none

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    const auto ichr = findTargetForSelf(selfContext,
                                        NEAREST,
                                        IDSZ2::None,
                                        TARGET_ITEMS | TARGET_FRIENDS | TARGET_ENEMIES);
    return trySetResolvedTarget(self, ichr);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToPassageID( script_state_t& state, ai_state_t& self )
{
    // SetTargetToPassageID( tmpargument = "passage", tmpdistance = "idsz" )
    /// @author ZZ
    /// @details This function finds a character who is both in the passage and who has
    /// an item with the given IDSZ

    if (!resolveSelfContext(self).isResolved()) return false;

    return trySetTargetFromPassageOccupant(self,
                                           state.argument,
                                           IDSZ2::None,
                                           TARGET_SELF | TARGET_FRIENDS | TARGET_ENEMIES,
                                           IDSZ2(state.distance));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToLastItemUsed( script_state_t& state, ai_state_t& self )
{
    // SetTargetToLastItemUsed()
    /// @author ZF
    /// @details This sets the Target to the last item the character used

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    const ObjectRef lastItemUsedRef = selfLastItemUsedRef(selfContext);
    return lastItemUsedRef != self.getSelf() &&
           trySetResolvedTarget(self, lastItemUsedRef);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToBlahInPassage( script_state_t& state, ai_state_t& self )
{
    // SetTargetToBlahInPassage()
    /// @author ZF
    /// @details This function sets the target to whatever object with the specified bits
    /// in tmpdistance is blocking the given passage. This function lets passage rectangles be used as event triggers

    if (!resolveSelfContext(self).isResolved()) return false;

    return trySetTargetFromPassageOccupant(self,
                                           state.argument,
                                           IDSZ2(state.turn),
                                           TARGET_SELF | state.distance,
                                           IDSZ2::None);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToNearbyMeleeWeapon( script_state_t& state, ai_state_t& self )
{
    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    ObjectRef best_target = findWeaponForSelf(selfContext, WIDE, IDSZ2('X', 'W', 'E', 'P'), false, true);
    return trySetResolvedTarget(self, best_target);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToDistantFriend( script_state_t& state, ai_state_t& self )
{
    // SetTargetToDistantFriend( tmpdistance = "distance" )
    /// @author ZF
    /// @details This function finds a character within a certain distance of the
    /// character, failing if there are none

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    const auto ichr = findTargetForSelf(selfContext, state.distance, IDSZ2::None, TARGET_FRIENDS);
    return trySetResolvedTarget(self, ichr);
}
