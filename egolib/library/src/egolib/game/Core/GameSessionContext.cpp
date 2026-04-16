#include "egolib/game/Core/GameSessionContext.hpp"

#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/Script/script.h"
#include "egolib/egoboo_setup.h"
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/game.h"

#include <ctime>
#include <stdexcept>

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

GameSessionContext::GameSessionContext() :
    _activeModule(),
    _importList(std::make_unique<import_list_t>()),
    _overrideSlots(false),
    _worldUpdateCount(0),
    _characterStatClock(0),
    _enchantStatClock(0)
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
        return static_cast<size_t>(local_stats.player_count);
    }

    return module->getPlayerList().size();
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
