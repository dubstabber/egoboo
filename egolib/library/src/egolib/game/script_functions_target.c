/// @file egolib/game/script_functions_target.c
/// @brief Target selection, target property queries, and order management

#include "egolib/game/script_functions_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

namespace
{
struct SelfTargetSelectorContext
{
    ObjectRef selfRef = ObjectRef::Invalid;
    const IScriptable* scriptable = nullptr;
    const ITargetInfo* info = nullptr;
    const IInventoryHolder* inventory = nullptr;
    const IPhysical* physical = nullptr;
    const IAppearanceProfile* appearance = nullptr;
};

struct TargetCompatibilityContext
{
    ObjectRef ref = ObjectRef::Invalid;
    const ITargetInfo* info = nullptr;
    IInventoryHolder* inventory = nullptr;
    IDamageable* damageable = nullptr;
    IScriptable* scriptable = nullptr;
    const IPhysical* physical = nullptr;
};

bool isFacing(const IPhysical& selfPhysical, const IPhysical& targetPhysical)
{
    FACING_T facing = FACING_T(vec_to_facing(targetPhysical.getPosX() - selfPhysical.getPosX(),
                                             targetPhysical.getPosY() - selfPhysical.getPosY()));
    facing -= FACING_T(selfPhysical.getFacingZ());
    return facing > 55535 || facing < 10000;
}

SelfTargetSelectorContext makeSelfTargetSelectorContext(const ai_state_t& self)
{
    SelfTargetSelectorContext context;
    context.selfRef = self.getSelf();
    context.scriptable = tryScriptable(context.selfRef);
    context.info = tryTargetInfo(context.selfRef);
    context.inventory = tryInventoryHolder(context.selfRef);
    context.physical = tryPhysical(context.selfRef);
    context.appearance = tryAppearanceProfile(context.selfRef);
    return context;
}

TargetCompatibilityContext makeTargetCompatibilityContext(ObjectRef objectRef)
{
    TargetCompatibilityContext context;
    context.ref = objectRef;
    context.info = tryTargetInfo(objectRef);
    context.inventory = tryInventoryHolder(objectRef);
    context.damageable = tryDamageable(objectRef);
    context.scriptable = tryScriptable(objectRef);
    context.physical = tryPhysical(objectRef);
    return context;
}

TargetCompatibilityContext makeTargetCompatibilityContext(const ai_state_t& self)
{
    return makeTargetCompatibilityContext(self.getTarget());
}

bool isLiveTargetRef(ObjectRef objectRef)
{
    return tryTargetInfo(objectRef) != nullptr;
}

const ITargetInfo* tryResolvedTargetInfo(const ai_state_t& self)
{
    return makeTargetCompatibilityContext(self).info;
}

bool trySetResolvedTarget(ai_state_t& self, ObjectRef objectRef)
{
    if (!isLiveTargetRef(objectRef))
    {
        return false;
    }

    self.setTarget(objectRef);
    return true;
}

bool trySetTargetFromHeldObject(ai_state_t& self,
                                const IInventoryHolder& holder,
                                slot_t slot)
{
    return trySetResolvedTarget(self, holder.getHeldObject(slot));
}

bool trySetTargetFromScriptableTarget(ai_state_t& self, const IScriptable* scriptableObject)
{
    return scriptableObject != nullptr &&
           trySetResolvedTarget(self, scriptableObject->getAITarget());
}

ObjectRef selfLastAttackerRef(const SelfTargetSelectorContext& context)
{
    return context.scriptable != nullptr ? context.scriptable->getAILastAttacker() : ObjectRef::Invalid;
}

ObjectRef selfBumpedRef(const SelfTargetSelectorContext& context)
{
    return context.scriptable != nullptr ? context.scriptable->getAIBumped() : ObjectRef::Invalid;
}

ObjectRef selfLastHitRef(const SelfTargetSelectorContext& context)
{
    return context.scriptable != nullptr ? context.scriptable->getAILastHit() : ObjectRef::Invalid;
}

ObjectRef selfLastItemUsedRef(const SelfTargetSelectorContext& context)
{
    return context.scriptable != nullptr ? context.scriptable->getAILastItemUsed() : ObjectRef::Invalid;
}

ObjectRef selfTeamLeaderRef(const SelfTargetSelectorContext& context)
{
    return context.info != nullptr ? teamLeaderRef(*context.info) : ObjectRef::Invalid;
}

ObjectRef selfTeamCallerForHelpRef(const SelfTargetSelectorContext& context)
{
    return context.info != nullptr ? teamCallerForHelpRef(*context.info) : ObjectRef::Invalid;
}

ObjectRef selfHolderRef(const SelfTargetSelectorContext& context)
{
    return context.info != nullptr ? context.info->getHolderRef() : ObjectRef::Invalid;
}

bool trySetTargetFromPassageOccupant(ai_state_t& self,
                                     int passageId,
                                     const IDSZ2& occupantIdsz,
                                     BIT_FIELD targetingBits,
                                     const IDSZ2& requiredItem)
{
    const std::shared_ptr<Passage> passage = tryPassage(passageId);
    return passage != nullptr &&
           trySetResolvedTarget(self,
                                passage->whoIsBlockingPassage(self.getSelf(),
                                                              occupantIdsz,
                                                              targetingBits,
                                                              requiredItem));
}

ObjectRef findTargetForSelf(const SelfTargetSelectorContext& context,
                            float maxDistance,
                            const IDSZ2& idsz,
                            BIT_FIELD targetingBits)
{
    return chr_find_target(context.selfRef, maxDistance, idsz, targetingBits);
}

ObjectRef findWeaponForSelf(const SelfTargetSelectorContext& context,
                            float maxDistance,
                            const IDSZ2& weaponIdsz,
                            bool findRanged,
                            bool useLineOfSight)
{
    return FindWeapon(context.selfRef, maxDistance, weaponIdsz, findRanged, useLineOfSight);
}

}

