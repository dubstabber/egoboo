/// @file egolib/game/script_functions_target.c
/// @brief Target property queries, order management, and miscellaneous target operations

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
uint8_t scr_IfTargetIsOldTarget( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsOldTarget()
    /// @author ZZ
    /// @details This function proceeds if the target is the same as it was last update

    if (!resolveSelfContext(self).isResolved()) return false;

    return ( self.getTarget() == self.getOldTarget() );
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
