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
#include "egolib/Audio/IAudioSystem.hpp"
#include "egolib/Entities/IParticleHandler.hpp"
#include "egolib/Log/_Include.hpp"
#include "egolib/Profiles/IProfileSystem.hpp"

#include <utility>

GameModule::GameModule(const std::shared_ptr<ModuleProfile> &profile, const uint32_t seed, GameModuleRuntime runtime) :
    _runtime(std::move(runtime)),
    _runtimeShutdown(false),
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
    _runtime.logTarget() << Log::Entry::create(Log::Level::Info, __FILE__, __LINE__, "loading module ", "`", profile->getPath(), "`", Log::EndOfEntry);

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
    _runtime.audioSystem().loadGlobalSounds();
    _runtime.profileSystem().loadGlobalParticleProfiles();
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
        _runtime.logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to load wawalite.txt for ", "`", _moduleProfile->getPath(), "`", Log::EndOfEntry);
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
    if (_runtime.config().debug_developerMode_enable.getValue()) {
        logSlotUsage("/debug/slotused.txt");
    }

    // Reset module-local runtime counters after load completes.
    timeron = false;
    _runtime.resetClocks();
}

void GameModule::shutdownRuntime()
{
    if (_runtimeShutdown)
    {
        return;
    }
    _runtimeShutdown = true;

    //free all particles
    _runtime.particleHandler().clear();

    //Free all profiles loaded by the module
    _runtime.profileSystem().reset();

    //Free all textures (internally guarded against a torn-down GFX / texture manager)
    gfx_system_release_all_graphics();
}

GameModule::~GameModule()
{
    // GameModule is normally shut down explicitly by GameSessionContext while services are still
    // installed. This fallback keeps abnormal/static teardown paths from throwing in a destructor.
    if (_runtimeShutdown)
    {
        return;
    }

    if (auto* particleHandler = tryActiveParticleHandler())
    {
        particleHandler->clear();
    }

    if (auto* profileSystem = tryActiveProfileSystem())
    {
        profileSystem->reset();
    }

    gfx_system_release_all_graphics();
}
