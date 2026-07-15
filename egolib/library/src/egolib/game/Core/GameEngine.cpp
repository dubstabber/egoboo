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
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Core/ISessionState.hpp"
#include "egolib/Core/System.hpp"                 // Ego::Core::System
#include "egolib/Console/Console.hpp"             // Ego::Core::Console
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

namespace
{
egoboo_config_t& config()
{
    return EngineContext::get().config();
}

Ego::Input::IInputSystem& inputSystem()
{
    return EngineContext::get().inputSystem();
}

uint32_t currentUpdateFrame()
{
    if (ISessionState* sessionState = tryActiveSessionState())
    {
        return sessionState->worldUpdateCount();
    }

    return GameSessionContext::get().worldUpdateCount();
}
}

//Declaration of class constants
const uint32_t GameEngine::GAME_TARGET_FPS;
const uint32_t GameEngine::GAME_TARGET_UPS;

const uint64_t GameEngine::DELAY_PER_RENDER_FRAME;
const uint64_t GameEngine::DELAY_PER_UPDATE_FRAME;

const uint32_t GameEngine::MAX_FRAMESKIP;

const std::string GameEngine::GAME_VERSION = "2.9.0";

GameEngine::GameEngine() :
    _startupTimestamp(),
	_terminateRequested(false),
	_updateTimeout(0),
	_renderTimeout(0),
	_gameStateStack(),
	_currentGameState(nullptr),
    _clearGameStateStackRequested(false),
	_config(),
    _drawCursor(true),
    _screenshotReady(true),
    _screenshotRequested(false),

    _lastFrameEstimation(0),
    _frameSkip(0),
    _lastFPSCount(0),
    _lastUPSCount(0),
    _estimatedFPS(GAME_TARGET_FPS),
    _estimatedUPS(GAME_TARGET_UPS),

    _totalFramesRendered(0),

    // Subscriptions
    shown(),
    hidden(),
    resized(),
#if 0
    mouseEntered(),
    mouseLeft(),
    keyboardFocusReceived(),
    keyboardFocusLost(),
#endif
    // Submodules
    _contentRuntimeBootstrap(nullptr),
    _gameplaySubsystemsBootstrap(nullptr),
    _uiManager(nullptr)
{
    //ctor
}

GameEngine::~GameEngine() = default;

void GameEngine::shutdown()
{
    _terminateRequested = true;
}

uint64_t GameEngine::getMicros() const
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - _startupTimestamp).count();
}

void GameEngine::start()
{    
    initialize();

    //Initialize clock timeout	
    _startupTimestamp = std::chrono::high_resolution_clock::now();
    _updateTimeout = getMicros() + DELAY_PER_UPDATE_FRAME;
    _renderTimeout = getMicros() + DELAY_PER_RENDER_FRAME;

    while(!_terminateRequested)
    {
        // Test the panic button
        const uint8_t *keyboardState = SDL_GetKeyboardState(nullptr);
        if (keyboardState[SDL_SCANCODE_Q] && keyboardState[SDL_SCANCODE_LCTRL])
        {
            // Terminate the program
            shutdown();
            break;
        }

        // Check if it is time to update everything
        for(_frameSkip = 0; _frameSkip < MAX_FRAMESKIP && getMicros() > _updateTimeout; ++_frameSkip)
        {
            updateOneFrame();
            _updateTimeout += DELAY_PER_UPDATE_FRAME;
        }

        //Prevent accumulating more than 1 second of game updates (can happen in severe frame drops or breakpoints while debugging)
        const uint64_t now = getMicros();
        if(now > _updateTimeout + GAME_TARGET_UPS*DELAY_PER_UPDATE_FRAME) {
            _updateTimeout = now + DELAY_PER_UPDATE_FRAME;
            _renderTimeout = now;
        }

        // Check if it is time to draw everything
        if(getMicros() >= _renderTimeout)
        {
            // Draw the current frame
            renderOneFrame();

            // Stabilize FPS throttle every so often in case rendering is lagging behind
            if(_totalFramesRendered % GAME_TARGET_FPS == 0)
            {
                _renderTimeout = getMicros() + DELAY_PER_RENDER_FRAME;
            }
            else
            {
                _renderTimeout += DELAY_PER_RENDER_FRAME;
            }
        }
        else
        {
            //Don't hog CPU if we have nothing to do
            uint64_t now = getMicros();
            if(now < _renderTimeout && now < _updateTimeout) {
                int delay = std::min(_renderTimeout-now, _updateTimeout-now);
                std::this_thread::sleep_for(std::chrono::microseconds(delay));
            }

        }

        // Calculate estimations for FPS and UPS
        estimateFrameRate();        
    }

    uninitialize();
}

void GameEngine::estimateFrameRate()
{
    const uint64_t now = getMicros();
    const float dt = (now-_lastFrameEstimation) / 1e6f;

    //Throttle estimations to ten times per second
    if(dt < 0.1f) {
        return;
    }

    _estimatedFPS = (_totalFramesRendered-_lastFPSCount) / dt;
    const uint32_t worldUpdateCount = currentUpdateFrame();
    _estimatedUPS = (worldUpdateCount - _lastUPSCount) / dt;

    _lastFPSCount = _totalFramesRendered;
    _lastUPSCount = worldUpdateCount;
    _lastFrameEstimation = now;
}

