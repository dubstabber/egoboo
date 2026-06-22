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

/// @file egolib/game/GameStates/PlayingState.cpp
/// @details Main state where the players are currently playing a module
/// @author Johan Jansen

#include "egolib/game/GameStates/PlayingState.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/GUI/UIManager.hpp"
#include "egolib/game/GameStates/InGameMenuState.hpp"
#include "egolib/game/GameStates/VictoryScreen.hpp"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/GUI/InternalDebugWindow.hpp"
#include "egolib/game/GUI/MiniMap.hpp"
#include "egolib/game/GUI/CharacterStatus.hpp"
#include "egolib/game/GUI/CharacterWindow.hpp"
#include "egolib/game/GUI/MessageLog.hpp"
#include "egolib/game/game.h"
#include "egolib/game/graphic.h"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/game/Graphics/Camera.hpp"
#include "egolib/Graphics/Viewport.hpp"
#include "egolib/Time/Time.hpp"                       // ::Time::now

#include "egolib/Graphics/GraphicsWindow.hpp" // Ego::GraphicsWindow
#include "egolib/font_bmp.h" // fontyspacing

//For cheats
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/Module/Module.hpp"

namespace
{
IAudioSystem& audioSystem()
{
    return EngineContext::get().audioSystem();
}

Ego::Input::IInputSystem& inputSystem()
{
    return EngineContext::get().inputSystem();
}

Object* tryObservedUiObject(ObjectRef objectRef)
{
    Object* object = GameSessionContext::get().tryObject(objectRef);
    return object != nullptr && !object->isTerminated() ? object : nullptr;
}
}

PlayingState::PlayingState() :
    _miniMap(std::make_shared<Ego::GUI::MiniMap>()),
    _messageLog(std::make_shared<Ego::GUI::MessageLog>()),
    _statusList()
{
    //For debug only
    if (EngineContext::get().config().debug_developerMode_enable.getValue())
    {
        auto debugWindow = std::make_shared<Ego::GUI::InternalDebugWindow>("CurrentModule");
        debugWindow->addWatchVariable("Passages", []{
            GameModule* module = GameSessionContext::get().tryActiveModule();
            return module ? std::to_string(module->getPassageCount()) : std::string("0");
        });
        debugWindow->addWatchVariable("ExportValid", []{
            GameModule* module = GameSessionContext::get().tryActiveModule();
            return module && module->isExportValid() ? "true" : "false";
        });
        debugWindow->addWatchVariable("ModuleBeaten", []{
            GameModule* module = GameSessionContext::get().tryActiveModule();
            return module && module->isBeaten() ? "true" : "false";
        });
        debugWindow->addWatchVariable("Players", []{
            GameModule* module = GameSessionContext::get().tryActiveModule();
            return module ? std::to_string(module->getPlayerAmount()) : std::string("0");
        });
        debugWindow->addWatchVariable("Imports", []{
            GameModule* module = GameSessionContext::get().tryActiveModule();
            return module ? std::to_string(module->getImportAmount()) : std::string("0");
        });
        debugWindow->addWatchVariable("Name", []{
            GameModule* module = GameSessionContext::get().tryActiveModule();
            return module ? module->getName() : std::string("n/a");
        });
        debugWindow->addWatchVariable("Path", []{
            GameModule* module = GameSessionContext::get().tryActiveModule();
            return module ? module->getPath() : std::string("n/a");
        });
        addComponent(debugWindow);        
    }

    //Add minimap to the list of GUI components to render
    _miniMap->setSize({ Ego::GUI::MiniMap::MAPSIZE, Ego::GUI::MiniMap::MAPSIZE });
    _miniMap->setPosition({ 0, uiManager().getScreenHeight() - _miniMap->getHeight() });
    addComponent(_miniMap);

    //Add the message log
    _messageLog->setSize({ uiManager().getScreenWidth() - WRAP_TOLERANCE, uiManager().getScreenHeight() / 3 });
    _messageLog->setPosition({ 0, fontyspacing });
    addComponent(_messageLog);

    //Show status display for all players
    GameModule& activeModule = GameSessionContext::get().activeModule();
    for(const std::shared_ptr<Ego::Player> &player : activeModule.getPlayerList()) {
        if (player != nullptr) {
            addStatusMonitor(player->getObjectRef());
        }
    }
}

