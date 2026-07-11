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

/// @file egolib/Script/IScriptSystem.hpp
/// @brief Lower-layer driver seam for the EgoScript VM.

#pragma once

#include "egolib/typedef.h"  // ObjectRef

namespace Ego
{
namespace Script
{

/// @brief The minimal VM-driver surface the game-core (egolib-library) reaches up into: run a
///        character's script, poll its per-tick alert conditions, and tear the VM down.
///
///        It is implemented by a VM-side adapter (ScriptSystemAdapter, which forwards to the
///        VM driver free functions) and installed once at boot from a layer above
///        egolib-library. Routing the three library call sites (game_loop's per-object AI tick,
///        the death path's final-script run, and the game session teardown) through this interface
///        removes the only remaining egolib-library -> VM-driver link edges, so the EgoScript VM
///        (script.c + script_driver.c + the script_functions_*.c family) can be carved into an above-library
///        egolib-scriptvm archive. This mirrors Ego::Entities::IObjectWorld (the entity-world
///        seam) and the GameEngine main-menu-state factory used by the gamestates carve.
///
///        ObjectRef is already lower-layer, so the interface drags in no concrete entity or
///        game/ header.
class IScriptSystem
{
public:
    virtual ~IScriptSystem() = default;

    /// @brief Run @a character's AI script once (the per-tick / final-kill dispatch).
    virtual void runCharacterScript(ObjectRef character) = 0;

    /// @brief Poll @a character's per-tick alert conditions (waypoint arrival, etc.).
    virtual void setAlerts(ObjectRef character) = 0;

    /// @brief Tear down the scripting runtime (called from the game session teardown).
    virtual void endScriptingSystem() = 0;
};

/// @brief Install @a system as the active script system. Passing @a nullptr is equivalent to
///        clearScriptSystem().
void installScriptSystem(IScriptSystem* system);

/// @brief Clear the active script system.
void clearScriptSystem();

/// @brief The installed script system, or @a nullptr if none is installed.
IScriptSystem* tryActiveScriptSystem();

/// @brief The installed script system.
/// @throw std::logic_error if no script system is installed.
IScriptSystem& activeScriptSystem();

/// @brief Install the default VM-backed script system (the ScriptSystemAdapter forwarding to
///        VM driver). Called once at boot from above egolib-library (the game's Main and the test
///        harness), since the adapter references VM symbols that live above egolib-library after
///        the egolib-scriptvm carve.
void installDefaultScriptSystem();

/// @brief Clear the default VM-backed script system.
void clearDefaultScriptSystem();

} // namespace Script
} // namespace Ego
