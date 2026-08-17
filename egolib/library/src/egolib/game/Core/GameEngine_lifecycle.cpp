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

#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/GameplaySubsystemsBootstrap.hpp"
#include "egolib/game/Core/ConsoleBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/Core/System.hpp"                 // Ego::Core::System
#include "egolib/Graphics/GraphicsWindow.hpp"     // Ego::GraphicsWindow (complete type)

#include "egolib/Graphics/Font.hpp"               // Ego::Font (complete type)
#include "egolib/InputControl/InputSystem.hpp"    // Ego::Input::InputSystem
#include "egolib/font_bmp.h"                       // font_bmp_load_vfs
#include "egolib/game/Graphics/GraphicsBootstrap.hpp"  // runGraphicsBootstrapInit/Teardown
#include "egolib/game/Graphics/BillboardSystem.hpp"  // Ego::Graphics::BillboardSystem (complete type)
#include "egolib/game/GameStates/GameState.hpp"
#include "egolib/game/IPlayingStateController.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/FileFormats/Globals.hpp"
#include "egolib/InputControl/ControlSettingsFile.hpp"
#include "egolib/game/GUI/UIManager.hpp"
#include "egolib/game/GUI/ScreenMessage.hpp"
#include "egolib/game/graphic.h"
#include "egolib/Renderer/OpenGL/Renderer.hpp"  // drainPendingTextureDeletions (deferred GL deletes)
#include "egolib/game/game.h"
#include "egolib/game/Physics/CollisionSystem.hpp"

void GameEngine::renderPreloadText(const std::string &text)
{
    static std::string preloadText("");

    preloadText += text + "\n";

    gfx_do_clear_screen();

    _uiManager->beginRenderUI();
        _uiManager->getDefaultFont()->drawTextBox(preloadText, 20, 20, 800, 600, 25);
    _uiManager->endRenderUI();

    gfx_do_flip_pages();
}

bool GameEngine::initialize()
{
    // Wire the GUI-layer on-screen status-message seam to the game's DisplayMsg log, so
    // lower-layer GUI code (e.g. UIManager screenshot status) can post messages without a
    // direct upward dependency on the game DisplayMsg system.
    Ego::GUI::installScreenMessageSink([](const std::string& text) { DisplayMsg_print(text); });

    /* ********************************************************************************** */
    // >>> This must be done as the crappy old systems do not "pull" their configuration.
    //      More recent systems like video or audio system pull their configuraiton data
    //      by the time they are initialized.

    // Initialize the input system and enable mouse and keyboard.
    Ego::Input::InputSystem::initialize();
    EngineContext::get().installInputSystem(Ego::Input::InputSystem::get());

    // renderer options
    gfx_config_t::download(gfx, EngineContext::get().config());

    // <<<
    /* ********************************************************************************** */

    // Initialize the GFX + camera systems. Their concrete construction (the GFX GameApp, the
    // 11 RenderPasses, the BillboardSystem, the CameraSystem, the TextureAtlasManager) lives in
    // egolib-game-graphics, ABOVE egolib-library; GameEngine triggers it here — at the original
    // call site, preserving ordering — through the bootstrap hook registered from Main.cpp.
    Ego::Graphics::runGraphicsBootstrapInit();


    // Subscribe to window events.
    subscribe();

	// TODO: REMOVE THIS.
	gfx_system_init_all_graphics();
	gfx_do_clear_screen();

    // Install the gameplay audio + particle subsystems (audio then particle).
    _gameplaySubsystemsBootstrap = std::make_unique<GameplaySubsystemsBootstrap>();

    // Install the in-game developer console (sized from the graphics window).
    _consoleBootstrap = std::make_unique<ConsoleBootstrap>([this]{ return getActivePlayingState() != nullptr; });


    // load the bitmapped font (must be done after gfx_system_init_all_graphics())
    font_bmp_load_vfs("mp_data/font_new_shadow", "mp_data/font.txt");

    // setup the system gui
    _uiManager = std::make_unique<Ego::GUI::UIManager>();
    // Publish it through the GUI-layer seam so lower-layer widgets (Component::uiManager()) can
    // reach it without depending on this app-layer engine context.
    Ego::GUI::installActiveUIManager(*_uiManager);

    //Tell them we are loading the game (This is earliest point we can render text to screen)
    renderPreloadText("Initializing game...");

#ifdef ID_OSX
    // Run the Cocoa event loop a few times so the window appears
    for (int i = 0; i < 4; i++) SDL_PumpEvents();
#endif

    // Initialize the sound system.
    renderPreloadText("Loading audio...");
    auto& audioSystem = EngineContext::get().audioSystem();
    audioSystem.loadAllMusic();
    playMainMenuSong();
    audioSystem.loadGlobalSounds();

    // synchronize the config values with the various game subsystems
    // do this after the ego_init_SDL() and gfx_system_init_OpenGL() in case the config values are clamped
    // to valid values
    renderPreloadText("Configurating game data...");
    config_synch(EngineContext::get().config(), false, false);

    // load input
    if (!input_settings_load_vfs("/controls.txt"))
    {
        // input_settings_load_vfs never throws for a missing/unreadable/malformed file
        // (ControlSettingsFile.cpp), it returns false instead - so this is a warning, not a
        // fatal boot condition. Neither failure mode it covers leaves a clean "default" state
        // though: a missing/unopenable file applies no mappings at all, leaving every device
        // exactly as InputDevice's constructor left it (all-unbound, InputDevice.cpp), while a
        // truncated file applies mappings up to the failure point with no rollback (pinned in
        // ControlSettingsFile.cpp's test suite) - so bindings can be entirely unbound or only
        // partially loaded.
        EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__,
                                                                 "unable to load input settings from ",
                                                                 "`", "/controls.txt", "`",
                                                                 "; control bindings may be missing or incomplete",
                                                                 Log::EndOfEntry);
    }

    ContentRuntimeBootstrap::Options contentBootstrapOptions;
    contentBootstrapOptions.initializePerkHandler = true;
    contentBootstrapOptions.initializeProfileSystem = true;
    _contentRuntimeBootstrap = std::make_unique<ContentRuntimeBootstrap>(contentBootstrapOptions);

    // Initialize the collision system.
    Ego::Physics::CollisionSystem::initialize();

    // Load all modules
    renderPreloadText("Loading modules...");
    EngineContext::get().profileSystem().loadModuleProfiles();

    // Check savegame folder
    renderPreloadText("Loading save games...");
    EngineContext::get().profileSystem().loadAllSavedCharacters("mp_players");

    // clear out the import and remote directories
    renderPreloadText("Finished!");
    vfs_empty_temp_directories();

    //Start the main menu (factory injected by the bootstrap before start())
    if (!_mainMenuStateFactory) throw std::logic_error("main-menu-state factory not installed before start()");
    pushGameState(_mainMenuStateFactory());

    return true;
}

