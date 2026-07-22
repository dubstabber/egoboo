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
#include "egolib/Audio/IAudioSystem.hpp"
#include "egolib/egoboo_setup.h"
#include "egolib/game/egoboo.h"
#include "egolib/game/game.h"
#include "egolib/game/mesh.h"
#include "egolib/Logic/Team.hpp"
#include "egolib/Log/_Include.hpp"
#include "egolib/Math/Random.hpp"
#include "egolib/Profiles/IProfileSystem.hpp"
#include "egolib/Profiles/ModuleProfile.hpp"

#include <cstddef>
#include <cstdlib>
#include <utility>

namespace module_loading
{

ModuleLoadPhase::ModuleLoadPhase(ModuleLoadContext context) :
    _context(std::move(context))
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
    if (!setup_init_module_vfs_paths(_context.moduleProfile->getFolderName())) {
        throw idlib::runtime_error(__FILE__, __LINE__, "Failed to setup module vfs");
    }

    // Initialize random seeds before content loading starts.
    srand(_context.seed);
    Random::setSeed(_context.seed);
}

void ModuleLoadPhase::initializeTeamsAndTextures()
{
    // Initialize all teams before module state is populated.
    for (int i = 0; i < Team::TEAM_MAX; ++i) {
        _context.teamList.push_back(Team(i));
    }

    // Load tile textures up front so rendering assets are ready once the mesh loads.
    for (size_t i = 0; i < _context.tileTextures.size(); ++i) {
        _context.tileTextures[i] = Ego::DeferredTexture("mp_data/tile" + std::to_string(i));
    }

    // Load water textures used by the module environment.
    _context.waterTextures[0] = Ego::DeferredTexture("mp_data/waterlow");
    _context.waterTextures[1] = Ego::DeferredTexture("mp_data/watertop");
}

void ModuleLoadPhase::initializeSharedAssets()
{
    // Load shared runtime assets that module content depends on.
    _context.runtime.audioSystem().loadGlobalSounds();
    _context.runtime.profileSystem().loadGlobalParticleProfiles();
}

void ModuleLoadPhase::loadEnvironment()
{
    // Load environment state before module content starts referencing it.
    wawalite_data_t *wavalite = read_wawalite_vfs();
    if (wavalite != nullptr) {
        _context.water.upload(wavalite->water);
        _context.damageTile.upload(wavalite->damagetile);
    }
    else {
        _context.runtime.logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__,
                                            "unable to load wawalite.txt for ", "`",
                                            _context.moduleProfile->getPath(), "`", Log::EndOfEntry);
    }
    upload_wawalite(_context.fog, _context.weatherState, _context.animatedTilesState);
}

void ModuleLoadPhase::loadContent()
{
    // Load the profiles and world data in the same order as the legacy constructor.
    loadProfiles(_context.runtime, *_context.moduleProfile);

    // Load mesh.
    MeshLoader meshLoader;
    _context.mesh = meshLoader(_context.moduleProfile->getPath());

    // Load passage.txt.
    loadPassages(_context.module, *_context.mesh, _context.passages);

    // Load alliance.txt.
    loadTeamAlliances(_context.teamList);
}

void ModuleLoadPhase::finalizeInitialization()
{
    // log debug info for every object loaded into the module
    if (_context.runtime.config().debug_developerMode_enable.getValue()) {
        logSlotUsage(_context.runtime.profileSystem(), "/debug/slotused.txt");
    }

    // Reset module-local runtime counters after load completes.
    timeron = false;
    _context.runtime.resetClocks();
}

} // namespace module_loading
