/// @file egolib/game/script_functions_stat_gifts.c
/// @brief Permanent stat gift script functions (experience, attributes, skills)

#include "egolib/game/script_functions_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

namespace
{

struct OwnedObjectHandle
{
    ObjectRef ref = ObjectRef::Invalid;
    std::shared_ptr<Object> object;
};

struct TargetCompatibilityContext
{
    ObjectRef targetRef = ObjectRef::Invalid;
    const ITargetInfo* info = nullptr;
    ICharacterState* characterState = nullptr;
    IInventoryHolder* inventory = nullptr;
    ITeamMember* teamMember = nullptr;
    IEnchantable* enchantable = nullptr;
};

struct TargetStateCompatibilityContext
{
    ICharacterState* characterState = nullptr;
};

struct HealingInvocationContext
{
    ICharacterState* targetState = nullptr;
    IDamageable* damageable = nullptr;
    OwnedObjectHandle healer;
};

TargetCompatibilityContext makeTargetCompatibilityContext(const ai_state_t& self)
{
    TargetCompatibilityContext context;
    context.targetRef = self.getTarget();
    context.info = tryTargetInfo(context.targetRef);
    context.characterState = tryCharacterState(context.targetRef);
    context.inventory = tryInventoryHolder(context.targetRef);
    context.teamMember = tryTeamMember(context.targetRef);
    context.enchantable = tryEnchantable(context.targetRef);
    return context;
}

ICharacterState* resolveAliveTargetState(const ai_state_t& self)
{
    const ITargetInfo* resolvedTargetInfo = tryTargetInfo(self.getTarget());
    ICharacterState* resolvedTargetState = tryCharacterState(self.getTarget());
    return resolvedTargetInfo != nullptr &&
           resolvedTargetState != nullptr &&
           resolvedTargetInfo->isAlive() ? resolvedTargetState : nullptr;
}

bool resolveTargetStateCompatibilityContext(const ai_state_t& self,
                                            TargetStateCompatibilityContext& context)
{
    context.characterState = resolveAliveTargetState(self);
    return context.characterState != nullptr;
}

bool resolveAliveTargetHealingContext(const ai_state_t& self,
                                      HealingInvocationContext& context)
{
    const ITargetInfo* resolvedTargetInfo = tryTargetInfo(self.getTarget());
    context.targetState = tryCharacterState(self.getTarget());
    context.damageable = tryDamageable(self.getTarget());
    context.healer.ref = self.getSelf();
    context.healer.object = tryObjectShared(self.getSelf());
    return resolvedTargetInfo != nullptr &&
           context.targetState != nullptr &&
           context.damageable != nullptr &&
           resolvedTargetInfo->isAlive() &&
           context.healer.object != nullptr;
}

void applyResolvedTargetBaseAttribute(const TargetStateCompatibilityContext& context,
                                      Ego::Attribute::AttributeType attribute,
                                      float value)
{
    if (context.characterState != nullptr)
    {
        context.characterState->increaseBaseAttribute(attribute, value);
    }
}

bool giveResolvedTargetExperience(const TargetCompatibilityContext& targetContext,
                                  int amount,
                                  XPType type)
{
    if (targetContext.characterState == nullptr)
    {
        return false;
    }

    targetContext.characterState->giveExperience(amount, type, false);
    return true;
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

bool giveResolvedTargetSkill(const TargetCompatibilityContext& targetContext, uint32_t skillId)
{
    if (targetContext.characterState == nullptr)
    {
        return false;
    }

    maybeAddSkillPerk(*targetContext.characterState, skillId);
    return true;
}

} // namespace


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveExperienceToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveExperienceToTarget( tmpargument = "amount", tmpdistance = "type" )
    /// @author ZZ
    /// @details This function gives the target some experience, xptype from distance,
    /// amount from argument.

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    return giveResolvedTargetExperience(targetContext,
                                        state.argument,
                                        static_cast<XPType>(state.distance));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveStrengthToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveStrengthToTarget(argument = "amount")
    // Permanently boost the target's strength

    if (!resolveSelfContext(self).isResolved()) return false;
    if ( ICharacterState* resolvedTargetState = resolveAliveTargetState(self) )
    {
        resolvedTargetState->increaseBaseAttribute(Ego::Attribute::MIGHT, FP8_TO_FLOAT(state.argument));
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveIntelligenceToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveIntelligenceToTarget(tmpargument = "amount")
    // Permanently boost the target's intelligence

    if (!resolveSelfContext(self).isResolved()) return false;
    if ( ICharacterState* resolvedTargetState = resolveAliveTargetState(self) )
    {
        resolvedTargetState->increaseBaseAttribute(Ego::Attribute::INTELLECT, FP8_TO_FLOAT(state.argument));
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveDexterityToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveDexterityToTarget(tmpargument = "amount")
    // Permanently boost the target's dexterity

    if (!resolveSelfContext(self).isResolved()) return false;
    if ( ICharacterState* resolvedTargetState = resolveAliveTargetState(self) )
    {
        resolvedTargetState->increaseBaseAttribute(Ego::Attribute::AGILITY, FP8_TO_FLOAT(state.argument));
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveLifeToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveLifeToTarget(tmpargument = "amount")
    /// @author ZZ
    /// @details Permanently boost the target's life

    if (!resolveSelfContext(self).isResolved()) return false;
    HealingInvocationContext healingContext;
    if (resolveAliveTargetHealingContext(self, healingContext))
    {
        healingContext.targetState->increaseBaseAttribute(Ego::Attribute::MAX_LIFE,
                                                          FP8_TO_FLOAT(state.argument));
        healingContext.damageable->heal(healingContext.healer.object, state.argument, true);
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveManaToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveManaToTarget(tmpargument = "amount")
    /// @author ZZ
    /// @details Permanently boost the target's mana

    if (!resolveSelfContext(self).isResolved()) return false;
    if ( ICharacterState* resolvedTargetState = resolveAliveTargetState(self) )
    {
        resolvedTargetState->increaseBaseAttribute(Ego::Attribute::MAX_MANA, FP8_TO_FLOAT(state.argument));
        resolvedTargetState->costMana(-state.argument, ObjectRef::Invalid);
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveManaFlowToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveManaFlowToTarget()
    /// @author ZF
    /// @details Permanently boost the target's mana flow

    if (!resolveSelfContext(self).isResolved()) return false;
    TargetStateCompatibilityContext targetContext;
    resolveTargetStateCompatibilityContext(self, targetContext);
    applyResolvedTargetBaseAttribute(targetContext,
                                     Ego::Attribute::SPELL_POWER,
                                     FP8_TO_FLOAT(state.argument));

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveManaReturnToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveManaReturnToTarget()
    /// @author ZF
    /// @details Permanently boost the target's mana return

    if (!resolveSelfContext(self).isResolved()) return false;
    TargetStateCompatibilityContext targetContext;
    resolveTargetStateCompatibilityContext(self, targetContext);
    applyResolvedTargetBaseAttribute(targetContext,
                                     Ego::Attribute::MANA_REGEN,
                                     FP8_TO_FLOAT(state.argument));

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveSkillToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveSkillToTarget( tmpargument = "skill_IDSZ" )
    /// @author ZF
    /// @details This function permanently gives the target character a Perk

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    return giveResolvedTargetSkill(targetContext, state.argument);
}
