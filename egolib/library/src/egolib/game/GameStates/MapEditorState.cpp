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
#include "egolib/game/GameStates/InGameMenuState.hpp"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/GUI/MiniMap.hpp"
#include "egolib/game/GUI/Button.hpp"
#include "egolib/game/game.h"
#include "egolib/game/Graphics/TileList.hpp"
#include "egolib/game/Graphics/CameraSystem.hpp"

#include "egolib/game/Module/Module.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Graphics/GraphicsSystemNew.hpp" // Ego::GraphicsSystemNew
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

    GameModule& activeModule = GameSessionContext::get().activeModule();

    //Center the camera in the middle of the map
    Vector3f mapCenter;
    mapCenter.x() = activeModule.getMeshPointer()->_info.getTileCountX()*Info<float>::Grid::Size() * 0.5f;
    mapCenter.y() = activeModule.getMeshPointer()->_info.getTileCountY()*Info<float>::Grid::Size() * 0.5f;
    mapCenter.z() = activeModule.getMeshPointer()->getElevation(Vector2f(mapCenter.x(), mapCenter.y()), false);
    CameraSystem::get().getMainCamera()->setPosition(mapCenter);
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

    // Get immediate mode state for the rest of the game
    inputSystem().update();

    //Rebuild the quadtree for fast object lookup
    module.getObjectHandler().updateQuadTree(0.0f, 0.0f, module.getMeshPointer()->_info.getTileCountX()*Info<float>::Grid::Size(),
		                                                         module.getMeshPointer()->_info.getTileCountY()*Info<float>::Grid::Size());

    //Always reveal all invisible monsters and objects in Map Editor mode
    LocalPlayerPerceptionState localPlayerPerception = session.localPlayerPerception();
    localPlayerPerception.seeInvisibleLevel = 100.0f;
    localPlayerPerception.seeInvisibleMagnitude = std::exp(0.32f * localPlayerPerception.seeInvisibleLevel);
    session.publishLocalPlayerPerception(localPlayerPerception);

    //Animate water
    module.getWater().update();

    //Update camera movement
    CameraSystem::get().getMainCamera()->updateFreeControl();
}

void MapEditorState::drawContainer(Ego::GUI::DrawingContext& drawingContext)
{
    CameraSystem::get().renderAll(gfx_system_render_world);
    draw_hud();

    //Draw passages?
    if(_editMode == EditorMode::MAP_EDIT_PASSAGES) {
        draw_passages(*CameraSystem::get().getMainCamera());
    }
}

void MapEditorState::beginState()
{
    // in-game settings
    Ego::GraphicsSystemNew::get().setCursorVisibility(true);
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

    // do some graphics initialization
    gfx_system_make_enviro();

    // try to start a new module
    GameSessionContext::get().beginModule(module);

    // set up the cameras *after* game_begin_module() or the player devices will not be initialized
    // and camera_system_begin() will not set up the correct view
    CameraSystem::get().setNumberOfCameras(1);
    CameraSystem::get().getMainCamera()->setCameraMovementMode(CameraMovementMode::Free);

    // make sure the per-module configuration settings are correct
    config_synch(EngineContext::get().config(), true, false);

    //Have to do this function in the OpenGL context thread or else it will fail
    EngineContext::get().textureAtlasManager().loadTileSet();
}


} //GameStates
} //Ego
