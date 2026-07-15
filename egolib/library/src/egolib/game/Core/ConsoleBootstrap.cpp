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

/// @file egolib/game/Core/ConsoleBootstrap.cpp

#include "egolib/game/Core/ConsoleBootstrap.hpp"

#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/Console/Console.hpp"
#include "egolib/Graphics/GraphicsWindow.hpp"

#include <string>
#include <utility>

ConsoleBootstrap::ConsoleBootstrap(std::function<bool()> isPlaying)
{
    // Initialize the console.
    auto rectangle = Ego::Rectangle2f(idlib::zero<Ego::Point2f>(), { EngineContext::get().graphicsSystem().getWindow()->drawable_size()(0),
                                                                     EngineContext::get().graphicsSystem().getWindow()->drawable_size()(1) * 0.25 });

    Ego::Core::Console::initialize(rectangle);
    Ego::Core::Console::get().ExecuteCommand.subscribe([isPlaying = std::move(isPlaying)](std::string command) {
        if (command == "grog()" || command == "daze()")
        {
            if (!isPlaying())
            {
                Ego::Core::Console::get().add_output(command + " can only be invoked when playing\n");
            }
        }
        if (command == "exit()")
        {}
    });
}

ConsoleBootstrap::~ConsoleBootstrap()
{
    // Uninitialize the console.
    Ego::Core::Console::uninitialize();
}
