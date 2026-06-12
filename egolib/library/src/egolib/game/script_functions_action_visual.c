/// @file egolib/game/script_functions_action_visual.c
/// @brief Visual-effect dispatch entries — color shifts, alpha/light tinting, flash, sparkle,
///        billboard rendering, charge-bar display. Split off script_functions_action.c on
///        2026-06-12 (11 entries, ~330 lines).
/// @details Shared infrastructure (SelfActionContext / makeSelfActionContext / gameSession)
///          lives in script_functions_action_internal.h. Visual-only helpers (billboardSystem,
///          tryMakeBillboard, resolveChargeTarget) live in this TU's anonymous namespace.

#include "egolib/game/script_functions_action_internal.h"

namespace
{
Ego::Graphics::IBillboardSystem& billboardSystem()
{
    return EngineContext::get().billboardSystem();
}

std::shared_ptr<Ego::Graphics::Billboard> tryMakeBillboard(const SelfActionContext& context,
                                                           const std::string& text,
                                                           const Ego::Colour4f& textColor,
                                                           const Ego::Colour4f& tint,
                                                           int lifetime)
{
    return billboardSystem().makeBillboard(context.selfRef,
                                           text,
                                           textColor,
                                           tint,
                                           lifetime,
                                           Ego::Graphics::Billboard::Flags::Fade);
}

const ITargetInfo* resolveChargeTarget(const SelfActionContext& selfContext)
{
    if (!selfContext.isResolved())
    {
        return nullptr;
    }

    const ITargetInfo* chargeTarget = selfContext.targetInfo;
    if (!chargeTarget->isPlayer() && chargeTarget->isBeingHeld())
    {
        chargeTarget = tryTargetInfo(chargeTarget->getHolderRef());
    }

    return chargeTarget;
}
}