//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetKilled( script_state_t& state, ai_state_t& self )
{
    // IfTargetKilled()
    /// @author ZZ
    /// @details This function proceeds if the character's target from last update was
    /// killed during this update

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    if (targetContext.damageable == nullptr)
    {
        return false;
    }

    // Proceed only if the character's target has just died or is already dead
    return ( HAS_SOME_BITS( self.alert, ALERTIF_TARGETKILLED ) || !targetContext.damageable->isAlive() );
}


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
uint8_t scr_IssueOrder( script_state_t& state, ai_state_t& self )
{
    // IssueOrder( tmpargument = "order"  )
    /// @author ZZ
    /// @details This function tells all of the character's teammates to do something,
    /// though each teammate needs to interpret the order using IfOrdered in
    /// its own script.

    if (!resolveSelfContext(self).isResolved()) return false;

    issue_order( self.getSelf(), state.argument );

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetCanOpenStuff( script_state_t& state, ai_state_t& self )
{
    // IfTargetCanOpenStuff()
    /// @author ZZ
    /// @details This function proceeds if the target can open stuff ( set in data.txt )
    /// Used by chests and buttons and such so only "smart" creatures can operate
    /// them

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    if (targetContext.info == nullptr || targetContext.inventory == nullptr)
    {
        return false;
    }

    if ( targetContext.info->isMount() )
    {
        const ITargetInfo* rider = tryTargetInfo(targetContext.inventory->getHeldObject(SLOT_LEFT));

        if (rider != nullptr)
        {
            // can the rider open stuff
            if (rider->canOpenStuff())
            {
                return true;
            }
        }
    }

    // if a rider can't openstuff, can the target openstuff?
    return targetContext.info->canOpenStuff();
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
uint8_t scr_IfTargetIsOnOtherTeam( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsOnOtherTeam()
    /// @author ZZ
    /// @details This function proceeds if the target is on another team

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    if (target == nullptr || selfContext.info == nullptr)
    {
        return false;
    }

    return ( target->isAlive() && !target->isOnSameTeam(selfContext.info->getTeamRef()) );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsOnHatedTeam( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsOnHatedTeam()
    /// @author ZZ
    /// @details This function proceeds if the target is on an enemy team

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    if (target == nullptr || targetContext.damageable == nullptr || selfContext.info == nullptr)
    {
        return false;
    }

    return target->isAlive() &&
           target->isHatedByTeam(selfContext.info->getTeamRef()) &&
           !targetContext.damageable->isInvincible();
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
uint8_t scr_IfTargetIsOldTarget( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsOldTarget()
    /// @author ZZ
    /// @details This function proceeds if the target is the same as it was last update

    if (!resolveSelfContext(self).isResolved()) return false;

    return ( self.getTarget() == self.getOldTarget() );
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
uint8_t scr_SetOldTarget( script_state_t& state, ai_state_t& self )
{
    // SetOldTarget()
    /// @author ZZ
    /// @details This function sets the old target to the current target.  To allow
    /// greater manipulations of the target

    if (!resolveSelfContext(self).isResolved()) return false;

    self.setOldTarget(self.getTarget());

    return true;
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
uint8_t scr_IfTargetIsHurt( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsHurt()
    /// @author ZZ
    /// @details This function passes only if the target is hurt and alive

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    return target->isHurt();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsAPlayer( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsAPlayer()
    /// @author ZZ
    /// @details This function proceeds if the target is controlled by a human ( may not be local )

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    return target->isPlayer();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsAlive( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsAlive()
    /// @author ZZ
    /// @details This function proceeds if the target is alive

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    return target->isAlive();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsSelf( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsSelf()
    /// @author ZZ
    /// @details This function proceeds if the character is targeting itself

    if (!resolveSelfContext(self).isResolved()) return false;

    return ( self.getTarget() == self.getSelf() );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsMale( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsMale()
    /// @author ZZ
    /// @details This function proceeds only if the target is male

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    return ( target->getGender() == Gender::Male );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsFemale( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsFemale()
    /// @author ZZ
    /// @details This function proceeds if the target is female

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    return ( target->getGender() == Gender::Female );
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
uint8_t scr_GetAttackTurn( script_state_t& state, ai_state_t& self )
{
    // tmpturn = GetAttackTurn()
    /// @author ZZ
    /// @details This function sets tmpturn to the direction from which the last attack
    /// came. Not particularly useful in most cases, but it could be.

    if (!resolveSelfContext(self).isResolved()) return false;

    state.turn = FACING_T(self.directionlast);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetDamageType( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetDamageType()
    /// @author ZZ
    /// @details This function sets tmpargument to the damage type of the last attack that
    /// hit the character

    if (!resolveSelfContext(self).isResolved()) return false;

    state.argument = self.damagetypelast;

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_TranslateOrder( script_state_t& state, ai_state_t& self )
{
    // tmpx,tmpy,tmpargument = TranslateOrder()
    /// @author ZZ
    /// @details This function translates a packed order into understandable values.
    /// See CreateOrder for more.  This function sets tmpx, tmpy, tmpargument,
    /// and sets the target ( if valid )

    if (!resolveSelfContext(self).isResolved()) return false;

    const ObjectRef targetRef = ObjectRef(Ego::Math::clipBits<16>(self.order_value >> 24));
    if (!trySetResolvedTarget(self, targetRef))
    {
        return false;
    }

    state.x = ((self.order_value >> 14) & 0x03FF) << 6;
    state.y = ((self.order_value >> 4) & 0x03FF) << 6;
    state.argument = (self.order_value >> 0) & 0x000F;
    return true;
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
    return trySetResolvedTarget(self, findTargetForSelf(selfContext, WIDE, IDSZ2::None, TARGET_ENEMIES));
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
uint8_t scr_GetTargetGrogTime( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetTargetGrogTime()
    /// @author ZZ
    /// @details This function sets tmpargument to the number of updates before the
    /// character is ungrogged, proceeding if the number is greater than 0

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    state.argument = target->getGrogTimer();

    return ( 0 != state.argument );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetTargetDazeTime( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetTargetDazeTime()
    /// @author ZZ
    /// @details This function sets tmpargument to the number of updates before the
    /// character is undazed, proceeding if the number is greater than 0

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    state.argument = target->getDazeTimer();

    return ( 0 != state.argument );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsOnSameTeam( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsOnSameTeam()
    /// @author ZZ
    /// @details This function proceeds if the target is on the character's team

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    if (target == nullptr || selfContext.info == nullptr)
    {
        return false;
    }

    return target->isOnSameTeam(selfContext.info->getTeamRef());
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
uint8_t scr_IfTargetIsDefending( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsDefending()
    /// @author ZZ
    /// @details This function proceeds if the target is holding up a shield or similar
    /// defense

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    return ACTION_IS_TYPE(target->getCurrentAnimation(), P);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsAttacking( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsAttacking()
    /// @author ZZ
    /// @details This function proceeds if the target is doing an attack action

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    return target->isAttacking();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsKursed( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsKursed()
    /// @author ZZ
    /// @details This function proceeds if the target is kursed

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    return target->isKursed();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsDressedUp( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsDressedUp()
    /// @author ZZ
    /// @details This function proceeds if the target is dressed in fancy clothes

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    if (selfContext.appearance == nullptr)
    {
        return false;
    }

    return selfContext.appearance->isCurrentSkinDressy();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfDistanceIsMoreThanTurn( script_state_t& state, ai_state_t& self )
{
    // IfDistanceIsMoreThanTurn()
    /// @author ZZ
    /// @details This function proceeds tmpdistance is greater than tmpturn

    if (!resolveSelfContext(self).isResolved()) return false;

    return ( state.distance > state.turn );
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
    const auto ichr = findTargetForSelf(selfContext,
                                        WIDE,
                                        IDSZ2(state.argument),
                                        state.distance);
    return trySetResolvedTarget(self, ichr);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfFacingTarget( script_state_t& state, ai_state_t& self )
{
    // IfFacingTarget()
    /// @author ZZ
    /// @details This function proceeds if the character is more or less facing its
    /// target

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    if (targetContext.physical == nullptr || selfContext.physical == nullptr)
    {
        return false;
    }

    return isFacing(*selfContext.physical, *targetContext.physical);
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
uint8_t scr_IfTargetIsMounted( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsMounted()
    /// @author ZZ
    /// @details This function proceeds if the target is riding a mount

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    if (targetContext.info == nullptr)
    {
        return false;
    }

    const ITargetInfo* holderInfo = tryTargetInfo(targetContext.info->getHolderRef());
    return holderInfo != nullptr && holderInfo->isMount();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_OrderTarget( script_state_t& state, ai_state_t& self )
{
    // OrderTarget( tmpargument = "order" )
    /// @author ZZ
    /// @details This function issues an order to the given target
    /// Be careful in using this, always checking IDSZ first

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    if (targetContext.scriptable == nullptr)
    {
        return false;
    }

    return targetContext.scriptable->addAIOrder(state.argument, 0);
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
uint8_t scr_CreateOrder( script_state_t& state, ai_state_t& self )
{
    // tmpargument = CreateOrder( tmpx = "value1", tmpy = "value2", tmpargument = "order" )

    /// @author ZZ
    /// @details This function compresses tmpx, tmpy, tmpargument ( 0 - 15 ), and the
    /// character's target into tmpargument.  This new tmpargument can then
    /// be issued as an order to teammates.  TranslateOrder will undo the
    /// compression

    uint16_t sTmp = 0;

    if (!resolveSelfContext(self).isResolved()) return false;

    sTmp = ( REF_TO_INT( self.getTarget().get() ) & 0x00FF ) << 24;
    sTmp |= (( state.x >> 6 ) & 0x03FF ) << 14;
    sTmp |= (( state.y >> 6 ) & 0x03FF ) << 4;
    sTmp |= ( state.argument & 0x000F );
    state.argument = sTmp;

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_OrderSpecialID( script_state_t& state, ai_state_t& self )
{
    // OrderSpecialID( tmpargument = "compressed order", tmpdistance = "idsz" )
    /// @author ZZ
    /// @details This function orders all characters with the given special IDSZ.

    if (!resolveSelfContext(self).isResolved()) return false;

    issue_special_order( state.argument, state.distance );

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsSneaking( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsSneaking()
    /// @author ZZ
    /// @details This function proceeds if the target is doing ACTION_WA or ACTION_DA

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    const ModelAction currentAnimation = target->getCurrentAnimation();
    return ( currentAnimation == ACTION_DA || currentAnimation == ACTION_WA );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetCanSeeInvisible( script_state_t& state, ai_state_t& self )
{
    // IfTargetCanSeeInvisible()
    /// @author ZZ
    /// @details This function proceeds if the target can see invisible

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    return target->canSeeInvisible();
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
uint8_t scr_IfTargetIsFlying( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsFlying()
    /// @author ZZ
    /// @details This function proceeds if the character target is flying

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    return target->isFlying();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetTargetState( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetTargetState()
    /// @author ZZ
    /// @details This function sets tmpargument to the state of the target

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    if (targetContext.scriptable == nullptr)
    {
        return false;
    }

    state.argument = targetContext.scriptable->getAIStateValue();

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetTargetContent( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetTargetContent()
    // This sets tmpargument to the current Target's content value

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    if (targetContext.scriptable == nullptr)
    {
        return false;
    }

    state.argument = targetContext.scriptable->getAIContent();

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsAMount( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsAMount()
    /// @author ZZ
    /// @details This function passes if the Target is a mountable character

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    return target->isMount();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsAPlatform( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsAPlatform()
    /// @author ZZ
    /// @details This function passes if the Target is a platform character

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    return target->isPlatform();
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
uint8_t scr_IfTargetHasNotFullMana( script_state_t& state, ai_state_t& self )
{
    // IfTargetHasNotFullMana()
    /// @author ZF
    /// @details This function passes only if the Target is not at max mana and alive

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    return target->hasNotFullMana();
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
uint8_t scr_IfTargetIsAWeapon( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsAWeapon()
    /// @author ZF
    /// @details Proceeds if the AI Target Is a melee or ranged weapon

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    if (targetContext.info == nullptr)
    {
        return false;
    }

    return targetContext.info->isWeapon();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsASpell( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsASpell()
    /// @author ZF
    /// @details roceeds if the AI Target has any particle with the [IDAM] expansion

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    if (selfContext.appearance == nullptr)
    {
        return false;
    }

    return selfContext.appearance->hasIntellectDamageParticle();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetTargetDamageType( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetTargetDamageType()
    /// @author ZF
    /// @details This function gets the last type of damage for the Target

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    if (targetContext.scriptable == nullptr)
    {
        return false;
    }

    state.argument = targetContext.scriptable->getAILastDamageType();

    return true;
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


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsOwner( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsOwner()
    /// @author ZF
    /// @details This function proceeds only if the Target is the character's owner

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    return ( target->isAlive() && self.owner == self.getTarget() );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetCanSeeKurses( script_state_t& state, ai_state_t& self )
{
    // IfTargetCanSeeKurses()
    /// @author ZF
    /// @details Proceeds if the target can see kursed stuff.

    if (!resolveSelfContext(self).isResolved()) return false;

    const ITargetInfo* target = tryResolvedTargetInfo(self);
    if (target == nullptr)
    {
        return false;
    }

    return target->canSeeKurses();
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
uint8_t scr_IfTargetIsFacingSelf( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsFacingSelf()
    /// @author ZF
    /// @details This function proceeds if the target is more or less facing the character

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    if (targetContext.physical == nullptr || selfContext.physical == nullptr)
    {
        return false;
    }

    return isFacing(*targetContext.physical, *selfContext.physical);
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
