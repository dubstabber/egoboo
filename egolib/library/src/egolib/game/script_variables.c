#include "egolib/game/script_variables.h"

#include "egolib/Entities/ICharacterState.hpp"
#include "egolib/Entities/IInventoryHolder.hpp"
#include "egolib/Entities/IPhysical.hpp"
#include "egolib/Entities/ITargetInfo.hpp"
#include "egolib/Entities/IWallet.hpp"
#include "egolib/Script/script.h"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/game.h"
#include "egolib/game/Graphics/Camera.hpp"
#include "egolib/game/Graphics/ICameraSystem.hpp"
#include "egolib/game/Module/IModuleCommands.hpp"

#include <cmath>
#include <limits>

namespace
{
egoboo_config_t& config()
{
    return EngineContext::get().config();
}

int32_t distanceBetween(const IPhysical& from, const IPhysical& to)
{
    return std::abs(to.getPosX() - from.getPosX()) + std::abs(to.getPosY() - from.getPosY());
}

int32_t turnToward(const IPhysical& from, const IPhysical& to)
{
    const int32_t temporary = FACING_T(vec_to_facing(to.getPosX() - from.getPosX(),
                                                     to.getPosY() - from.getPosY()));
    return Ego::Math::clipBits<16>(temporary);
}
}

int32_t load_VARTMPX(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return scriptState.x;
}

int32_t load_VARTMPY(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return scriptState.y;
}

int32_t load_VARTMPDISTANCE(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return scriptState.distance;
}

int32_t load_VARTMPTURN(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return scriptState.turn;
}

int32_t load_VARTMPARGUMENT(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return scriptState.argument;
}

int32_t load_VARRAND(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return Random::next(std::numeric_limits<uint16_t>::max());
}

int32_t load_VARSELFX(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return context.selfPhysical->getPosX();
}

int32_t load_VARSELFY(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return context.selfPhysical->getPosY();
}

int32_t load_VARSELFTURN(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return uint16_t(context.selfPhysical->getFacingZ());
}

int32_t load_VARSELFCOUNTER(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return aiState.order_counter;
}

int32_t load_VARSELFORDER(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return aiState.order_value;
}

int32_t load_VARSELFMORALE(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return activeModuleCommands().getTeamMorale(context.selfTargetInfo->getBaseTeamRef());
}

int32_t load_VARSELFLIFE(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return FLOAT_TO_FP8(context.selfCharacterState->getLife());
}

int32_t load_VARTARGETX(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical* targetPhysical = context.targetPhysical;
    return (nullptr == targetPhysical) ? 0 : targetPhysical->getPosX();
}

int32_t load_VARTARGETY(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical* targetPhysical = context.targetPhysical;
    return (nullptr == targetPhysical) ? 0 : targetPhysical->getPosY();
}

int32_t load_VARTARGETDISTANCE(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical* targetPhysical = context.targetPhysical;
    if (nullptr == targetPhysical)
    {
        return 0x7FFFFFFF;
    }

    return distanceBetween(*context.selfPhysical, *targetPhysical);
}

int32_t load_VARTARGETTURN(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical* targetPhysical = context.targetPhysical;
    return (nullptr == targetPhysical) ? 0 : uint16_t(targetPhysical->getFacingZ());
}

int32_t load_VARLEADERX(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical* leaderPhysical = context.leaderPhysical;
    return leaderPhysical ? leaderPhysical->getPosX() : context.selfPhysical->getPosX();
}

int32_t load_VARLEADERY(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical* leaderPhysical = context.leaderPhysical;
    return leaderPhysical ? leaderPhysical->getPosY() : context.selfPhysical->getPosY();
}

int32_t load_VARLEADERDISTANCE(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical* leaderPhysical = context.leaderPhysical;
    if (!leaderPhysical)
    {
        return 0x7FFFFFFF;
    }

    return distanceBetween(*context.selfPhysical, *leaderPhysical);
}

int32_t load_VARLEADERTURN(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical* leaderPhysical = context.leaderPhysical;
    return leaderPhysical ? uint16_t(leaderPhysical->getFacingZ()) : uint16_t(context.selfPhysical->getFacingZ());
}

int32_t load_VARGOTOX(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    ai_state_t::ensure_wp(aiState);

    if (!aiState.wp_valid)
    {
        return context.selfPhysical->getPosX();
    }
    else
    {
        return aiState.wp[kX];
    }
}

int32_t load_VARGOTOY(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    ai_state_t::ensure_wp(aiState);

    if (!aiState.wp_valid)
    {
        return context.selfPhysical->getPosY();
    }
    else
    {
        return aiState.wp[kY];
    }
}

int32_t load_VARGOTODISTANCE(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    ai_state_t::ensure_wp(aiState);

    if (!aiState.wp_valid)
    {
        return 0x7FFFFFFF;
    }

    return std::abs(aiState.wp[kX] - context.selfPhysical->getPosX())
        + std::abs(aiState.wp[kY] - context.selfPhysical->getPosY());
}

