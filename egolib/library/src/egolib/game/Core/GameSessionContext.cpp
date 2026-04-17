#include "egolib/game/Core/GameSessionContext.hpp"

#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/Script/script.h"
#include "egolib/egoboo_setup.h"
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/LegacyLocalStats.hpp"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/game.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <stdexcept>

namespace
{
void publishLegacyLocalPlayerStatusCompatibilityMirrors(const GameSessionContext& session)
{
    // Preserve the exported local_stats ABI while GameSessionContext owns the state.
    local_stats.player_count = static_cast<int>(session.localPlayerCount());
    local_stats.noplayers = !session.hasLocalPlayers();
    local_stats.allpladead = session.allLocalPlayersDead();
}

void publishLegacyLocalPlayerPerceptionCompatibilityMirrors(const GameSessionContext& session)
{
    const LocalPlayerPerceptionState& perception = session.localPlayerPerception();
    local_stats.grog_level = perception.grogLevel;
    local_stats.daze_level = perception.dazeLevel;
    local_stats.seeinvis_level = perception.seeInvisibleLevel;
    local_stats.seeinvis_mag = perception.seeInvisibleMagnitude;
    local_stats.seedark_level = perception.seeDarkLevel;
    local_stats.seedark_mag = perception.seeDarkMagnitude;
    local_stats.seekurse_level = perception.seeKurseLevel;
}

void publishLegacyEnemySenseCompatibilityMirrors(const GameSessionContext& session)
{
    const EnemySenseState& enemySense = session.enemySense();
    local_stats.sense_enemies_team = enemySense.team;
    local_stats.sense_enemies_idsz = enemySense.idsz;
}

void publishLegacyRespawnCooldownCompatibilityMirrors(const GameSessionContext& session)
{
    local_stats.revivetimer = session.respawnCooldown();
}
}

GameSessionContext& GameSessionContext::get()
{
    static GameSessionContext instance;
    return instance;
}

LocalPlayerStatus collectLocalPlayerStatus(const std::vector<std::shared_ptr<Ego::Player>>& players)
{
    LocalPlayerStatus status;
    status.registeredCount = players.size();

    for (const auto& player : players)
    {
        if (!player)
        {
            continue;
        }

        const std::shared_ptr<Object> object = player->getObject();
        if (!object || object->isTerminated())
        {
            continue;
        }

        if (object->isAlive())
        {
            ++status.aliveCount;
        }
        else
        {
            ++status.deadCount;
        }
    }

    return status;
}

LocalPlayerPerceptionState collectLocalPlayerPerception(const std::vector<std::shared_ptr<Ego::Player>>& players)
{
    LocalPlayerPerceptionState perception;
    size_t alivePlayerCount = 0;

    for (const auto& player : players)
    {
        if (!player)
        {
            continue;
        }

        const std::shared_ptr<Object> object = player->getObject();
        if (!object || object->isTerminated() || !object->isAlive())
        {
            continue;
        }

        perception.seeInvisibleLevel += object->getAttribute(Ego::Attribute::SEE_INVISIBLE);
        perception.seeKurseLevel += object->getAttribute(Ego::Attribute::SENSE_KURSES);
        perception.seeDarkLevel += object->getAttribute(Ego::Attribute::DARKVISION);
        perception.grogLevel += object->grog_timer;
        perception.dazeLevel += object->daze_timer;

        if (object->hasPerk(Ego::Perks::SENSE_INVISIBLE))
        {
            perception.seeInvisibleLevel += 1.0f;
        }

        ++alivePlayerCount;
    }

    if (alivePlayerCount > 0)
    {
        const float alivePlayerCountFloat = static_cast<float>(alivePlayerCount);
        perception.seeInvisibleLevel /= alivePlayerCountFloat;
        perception.seeKurseLevel /= alivePlayerCountFloat;
        perception.seeDarkLevel /= alivePlayerCountFloat;
        perception.grogLevel /= alivePlayerCountFloat;
        perception.dazeLevel /= alivePlayerCountFloat;
    }

    perception.seeInvisibleMagnitude = std::exp(0.32f * perception.seeInvisibleLevel);
    perception.seeDarkMagnitude = std::exp(0.32f * perception.seeDarkLevel);
    return perception;
}