//--------------------------------------------------------------------------------------------
uint8_t scr_FlashTarget( script_state_t& state, ai_state_t& self )
{
    // FlashTarget()
    /// @author ZZ
    /// @details This function makes the target flash

    if (!resolveSelfContext(self).isResolved()) return false;

    IVisualControl* targetVisual = tryVisualControl(self.getTarget());
    if (targetVisual == nullptr)
    {
        return false;
    }

    targetVisual->flash(255);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetRedShift( script_state_t& state, ai_state_t& self )
{
    // SetRedShift( tmpargument = "red darkening" )
    /// @author ZZ
    /// @details This function sets the character's red shift ( 0 - 3 ), higher values
    /// making the character less red and darker

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    selfContext.visual->setRedShift(Ego::Math::constrain(state.argument, 0, 6));

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetGreenShift( script_state_t& state, ai_state_t& self )
{
    // SetGreenShift( tmpargument = "green darkening" )
    /// @author ZZ
    /// @details This function sets the character's green shift ( 0 - 3 ), higher values
    /// making the character less green and darker

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    selfContext.visual->setGreenShift(Ego::Math::constrain(state.argument, 0, 6));

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetBlueShift( script_state_t& state, ai_state_t& self )
{
    // SetBlueShift( tmpargument = "blue darkening" )
    /// @author ZZ
    /// @details This function sets the character's blue shift ( 0 - 3 ), higher values
    /// making the character less blue and darker

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    selfContext.visual->setBlueShift(Ego::Math::constrain(state.argument, 0, 6));

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetLight( script_state_t& state, ai_state_t& self )
{
    // SetLight( tmpargument = "lighness" )
    /// @author ZZ
    /// @details This function alters the character's transparency ( 0 - 254 )
    /// 255 = no transparency

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    selfContext.visual->setLight(state.argument);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetAlpha( script_state_t& state, ai_state_t& self )
{
    // SetAlpha( tmpargument = "alpha" )
    /// @author ZZ
    /// @details This function alters the character's transparency ( 0 - 255 )
    /// 255 = no transparency

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    selfContext.visual->setAlpha(state.argument);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_BlackTarget( script_state_t& state, ai_state_t& self )
{
    // BlackTarget()
    /// @author ZZ
    /// @details  The opposite of FlashTarget, causing the target to turn black

    if (!resolveSelfContext(self).isResolved()) return false;

    IVisualControl* targetVisual = tryVisualControl(self.getTarget());
    if (targetVisual == nullptr)
    {
        return false;
    }

    targetVisual->flash(0);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SparkleIcon( script_state_t& state, ai_state_t& self )
{
    // SparkleIcon( tmpargument = "color" )
    /// @author ZZ
    /// @details This function starts little sparklies going around the character's icon

    if (!resolveSelfContext(self).isResolved()) return false;
    const SelfActionContext selfContext = makeSelfActionContext(self);
    if ( state.argument < COLOR_MAX )
    {
        if ( state.argument < -1 )
        {
            selfContext.visual->setSparkle(NOSPARKLE);
        }
        else
        {
            selfContext.visual->setSparkle(state.argument % COLOR_MAX);
        }
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_UnsparkleIcon( script_state_t& state, ai_state_t& self )
{
    // UnsparkleIcon()
    /// @author ZZ
    /// @details This function stops little sparklies going around the character's icon

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    selfContext.visual->setSparkle(NOSPARKLE);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DrawBillboard( script_state_t& state, ai_state_t& self )
{
    // DrawBillboard( tmpargument = "message", tmpdistance = "duration", tmpturn = "color" )
    /// @author ZF
    /// @details This function draws one of those billboards above the character

    const auto text_color = Ego::Colour4f(Ego::Colour4b(255, 255, 255, 255));

    //List of avalible colours
    const auto tint_red  = Ego::Colour4f{ 1.00f, 0.25f, 0.25f, 1.00f };
    const auto tint_purple = Ego::Colour4f{ 0.88f, 0.75f, 1.00f, 1.00f };
    const auto tint_white = Ego::Colour4f{ 1.00f, 1.00f, 1.00f, 1.00f };
    const auto tint_yellow = Ego::Colour4f{ 1.00f, 1.00f, 0.75f, 1.00f };
    const auto tint_green = Ego::Colour4f{ 0.25f, 1.00f, 0.25f, 1.00f };
    const auto tint_blue = Ego::Colour4f{ 0.25f, 0.25f, 1.00f, 1.00f };

    if (!resolveSelfContext(self).isResolved()) return false;
    const SelfActionContext selfContext = makeSelfActionContext(self);

    if ( !selfContext.hasMessageID(state.argument) ) return false;

    auto* tint = &tint_white;
    //Figure out which color to use
    switch ( state.turn )
    {
        case COLOR_WHITE:   tint = &tint_white;   break;
        case COLOR_RED:     tint = &tint_red;     break;
        case COLOR_PURPLE:  tint = &tint_purple;  break;
        case COLOR_YELLOW:  tint = &tint_yellow;  break;
        case COLOR_GREEN:   tint = &tint_green;   break;
        case COLOR_BLUE:    tint = &tint_blue;    break;
    }

    return nullptr != tryMakeBillboard(selfContext,
                                       selfContext.messageText(state.argument),
                                       text_color,
                                       *tint,
                                       state.distance);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisplayCharge(script_state_t& state, ai_state_t& self)
{
    // DisplayCharge( tmpargument = "progress", tmpdistance = "max progress", tmpturn = "pip width" )
    /// @author ZF
    /// @details Draws a special progress bar this update frame

    if (!resolveSelfContext(self).isResolved()) return false;
    const SelfActionContext selfContext = makeSelfActionContext(self);

    //We ourselves must be a player or our holder must be one
    const ITargetInfo* chargeTarget = resolveChargeTarget(selfContext);

    //Only do this for players
    if (chargeTarget == nullptr || !chargeTarget->isPlayer()) {
        return false;
    }

    //Validate arguments
    else if(state.distance <= 0 || state.argument < 0)  {
        return false;
    }

    //Render it!
    else {
        const std::shared_ptr<Ego::Player> player = tryPlayer(*chargeTarget);
        if (player == nullptr)
        {
            return false;
        }
        else
        {
            player->setChargeBar(state.argument, state.distance, state.turn);
        }
    }

    return true;
}
