/// @file egolib/game/script_functions_target.c
/// @brief Target state predicates (Is*/Facing/Distance). Companion TUs hold the
///        IDSZ identity queries (script_functions_target_identity.c) and the
///        order/getter/mutator ops (script_functions_target_orders.c) since the
///        3-way split on 2026-06-12.

#include "egolib/game/script_functions_target_impl.h"

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
uint8_t scr_IfTargetIsOnOtherTeam( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsOnOtherTeam()
    /// @author ZZ
    /// @details This function proceeds if the target is on another team

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    if (targetContext.info == nullptr ||
        targetContext.damageable == nullptr ||
        selfContext.info == nullptr)
    {
        return false;
    }

    return ( targetContext.damageable->isAlive() &&
             !targetContext.info->isOnSameTeam(selfContext.info->getTeamRef()) );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsOnHatedTeam( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsOnHatedTeam()
    /// @author ZZ
    /// @details This function proceeds if the target is on an enemy team

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    const SelfTargetSelectorContext selfContext = makeSelfTargetSelectorContext(self);
    if (targetContext.info == nullptr ||
        targetContext.damageable == nullptr ||
        selfContext.info == nullptr)
    {
        return false;
    }

    return targetContext.damageable->isAlive() &&
           targetContext.info->isHatedByTeam(selfContext.info->getTeamRef()) &&
           !targetContext.damageable->isInvincible();
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

    return tryLivingDamageable(self.getTarget()) != nullptr;
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
uint8_t scr_IfTargetIsOwner( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsOwner()
    /// @author ZF
    /// @details This function proceeds only if the Target is the character's owner

    if (!resolveSelfContext(self).isResolved()) return false;

    return tryLivingDamageable(self.getTarget()) != nullptr &&
           self.owner == self.getTarget();
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
