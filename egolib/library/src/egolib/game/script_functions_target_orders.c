/// @file egolib/game/script_functions_target_orders.c
/// @brief Order management and miscellaneous target getters/mutators. Split off
///        script_functions_target.c on 2026-06-12 (13 entries: IssueOrder, OrderTarget,
///        OrderSpecialID, CreateOrder, TranslateOrder, SetOldTarget, GetAttackTurn,
///        GetDamageType, GetTargetGrogTime, GetTargetDazeTime, GetTargetState,
///        GetTargetContent, GetTargetDamageType).
/// @details Shared infrastructure (SelfTargetSelectorContext / TargetCompatibilityContext /
///          isFacing / makeSelfTargetSelectorContext / makeTargetCompatibilityContext /
///          tryResolvedTargetInfo and friends) lives in script_functions_target_impl.h.

#include "egolib/game/script_functions_target_impl.h"

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


