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

/// @file egolib/game/Core/GameplaySubsystemsBootstrap.hpp
/// @details RAII composition-root for the gameplay audio + particle subsystems.

#pragma once

#include "idlib/non_copyable.hpp"

/// @brief RAII composition-root for the gameplay audio and particle subsystems.
///
/// Construction installs the audio system and then the particle handler into the
/// active @c EngineContext, in that order. Destruction clears the particle handler
/// and then the audio system, i.e. the exact reverse order. This encapsulates the
/// subsystem lifecycle that @c GameEngine::initialize() / @c GameEngine::uninitialize()
/// previously orchestrated inline, keeping the concrete @c AudioSystem and
/// @c ParticleHandler headers (and the heavy @c Object.hpp aggregate they pull in)
/// out of the @c GameEngine translation units.
class GameplaySubsystemsBootstrap : private idlib::non_copyable
{
public:
    GameplaySubsystemsBootstrap();
    ~GameplaySubsystemsBootstrap();
};