void GameEngine::updateOneFrame()
{
    //Handle clearing the game state stack first. Should be done before any GUI components
    //become locked by the event or rendering loop
    if(_clearGameStateStackRequested) {
        _gameStateStack.clear();
        _gameStateStack.push_front(_currentGameState);
        _clearGameStateStackRequested = false;
    }

    // Fall through to next state if needed
    while(_currentGameState->isEnded())
    {
        if(!_gameStateStack.empty()) {
            _gameStateStack.pop_front();
        }

        // No more states? Default back to main menu
        if(_gameStateStack.empty())
        {
            if (!_mainMenuStateFactory) throw std::logic_error("main-menu-state factory not installed before start()");
            pushGameState(_mainMenuStateFactory());
        }
        else
        {
            _currentGameState = _gameStateStack.front();
            _currentGameState->beginState();
            _updateTimeout = getMicros() + DELAY_PER_UPDATE_FRAME;
            _renderTimeout = getMicros() + DELAY_PER_RENDER_FRAME;
        }
    }

    // Handle all SDL events    
    pollEvents();

    //Deferred loading for any textures requested by other threads
    EngineContext::get().textureManager().updateDeferredLoading();

    //Update current game state
    _currentGameState->update();

    updateScreenshotRequest();
}

void GameEngine::updateScreenshotRequest()
{
    if (inputSystem().isKeyDown(SDLK_F11))
    {
        requestScreenshot();
    }
}

void GameEngine::renderOneFrame()
{
    // clear the screen
    gfx_do_clear_screen();

    Ego::GUI::DrawingContext drawingContext;
    _currentGameState->drawAll(drawingContext);
    _totalFramesRendered++;

    //Draw mouse cursor last
    if(_drawCursor)
    {
        draw_mouse_cursor();
    }

    // Free any GL textures queued for deletion from a background thread (e.g. the module
    // loading thread tearing down the previous module). We are on the GL/main thread here,
    // after the frame's draws, so deleting them now is safe and runs every frame.
    if (Ego::Renderer::is_initialized())
    {
        static_cast<Ego::OpenGL::Renderer&>(EngineContext::get().renderer()).drainPendingTextureDeletions();
    }

    // flip the graphics page
    gfx_do_flip_pages();

    //Save screenshot if it has been requested
    if(_screenshotRequested)
    {
        if(_screenshotReady)
        {
            _screenshotReady = false;
            _screenshotRequested = false;
            
            if (!_uiManager->dumpScreenshot())
            {
                DisplayMsg_printf("Error writing screenshot!"); // send a failure message to the screen
                EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to write screenshot", Log::EndOfEntry);
            }
        }
    }
    else
    {
        _screenshotReady = true;
    }
}

void GameEngine::setGameState(std::shared_ptr<GameState> gameState)
{
    _clearGameStateStackRequested = true;
    pushGameState(gameState);
}

void GameEngine::pushGameState(std::shared_ptr<GameState> gameState)
{
    _gameStateStack.push_front(gameState);
    _currentGameState = _gameStateStack.front();
    _currentGameState->beginState();
    _updateTimeout = getMicros() + DELAY_PER_UPDATE_FRAME;
    _renderTimeout = getMicros() + DELAY_PER_RENDER_FRAME;
}

void GameEngine::pollEvents()
{
    EngineContext::get().graphicsSystem().update();
    EngineContext::get().graphicsSystem().getWindow()->update();
    // Message processing loop.
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        // Console has first say in events.
        if (config().debug_developerMode_enable.getValue())
        {
            if (!Ego::Core::Console::get().handle_event(&event))
            {
                continue;
            }
        }

        // Check for messages.
        switch (event.type)
        {
            // Exit if the window is closed.
            case SDL_QUIT:
                shutdown();
                return;
            case SDL_MOUSEWHEEL:
            {
                auto e = Ego::Events::MouseWheelTurnedEvent(Ego::Vector2f(event.wheel.x, event.wheel.y));
                _currentGameState->notifyMouseWheelTurned(e);
            }
            break;
                
            case SDL_MOUSEBUTTONDOWN:
            {
                auto e = Ego::Events::MouseButtonPressedEvent(Ego::Point2f(event.button.x, event.button.y), event.button.button);
                _currentGameState->notifyMouseButtonPressed(e);
            }
            break;

            case SDL_MOUSEBUTTONUP:
            {
                auto e = Ego::Events::MouseButtonReleasedEvent(Ego::Point2f(event.button.x, event.button.y), event.button.button);
                _currentGameState->notifyMouseButtonReleased(e);
            }
            break;
                
            case SDL_MOUSEMOTION:
            {
                auto e = Ego::Events::MousePointerMovedEvent(Ego::Point2f(event.motion.x, event.motion.y));
                _currentGameState->notifyMousePointerMoved(e);
            }
            break;
                
            case SDL_KEYUP:
            {
                auto e = Ego::Events::KeyboardKeyReleasedEvent(event.key.keysym.sym);
                _currentGameState->notifyKeyboardKeyReleased(e);
            }
            break;
            case SDL_KEYDOWN:
            {
                auto e = Ego::Events::KeyboardKeyPressedEvent(event.key.keysym.sym);
                _currentGameState->notifyKeyboardKeyPressed(e);
            }
            break;
        }
    } // end of message processing
}

float GameEngine::getFPS() const
{
    return _estimatedFPS;
}

float GameEngine::getUPS() const
{
    return _estimatedUPS;
}

int GameEngine::getFrameSkip() const
{
    return _frameSkip;
}

std::shared_ptr<IPlayingStateController> GameEngine::getActivePlayingState() const
{
    return std::dynamic_pointer_cast<IPlayingStateController>(getActiveGameState());
}

void GameEngine::setMainMenuStateFactory(std::function<std::shared_ptr<GameState>()> factory)
{
    _mainMenuStateFactory = std::move(factory);
}

std::shared_ptr<GameState> GameEngine::getActiveGameState() const
{
	return _currentGameState;
}

uint32_t GameEngine::getCurrentUpdateFrame() const
{
    return currentUpdateFrame();
}

uint32_t GameEngine::getNumberOfFramesRendered() const
{
    return _totalFramesRendered;
}
