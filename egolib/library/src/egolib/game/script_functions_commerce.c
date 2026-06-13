/// @file egolib/game/script_functions_commerce.c
/// @brief Money and armor economy script functions (drop money, armor pricing/trade-in, money transfer)

#include "egolib/game/script_functions_internal.h"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/GUI/MessageLog.hpp"

namespace
{
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

TargetEconomyCompatibilityContext makeTargetEconomyCompatibilityContext(const ai_state_t& self)
{
    TargetEconomyCompatibilityContext context;
    context.targetAppearance = tryAppearanceProfile(self.getTarget());
    context.selfWallet = tryWallet(self.getSelf());
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

bool dropMoney(const script_state_t& state, IWallet* targetWallet)
{
    if (targetWallet == nullptr)
    {
        return false;
    }

    targetWallet->dropMoney(state.argument);
    return true;
}
} // namespace

//--------------------------------------------------------------------------------------------
uint8_t scr_GetTargetArmorPrice( script_state_t& state, ai_state_t& self )
{
    // tmpx = GetTargetArmorPrice( tmpargument = "skin" )
    /// @author ZZ
    /// @details This function returns the cost of the desired skin upgrade, setting
    /// tmpx to the price

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetEconomyCompatibilityContext targetContext = makeTargetEconomyCompatibilityContext(self);
    return setTargetArmorPrice(state, targetContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DropMoney( script_state_t& state, ai_state_t& self )
{
    // DropMoney( tmpargument = "money" )
    /// @author ZZ
    /// @details This function drops a certain amount of money, if the character has that
    /// much

    if (!resolveSelfContext(self).isResolved()) return false;

    IWallet* selfWallet = tryWallet(self.getSelf());
    return dropMoney(state, selfWallet);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DropTargetMoney( script_state_t& state, ai_state_t& self )
{
    // DropTargetMoney( tmpargument = "amount" )
    /// @author ZZ
    /// @details This function drops some of the target's money

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetEconomyCompatibilityContext targetContext = makeTargetEconomyCompatibilityContext(self);
    return dropMoney(state, targetContext.targetWallet);
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

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetEconomyCompatibilityContext targetContext = makeTargetEconomyCompatibilityContext(self);
    return chargeTargetArmor(state, targetContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ChangeTargetArmor( script_state_t& state, ai_state_t& self )
{
    // ChangeTargetArmor( tmpargument = "armor" )

    /// @author ZZ
    /// @details This function sets the target's armor type and returns the old type
    /// as tmpargument and the new type as tmpx

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetEconomyCompatibilityContext targetContext = makeTargetEconomyCompatibilityContext(self);
    return changeTargetArmor(state, targetContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveMoneyToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveMoneyToTarget( tmpargument = "money" )
    /// @author ZZ
    /// @details This function increases the target's money, while decreasing the
    /// character's own money.  tmpargument is set to the amount transferred

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetEconomyCompatibilityContext targetContext = makeTargetEconomyCompatibilityContext(self);
    return giveMoneyToTarget(state, targetContext);
}