PlayingState::~PlayingState()
{
    //Check for player exports
    if (GameModule* module = GameSessionContext::get().tryActiveModule();
        module && module->isExportValid())
    {
        // export the players
        export_all_players(false);

        //Reload list of loadable characters
        EngineContext::get().profileSystem().loadAllSavedCharacters("mp_players");
    }

    //Stop music. Guard against teardown ordering: on shutdown the engine clears the
    //audio system (GameEngine::uninitialize) before the game-state stack is destroyed,
    //so the throwing accessor would raise during destruction and terminate the process.
    if (IAudioSystem* audio = EngineContext::get().tryAudioSystem())
    {
        audio->fadeAllSounds();
    }
}

void PlayingState::updateStatusBarPosition()
{
    static uint32_t recalculateStatusBarPosition = 0;
    if(Time::now<Time::Unit::Ticks>() > recalculateStatusBarPosition) 
    {
        //Apply throttle... no need to do every update frame (5 Hz)
        recalculateStatusBarPosition = Time::now<Time::Unit::Ticks>() + 200;

        std::unordered_map<std::shared_ptr<Camera>, float> maxY;
        for(const std::weak_ptr<Ego::GUI::CharacterStatus> &weakStatus : _statusList)
        {
            auto status = weakStatus.lock();
            if(status)
            {
                Object* object = tryObservedUiObject(status->getObjectRef());
                if(object)
                {
                    auto camera = EngineContext::get().cameraSystem().getCamera(object->getObjRef());

                    //Shift component down a bit if required
                    status->setPosition({ status->getX(), maxY[camera] + 10.0f });

                    //Calculate bottom Y coordinate for this component
                    maxY[camera] = std::max<float>(maxY[camera], status->getY() + status->getHeight());                    
                }
                else
                {
                    status->destroy();
                }
            }
        }

    }
}

void PlayingState::update()
{
    // Get immediate mode state for the rest of the game
    inputSystem().update();

    GameSessionContext::get().activeModule().update();

    //Calculate position of all status bars
    updateStatusBarPosition();
}

void PlayingState::drawContainer(Ego::GUI::DrawingContext& drawingContext)
{
    EngineContext::get().cameraSystem().renderAll(gfx_system_render_world);
    draw_hud();
}

void PlayingState::beginState()
{
    // in-game settings
    EngineContext::get().graphicsSystem().setCursorVisibility(EngineContext::get().config().debug_hideMouse.getValue());
    EngineContext::get().graphicsSystem().getWindow()->grab_enabled(EngineContext::get().config().debug_grabMouse.getValue());
}

bool PlayingState::notifyKeyboardKeyPressed(const Ego::Events::KeyboardKeyPressedEvent& e)
{
    switch(e.get_key())
    {
        case SDLK_ESCAPE:

            //If we have won show the Victory Screen
            if(GameSessionContext::get().activeModule().isBeaten()) {
                engine().pushGameState(std::make_shared<VictoryScreen>(this));
            }

            //Else do the ingame menu
            else {
                engine().pushGameState(std::make_shared<InGameMenuState>(*this));                
            }
        return true;

        //Cheat debug button to win a module
        case SDLK_F9:
            if (EngineContext::get().config().debug_developerMode_enable.getValue())
            {
                GameModule& activeModule = GameSessionContext::get().activeModule();
                for(const std::shared_ptr<Object> &object : activeModule.getObjectHandler().iterator())
                {
                    if(object->isTerminated() || object->getProfile()->isInvincible()) {
                        continue;
                    }

                    if(!object->isPlayer() && object->isAlive())
                    {
                        object->kill(Object::INVALID_OBJECT, false);
                    }
                }
                return true;
            }
        break;

        //Show character sheet
        case SDLK_1:
        case SDLK_2:
        case SDLK_3:
        case SDLK_4:
        case SDLK_5:
        case SDLK_6:
        case SDLK_7:
        case SDLK_8:
        {
            //Ensure that the same character cannot open more than 1 character window
            const size_t statusNumber = e.get_key() - SDLK_1;
            displayCharacterWindow(statusNumber);
        }
        return true;
    }

    return Container::notifyKeyboardKeyPressed(e);
}

