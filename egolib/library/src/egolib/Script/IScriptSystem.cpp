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

/// @file egolib/Script/IScriptSystem.cpp
/// @brief Ownership of the installed active script system.

#include "egolib/Script/IScriptSystem.hpp"

#include <stdexcept>

namespace Ego
{
namespace Script
{

namespace
{
IScriptSystem* g_activeScriptSystem = nullptr;
}

void installScriptSystem(IScriptSystem* system)
{
    g_activeScriptSystem = system;
}

void clearScriptSystem()
{
    g_activeScriptSystem = nullptr;
}

IScriptSystem* tryActiveScriptSystem()
{
    return g_activeScriptSystem;
}

IScriptSystem& activeScriptSystem()
{
    if (!g_activeScriptSystem)
    {
        // Mirrors Ego::Entities::activeObjectWorld(): the library call sites all run mid-engine
        // (per-object AI tick, final death dispatch, session teardown) after the boot install, so the
        // uninstalled state is a programmer error rather than an expected boundary outcome.
        throw std::logic_error("no active script system");
    }
    return *g_activeScriptSystem;
}

} // namespace Script
} // namespace Ego
