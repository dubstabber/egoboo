#include "egolib/game/Core/GameSessionContext.hpp"

#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/Script/script.h"
#include "egolib/egoboo_setup.h"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/game.h"

#include <ctime>
#include <stdexcept>

namespace
{
std::unique_ptr<GameModule>& activeModuleStorage()
{
    return _currentModule;
}
}

GameSessionContext& GameSessionContext::get()
{
    static GameSessionContext instance;
    return instance;
}

bool GameSessionContext::hasActiveModule() const
{
    return static_cast<bool>(activeModuleStorage());
}

GameModule* GameSessionContext::tryActiveModule()
{
    return activeModuleStorage().get();
}

const GameModule* GameSessionContext::tryActiveModule() const
{
    return activeModuleStorage().get();
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
    activeModuleStorage() = std::make_unique<GameModule>(module, seed);

    // Due to legacy `_currentModule` dependencies, live spawn still happens after construction.
    activeModule().spawnAllObjects();

    return true;
}

void GameSessionContext::quitModule()
{
    activeModuleStorage().reset();

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

import_list_t& GameSessionContext::importList()
{
    return g_importList;
}

const import_list_t& GameSessionContext::importList() const
{
    return g_importList;
}

bool& GameSessionContext::overrideSlots()
{
    return overrideslots;
}

uint32_t& GameSessionContext::worldUpdateCount()
{
    return update_wld;
}

uint32_t& GameSessionContext::characterStatClock()
{
    return clock_chr_stat;
}

uint32_t& GameSessionContext::enchantStatClock()
{
    return clock_enc_stat;
}

void GameSessionContext::resetClocks()
{
    worldUpdateCount() = 0;
    characterStatClock() = 0;
    enchantStatClock() = 0;
}