EnemySenseState::EnemySenseState() :
    team(static_cast<TEAM_REF>(Team::TEAM_MAX)),
    idsz(IDSZ2::None)
{}

EnemySenseState::EnemySenseState(TEAM_REF team, const IDSZ2& idsz) :
    team(team),
    idsz(idsz)
{}

GameSessionContext::GameSessionContext() :
    _activeModule(),
    _importList(std::make_unique<import_list_t>()),
    _overrideSlots(false),
    _worldUpdateCount(0),
    _characterStatClock(0),
    _enchantStatClock(0),
    _preModuleLocalPlayerCount(0),
    _localPlayerStatus(),
    _localPlayerPerception(),
    _enemySense(),
    _respawnCooldown(0),
    _hasPublishedLocalPlayerStatus(false)
{}

GameSessionContext::~GameSessionContext() = default;

bool GameSessionContext::hasActiveModule() const
{
    return static_cast<bool>(_activeModule);
}

GameModule* GameSessionContext::tryActiveModule()
{
    return _activeModule.get();
}

const GameModule* GameSessionContext::tryActiveModule() const
{
    return _activeModule.get();
}

GameModule& GameSessionContext::activeModule()
{
    GameModule* module = tryActiveModule();
    if (!module)
    {
        throw std::logic_error("no active game module");
    }
    return *module;
}

const GameModule& GameSessionContext::activeModule() const
{
    const GameModule* module = tryActiveModule();
    if (!module)
    {
        throw std::logic_error("no active game module");
    }
    return *module;
}

bool GameSessionContext::beginModule(const std::shared_ptr<ModuleProfile>& module)
{
    return beginModule(module, static_cast<uint32_t>(std::time(nullptr)));
}

bool GameSessionContext::beginModule(const std::shared_ptr<ModuleProfile>& module, uint32_t seed)
{
    resetLocalPlayerState();
    resetLocalPlayerPerception();
    resetEnemySense();
    resetRespawnCooldown();

    _activeModule = std::make_unique<GameModule>(module, seed);

    // Live spawn still happens after construction because the runtime is not fully decoupled.
    activeModule().spawnAllObjects();

    return true;
}

void GameSessionContext::quitModule()
{
    _activeModule.reset();

    scripting_system_end();

    ProfileSystem::get().reset();
    game_reset_players();
    reset_end_text();

    AudioSystem::get().fadeAllSounds();

    setup_clear_module_vfs_paths();
}

bool GameSessionContext::finishModule()
{
    if (activeModule().isExportValid())
    {
        export_all_players(false);
        import_list_t::from_players(importList());
    }

    vfs_removeDirectoryAndContents("import");
    game_copy_imports(&importList());

    return true;
}

ObjectHandler* GameSessionContext::tryObjectHandler()
{
    GameModule* module = tryActiveModule();
    if (!module)
    {
        return nullptr;
    }
    return &module->getObjectHandler();
}

ObjectHandler& GameSessionContext::objectHandler()
{
    ObjectHandler* handler = tryObjectHandler();
    if (!handler)
    {
        throw std::logic_error("no active game module");
    }
    return *handler;
}

std::shared_ptr<ego_mesh_t> GameSessionContext::mesh()
{
    return activeModule().getMeshPointer();
}

std::shared_ptr<const Ego::Texture> GameSessionContext::tileTexture(size_t index)
{
    return activeModule().getTileTexture(index);
}

std::shared_ptr<const Ego::Texture> GameSessionContext::waterTexture(uint8_t layer)
{
    return activeModule().getWaterTexture(layer);
}

water_instance_t& GameSessionContext::water()
{
    return activeModule().getWater();
}

WeatherState& GameSessionContext::weatherState()
{
    return activeModule().getWeatherState();
}

fog_instance_t& GameSessionContext::fog()
{
    return activeModule().getFog();
}

