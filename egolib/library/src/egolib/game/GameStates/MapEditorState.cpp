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

/// @file egolib/game/GameStates/MapEditorState.cpp
/// @details Main state where the players are currently playing a module
/// @author Johan Jansen

#include "egolib/game/GameStates/MapEditorState.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/GUI/UIManager.hpp"
#include "egolib/game/GameStates/InGameMenuState.hpp"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/GUI/MiniMap.hpp"
#include "egolib/game/GUI/Button.hpp"
#include "egolib/Physics/ICollisionWorld.hpp"
#include "egolib/game/game.h"
#include "egolib/game/Graphics/TileList.hpp"
#include "egolib/game/Graphics/Camera.hpp"

#include "egolib/game/Module/Module.hpp"
#include "egolib/Entities/_Include.hpp"

#include "egolib/Graphics/GraphicsWindow.hpp"    // Ego::GraphicsWindow

namespace Ego
{
namespace GameStates
{

namespace
{
Ego::Input::IInputSystem& inputSystem()
{
    return EngineContext::get().inputSystem();
}
}

MapEditorState::MapEditorState(std::shared_ptr<ModuleProfile> module) :
    _miniMap(std::make_shared<Ego::GUI::MiniMap>()),
    _modeButtons(),
    _editMode(EditorMode::MAP_EDIT_NONE)
{
    //Add minimap to the list of GUI components to render
    _miniMap->setSize(Vector2f(Ego::GUI::MiniMap::MAPSIZE, Ego::GUI::MiniMap::MAPSIZE));
    _miniMap->setPosition(Point2f(0, uiManager().getScreenHeight()-_miniMap->getHeight()));
    _miniMap->setVisible(true);
    addComponent(_miniMap);

    //Load the module
    loadModuleData(module);

    //Add edit modes
    addModeEditButton(EditorMode::MAP_EDIT_OBJECTS, "Objects");
    addModeEditButton(EditorMode::MAP_EDIT_PASSAGES, "Passages");
    addModeEditButton(EditorMode::MAP_EDIT_MESH, "Mesh");

    //Center the camera in the middle of the map
    auto& cw = Ego::Physics::activeCollisionWorld();
    Vector3f mapCenter;
    mapCenter.x() = cw.getTileCountX()*Info<float>::Grid::Size() * 0.5f;
    mapCenter.y() = cw.getTileCountY()*Info<float>::Grid::Size() * 0.5f;
    mapCenter.z() = cw.getElevation(Vector2f(mapCenter.x(), mapCenter.y()), false);
    EngineContext::get().cameraSystem().getMainCamera()->setPosition(mapCenter);
}

void MapEditorState::addModeEditButton(EditorMode mode, const std::string &label)
{
    auto editModeButton = std::make_shared<Ego::GUI::Button>(label);
    editModeButton->setSize(Vector2f(120, 30));
    editModeButton->setPosition(Point2f(_modeButtons.size() * (editModeButton->getWidth() + 5), 0));
    editModeButton->setOnClickFunction([this, mode, editModeButton]{
        _editMode = mode;
        for(const auto& button : _modeButtons) {
            button->setEnabled(true);
        }
        editModeButton->setEnabled(false);
    });
    addComponent(editModeButton);
    _modeButtons.push_back(editModeButton);    
}

void MapEditorState::update()
{
    GameSessionContext& session = GameSessionContext::get();
    GameModule& module = session.activeModule();
    const ISessionState& sessionState = activeSessionState();
    ISessionStatePublisher& sessionPublisher = activeSessionStatePublisher();

    // Get immediate mode state for the rest of the game
    inputSystem().update();

    //Rebuild the quadtree for fast object lookup
    auto& cw = Ego::Physics::activeCollisionWorld();
    module.getObjectHandler().updateQuadTree(0.0f, 0.0f, cw.getTileCountX()*Info<float>::Grid::Size(),
                                                         cw.getTileCountY()*Info<float>::Grid::Size());

    //Always reveal all invisible monsters and objects in Map Editor mode
    LocalPlayerPerceptionState localPlayerPerception = sessionState.localPlayerPerception();
    localPlayerPerception.seeInvisibleLevel = 100.0f;
    localPlayerPerception.seeInvisibleMagnitude = std::exp(0.32f * localPlayerPerception.seeInvisibleLevel);
    sessionPublisher.publishLocalPlayerPerception(localPlayerPerception);

    //Animate water
    module.getWater().update();

    //Update camera movement
    EngineContext::get().cameraSystem().getMainCamera()->updateFreeControl();
}

void MapEditorState::drawContainer(Ego::GUI::DrawingContext& drawingContext)
{
    EngineContext::get().cameraSystem().renderAll(gfx_system_render_world);

    // NOTE: deliberately do NOT call draw_hud() here. draw_hud() renders the in-game
    // PLAYER HUD (FPS / help / debug / timer / game-status), all anchored at the top-left
    // (y=0) — exactly where the editor mode buttons sit — so it drew on top of them and
    // garbled the UI. The editor's own widgets (mode buttons + minimap) are rendered by
    // Container::drawAll() after this; the player HUD is not wanted in the editor.

    //Draw passages?
    if(_editMode == EditorMode::MAP_EDIT_PASSAGES) {
        draw_passages(*EngineContext::get().cameraSystem().getMainCamera());
    }
}

void MapEditorState::beginState()
{
    // in-game settings
    EngineContext::get().graphicsSystem().setCursorVisibility(true);
    EngineContext::get().graphicsSystem().getWindow()->grab_enabled(EngineContext::get().config().debug_grabMouse.getValue());
}

bool MapEditorState::notifyKeyboardKeyPressed(const Ego::Events::KeyboardKeyPressedEvent& e)
{
    switch(e.get_key())
    {
        case SDLK_ESCAPE:
            engine().pushGameState(std::make_shared<InGameMenuState>(*this));
        return true;
    }

    return Container::notifyKeyboardKeyPressed(e);
}

void MapEditorState::loadModuleData(std::shared_ptr<ModuleProfile> module)
{
    //Make sure any old data is cleared first
    game_quit_module();

    // Reset all loaded "profiles" in the "profile system".
    EngineContext::get().profileSystem().reset();

    // try to start a new module
    GameSessionContext::get().beginModule(module);

    // set up the cameras *after* game_begin_module() or the player devices will not be initialized
    // and camera_system_begin() will not set up the correct view
    EngineContext::get().cameraSystem().setNumberOfCameras(1);
    EngineContext::get().cameraSystem().getMainCamera()->setCameraMovementMode(CameraMovementMode::Free);

    // make sure the per-module configuration settings are correct
    config_synch(EngineContext::get().config(), true, false);

    //Have to do this function in the OpenGL context thread or else it will fail
    EngineContext::get().textureAtlasManager().loadTileSet();
}


} //GameStates
} //Ego