int32_t load_VARTARGETTURNTO(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical* targetPhysical = context.targetPhysical;
    if (nullptr == targetPhysical)
    {
        return 0;
    }

    return turnToward(*context.selfPhysical, *targetPhysical);
}

int32_t load_VARPASSAGE(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return aiState.passage;
}

int32_t load_VARWEIGHT(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return context.selfInventoryHolder->getHoldingWeight();
}

int32_t load_VARSELFALTITUDE(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical& selfPhysical = *context.selfPhysical;
    return selfPhysical.getPosZ() - selfPhysical.getFloorElevation();
}

int32_t load_VARSELFID(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return context.selfTargetInfo->getTypeIDSZ().toUint32();
}

int32_t load_VARSELFHATEID(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return context.selfTargetInfo->getHateIDSZ().toUint32();
}

int32_t load_VARSELFMANA(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const ICharacterState& selfState = *context.selfCharacterState;
    int32_t temporary = FLOAT_TO_FP8(selfState.getMana());
    if (selfState.getAttribute(Ego::Attribute::CHANNEL_LIFE))
    {
        temporary += FLOAT_TO_FP8(selfState.getLife());
    }
    return temporary;
}

int32_t load_VARTARGETSTR(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const ICharacterState* targetState = context.targetCharacterState;
    return (nullptr == targetState) ? 0 : FLOAT_TO_FP8(targetState->getAttribute(Ego::Attribute::MIGHT));
}

int32_t load_VARTARGETINT(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const ICharacterState* targetState = context.targetCharacterState;
    return (nullptr == targetState) ? 0 : FLOAT_TO_FP8(targetState->getAttribute(Ego::Attribute::INTELLECT));
}

int32_t load_VARTARGETDEX(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const ICharacterState* targetState = context.targetCharacterState;
    return (nullptr == targetState) ? 0 : FLOAT_TO_FP8(targetState->getAttribute(Ego::Attribute::AGILITY));
}

int32_t load_VARTARGETLIFE(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const ICharacterState* targetState = context.targetCharacterState;
    return (nullptr == targetState) ? 0 : FLOAT_TO_FP8(targetState->getLife());
}

int32_t load_VARTARGETMANA(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const ICharacterState* targetState = context.targetCharacterState;
    if (nullptr == targetState)
    {
        return 0;
    }

    int32_t temporary = FLOAT_TO_FP8(targetState->getMana());
    if (targetState->getAttribute(Ego::Attribute::CHANNEL_LIFE))
    {
        temporary += FLOAT_TO_FP8(targetState->getLife());
    }
    return temporary;
}

int32_t load_VARTARGETSPEEDX(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical* targetPhysical = context.targetPhysical;
    return (nullptr == targetPhysical) ? 0 : std::abs(targetPhysical->getVelocity().x());
}

int32_t load_VARTARGETSPEEDY(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical* targetPhysical = context.targetPhysical;
    return (nullptr == targetPhysical) ? 0 : std::abs(targetPhysical->getVelocity().y());
}

int32_t load_VARTARGETSPEEDZ(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical* targetPhysical = context.targetPhysical;
    return (nullptr == targetPhysical) ? 0 : std::abs(targetPhysical->getVelocity().z());
}

int32_t load_VARSELFSPAWNX(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return context.selfPhysical->getSpawnPosition()[kX];
}

int32_t load_VARSELFSPAWNY(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return context.selfPhysical->getSpawnPosition()[kY];
}

int32_t load_VARSELFSTATE(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return aiState.state;
}

int32_t load_VARSELFCONTENT(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return aiState.content;
}

int32_t load_VARSELFSTR(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return FLOAT_TO_FP8(context.selfCharacterState->getAttribute(Ego::Attribute::MIGHT));
}

int32_t load_VARSELFINT(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return FLOAT_TO_FP8(context.selfCharacterState->getAttribute(Ego::Attribute::INTELLECT));
}

int32_t load_VARSELFDEX(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return FLOAT_TO_FP8(context.selfCharacterState->getAttribute(Ego::Attribute::AGILITY));
}

int32_t load_VARSELFMANAFLOW(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return FLOAT_TO_FP8(context.selfCharacterState->getAttribute(Ego::Attribute::SPELL_POWER));
}

int32_t load_VARTARGETMANAFLOW(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const ICharacterState* targetState = context.targetCharacterState;
    return (nullptr == targetState) ? 0 : FLOAT_TO_FP8(targetState->getAttribute(Ego::Attribute::SPELL_POWER));
}

int32_t load_VARSELFATTACHED(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return number_of_attached_particles(context.selfRef);
}

int32_t load_VARTARGETLEVEL(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const ICharacterState* targetState = context.targetCharacterState;
    return (nullptr == targetState) ? 0 : targetState->getExperienceLevelIndex();
}

int32_t load_VARTARGETZ(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical* targetPhysical = context.targetPhysical;
    return (nullptr == targetPhysical) ? 0 : targetPhysical->getPosZ();
}

int32_t load_VARSELFINDEX(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return aiState.getSelf().get();
}

int32_t load_VAROWNERX(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical* ownerPhysical = context.ownerPhysical;
    return (nullptr == ownerPhysical) ? 0 : ownerPhysical->getPosX();
}

