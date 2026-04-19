/// @file egolib/game/script_functions_target.c
/// @brief Target selection, target property queries, and order management

#include "egolib/game/script_functions_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetKilled( script_state_t& state, ai_state_t& self )
{
    // IfTargetKilled()
    /// @author ZZ
    /// @details This function proceeds if the character's target from last update was
    /// killed during this update

    SCRIPT_FUNCTION_BEGIN();

    IDamageable* damageableTarget = tryDamageable(self.getTarget());
    if (damageableTarget == nullptr)
    {
        return false;
    }

    // Proceed only if the character's target has just died or is already dead
    returncode = ( HAS_SOME_BITS( self.alert, ALERTIF_TARGETKILLED ) || !damageableTarget->isAlive() );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToNearbyEnemy( script_state_t& state, ai_state_t& self )
{
    // SetTargetToNearbyEnemy()
    /// @author ZZ
    /// @details This function sets the target to a nearby enemy, failing if there are none

    SCRIPT_FUNCTION_BEGIN();

    auto ichr = chr_find_target(pchr, NEARBY, IDSZ2::None, TARGET_ENEMIES);

    if ( objectHandler().exists(ichr) )
    {
        self.setTarget(ichr);
        returncode = true;
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToTargetLeftHand( script_state_t& state, ai_state_t& self )
{
    // SetTargetToTargetLeftHand()
    /// @author ZZ
    /// @details This function sets the target to the item in the target's left hand,
    /// failing if the target has no left hand item

    SCRIPT_FUNCTION_BEGIN();

    IInventoryHolder* targetInventory = tryInventoryHolder(self.getTarget());
    if (targetInventory == nullptr)
    {
        return false;
    }

    auto ichr = targetInventory->getHeldObject(SLOT_LEFT);
    returncode = false;
    if ( objectHandler().exists( ichr ) )
    {
        self.setTarget(ichr);
        returncode = true;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToTargetRightHand( script_state_t& state, ai_state_t& self )
{
    // SetTargetToTargetRightHand()
    /// @author ZZ
    /// @details This function sets the target to the item in the target's right hand,
    /// failing if the target has no right hand item

    SCRIPT_FUNCTION_BEGIN();

    IInventoryHolder* targetInventory = tryInventoryHolder(self.getTarget());
    if (targetInventory == nullptr)
    {
        return false;
    }

    auto ichr = targetInventory->getHeldObject(SLOT_RIGHT);
    returncode = false;
    if ( objectHandler().exists( ichr ) )
    {
        self.setTarget(ichr);
        returncode = true;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToWhoeverAttacked( script_state_t& state, ai_state_t& self )
{
    // SetTargetToWhoeverAttacked()
    /// @author ZZ
    /// @details This function sets the target to whoever attacked the character last, failing for damage tiles

    SCRIPT_FUNCTION_BEGIN();

    if ( objectHandler().exists( self.getLastAttacker() ) )
    {
        self.setTarget(self.getLastAttacker());
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToWhoeverBumped( script_state_t& state, ai_state_t& self )
{
    // SetTargetToWhoeverBumped()
    /// @author ZZ
    /// @details This function sets the target to whoever bumped the character last. It never fails

    SCRIPT_FUNCTION_BEGIN();

    if ( objectHandler().exists( self.getBumped() ) )
    {
        self.setTarget(self.getBumped());
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToWhoeverCalledForHelp( script_state_t& state, ai_state_t& self )
{
    // SetTargetToWhoeverCalledForHelp()
    /// @author ZZ
    /// @details This function sets the target to whoever called for help last.

    SCRIPT_FUNCTION_BEGIN();

    if ( VALID_TEAM_RANGE( pchr->getTeamRef() ) )
    {
        std::shared_ptr<Object> sissy = pchr->getTeam().getSissy();

        if ( sissy )
        {
            self.setTarget(sissy->getObjRef());
        }
        else
        {
            returncode = false;
        }
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToOldTarget( script_state_t& state, ai_state_t& self )
{
    // SetTargetToOldTarget()
    /// @author ZZ
    /// @details This function sets the target to the target from last update, used to
    /// undo other set_Target functions

    SCRIPT_FUNCTION_BEGIN();

    if ( objectHandler().exists( self.getOldTarget() ) )
    {
        self.setTarget(self.getOldTarget());
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetHasID( script_state_t& state, ai_state_t& self )
{
    // IfTargetHasID( tmpargument = "idsz" )
    /// @author ZZ
    /// @details This function proceeds if the target has either a parent or type IDSZ
    /// matching tmpargument.

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = targetInfo->hasTypeIDSZ(Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument));


    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetHasItemID( script_state_t& state, ai_state_t& self )
{
    // IfTargetHasItemID( tmpargument = "idsz" )
    /// @author ZZ
    /// @details This function proceeds if the target has a matching item in his/her
    /// pockets or hands.

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    IInventoryHolder* targetInventory = tryInventoryHolder(self.getTarget());
    if (targetInfo == nullptr || targetInventory == nullptr)
    {
        return false;
    }

    //Assume nothing is found
    returncode = false;

    const IDSZ2 itemId = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);

    //Check hands
    if (targetInfo->wieldsItemIDSZ(itemId)) {
        returncode = true;
    }

    //Check inventory
    if (!returncode) {
        if (ObjectRef::Invalid != Inventory::findItem(*targetInventory, itemId, false)) {
            returncode = true;
        }
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetHoldingItemID( script_state_t& state, ai_state_t& self )
{
    // IfTargetHoldingItemID( tmpargument = "idsz" )
    /// @author ZZ
    /// @details This function proceeds if the target has a matching item in his/her
    /// hands.  It also sets tmpargument to the proper latch button to press
    /// to use that item

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = targetInfo->wieldsItemIDSZ(Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetHasSkillID( script_state_t& state, ai_state_t& self )
{
    // IfTargetHasSkillID( tmpargument = "skill idsz" )
    /// @author ZZ
    /// @details This function proceeds if ID matches tmpargument

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = targetInfo->hasSkillIDSZ(Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IssueOrder( script_state_t& state, ai_state_t& self )
{
    // IssueOrder( tmpargument = "order"  )
    /// @author ZZ
    /// @details This function tells all of the character's teammates to do something,
    /// though each teammate needs to interpret the order using IfOrdered in
    /// its own script.

    SCRIPT_FUNCTION_BEGIN();

    issue_order( self.getSelf(), state.argument );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetCanOpenStuff( script_state_t& state, ai_state_t& self )
{
    // IfTargetCanOpenStuff()
    /// @author ZZ
    /// @details This function proceeds if the target can open stuff ( set in data.txt )
    /// Used by chests and buttons and such so only "smart" creatures can operate
    /// them

    SCRIPT_FUNCTION_BEGIN();
    returncode = false;

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    IInventoryHolder* targetInventory = tryInventoryHolder(self.getTarget());
    if (targetInfo == nullptr || targetInventory == nullptr)
    {
        return false;
    }

    if ( targetInfo->isMount() )
    {
        const ITargetInfo* rider = tryTargetInfo(targetInventory->getHeldObject(SLOT_LEFT));

        if (rider != nullptr)
        {
            // can the rider open stuff
            returncode = rider->canOpenStuff();
        }
    }

    if ( !returncode )
    {
        // if a rider can't openstuff, can the target openstuff?
        returncode = targetInfo->canOpenStuff();
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToWhoeverIsHolding( script_state_t& state, ai_state_t& self )
{
    // SetTargetToWhoeverIsHolding()
    /// @author ZZ
    /// @details This function sets the target to the character's holder or mount,
    /// failing if the character has no mount or holder

    SCRIPT_FUNCTION_BEGIN();

    if ( objectHandler().exists( pchr->getHolderRef() ) )
    {
        self.setTarget(pchr->getHolderRef());
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsOnOtherTeam( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsOnOtherTeam()
    /// @author ZZ
    /// @details This function proceeds if the target is on another team

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = ( targetInfo->isAlive() && !targetInfo->isOnSameTeam(pchr->getTeamRef()) );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsOnHatedTeam( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsOnHatedTeam()
    /// @author ZZ
    /// @details This function proceeds if the target is on an enemy team

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    IDamageable* damageableTarget = tryDamageable(self.getTarget());
    if (targetInfo == nullptr || damageableTarget == nullptr)
    {
        return false;
    }

    returncode = ( targetInfo->isAlive() && targetInfo->isHatedByTeam(pchr->getTeamRef()) && !damageableTarget->isInvincible() );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToTargetOfLeader( script_state_t& state, ai_state_t& self )
{
    // SetTargetToTargetOfLeader()
    /// @author ZZ
    /// @details This function sets the character's target to the target of its leader,
    /// or it fails with no change if the leader is dead

    SCRIPT_FUNCTION_BEGIN();

    if ( VALID_TEAM_RANGE( pchr->getTeamRef() ) )
    {
        const std::shared_ptr<Object> &leader = activeModule().getTeamList()[pchr->getTeamRef()].getLeader();

        if ( leader )
        {
            IScriptable* scriptableLeader = tryScriptable(leader->getObjRef());
            if (scriptableLeader == nullptr)
            {
                return false;
            }

            auto itarget = scriptableLeader->getAITarget();

            if ( objectHandler().exists( itarget ) )
            {
                self.setTarget( itarget );
            }
            else
            {
                returncode = false;
            }
        }
        else
        {
            returncode = false;
        }
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsOldTarget( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsOldTarget()
    /// @author ZZ
    /// @details This function proceeds if the target is the same as it was last update

    SCRIPT_FUNCTION_BEGIN();

    returncode = ( self.getTarget() == self.getOldTarget() );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToLeader( script_state_t& state, ai_state_t& self )
{
    // SetTargetToLeader()
    /// @author ZZ
    /// @details This function sets the target to the leader, proceeding if their is
    /// a valid leader for the character's team

    SCRIPT_FUNCTION_BEGIN();

    returncode = false;
    if ( VALID_TEAM_RANGE( pchr->getTeamRef() ) )
    {
        const std::shared_ptr<Object> &leader = activeModule().getTeamList()[pchr->getTeamRef()].getLeader();
        if ( leader )
        {
            self.setTarget(leader->getObjRef());
            returncode = true;
        }
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetOldTarget( script_state_t& state, ai_state_t& self )
{
    // SetOldTarget()
    /// @author ZZ
    /// @details This function sets the old target to the current target.  To allow
    /// greater manipulations of the target

    SCRIPT_FUNCTION_BEGIN();

    self.setOldTarget(self.getTarget());

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetHasVulnerabilityID( script_state_t& state, ai_state_t& self )
{
    // IfTargetHasVulnerabilityID( tmpargument = "vulnerability idsz" )
    /// @author ZZ
    /// @details This function proceeds if the target is vulnerable to the given IDSZ.
    
    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = targetInfo->matchesVulnerabilityIDSZ(Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsHurt( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsHurt()
    /// @author ZZ
    /// @details This function passes only if the target is hurt and alive

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = targetInfo->isHurt();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsAPlayer( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsAPlayer()
    /// @author ZZ
    /// @details This function proceeds if the target is controlled by a human ( may not be local )

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = targetInfo->isPlayer();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsAlive( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsAlive()
    /// @author ZZ
    /// @details This function proceeds if the target is alive

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = targetInfo->isAlive();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsSelf( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsSelf()
    /// @author ZZ
    /// @details This function proceeds if the character is targeting itself

    SCRIPT_FUNCTION_BEGIN();

    returncode = ( self.getTarget() == self.getSelf() );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsMale( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsMale()
    /// @author ZZ
    /// @details This function proceeds only if the target is male

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = ( targetInfo->getGender() == Gender::Male );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsFemale( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsFemale()
    /// @author ZZ
    /// @details This function proceeds if the target is female

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = ( targetInfo->getGender() == Gender::Female );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToSelf( script_state_t& state, ai_state_t& self )
{
    // SetTargetToSelf()
    /// @author ZZ
    /// @details This function sets the target to the character itself

    SCRIPT_FUNCTION_BEGIN();

    self.setTarget(self.getSelf());

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToRider( script_state_t& state, ai_state_t& self )
{
    // SetTargetToRider()
    /// @author ZZ
    /// @details This function sets the target to whoever is riding the character (left/only grip),
    /// failing if there is no rider

    SCRIPT_FUNCTION_BEGIN();

    IInventoryHolder* selfInventory = tryInventoryHolder(self.getSelf());
    if (selfInventory == nullptr)
    {
        return false;
    }

    const ObjectRef riderRef = selfInventory->getHeldObject(SLOT_LEFT);
    if ( objectHandler().exists(riderRef) )
    {
        self.setTarget(riderRef);
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetAttackTurn( script_state_t& state, ai_state_t& self )
{
    // tmpturn = GetAttackTurn()
    /// @author ZZ
    /// @details This function sets tmpturn to the direction from which the last attack
    /// came. Not particularly useful in most cases, but it could be.

    SCRIPT_FUNCTION_BEGIN();

    state.turn = FACING_T(self.directionlast);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetDamageType( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetDamageType()
    /// @author ZZ
    /// @details This function sets tmpargument to the damage type of the last attack that
    /// hit the character

    SCRIPT_FUNCTION_BEGIN();

    state.argument = self.damagetypelast;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_TranslateOrder( script_state_t& state, ai_state_t& self )
{
    // tmpx,tmpy,tmpargument = TranslateOrder()
    /// @author ZZ
    /// @details This function translates a packed order into understandable values.
    /// See CreateOrder for more.  This function sets tmpx, tmpy, tmpargument,
    /// and sets the target ( if valid )

    SCRIPT_FUNCTION_BEGIN();

    auto ichr = ObjectRef(Ego::Math::clipBits<16>( self.order_value >> 24 ));

    if ( objectHandler().exists( ichr ) )
    {
        self.setTarget( ichr );

        state.x        = (( self.order_value >> 14 ) & 0x03FF ) << 6;
        state.y        = (( self.order_value >>  4 ) & 0x03FF ) << 6;
        state.argument = (( self.order_value >>  0 ) & 0x000F );
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToWhoeverWasHit( script_state_t& state, ai_state_t& self )
{
    // SetTargetToWhoeverWasHit()
    /// @author ZZ
    /// @details This function sets the target to whoever was hit by the character last

    SCRIPT_FUNCTION_BEGIN();

    if ( objectHandler().exists( self.hitlast ) )
    {
        self.setTarget(self.hitlast);
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToWideEnemy( script_state_t& state, ai_state_t& self )
{
    // SetTargetToWideEnemy()
    /// @author ZZ
    /// @details This function sets the target to an enemy in the vicinity around the
    /// character, failing if there are none

    SCRIPT_FUNCTION_BEGIN();

    auto ichr = chr_find_target( pchr, WIDE, IDSZ2::None, TARGET_ENEMIES );

    if ( objectHandler().exists( ichr ) )
    {
        self.setTarget( ichr );
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetHasSpecialID( script_state_t& state, ai_state_t& self )
{
    // IfTargetHasSpecialID( tmpargument = "special idsz" )
    /// @author ZZ
    /// @details This function proceeds if the character has a special IDSZ ( in data.txt )

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = targetInfo->matchesSpecialIDSZ(Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetTargetGrogTime( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetTargetGrogTime()
    /// @author ZZ
    /// @details This function sets tmpargument to the number of updates before the
    /// character is ungrogged, proceeding if the number is greater than 0

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    state.argument = targetInfo->getGrogTimer();

    returncode = ( 0 != state.argument );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetTargetDazeTime( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetTargetDazeTime()
    /// @author ZZ
    /// @details This function sets tmpargument to the number of updates before the
    /// character is undazed, proceeding if the number is greater than 0

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    state.argument = targetInfo->getDazeTimer();

    returncode = ( 0 != state.argument );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsOnSameTeam( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsOnSameTeam()
    /// @author ZZ
    /// @details This function proceeds if the target is on the character's team

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = targetInfo->isOnSameTeam(pchr->getTeamRef());

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetHasAnyID( script_state_t& state, ai_state_t& self )
{
    // IfTargetHasAnyID( tmpargument = "idsz" )
    /// @author ZZ
    /// @details This function proceeds if the target has any IDSZ that matches the given one

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = targetInfo->hasAnyIDSZ(Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsDefending( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsDefending()
    /// @author ZZ
    /// @details This function proceeds if the target is holding up a shield or similar
    /// defense

    Object * pself_target;

    SCRIPT_FUNCTION_BEGIN();

    SCRIPT_REQUIRE_TARGET( pself_target );

    returncode = ACTION_IS_TYPE( pself_target->getCurrentAnimation(), P );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsAttacking( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsAttacking()
    /// @author ZZ
    /// @details This function proceeds if the target is doing an attack action

    Object * pself_target;

    SCRIPT_FUNCTION_BEGIN();

    SCRIPT_REQUIRE_TARGET( pself_target );

    returncode = pself_target->isAttacking();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsKursed( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsKursed()
    /// @author ZZ
    /// @details This function proceeds if the target is kursed

    Object * pself_target;

    SCRIPT_FUNCTION_BEGIN();

    SCRIPT_REQUIRE_TARGET( pself_target );

    returncode = pself_target->isKursed();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsDressedUp( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsDressedUp()
    /// @author ZZ
    /// @details This function proceeds if the target is dressed in fancy clothes

    SCRIPT_FUNCTION_BEGIN();

    returncode = ppro->getSkinInfo(pchr->getSkin()).dressy;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfDistanceIsMoreThanTurn( script_state_t& state, ai_state_t& self )
{
    // IfDistanceIsMoreThanTurn()
    /// @author ZZ
    /// @details This function proceeds tmpdistance is greater than tmpturn

    SCRIPT_FUNCTION_BEGIN();

    returncode = ( state.distance > state.turn );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToLowestTarget( script_state_t& state, ai_state_t& self )
{
    // SetTargetToLowestTarget()
    /// @author ZZ
    /// @details This function sets the target to the absolute bottom character.
    /// The holder of the target, or the holder of the holder of the target, or
    /// the holder of the holder of ther holder of the target, etc.   This function never fails

    SCRIPT_FUNCTION_BEGIN();

	auto itarget = chr_get_lowest_attachment( self.getTarget(), false );

    if ( objectHandler().exists( itarget ) )
    {
        self.setTarget(itarget);
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetHasItemIDEquipped( script_state_t& state, ai_state_t& self )
{
    // IfTargetHasItemIDEquipped( tmpargument = "item idsz" )
    /// @author ZZ
    /// @details This function proceeds if the target already wearing a matching item

    SCRIPT_FUNCTION_BEGIN();

    IInventoryHolder* targetInventory = tryInventoryHolder(self.getTarget());
    if (targetInventory == nullptr)
    {
        return false;
    }

	auto iitem = Inventory::findItem(*targetInventory, Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument), true );

    returncode = objectHandler().exists(iitem);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetOwnerToTarget( script_state_t& state, ai_state_t& self )
{
    // SetOwnerToTarget()
    /// @author ZZ
    /// @details This function must be called before enchanting anything.
    /// The owner is the character that pays the sustain costs and such for the enchantment

    SCRIPT_FUNCTION_BEGIN();

    self.owner = self.getTarget();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToOwner( script_state_t& state, ai_state_t& self )
{
    // SetTargetToOwner()
    /// @author ZZ
    /// @details This function sets the target to whoever was previously declared as the
    /// owner.

    SCRIPT_FUNCTION_BEGIN();

    if ( objectHandler().exists( self.owner ) )
    {
        self.setTarget(self.owner);
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToWideBlahID( script_state_t& state, ai_state_t& self )
{
    // SetTargetToWideBlahID( tmpargument = "idsz", tmpdistance = "blah bits" )
    /// @author ZZ
    /// @details This function sets the target to a character that matches the description,
    /// and who is located in the general vicinity of the character

    SCRIPT_FUNCTION_BEGIN();

    // Try to find one
    auto ichr = chr_find_target( pchr, WIDE, state.argument, state.distance );

    if ( objectHandler().exists( ichr ) )
    {
        self.setTarget( ichr );
        returncode = true;
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfFacingTarget( script_state_t& state, ai_state_t& self )
{
    // IfFacingTarget()
    /// @author ZZ
    /// @details This function proceeds if the character is more or less facing its
    /// target

    Object *  pself_target;

    SCRIPT_FUNCTION_BEGIN();

    const IPhysical* physicalTarget = tryPhysical(self.getTarget());
    if (physicalTarget == nullptr)
    {
        return false;
    }

    returncode = pchr->isFacingLocation(physicalTarget->getPosX(), physicalTarget->getPosY());

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToDistantEnemy( script_state_t& state, ai_state_t& self )
{
    // SetTargetToDistantEnemy( tmpdistance = "distance" )
    /// @author ZZ
    /// @details This function finds a character within a certain distance of the
    /// character, failing if there are none

    SCRIPT_FUNCTION_BEGIN();

    auto ichr = chr_find_target( pchr, state.distance, IDSZ2::None, TARGET_ENEMIES );

    if ( objectHandler().exists( ichr ) )
    {
        self.setTarget(ichr);
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsMounted( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsMounted()
    /// @author ZZ
    /// @details This function proceeds if the target is riding a mount

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = false;

    const ITargetInfo* holderInfo = tryTargetInfo(targetInfo->getHolderRef());
    if ( holderInfo != nullptr )
    {
        returncode = holderInfo->isMount();
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_OrderTarget( script_state_t& state, ai_state_t& self )
{
    // OrderTarget( tmpargument = "order" )
    /// @author ZZ
    /// @details This function issues an order to the given target
    /// Be careful in using this, always checking IDSZ first

    SCRIPT_FUNCTION_BEGIN();

    IScriptable* scriptableTarget = tryScriptable(self.getTarget());
    if (scriptableTarget == nullptr)
    {
        return false;
    }

    returncode = scriptableTarget->addAIOrder(state.argument, 0);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToWhoeverIsInPassage( script_state_t& state, ai_state_t& self )
{
    // SetTargetToWhoeverIsInPassage()
    /// @author ZZ
    /// @details This function sets the target to whoever is blocking the given passage
    /// This function lets passage rectangles be used as event triggers

    SCRIPT_FUNCTION_BEGIN();

    std::shared_ptr<Passage> passage = activeModule().getPassageByID(state.argument);

    returncode = false;
    if(passage)
    {
        auto objRef = passage->whoIsBlockingPassage(self.getSelf(), IDSZ2::None, TARGET_SELF | TARGET_FRIENDS | TARGET_ENEMIES, IDSZ2::None);

        if (objectHandler().exists(objRef))
        {
            self.setTarget(objRef);
            returncode = true;
        }
    }

    SCRIPT_FUNCTION_END();
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

    SCRIPT_FUNCTION_BEGIN();

    sTmp = ( REF_TO_INT( self.getTarget().get() ) & 0x00FF ) << 24;
    sTmp |= (( state.x >> 6 ) & 0x03FF ) << 14;
    sTmp |= (( state.y >> 6 ) & 0x03FF ) << 4;
    sTmp |= ( state.argument & 0x000F );
    state.argument = sTmp;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_OrderSpecialID( script_state_t& state, ai_state_t& self )
{
    // OrderSpecialID( tmpargument = "compressed order", tmpdistance = "idsz" )
    /// @author ZZ
    /// @details This function orders all characters with the given special IDSZ.

    SCRIPT_FUNCTION_BEGIN();

    issue_special_order( state.argument, state.distance );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsSneaking( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsSneaking()
    /// @author ZZ
    /// @details This function proceeds if the target is doing ACTION_WA or ACTION_DA

    Object * pself_target;

    SCRIPT_FUNCTION_BEGIN();

    SCRIPT_REQUIRE_TARGET( pself_target );

    returncode = ( pself_target->getCurrentAnimation() == ACTION_DA || pself_target->getCurrentAnimation() == ACTION_WA );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetCanSeeInvisible( script_state_t& state, ai_state_t& self )
{
    // IfTargetCanSeeInvisible()
    /// @author ZZ
    /// @details This function proceeds if the target can see invisible

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = targetInfo->canSeeInvisible();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToNearestBlahID( script_state_t& state, ai_state_t& self )
{
    // SetTargetToNearestBlahID( tmpargument = "idsz", tmpdistance = "blah bits" )

    /// @author ZZ
    /// @details This function finds the NEAREST ( exact ) character that fits the given
    /// parameters, failing if it finds none

    SCRIPT_FUNCTION_BEGIN();

    // Try to find one
    auto ichr = chr_find_target(pchr, NEAREST, IDSZ2(state.argument), state.distance);

    if (objectHandler().exists(ichr))
    {
        self.setTarget( ichr );
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToNearestEnemy( script_state_t& state, ai_state_t& self )
{
    // SetTargetToNearestEnemy()
    /// @author ZZ
    /// @details This function finds the NEAREST ( exact ) enemy, failing if it finds none

    SCRIPT_FUNCTION_BEGIN();

    auto ichr = chr_find_target( pchr, NEAREST, IDSZ2::None, TARGET_ENEMIES );

    if ( objectHandler().exists( ichr ) )
    {
        self.setTarget( ichr );
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToNearestFriend( script_state_t& state, ai_state_t& self )
{
    // SetTargetToNearestFriend()
    /// @author ZZ
    /// @details This function finds the NEAREST ( exact ) friend, failing if it finds none

    SCRIPT_FUNCTION_BEGIN();

    auto ichr = chr_find_target( pchr, NEAREST, IDSZ2::None, TARGET_FRIENDS );

    if ( objectHandler().exists( ichr ) )
    {
        self.setTarget( ichr );
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToNearestLifeform( script_state_t& state, ai_state_t& self )
{
    // SetTargetToNearestLifeform()

    /// @author ZZ
    /// @details This function finds the NEAREST ( exact ) friend or enemy, failing if it
    /// finds none

    SCRIPT_FUNCTION_BEGIN();

    auto ichr = chr_find_target( pchr, NEAREST, IDSZ2::None, TARGET_ITEMS | TARGET_FRIENDS | TARGET_ENEMIES );

    if ( objectHandler().exists( ichr ) )
    {
        self.setTarget( ichr );
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsFlying( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsFlying()
    /// @author ZZ
    /// @details This function proceeds if the character target is flying

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = targetInfo->isFlying();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetTargetState( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetTargetState()
    /// @author ZZ
    /// @details This function sets tmpargument to the state of the target

    SCRIPT_FUNCTION_BEGIN();

    IScriptable* scriptableTarget = tryScriptable(self.getTarget());
    if (scriptableTarget == nullptr)
    {
        return false;
    }

    state.argument = scriptableTarget->getAIStateValue();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetTargetContent( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetTargetContent()
    // This sets tmpargument to the current Target's content value

    SCRIPT_FUNCTION_BEGIN();

    IScriptable* scriptableTarget = tryScriptable(self.getTarget());
    if (scriptableTarget == nullptr)
    {
        return false;
    }

    state.argument = scriptableTarget->getAIContent();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsAMount( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsAMount()
    /// @author ZZ
    /// @details This function passes if the Target is a mountable character

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = targetInfo->isMount();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsAPlatform( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsAPlatform()
    /// @author ZZ
    /// @details This function passes if the Target is a platform character

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = targetInfo->isPlatform();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToPassageID( script_state_t& state, ai_state_t& self )
{
    // SetTargetToPassageID( tmpargument = "passage", tmpdistance = "idsz" )
    /// @author ZZ
    /// @details This function finds a character who is both in the passage and who has
    /// an item with the given IDSZ

    SCRIPT_FUNCTION_BEGIN();

    std::shared_ptr<Passage> passage = activeModule().getPassageByID(state.argument);

    returncode = false;
    if(passage) {
        ObjectRef objRef = passage->whoIsBlockingPassage(self.getSelf(), IDSZ2::None, TARGET_SELF | TARGET_FRIENDS | TARGET_ENEMIES, state.distance);
        if ( objectHandler().exists(objRef) )
        {
            self.setTarget(objRef);
            returncode = true;
        }
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetHasNotFullMana( script_state_t& state, ai_state_t& self )
{
    // IfTargetHasNotFullMana()
    /// @author ZF
    /// @details This function passes only if the Target is not at max mana and alive

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = targetInfo->hasNotFullMana();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToLastItemUsed( script_state_t& state, ai_state_t& self )
{
    // SetTargetToLastItemUsed()
    /// @author ZF
    /// @details This sets the Target to the last item the character used

    SCRIPT_FUNCTION_BEGIN();

    if ( self.lastitemused != self.getSelf() && objectHandler().exists( self.lastitemused ) )
    {
        self.setTarget(self.lastitemused);
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsAWeapon( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsAWeapon()
    /// @author ZF
    /// @details Proceeds if the AI Target Is a melee or ranged weapon

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = targetInfo->isWeapon();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsASpell( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsASpell()
    /// @author ZF
    /// @details roceeds if the AI Target has any particle with the [IDAM] expansion

    SCRIPT_FUNCTION_BEGIN();

    returncode = false;
    for (LocalParticleProfileRef iTmp(0); iTmp.get() < MAX_PIP_PER_PROFILE; ++iTmp)
    {
        std::shared_ptr<ParticleProfile> ppip = EngineContext::get().profileSystem().getParticleProfile(pchr->getProfile()->getParticleProfile(iTmp));
        if (!ppip) continue;

        if (ppip->_intellectDamageBonus)
        {
            returncode = true;
            break;
        }
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetTargetDamageType( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetTargetDamageType()
    /// @author ZF
    /// @details This function gets the last type of damage for the Target

    SCRIPT_FUNCTION_BEGIN();

    IScriptable* scriptableTarget = tryScriptable(self.getTarget());
    if (scriptableTarget == nullptr)
    {
        return false;
    }

    state.argument = scriptableTarget->getAILastDamageType();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetHasQuest( script_state_t& state, ai_state_t& self )
{
    // tmpdistance = IfTargetHasQuest( tmpargument = "quest idsz )
    /// @author ZF
    /// @details This function proceeds if the Target has the unfinIshed quest specified in tmpargument
    /// and sets tmpdistance to the Quest Level of the specified quest.

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = false;

    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);
    if(targetInfo->isPlayer()) {
        const std::shared_ptr<Ego::Player>& player = activeModule().getPlayer(targetInfo->getPlayerNumber());

        // only find active quests
        if(player->getQuestLog().hasActiveQuest(idsz)) {
            returncode = true;            
            state.distance = player->getQuestLog()[idsz];
        }
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsOwner( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsOwner()
    /// @author ZF
    /// @details This function proceeds only if the Target is the character's owner

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = ( targetInfo->isAlive() && self.owner == self.getTarget() );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetCanSeeKurses( script_state_t& state, ai_state_t& self )
{
    // IfTargetCanSeeKurses()
    /// @author ZF
    /// @details Proceeds if the target can see kursed stuff.

    SCRIPT_FUNCTION_BEGIN();

    const ITargetInfo* targetInfo = tryTargetInfo(self.getTarget());
    if (targetInfo == nullptr)
    {
        return false;
    }

    returncode = targetInfo->canSeeKurses();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToBlahInPassage( script_state_t& state, ai_state_t& self )
{
    // SetTargetToBlahInPassage()
    /// @author ZF
    /// @details This function sets the target to whatever object with the specified bits
    /// in tmpdistance is blocking the given passage. This function lets passage rectangles be used as event triggers

    SCRIPT_FUNCTION_BEGIN();

    std::shared_ptr<Passage> passage = activeModule().getPassageByID(state.argument);
    returncode = false;
    if(passage) {
        auto objRef = passage->whoIsBlockingPassage(self.getSelf(), state.turn, TARGET_SELF | state.distance, IDSZ2::None );

        if ( objectHandler().exists(objRef) )
        {
            self.setTarget(objRef);
            returncode = true;
        }
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfTargetIsFacingSelf( script_state_t& state, ai_state_t& self )
{
    // IfTargetIsFacingSelf()
    /// @author ZF
    /// @details This function proceeds if the target is more or less facing the character

    SCRIPT_FUNCTION_BEGIN();

    const IPhysical* physicalTarget = tryPhysical(self.getTarget());
    if (physicalTarget == nullptr)
    {
        return false;
    }

	FACING_T sTmp = 0;
    sTmp = FACING_T(vec_to_facing( pchr->getPosX() - physicalTarget->getPosX() , pchr->getPosY() - physicalTarget->getPosY() ));
    sTmp -= FACING_T(physicalTarget->getFacingZ());
    returncode = ( sTmp > 55535 || sTmp < 10000 );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToNearbyMeleeWeapon( script_state_t& state, ai_state_t& self )
{
    SCRIPT_FUNCTION_BEGIN();

    ObjectRef best_target = FindWeapon( pchr, WIDE, IDSZ2('X', 'W', 'E', 'P'), false, true );

    //Did we find anything good?
    if ( objectHandler().exists( best_target ) )
    {
        self.setTarget(best_target);
        returncode = true;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToDistantFriend( script_state_t& state, ai_state_t& self )
{
    // SetTargetToDistantFriend( tmpdistance = "distance" )
    /// @author ZF
    /// @details This function finds a character within a certain distance of the
    /// character, failing if there are none

    SCRIPT_FUNCTION_BEGIN();

    auto ichr = chr_find_target(pchr, state.distance, IDSZ2::None, TARGET_FRIENDS);

    if (objectHandler().exists(ichr))
    {
        self.setTarget(ichr);
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}
