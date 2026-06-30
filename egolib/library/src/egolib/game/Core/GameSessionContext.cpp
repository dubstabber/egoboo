#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Core/EngineContext.hpp"

#include "egolib/Profiles/_Include.hpp"
#include "egolib/Script/script.h"
#include "egolib/Script/IScriptSystem.hpp"  // activeScriptSystem() driver seam
#include "egolib/Audio/IAudioSystem.hpp"
#include "egolib/egoboo_setup.h"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Entities/IParticleHandler.hpp"
#include "egolib/Graphics/IBillboardSystem.hpp"
#include "egolib/Graphics/ICameraSystem.hpp"
#include "egolib/Log/_Include.hpp"
#include "egolib/Profiles/IProfileSystem.hpp"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/LegacyLocalStats.hpp"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/game/Module/IModuleEnvironment.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/Physics/ICollisionWorld.hpp"  // install/clearCollisionWorld
#include "egolib/Entities/IObjectWorld.hpp"     // install/clearObjectWorld
#include "egolib/game/game.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <stdexcept>
#include <utility>

namespace
{
void publishLegacyLocalPlayerStatusCompatibilityMirrors(const GameSessionContext& session)
{
    local_stats_t& legacyStats = *legacy_local_stats();
    // Preserve the legacy local-stats mirror while GameSessionContext owns the state.
    legacyStats.player_count = static_cast<int>(session.localPlayerCount());
    legacyStats.noplayers = !session.hasLocalPlayers();
    legacyStats.allpladead = session.allLocalPlayersDead();
}

void publishLegacyLocalPlayerPerceptionCompatibilityMirrors(const GameSessionContext& session)
{
    local_stats_t& legacyStats = *legacy_local_stats();
    const LocalPlayerPerceptionState& perception = session.localPlayerPerception();
    legacyStats.grog_level = perception.grogLevel;
    legacyStats.daze_level = perception.dazeLevel;
    legacyStats.seeinvis_level = perception.seeInvisibleLevel;
    legacyStats.seeinvis_mag = perception.seeInvisibleMagnitude;
    legacyStats.seedark_level = perception.seeDarkLevel;
    legacyStats.seedark_mag = perception.seeDarkMagnitude;
    legacyStats.seekurse_level = perception.seeKurseLevel;
}

void publishLegacyEnemySenseCompatibilityMirrors(const GameSessionContext& session)
{
    local_stats_t& legacyStats = *legacy_local_stats();
    const EnemySenseState& enemySense = session.enemySense();
    legacyStats.sense_enemies_team = enemySense.team;
    legacyStats.sense_enemies_idsz = enemySense.idsz;
}

void publishLegacyRespawnCooldownCompatibilityMirrors(const GameSessionContext& session)
{
    legacy_local_stats()->revivetimer = session.respawnCooldown();
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

        Object* object = player->tryObject();
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

        const Object* object = player->tryObject();
        if (!object || object->isTerminated() || !object->isAlive())
        {
            continue;
        }

        perception.seeInvisibleLevel += object->getAttribute(Ego::Attribute::SEE_INVISIBLE);
        perception.seeKurseLevel += object->getAttribute(Ego::Attribute::SENSE_KURSES);
        perception.seeDarkLevel += object->getAttribute(Ego::Attribute::DARKVISION);
        perception.grogLevel += object->getGrogTimer();
        perception.dazeLevel += object->getDazeTimer();

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

    GameModuleRuntime runtime;
    runtime.profileSystem = []() -> IProfileSystem& { return activeProfileSystem(); };
    runtime.audioSystem = []() -> IAudioSystem& { return activeAudioSystem(); };
    runtime.particleHandler = []() -> IParticleHandler& { return activeParticleHandler(); };
    runtime.cameraSystem = []() -> ICameraSystem& { return activeCameraSystem(); };
    runtime.billboardSystem = []() -> Ego::Graphics::IBillboardSystem& { return Ego::Graphics::activeBillboardSystem(); };
    runtime.config = []() -> egoboo_config_t& { return Ego::activeConfig(); };
    runtime.logTarget = []() -> Log::Target& { return EngineContext::get().logTarget(); };
    runtime.importList = [this]() -> import_list_t& { return importList(); };
    runtime.overrideSlots = [this]() -> bool& { return overrideSlots(); };
    runtime.worldUpdateCount = [this]() -> uint32_t& { return worldUpdateCount(); };
    runtime.characterStatClock = [this]() -> uint32_t& { return characterStatClock(); };
    runtime.enchantStatClock = [this]() -> uint32_t& { return enchantStatClock(); };
    runtime.localPlayerCount = [this]() -> size_t { return localPlayerCount(); };
    runtime.publishLocalPlayerCount = [this](size_t count) { publishLocalPlayerCount(count); };
    runtime.resetClocks = [this]() { resetClocks(); };

    _activeModule = std::make_unique<GameModule>(module, seed, std::move(runtime));

    // Publish the active module as the collision world so the lower-layer Collidable base can
    // validate positions without reaching up into GameModule. Installed before any spawning,
    // since spawnAllObjects() sets object positions through Collidable::setPosition.
    Ego::Physics::installCollisionWorld(_activeModule.get());

    // Publish the active module as the object world too, so the physics translation units can
    // reach the object/team containers through the IObjectWorld seam instead of up into
    // GameModule / GameSessionContext. Same installed pointer, same lifetime as the collision
    // world above.
    Ego::Entities::installObjectWorld(_activeModule.get());

    // Publish module environment and session state separately so read-only runtime callers can
    // describe the state they need without depending on the concrete session owner.
    installModuleEnvironment(_activeModule.get());
    installSessionState(this);

    // Publish the session-owned world-update counter so the physics translation units read the
    // current tick through the lower-layer activeWorldUpdateCount() seam instead of reaching into
    // GameSessionContext. The pointer aliases _worldUpdateCount (this singleton outlives every
    // module), so reads see the live, still-incrementing value; cleared in quitModule().
    Ego::Entities::installWorldUpdateCounter(&_worldUpdateCount);

    // Live spawn still happens after construction because the runtime is not fully decoupled.
    activeModule().spawnAllObjects();

    return true;
}

void GameSessionContext::quitModule()
{
    const bool hadActiveModule = static_cast<bool>(_activeModule);

    Ego::Entities::clearWorldUpdateCounter();
    clearSessionState();
    clearModuleEnvironment();
    Ego::Entities::clearObjectWorld();
    Ego::Physics::clearCollisionWorld();

    if (_activeModule)
    {
        _activeModule->shutdownRuntime();
    }
    _activeModule.reset();

    Ego::Script::activeScriptSystem().endScriptingSystem();

    if (!hadActiveModule)
    {
        if (auto* profileSystem = tryActiveProfileSystem())
        {
            profileSystem->reset();
        }
    }
    game_reset_players();
    reset_end_text();

    if (auto* audioSystem = tryActiveAudioSystem())
    {
        audioSystem->fadeAllSounds();
    }

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
    return game_copy_imports(importList());
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

uint32_t GameSessionContext::worldUpdateCount() const
{
    return _worldUpdateCount;
}

uint32_t& GameSessionContext::characterStatClock()
{
    return _characterStatClock;
}

uint32_t GameSessionContext::characterStatClock() const
{
    return _characterStatClock;
}

uint32_t& GameSessionContext::enchantStatClock()
{
    return _enchantStatClock;
}

uint32_t GameSessionContext::enchantStatClock() const
{
    return _enchantStatClock;
}

void GameSessionContext::resetClocks()
{
    worldUpdateCount() = 0;
    characterStatClock() = 0;
    enchantStatClock() = 0;
}
