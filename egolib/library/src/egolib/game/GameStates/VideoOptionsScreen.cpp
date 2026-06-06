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

/// @file egolib/game/GameStates/VideoOptionsScreen.cpp
/// @details Video settings
/// @author Johan Jansen

#include "egolib/game/GameStates/VideoOptionsScreen.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/GameStates/OptionsConfigActions.hpp"
#include "egolib/game/GUI/Button.hpp"
#include "egolib/game/GUI/Image.hpp"
#include "egolib/game/GUI/Label.hpp"
#include "egolib/game/GUI/ScrollableList.hpp"

namespace
{
namespace Actions = Ego::GameStates::Internal::OptionsConfigActions;
}

VideoOptionsScreen::VideoOptionsScreen() :
    _resolutionList(std::make_shared<Ego::GUI::ScrollableList>())
{
    auto background = std::make_shared<Ego::GUI::Image>("mp_data/menu/menu_video");

    const int SCREEN_WIDTH = uiManager().getScreenWidth();
    const int SCREEN_HEIGHT = uiManager().getScreenHeight();

    // calculate the centered position of the background
    background->setSize({ background->getTextureWidth() * 0.75f, background->getTextureHeight() * 0.75f });
    background->setPosition({ SCREEN_WIDTH - background->getWidth(), SCREEN_HEIGHT - background->getHeight() });
    addComponent(background);

    //Resolution
    auto resolutionLabel = std::make_shared<Ego::GUI::Label>("Resolution");
    resolutionLabel->setPosition({ 20, 5 });
    addComponent(resolutionLabel);

    _resolutionList->setSize({ SCREEN_WIDTH / 3, SCREEN_HEIGHT / 2 });
    _resolutionList->setPosition(resolutionLabel->getPosition() + Ego::Vector2f(0, resolutionLabel->getHeight()));
    addComponent(_resolutionList);

    //Build list of available resolutions
    std::unordered_set<uint32_t> resolutions;
    const auto& displays = Ego::GraphicsSystemNew::get().getDisplays();
    auto displayIt = std::find_if(displays.cbegin(), displays.cend(), [](const auto& display) { return display->isPrimaryDisplay(); });
    if (displayIt == displays.cend())
    {
        throw idlib::runtime_error(__FILE__, __LINE__, "unable to get primary display");
    }
    for (const auto &displayMode : (*displayIt)->getDisplayModes())
    {
        //Skip duplicate resolutions (32-bit, 24-bit, 16-bit etc.)
        if(resolutions.find(displayMode->getHorizontalResolution() | displayMode->getVerticalResolution() << 16) != resolutions.end()) {
            continue;
        }

        addResolutionButton(displayMode->getHorizontalResolution(), displayMode->getVerticalResolution());
        resolutions.insert(displayMode->getHorizontalResolution() | displayMode->getVerticalResolution() << 16);
    }
    
    _resolutionList->forceUpdate();

    int xPos = 50 + SCREEN_WIDTH/3;
    int yPos = 30;

    //Fullscreen button
    yPos += addOptionsButton(xPos, yPos, 
        "Fullscreen", 
        
        //String description of current state
        []{ 
            return Actions::fullscreenLabel();
        },

        //Change option effect
        []{
            Actions::toggleFullscreen([](bool enabled)
            {
                SDL_SetWindowFullscreen(EngineContext::get().graphicsSystem().getWindow()->get(), enabled ? SDL_WINDOW_FULLSCREEN : 0);
            });
        }
    );

    //Shadows
    yPos += addOptionsButton(xPos, yPos, 
        "Shadows", 
        
        //String description of current state
        []{ 
            return Actions::shadowsLabel();
        },

        //Change option effect
        []{
            Actions::cycleShadows();
        }
    );

    //Texture Filtering
    yPos += addOptionsButton(xPos, yPos, 
        "Texture Quality", 
        
        //String description of current state
        []{ 
            return Actions::textureQualityLabel();
        },

        //Change option effect
        []{
            Actions::cycleTextureQuality();
        }
    );

    //Anisotropic Filtering
    yPos += addOptionsButton(xPos, yPos, 
        "Anisotropic Filtering", 
        
        //String description of current state
        []{ 
            return Actions::anisotropyLabel();
        },

        //Change option effect
        []{
            Actions::cycleAnisotropy();
        },

        //Only enable button if option is supported by graphics card
        Actions::anisotropySupported()
    );

    //Anti-Aliasing
    yPos += addOptionsButton(xPos, yPos, 
        "Anti-Aliasing", 
        
        //String description of current state
        []{ 
            return Actions::antiAliasingLabel();
        },

        //Change option effect
        []{
            Actions::toggleAntiAliasing();
        }
    );

    //HD Textures
    yPos += addOptionsButton(xPos, yPos, 
        "Use HD Textures", 
        
        //String description of current state
        []{ 
            return Actions::hdTexturesLabel();
        },

        //Change option effect
        []{
            Actions::toggleHDTextures();
        }
    );    

    // Back button
    auto backButton = std::make_shared<Ego::GUI::Button>("Back", SDLK_ESCAPE);
    backButton->setPosition({ 20, SCREEN_HEIGHT - 80 });
    backButton->setSize({ 200, 30 });
    backButton->setOnClickFunction(
    [this]{
        endState();

        // save the setup file
        Actions::saveConfig([](egoboo_config_t& config)
        {
            Ego::Setup::upload(config);
        });
    });
    addComponent(backButton);

    //Add version label and copyright text
    auto welcomeLabel = std::make_shared<Ego::GUI::Label>("Change video settings here");
    welcomeLabel->setPosition({ backButton->getX() + backButton->getWidth() + 40,
                                SCREEN_HEIGHT - SCREEN_HEIGHT / 60 - welcomeLabel->getHeight() });
    addComponent(welcomeLabel);
}