int32_t load_VAROWNERY(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical* ownerPhysical = context.ownerPhysical;
    return (nullptr == ownerPhysical) ? 0 : ownerPhysical->getPosY();
}

int32_t load_VAROWNERTURN(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical* ownerPhysical = context.ownerPhysical;
    return (nullptr == ownerPhysical) ? 0 : uint16_t(ownerPhysical->getFacingZ());
}

int32_t load_VAROWNERDISTANCE(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical* ownerPhysical = context.ownerPhysical;
    if (nullptr == ownerPhysical)
    {
        return 0x7FFFFFFF;
    }

    return distanceBetween(*context.selfPhysical, *ownerPhysical);
}

int32_t load_VAROWNERTURNTO(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical* ownerPhysical = context.ownerPhysical;
    if (nullptr == ownerPhysical)
    {
        return 0;
    }

    return turnToward(*context.selfPhysical, *ownerPhysical);
}

int32_t load_VARXYTURNTO(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    int32_t temporary = FACING_T(vec_to_facing(scriptState.x - context.selfPhysical->getPosX(),
                                               scriptState.y - context.selfPhysical->getPosY()));
    return Ego::Math::clipBits<16>(temporary);
}

int32_t load_VARSELFMONEY(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return context.selfWallet->getMoney();
}

int32_t load_VARSELFACCEL(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return context.selfCharacterState->getAttribute(Ego::Attribute::ACCELERATION) * 100.0f;
}

int32_t load_VARTARGETEXP(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const ICharacterState* targetState = context.targetCharacterState;
    return (nullptr == targetState) ? 0 : targetState->getExperience();
}

int32_t load_VARSELFAMMO(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return context.selfCharacterState->getAmmo();
}

int32_t load_VARTARGETAMMO(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const ICharacterState* targetState = context.targetCharacterState;
    return (nullptr == targetState) ? 0 : targetState->getAmmo();
}

int32_t load_VARTARGETMONEY(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IWallet* targetWallet = context.targetWallet;
    return (nullptr == targetWallet) ? 0 : targetWallet->getMoney();
}

int32_t load_VARTARGETTURNAWAY(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical* targetPhysical = context.targetPhysical;
    if (nullptr == targetPhysical)
    {
        return 0;
    }

    return turnToward(*context.selfPhysical, *targetPhysical);
}

int32_t load_VARSELFLEVEL(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return context.selfCharacterState->getExperienceLevelIndex();
}

int32_t load_VARTARGETRELOADTIME(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const ICharacterState* targetState = context.targetCharacterState;
    return (nullptr == targetState) ? 0 : targetState->getReloadTimer();
}

int32_t load_VARSPAWNDISTANCE(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical& selfPhysical = *context.selfPhysical;
    return std::abs(selfPhysical.getSpawnPosition()[kX] - selfPhysical.getPosX())
        + std::abs(selfPhysical.getSpawnPosition()[kY] - selfPhysical.getPosY());
}

int32_t load_VARTARGETMAXLIFE(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const ICharacterState* targetState = context.targetCharacterState;
    return (nullptr == targetState) ? 0 : FLOAT_TO_FP8(targetState->getAttribute(Ego::Attribute::MAX_LIFE));
}

int32_t load_VARTARGETTEAM(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const ITargetInfo* target = context.targetTargetInfo;
    return (nullptr == target) ? 0 : target->getTeamRef();
}

int32_t load_VARTARGETARMOR(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const ITargetInfo* target = context.targetTargetInfo;
    return (nullptr == target) ? 0 : target->getSkin();
}

int32_t load_VARDIFFICULTY(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return static_cast<uint32_t>(config().game_difficulty.getValue());
}

int32_t load_VARTIMEHOURS(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return Ego::Time::LocalTime().getHours();
}

int32_t load_VARTIMEMINUTES(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return Ego::Time::LocalTime().getMinutes();
}

int32_t load_VARTIMESECONDS(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return Ego::Time::LocalTime().getSeconds();
}

int32_t load_VARDATEMONTH(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return Ego::Time::LocalTime().getMonth() + 1; /// @todo The addition of +1 should be removed and
                                                  /// the whole Ego::Time::LocalTime class should be
                                                  /// made available via EgoScript. However, EgoScript
                                                  /// is not yet ready for that ... not yet.
}

int32_t load_VARDATEDAY(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return Ego::Time::LocalTime().getDayOfMonth();
}

int32_t load_VARSWINGTURN(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    auto camera = EngineContext::get().cameraSystem().getCamera(context.selfRef);
    return nullptr != camera ? camera->getSwing() << 2 : 0;
}

int32_t load_VARXYDISTANCE(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return std::sqrt(scriptState.x * scriptState.x + scriptState.y * scriptState.y);
}

int32_t load_VARSELFZ(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    return context.selfPhysical->getPosZ();
}

int32_t load_VARTARGETALTITUDE(script_state_t& scriptState, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    const IPhysical* targetPhysical = context.targetPhysical;
    return (nullptr == targetPhysical) ? 0 : targetPhysical->getPosZ() - targetPhysical->getFloorElevation();
}
