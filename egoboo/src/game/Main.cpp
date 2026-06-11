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
/// @author Johan Jansen
/// @author Michael Heilmann

#include "egolib/Core/System.hpp"  // Ego::Core::System
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/GUI/UIManager.hpp"
#include "egolib/game/GameStates/MainMenuState.hpp"  // initial-state factory (injected into GameEngine)
#include "egolib/Script/IScriptSystem.hpp"            // installDefaultScriptSystem() (injected from above egolib-library)
#include "egolib/game/Graphics/GraphicsBootstrap.hpp" // installDefaultGraphicsSystems() (injected from egolib-game-graphics)

/**
 * @brief
 *  The entry point of the program.
 * @param argc
 *  the number of command-line arguments (number of elements in the array pointed by @a argv)
 * @param argv
 *  the command-line arguments (a static constant array of @a argc pointers to static constant zero-terminated strings)
 * @return
 *  EXIT_SUCCESS upon regular termination, EXIT_FAILURE otherwise
 */
int main(int argc, char **argv)
{
    try
    {
        Ego::Core::System::initialize(std::string(argv[0]));
        try
        {
            EngineContext& engineContext = EngineContext::get();
            engineContext.setEngine(std::make_unique<GameEngine>());
            engineContext.engine().setMainMenuStateFactory([]() -> std::shared_ptr<GameState> {
                return std::make_shared<MainMenuState>();
            });
            // Install the VM-backed script system from above egolib-library (the adapter lives in
            // the egolib-scriptvm archive, so the install must come from here, not from inside the
            // library — same shape as the main-menu-state factory injection above).
            Ego::Script::installDefaultScriptSystem();
            // Register the graphics-systems bootstrap (GFX GameApp + camera + billboard + atlas).
            // It is constructed in egolib-game-graphics (above egolib-library), so — like the
            // script adapter — the install must come from here; GameEngine::initialize() runs the
            // registered hook at the original call site, preserving init ordering.
            Ego::Graphics::installDefaultGraphicsSystems();
            engineContext.engine().start();
            engineContext.clearEngine();
        }
        catch (...)
        {
            EngineContext::get().clearEngine();
            Ego::Core::System::uninitialize();
            std::rethrow_exception(std::current_exception());
		}
		Ego::Core::System::uninitialize();
    }
    catch (const idlib::exception& ex)
    {
        std::cerr << "unhandled exception: " << std::endl
                  << ex.to_string() << std::endl;

        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                 "Unhandled Exception",
                                 ex.to_string().c_str(),
                                 nullptr);

        return EXIT_FAILURE;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "unhandled exception: " << std::endl
                  << ex.what() << std::endl;

        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                 "Unhandled asException",
                                 ex.what(),
                                 nullptr);

        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cerr << "unhandled exception" << std::endl;

        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                 "Unhandled Exception",
                                 "Unknown exception type",
                                 nullptr);

        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