const std::shared_ptr<Ego::GUI::MiniMap>& PlayingState::getMiniMap() const
{
    return _miniMap;
}

bool PlayingState::showMiniMap()
{
    const bool wasHidden = !_miniMap->isVisible();
    _miniMap->setVisible(true);
    return wasHidden;
}

void PlayingState::setMiniMapShowPlayerPosition(bool showPlayerPosition)
{
    _miniMap->setShowPlayerPosition(showPlayerPosition);
}

void PlayingState::addMiniMapBlip(float x, float y, const std::shared_ptr<const Ego::Texture>& icon)
{
    _miniMap->addBlip(x, y, icon);
}

void PlayingState::addStatusMonitor(ObjectRef objectRef)
{
    Object* object = tryObservedUiObject(objectRef);
    if (object == nullptr) {
        return;
    }

    //Disabled by configuration?
    if(!EngineContext::get().config().hud_displayStatusBars.getValue()) {
        return;
    }

    //Already added?
    if(object->getShowStatus()) {
        return;
    }

    //Get the camera that is following this object (defaults to main camera)
    auto camera = EngineContext::get().cameraSystem().getCamera(object->getObjRef());

    auto status = std::make_shared<Ego::GUI::CharacterStatus>(objectRef);

    status->setSize({ BARX, BARY });
    status->setPosition({ camera->getViewport().getLeftPixels() + camera->getViewport().getWidthPixels() - status->getWidth(),
                          camera->getViewport().getTopPixels() });

    addComponent(status);
    _statusList.push_back(status);

    object->setShowStatus(true);
}

ObjectRef PlayingState::getStatusCharacterRef(size_t index)
{
    //First remove all expired elements
    auto condition = 
        [](const std::weak_ptr<Ego::GUI::CharacterStatus> &element)
        {   
            return element.expired();
        };
    _statusList.erase(std::remove_if(_statusList.begin(), _statusList.end(), condition), _statusList.end());


    if(index >= _statusList.size()) {
        return ObjectRef::Invalid;
    }

    std::shared_ptr<Ego::GUI::CharacterStatus> status = _statusList[index].lock();
    if(!status) {
        return ObjectRef::Invalid;
    }

    return status->getObjectRef();
}

void PlayingState::displayCharacterWindow(uint8_t statusNumber)
{
    if(statusNumber >= _characterWindows.size()) {
        return;
    }

    std::shared_ptr<Ego::GUI::CharacterWindow> chrWindow = _characterWindows[statusNumber].lock();
    if(chrWindow == nullptr || _characterWindows[statusNumber].expired())
    {
        const ObjectRef characterRef = getStatusCharacterRef(statusNumber);
        if(tryObservedUiObject(characterRef) != nullptr)
        {
            chrWindow = std::make_shared<Ego::GUI::CharacterWindow>(characterRef);
            _characterWindows[statusNumber] = chrWindow;
            addComponent(chrWindow);
        }
    }
    else
    {
        //Close window if same button is pressed twice
        chrWindow->destroy();
    }
}

const std::shared_ptr<Ego::GUI::MessageLog>& PlayingState::getMessageLog() const
{
    return _messageLog;
}

void PlayingState::endModuleInVictory()
{
    engine().pushGameState(std::make_shared<VictoryScreen>(nullptr, true));
}
