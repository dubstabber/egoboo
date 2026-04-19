//********************************************************************************************
//*
//*    This file is part of Egoboo.
//*
//*    Egoboo is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Egoboo is distributed in the hope that it will be useful, but
//*    WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Egoboo.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************

/// @file egolib/game/Module/Module_bootstrap.cpp
/// @brief GameModule construction, teardown, and bootstrap phases.

#include "egolib/game/Module/Module_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

GameModule::GameModule(const std::shared_ptr<ModuleProfile> &profile, const uint32_t seed) :
    _moduleProfile(profile),
    _gameObjects(),
    _playerNameList(),
    _playerList(),
    _teamList(),
    _name(profile->getName()),
    _exportValid(profile->isExportAllowed()),
    _exportReset(profile->isExportAllowed()),
    _isRespawnValid(profile->isRespawnValid()),
    _isBeaten(false),
    _seed(seed),

    _water(),
    _damageTile(),
    _weatherState(),
    _fog(),
    _animatedTilesState(),

    _passages(),
    _mesh(std::make_shared<ego_mesh_t>()),
    _tileTextures(),
    _waterTextures(),

    _pitsClock(PIT_CLOCK_RATE),
    _pitsKill(false),
    _pitsTeleport(false),
    _pitsTeleportPos()
{
    Log::get() << Log::Entry::create(Log::Level::Info, __FILE__, __LINE__, "loading module ", "`", profile->getPath(), "`", Log::EndOfEntry);

    initializeModuleRuntime();
    initializeModuleTeamsAndTextures();
    initializeSharedModuleAssets();
    loadModuleEnvironment();
    loadModuleContent();
    finalizeModuleInitialization();
}

void GameModule::initializeModuleRuntime()
{
    // Set up the virtual file system for the module before any module-local loads.
    if (!setup_init_module_vfs_paths(getPath())) {
        throw idlib::runtime_error(__FILE__, __LINE__, "Failed to setup module vfs");
    }

    // Initialize random seeds before content loading starts.
    srand(_seed);
    Random::setSeed(_seed);
}

void GameModule::initializeModuleTeamsAndTextures()
{
    // Initialize all teams before module state is populated.
    for (int i = 0; i < Team::TEAM_MAX; ++i) {
        _teamList.push_back(Team(i));
    }

    // Load tile textures up front so rendering assets are ready once the mesh loads.
    for (size_t i = 0; i < _tileTextures.size(); ++i) {
        _tileTextures[i] = Ego::DeferredTexture("mp_data/tile" + std::to_string(i));
    }

    // Load water textures used by the module environment.
    _waterTextures[0] = Ego::DeferredTexture("mp_data/waterlow");
    _waterTextures[1] = Ego::DeferredTexture("mp_data/watertop");
}

void GameModule::initializeSharedModuleAssets()
{
    // Load shared runtime assets that module content depends on.
    AudioSystem::get().loadGlobalSounds();
    EngineContext::get().profileSystem().loadGlobalParticleProfiles();
}

void GameModule::loadModuleEnvironment()
{
    // Load environment state before module content starts referencing it.
    wawalite_data_t *wavalite = read_wawalite_vfs();
    if (wavalite != nullptr) {
        _water.upload(wavalite->water);
        _damageTile.upload(wavalite->damagetile);
    }
    else {
        Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to load wawalite.txt for ", "`", _moduleProfile->getPath(), "`", Log::EndOfEntry);
    }
    upload_wawalite(_fog, _weatherState, _animatedTilesState);
}

void GameModule::loadModuleContent()
{
    // Load the profiles and world data in the same order as the legacy constructor.
    loadProfiles();

    // Load mesh.
    MeshLoader meshLoader;
    _mesh = meshLoader(_moduleProfile->getPath());

    // Load passage.txt.
    loadAllPassages();

    // Load alliance.txt.
    loadTeamAlliances();
}

void GameModule::finalizeModuleInitialization()
{
    // log debug info for every object loaded into the module
    if (egoboo_config_t::get().debug_developerMode_enable.getValue()) {
        logSlotUsage("/debug/slotused.txt");
    }

    // Reset module-local runtime counters after load completes.
    timeron = false;
    gameSession().resetClocks();
}

GameModule::~GameModule()
{
    //free all particles
    EngineContext::get().particleHandler().clear();

    //Free all profiles loaded by the module
    EngineContext::get().profileSystem().reset();

    //Free all textures
    gfx_system_release_all_graphics();
}
