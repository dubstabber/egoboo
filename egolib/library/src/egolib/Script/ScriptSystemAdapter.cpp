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

/// @file egolib/Script/ScriptSystemAdapter.cpp
/// @brief VM-side implementation of the IScriptSystem driver seam.
/// @details Forwards each interface method to the VM driver free functions. Lives in the
///   egolib-scriptvm archive (above egolib-library), so its references to scr_run_chr_script /
///   set_alerts / scripting_system_end are intra-layer (scriptvm -> scriptvm), never a
///   library -> scriptvm reverse edge.

#include "egolib/Script/IScriptSystem.hpp"
#include "egolib/Script/script.h"  // scr_run_chr_script, set_alerts, scripting_system_end

namespace Ego
{
namespace Script
{

namespace
{
struct ScriptSystemAdapter final : IScriptSystem
{
    void runCharacterScript(ObjectRef character) override { scr_run_chr_script(character); }
    void setAlerts(ObjectRef character) override { set_alerts(character); }
    void endScriptingSystem() override { scripting_system_end(); }
};

ScriptSystemAdapter g_defaultScriptSystem;
}

void installDefaultScriptSystem()
{
    installScriptSystem(&g_defaultScriptSystem);
}

void clearDefaultScriptSystem()
{
    clearScriptSystem();
}

} // namespace Script
} // namespace Ego
