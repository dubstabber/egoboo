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

/// @file egolib/tests/egolib/tests/ScriptSystemEnvironment.cpp
/// @brief Installs the VM-backed script system for the whole test process.
/// @details The egolib-scriptvm driver seam routes GameSessionContext::quitModule()
///   (-> endScriptingSystem()) and Object::kill() (-> runCharacterScript()) through
///   Ego::Script::activeScriptSystem(), which throws when no system is installed. The game
///   installs it from Main.cpp; the test executable (gtest_main, no custom main) installs it
///   here via a global test environment so every test driving those paths finds it present.

#include "egolib/Script/IScriptSystem.hpp"

#include <gtest/gtest.h>

namespace
{
class ScriptSystemEnvironment : public ::testing::Environment
{
public:
    void SetUp() override { Ego::Script::installDefaultScriptSystem(); }
    void TearDown() override { Ego::Script::clearDefaultScriptSystem(); }
};

// Registered at static-init time; gtest invokes SetUp() once before any test runs.
const ::testing::Environment* const g_scriptSystemEnvironment =
    ::testing::AddGlobalTestEnvironment(new ScriptSystemEnvironment());
}
