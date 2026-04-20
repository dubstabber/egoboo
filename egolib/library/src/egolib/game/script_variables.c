#include "egolib/game/script_variables.h"

#include "egolib/Entities/_Include.hpp"
#include "egolib/Script/script.h"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/game.h"
#include "egolib/game/Graphics/CameraSystem.hpp"

namespace
{
egoboo_config_t& config()
{
    return EngineContext::get().config();
}

GameModule& activeModule()
{
    return GameSessionContext::get().activeModule();
}

const IPhysical& physical(const Object& object)
{
    return object;
}

const IPhysical* tryPhysicalRole(const Object* object)
{
    return object ? static_cast<const IPhysical*>(object) : nullptr;
}

const ICharacterState& characterState(const Object& object)
{
    return object;
}

const ICharacterState* tryCharacterStateRole(const Object* object)
{
    return object ? static_cast<const ICharacterState*>(object) : nullptr;
}

const ITargetInfo& targetInfo(const Object& object)
{
    return object;
}

const ITargetInfo* tryTargetInfoRole(const Object* object)
{
    return object ? static_cast<const ITargetInfo*>(object) : nullptr;
}

const IWallet& wallet(const Object& object)
{
    return object;
}

const IWallet* tryWalletRole(const Object* object)
{
    return object ? static_cast<const IWallet*>(object) : nullptr;
}

const IInventoryHolder& inventoryHolder(const Object& object)
{
    return object;
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

int32_t load_VARTMPX(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return scriptState.x;
}

int32_t load_VARTMPY(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return scriptState.y;
}

int32_t load_VARTMPDISTANCE(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return scriptState.distance;
}

int32_t load_VARTMPTURN(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return scriptState.turn;
}

int32_t load_VARTMPARGUMENT(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return scriptState.argument;
}

int32_t load_VARRAND(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return Random::next(std::numeric_limits<uint16_t>::max());
}

int32_t load_VARSELFX(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return physical(*pobject).getPosX();
}

int32_t load_VARSELFY(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return physical(*pobject).getPosY();
}

int32_t load_VARSELFTURN(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return uint16_t(physical(*pobject).getFacingZ());
}

int32_t load_VARSELFCOUNTER(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return aiState.order_counter;
}

int32_t load_VARSELFORDER(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return aiState.order_value;
}

int32_t load_VARSELFMORALE(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return activeModule().getTeamList()[targetInfo(*pobject).getBaseTeamRef()].getMorale();
}

int32_t load_VARSELFLIFE(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return FLOAT_TO_FP8(characterState(*pobject).getLife());
}

int32_t load_VARTARGETX(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical* targetPhysical = tryPhysicalRole(ptarget);
    return (nullptr == targetPhysical) ? 0 : targetPhysical->getPosX();
}

int32_t load_VARTARGETY(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical* targetPhysical = tryPhysicalRole(ptarget);
    return (nullptr == targetPhysical) ? 0 : targetPhysical->getPosY();
}

int32_t load_VARTARGETDISTANCE(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical* targetPhysical = tryPhysicalRole(ptarget);
    if (nullptr == targetPhysical)
    {
        return 0x7FFFFFFF;
    }

    return distanceBetween(physical(*pobject), *targetPhysical);
}

int32_t load_VARTARGETTURN(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical* targetPhysical = tryPhysicalRole(ptarget);
    return (nullptr == targetPhysical) ? 0 : uint16_t(targetPhysical->getFacingZ());
}

int32_t load_VARLEADERX(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical* leaderPhysical = tryPhysicalRole(pleader);
    return leaderPhysical ? leaderPhysical->getPosX() : physical(*pobject).getPosX();
}

int32_t load_VARLEADERY(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical* leaderPhysical = tryPhysicalRole(pleader);
    return leaderPhysical ? leaderPhysical->getPosY() : physical(*pobject).getPosY();
}

int32_t load_VARLEADERDISTANCE(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical* leaderPhysical = tryPhysicalRole(pleader);
    if (!leaderPhysical)
    {
        return 0x7FFFFFFF;
    }

    return distanceBetween(physical(*pobject), *leaderPhysical);
}

int32_t load_VARLEADERTURN(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical* leaderPhysical = tryPhysicalRole(pleader);
    return leaderPhysical ? uint16_t(leaderPhysical->getFacingZ()) : uint16_t(physical(*pobject).getFacingZ());
}

int32_t load_VARGOTOX(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    ai_state_t::ensure_wp(aiState);

    if (!aiState.wp_valid)
    {
        return physical(*pobject).getPosX();
    }
    else
    {
        return aiState.wp[kX];
    }
}

int32_t load_VARGOTOY(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    ai_state_t::ensure_wp(aiState);

    if (!aiState.wp_valid)
    {
        return physical(*pobject).getPosY();
    }
    else
    {
        return aiState.wp[kY];
    }
}

int32_t load_VARGOTODISTANCE(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    ai_state_t::ensure_wp(aiState);

    if (!aiState.wp_valid)
    {
        return 0x7FFFFFFF;
    }

    return std::abs(aiState.wp[kX] - physical(*pobject).getPosX())
        + std::abs(aiState.wp[kY] - physical(*pobject).getPosY());
}

int32_t load_VARTARGETTURNTO(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical* targetPhysical = tryPhysicalRole(ptarget);
    if (nullptr == targetPhysical)
    {
        return 0;
    }

    return turnToward(physical(*pobject), *targetPhysical);
}

int32_t load_VARPASSAGE(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return aiState.passage;
}

int32_t load_VARWEIGHT(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return inventoryHolder(*pobject).getHoldingWeight();
}

int32_t load_VARSELFALTITUDE(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical& selfPhysical = physical(*pobject);
    return selfPhysical.getPosZ() - selfPhysical.getFloorElevation();
}

int32_t load_VARSELFID(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return targetInfo(*pobject).getTypeIDSZ().toUint32();
}

int32_t load_VARSELFHATEID(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return targetInfo(*pobject).getHateIDSZ().toUint32();
}

int32_t load_VARSELFMANA(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const ICharacterState& selfState = characterState(*pobject);
    int32_t temporary = FLOAT_TO_FP8(selfState.getMana());
    if (selfState.getAttribute(Ego::Attribute::CHANNEL_LIFE))
    {
        temporary += FLOAT_TO_FP8(selfState.getLife());
    }
    return temporary;
}

int32_t load_VARTARGETSTR(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const ICharacterState* targetState = tryCharacterStateRole(ptarget);
    return (nullptr == targetState) ? 0 : FLOAT_TO_FP8(targetState->getAttribute(Ego::Attribute::MIGHT));
}

int32_t load_VARTARGETINT(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const ICharacterState* targetState = tryCharacterStateRole(ptarget);
    return (nullptr == targetState) ? 0 : FLOAT_TO_FP8(targetState->getAttribute(Ego::Attribute::INTELLECT));
}

int32_t load_VARTARGETDEX(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const ICharacterState* targetState = tryCharacterStateRole(ptarget);
    return (nullptr == targetState) ? 0 : FLOAT_TO_FP8(targetState->getAttribute(Ego::Attribute::AGILITY));
}

int32_t load_VARTARGETLIFE(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const ICharacterState* targetState = tryCharacterStateRole(ptarget);
    return (nullptr == targetState) ? 0 : FLOAT_TO_FP8(targetState->getLife());
}

int32_t load_VARTARGETMANA(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const ICharacterState* targetState = tryCharacterStateRole(ptarget);
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

int32_t load_VARTARGETSPEEDX(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical* targetPhysical = tryPhysicalRole(ptarget);
    return (nullptr == targetPhysical) ? 0 : std::abs(targetPhysical->getVelocity().x());
}

int32_t load_VARTARGETSPEEDY(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical* targetPhysical = tryPhysicalRole(ptarget);
    return (nullptr == targetPhysical) ? 0 : std::abs(targetPhysical->getVelocity().y());
}

int32_t load_VARTARGETSPEEDZ(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical* targetPhysical = tryPhysicalRole(ptarget);
    return (nullptr == targetPhysical) ? 0 : std::abs(targetPhysical->getVelocity().z());
}

int32_t load_VARSELFSPAWNX(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return physical(*pobject).getSpawnPosition()[kX];
}

int32_t load_VARSELFSPAWNY(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return physical(*pobject).getSpawnPosition()[kY];
}

int32_t load_VARSELFSTATE(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return aiState.state;
}

int32_t load_VARSELFCONTENT(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return aiState.content;
}

int32_t load_VARSELFSTR(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return FLOAT_TO_FP8(characterState(*pobject).getAttribute(Ego::Attribute::MIGHT));
}

int32_t load_VARSELFINT(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return FLOAT_TO_FP8(characterState(*pobject).getAttribute(Ego::Attribute::INTELLECT));
}

int32_t load_VARSELFDEX(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return FLOAT_TO_FP8(characterState(*pobject).getAttribute(Ego::Attribute::AGILITY));
}

int32_t load_VARSELFMANAFLOW(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return FLOAT_TO_FP8(characterState(*pobject).getAttribute(Ego::Attribute::SPELL_POWER));
}

int32_t load_VARTARGETMANAFLOW(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const ICharacterState* targetState = tryCharacterStateRole(ptarget);
    return (nullptr == targetState) ? 0 : FLOAT_TO_FP8(targetState->getAttribute(Ego::Attribute::SPELL_POWER));
}

int32_t load_VARSELFATTACHED(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return number_of_attached_particles(aiState.getSelf());
}

int32_t load_VARTARGETLEVEL(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const ICharacterState* targetState = tryCharacterStateRole(ptarget);
    return (nullptr == targetState) ? 0 : targetState->getExperienceLevelIndex();
}

int32_t load_VARTARGETZ(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical* targetPhysical = tryPhysicalRole(ptarget);
    return (nullptr == targetPhysical) ? 0 : targetPhysical->getPosZ();
}

int32_t load_VARSELFINDEX(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return aiState.getSelf().get();
}

int32_t load_VAROWNERX(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical* ownerPhysical = tryPhysicalRole(powner);
    return (nullptr == ownerPhysical) ? 0 : ownerPhysical->getPosX();
}

int32_t load_VAROWNERY(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical* ownerPhysical = tryPhysicalRole(powner);
    return (nullptr == ownerPhysical) ? 0 : ownerPhysical->getPosY();
}

int32_t load_VAROWNERTURN(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical* ownerPhysical = tryPhysicalRole(powner);
    return (nullptr == ownerPhysical) ? 0 : uint16_t(ownerPhysical->getFacingZ());
}

int32_t load_VAROWNERDISTANCE(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical* ownerPhysical = tryPhysicalRole(powner);
    if (nullptr == ownerPhysical)
    {
        return 0x7FFFFFFF;
    }

    return distanceBetween(physical(*pobject), *ownerPhysical);
}

int32_t load_VAROWNERTURNTO(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical* ownerPhysical = tryPhysicalRole(powner);
    if (nullptr == ownerPhysical)
    {
        return 0;
    }

    return turnToward(physical(*pobject), *ownerPhysical);
}

int32_t load_VARXYTURNTO(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    int32_t temporary = FACING_T(vec_to_facing(scriptState.x - physical(*pobject).getPosX(),
                                               scriptState.y - physical(*pobject).getPosY()));
    return Ego::Math::clipBits<16>(temporary);
}

int32_t load_VARSELFMONEY(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return wallet(*pobject).getMoney();
}

int32_t load_VARSELFACCEL(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return characterState(*pobject).getAttribute(Ego::Attribute::ACCELERATION) * 100.0f;
}

int32_t load_VARTARGETEXP(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const ICharacterState* targetState = tryCharacterStateRole(ptarget);
    return (nullptr == targetState) ? 0 : targetState->getExperience();
}

int32_t load_VARSELFAMMO(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return characterState(*pobject).getAmmo();
}

int32_t load_VARTARGETAMMO(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const ICharacterState* targetState = tryCharacterStateRole(ptarget);
    return (nullptr == targetState) ? 0 : targetState->getAmmo();
}

int32_t load_VARTARGETMONEY(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IWallet* targetWallet = tryWalletRole(ptarget);
    return (nullptr == targetWallet) ? 0 : targetWallet->getMoney();
}

int32_t load_VARTARGETTURNAWAY(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical* targetPhysical = tryPhysicalRole(ptarget);
    if (nullptr == targetPhysical)
    {
        return 0;
    }

    return turnToward(physical(*pobject), *targetPhysical);
}

int32_t load_VARSELFLEVEL(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return characterState(*pobject).getExperienceLevelIndex();
}

int32_t load_VARTARGETRELOADTIME(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const ICharacterState* targetState = tryCharacterStateRole(ptarget);
    return (nullptr == targetState) ? 0 : targetState->getReloadTimer();
}

int32_t load_VARSPAWNDISTANCE(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical& selfPhysical = physical(*pobject);
    return std::abs(selfPhysical.getSpawnPosition()[kX] - selfPhysical.getPosX())
        + std::abs(selfPhysical.getSpawnPosition()[kY] - selfPhysical.getPosY());
}

int32_t load_VARTARGETMAXLIFE(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const ICharacterState* targetState = tryCharacterStateRole(ptarget);
    return (nullptr == targetState) ? 0 : FLOAT_TO_FP8(targetState->getAttribute(Ego::Attribute::MAX_LIFE));
}

int32_t load_VARTARGETTEAM(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const ITargetInfo* target = tryTargetInfoRole(ptarget);
    return (nullptr == target) ? 0 : target->getTeamRef();
}

int32_t load_VARTARGETARMOR(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const ITargetInfo* target = tryTargetInfoRole(ptarget);
    return (nullptr == target) ? 0 : target->getSkin();
}

int32_t load_VARDIFFICULTY(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return static_cast<uint32_t>(config().game_difficulty.getValue());
}

int32_t load_VARTIMEHOURS(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return Ego::Time::LocalTime().getHours();
}

int32_t load_VARTIMEMINUTES(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return Ego::Time::LocalTime().getMinutes();
}

int32_t load_VARTIMESECONDS(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return Ego::Time::LocalTime().getSeconds();
}

int32_t load_VARDATEMONTH(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return Ego::Time::LocalTime().getMonth() + 1; /// @todo The addition of +1 should be removed and
                                                  /// the whole Ego::Time::LocalTime class should be
                                                  /// made available via EgoScript. However, EgoScript
                                                  /// is not yet ready for that ... not yet.
}

int32_t load_VARDATEDAY(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return Ego::Time::LocalTime().getDayOfMonth();
}

int32_t load_VARSWINGTURN(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    auto camera = CameraSystem::get().getCamera(aiState.getSelf());
    return nullptr != camera ? camera->getSwing() << 2 : 0;
}

int32_t load_VARXYDISTANCE(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return std::sqrt(scriptState.x * scriptState.x + scriptState.y * scriptState.y);
}

int32_t load_VARSELFZ(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    return physical(*pobject).getPosZ();
}

int32_t load_VARTARGETALTITUDE(script_state_t& scriptState, ai_state_t& aiState, Object *pobject, Object *ptarget, Object *powner, Object *pleader)
{
    const IPhysical* targetPhysical = tryPhysicalRole(ptarget);
    return (nullptr == targetPhysical) ? 0 : targetPhysical->getPosZ() - targetPhysical->getFloorElevation();
}