int VideoOptionsScreen::addOptionsButton(int xPos, int yPos, const std::string &label, std::function<std::string()> labelFunction, std::function<void()> onClickFunction, bool enabled)
{
    auto optionLabel = std::make_shared<Ego::GUI::Label>(label + ": ");
    optionLabel->setPosition({ xPos, yPos });
    addComponent(optionLabel);

    auto optionButton = std::make_shared<Ego::GUI::Button>(labelFunction());
    optionButton->setSize({ 150, 30 });
    optionButton->setPosition({ xPos + 250, optionLabel->getY() });
    optionButton->setOnClickFunction(
        [optionButton, onClickFunction, labelFunction]{
            onClickFunction();
            optionButton->setText(labelFunction());
        });
    optionButton->setEnabled(enabled);
    addComponent(optionButton); 

    return optionButton->getHeight() + 5;
}

void VideoOptionsScreen::update()
{
}

void VideoOptionsScreen::drawContainer(Ego::GUI::DrawingContext& drawingContext)
{

}

void VideoOptionsScreen::beginState()
{
    // menu settings
    EngineContext::get().graphicsSystem().getWindow()->grab_enabled(false);
    engine().enableMouseCursor();
}

void VideoOptionsScreen::addResolutionButton(int width, int height)
{
    auto resolutionButton = std::make_shared<Ego::GUI::Button>(std::to_string(width) + "x" + std::to_string(height));

    resolutionButton->setSize({ 200, 30 });
    resolutionButton->setOnClickFunction(
        [width, height, resolutionButton, this]
        {
            Actions::selectResolution(width, height);

            // Enable all resolution buttons except the one we just selected.
            for(const auto& button : _resolutionList->iterator())
            {
                button->setEnabled(true);
            }
            resolutionButton->setEnabled(false);
        }
    );
    _resolutionList->addComponent(resolutionButton);

    //If this is our current resolution then make it greyed out
    if (Actions::isResolutionSelected(width, height))
    {
        resolutionButton->setEnabled(false);
    }
}
