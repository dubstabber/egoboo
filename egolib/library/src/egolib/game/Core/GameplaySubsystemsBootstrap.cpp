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

/// @file egolib/game/Core/GameplaySubsystemsBootstrap.cpp

#include "egolib/game/Core/GameplaySubsystemsBootstrap.hpp"

#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/Entities/_Include.hpp"  // ParticleHandler (guarded aggregate include)

GameplaySubsystemsBootstrap::GameplaySubsystemsBootstrap()
{
    // Initialize the audio system.
    AudioSystem::initialize();
    EngineContext::get().installAudioSystem(AudioSystem::get());

    // Initialize the particle handler.
    ParticleHandler::initialize();
    EngineContext::get().installParticleHandler(ParticleHandler::get());
}

GameplaySubsystemsBootstrap::~GameplaySubsystemsBootstrap()
{
    // Uninitialize the particle handler.
    EngineContext::get().clearParticleHandler();
    ParticleHandler::uninitialize();

    // Uninitialize the audio system.
    EngineContext::get().clearAudioSystem();
    AudioSystem::uninitialize();
}
