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
#include "egolib/game/Module/Module_load_phase.hpp"
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

    module_loading::ModuleLoadContext loadContext{
        _runtime,
        _moduleProfile,
        _seed,
        _teamList,
        _tileTextures,
        _waterTextures,
        _water,
        _damageTile,
        _weatherState,
        _fog,
        _animatedTilesState,
        _mesh,
        *this,
        _passages
    };

    module_loading::ModuleLoadPhase(std::move(loadContext)).run();
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