void GameEngine::subscribe() {
    auto window = EngineContext::get().graphicsSystem().getWindow();
    shown = window->window_shown.subscribe([](const idlib::events::window_shown_event& e) {
        /// @todo Is this still needed?
        gfx_system_reload_all_textures();
    });
    hidden = window->window_hidden.subscribe([](const idlib::events::window_hidden_event& e) {
    });
    resized = window->window_resized.subscribe([](const idlib::events::window_resized_event& e) {
    });
#if 0
    mouseEntered = window->mouse_entered.subscribe([](const idlib::events::mouse_pointer_entered_event& e) {
        Ego::Input::InputSystem::get().mouse.enabled = true;
    });
    mouseLeft = window->mouse_left.subscribe([](const idlib::events::mouse_pointer_exited_event& e) {
        Ego::Input::InputSystem::get().mouse.enabled = false;
    });
    keyboardFocusReceived = window->keyboard_input_focus_received.subscribe([](const idlib::events::keyboard_input_focus_received_event& e) {
        Ego::Input::InputSystem::get().keyboard.enabled = true;
    });
    keyboardFocusLost = window->keyboard_input_focus_lost.subscribe([](const idlib::events::keyboard_input_focus_lost_event& e) {
        Ego::Input::InputSystem::get().keyboard.enabled = false;
    });
#endif
}

void GameEngine::unsubscribe() {
#if 0
    keyboardFocusLost.disconnect();
    keyboardFocusReceived.disconnect();
    mouseLeft.disconnect();
    mouseEntered.disconnect();
#endif
    resized.disconnect();
    hidden.disconnect();
    shown.disconnect();
}

void GameEngine::uninitialize()
{
    EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Message, __FILE__, __LINE__, "uninitializing Egoboo ", GAME_VERSION, Log::EndOfEntry);

    _gameStateStack.clear();
    _currentGameState.reset();
    GameSessionContext::get().quitModule();

    // synchronize the config values with the various game subsystems
    config_synch(EngineContext::get().config(), true, true);

    // delete all the graphics allocated by SDL and OpenGL
    gfx_system_release_all_graphics();

    // make sure that the current control configuration is written
    input_settings_save_vfs("controls.txt");

    // @todo This should be 'UIManager::uninitialize'.
    Ego::GUI::clearActiveUIManager();
    _uiManager.reset(nullptr);

    // Tear down the on-screen status-message seam installed in initialize().
    Ego::GUI::clearScreenMessageSink();

    // Uninitialize the collision system.
    Ego::Physics::CollisionSystem::uninitialize();

    _contentRuntimeBootstrap.reset();

    // Uninitialize the in-game developer console.
    _consoleBootstrap.reset();

    // Tear down the gameplay audio + particle subsystems (particle then audio).
    _gameplaySubsystemsBootstrap.reset();

    // Unsubscribe from window events.
    unsubscribe();

    // Uninitialize the GFX + camera systems (teardown hook; mirror of runGraphicsBootstrapInit,
    // implemented in egolib-game-graphics).
    Ego::Graphics::runGraphicsBootstrapTeardown();

	// Uninitialize the input system.
    EngineContext::get().clearInputSystem();
	Ego::Input::InputSystem::uninitialize();

    // Shut down the log services.
	EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Info, __FILE__, __LINE__, "exiting Egoboo ", GAME_VERSION, ". See you next time", Log::EndOfEntry);
}
