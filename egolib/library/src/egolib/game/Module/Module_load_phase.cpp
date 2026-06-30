//********************************************************************************************
//*
//*    This file is part of Egoboo.
//*
//*    Egoboo is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Egoboo is distributed in the hope that it will be useful,
//*    but WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Egoboo.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************

/// @file egolib/game/Module/Module_load_phase.cpp
/// @brief Named GameModule construction loading phases.

#include "egolib/game/Module/Module_load_phase.hpp"
#include "egolib/game/Module/Module_internal.h"
#include "egolib/Audio/IAudioSystem.hpp"
#include "egolib/Log/_Include.hpp"
#include "egolib/Profiles/IProfileSystem.hpp"

#include <cstdlib>

namespace module_loading
{

ModuleLoadPhase::ModuleLoadPhase(GameModule& module) :
    _module(module)
{}

void ModuleLoadPhase::run()
{
    initializeRuntime();
    initializeTeamsAndTextures();
    initializeSharedAssets();
    loadEnvironment();
    loadContent();
    finalizeInitialization();
}

void ModuleLoadPhase::initializeRuntime()
{
    // Set up the virtual file system for the module before any module-local loads.
    if (!setup_init_module_vfs_paths(_module.getPath())) {
        throw idlib::runtime_error(__FILE__, __LINE__, "Failed to setup module vfs");
    }

    // Initialize random seeds before content loading starts.
    srand(_module._seed);
    Random::setSeed(_module._seed);
}

void ModuleLoadPhase::initializeTeamsAndTextures()
{
    // Initialize all teams before module state is populated.
    for (int i = 0; i < Team::TEAM_MAX; ++i) {
        _module._teamList.push_back(Team(i));
    }

    // Load tile textures up front so rendering assets are ready once the mesh loads.
    for (size_t i = 0; i < _module._tileTextures.size(); ++i) {
        _module._tileTextures[i] = Ego::DeferredTexture("mp_data/tile" + std::to_string(i));
    }

    // Load water textures used by the module environment.
    _module._waterTextures[0] = Ego::DeferredTexture("mp_data/waterlow");
    _module._waterTextures[1] = Ego::DeferredTexture("mp_data/watertop");
}

void ModuleLoadPhase::initializeSharedAssets()
{
    // Load shared runtime assets that module content depends on.
    _module._runtime.audioSystem().loadGlobalSounds();
    _module._runtime.profileSystem().loadGlobalParticleProfiles();
}

void ModuleLoadPhase::loadEnvironment()
{
    // Load environment state before module content starts referencing it.
    wawalite_data_t *wavalite = read_wawalite_vfs();
    if (wavalite != nullptr) {
        _module._water.upload(wavalite->water);
        _module._damageTile.upload(wavalite->damagetile);
    }
    else {
        _module._runtime.logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__,
                                            "unable to load wawalite.txt for ", "`",
                                            _module._moduleProfile->getPath(), "`", Log::EndOfEntry);
    }
    upload_wawalite(_module._fog, _module._weatherState, _module._animatedTilesState);
}

void ModuleLoadPhase::loadContent()
{
    // Load the profiles and world data in the same order as the legacy constructor.
    _module.loadProfiles();

    // Load mesh.
    MeshLoader meshLoader;
    _module._mesh = meshLoader(_module._moduleProfile->getPath());

    // Load passage.txt.
    _module.loadAllPassages();

    // Load alliance.txt.
    _module.loadTeamAlliances();
}

void ModuleLoadPhase::finalizeInitialization()
{
    // log debug info for every object loaded into the module
    if (_module._runtime.config().debug_developerMode_enable.getValue()) {
        _module.logSlotUsage("/debug/slotused.txt");
    }

    // Reset module-local runtime counters after load completes.
    timeron = false;
    _module._runtime.resetClocks();
}

} // namespace module_loading