AnimatedTilesState& GameSessionContext::animatedTilesState()
{
    return activeModule().getAnimatedTilesState();
}

const std::vector<std::shared_ptr<Ego::Player>>& GameSessionContext::playerList() const
{
    return activeModule().getPlayerList();
}

size_t GameSessionContext::localPlayerCount() const
{
    const GameModule* module = tryActiveModule();
    if (!module)
    {
        return _preModuleLocalPlayerCount;
    }

    return module->getPlayerList().size();
}

const LocalPlayerStatus& GameSessionContext::localPlayerStatus() const
{
    return _localPlayerStatus;
}

const LocalPlayerPerceptionState& GameSessionContext::localPlayerPerception() const
{
    return _localPlayerPerception;
}

const EnemySenseState& GameSessionContext::enemySense() const
{
    return _enemySense;
}

int GameSessionContext::respawnCooldown() const
{
    return _respawnCooldown;
}

bool GameSessionContext::hasLocalPlayers() const
{
    return localPlayerCount() > 0;
}

bool GameSessionContext::allLocalPlayersDead() const
{
    return _hasPublishedLocalPlayerStatus && _localPlayerStatus.allPlayersDead();
}

void GameSessionContext::publishLocalPlayerCount(size_t count)
{
    _preModuleLocalPlayerCount = count;
    publishLegacyLocalPlayerStatusCompatibilityMirrors(*this);
}

void GameSessionContext::publishLocalPlayerStatus(const LocalPlayerStatus& status)
{
    _localPlayerStatus = status;
    _hasPublishedLocalPlayerStatus = true;
    publishLegacyLocalPlayerStatusCompatibilityMirrors(*this);
}

void GameSessionContext::publishLocalPlayerPerception(const LocalPlayerPerceptionState& state)
{
    _localPlayerPerception = state;
    publishLegacyLocalPlayerPerceptionCompatibilityMirrors(*this);
}

void GameSessionContext::publishEnemySense(const EnemySenseState& state)
{
    _enemySense = state;
    publishLegacyEnemySenseCompatibilityMirrors(*this);
}

void GameSessionContext::publishRespawnCooldown(int ticks)
{
    _respawnCooldown = std::max(0, ticks);
    publishLegacyRespawnCooldownCompatibilityMirrors(*this);
}

void GameSessionContext::tickRespawnCooldown()
{
    if (_respawnCooldown > 0)
    {
        --_respawnCooldown;
        publishLegacyRespawnCooldownCompatibilityMirrors(*this);
    }
}

void GameSessionContext::resetLocalPlayerState()
{
    _preModuleLocalPlayerCount = 0;
    _localPlayerStatus = LocalPlayerStatus{};
    _hasPublishedLocalPlayerStatus = false;
    publishLegacyLocalPlayerStatusCompatibilityMirrors(*this);
}

void GameSessionContext::resetLocalPlayerPerception()
{
    _localPlayerPerception = LocalPlayerPerceptionState{};
    publishLegacyLocalPlayerPerceptionCompatibilityMirrors(*this);
}

void GameSessionContext::resetEnemySense()
{
    _enemySense = EnemySenseState{};
    publishLegacyEnemySenseCompatibilityMirrors(*this);
}

void GameSessionContext::resetRespawnCooldown()
{
    _respawnCooldown = 0;
    publishLegacyRespawnCooldownCompatibilityMirrors(*this);
}

import_list_t& GameSessionContext::importList()
{
    return *_importList;
}

const import_list_t& GameSessionContext::importList() const
{
    return *_importList;
}

bool& GameSessionContext::overrideSlots()
{
    return _overrideSlots;
}

uint32_t& GameSessionContext::worldUpdateCount()
{
    return _worldUpdateCount;
}

uint32_t& GameSessionContext::characterStatClock()
{
    return _characterStatClock;
}

uint32_t& GameSessionContext::enchantStatClock()
{
    return _enchantStatClock;
}

void GameSessionContext::resetClocks()
{
    worldUpdateCount() = 0;
    characterStatClock() = 0;
    enchantStatClock() = 0;
}
