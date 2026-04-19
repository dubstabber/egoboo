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

/// @file egolib/game/graphic_hud.c
/// @brief HUD and UI drawing helpers for the graphics shell

#include "egolib/game/graphic_internal.h"

#include "egolib/Entities/_Include.hpp"
#include "egolib/game/GUI/Material.hpp"
#include "egolib/game/Graphics/CameraSystem.hpp"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/game/script_compile.h"
#include "egolib/game/game.h"

#include <algorithm>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

#define SPARKLE_SIZE ICON_SIZE
#define SPARKLE_AND  (SPARKLE_SIZE - 1)

namespace
{
using namespace gfx_internal;

float draw_fps(float y)
{
    parser_state_t& ps = parser_state_t::get();

    if (ps.get_error())
    {
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "SCRIPT ERROR ( see \"/debug/log.txt\" )", 0, 1.0f);
    }

    if (egoboo_config_t::get().hud_displayFramesPerSecond.getValue())
    {
        std::ostringstream os;
        os.setf(std::ios_base::fixed, std::ios_base::floatfield);
        os << std::setw(2) << std::setprecision(2) << engine().getFPS() << " FPS, "
           << std::setw(2) << std::setprecision(2) << engine().getUPS() << " UPS, "
           << engine().getFrameSkip() << " update lag";
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), os.str(), 0.0f, 1.0f);

        if (egoboo_config_t::get().debug_developerMode_enable.getValue())
        {
            /** @todo This should be made available through the GUI. Too much information just to print out things on screen. */
        }
    }

    return y;
}

float draw_help(float y)
{
    if (Ego::Input::InputSystem::get().isKeyDown(SDLK_F1))
    {
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "!!!MOUSE HELP!!!");
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "~~Go to input settings to change");
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "Default settings");
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "~~Left Click to use an item");
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "~~Left and Right Click to grab");
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "~~Middle Click to jump");
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "~~A and S keys do stuff");
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "~~Right Drag to move camera");
    }
    if (Ego::Input::InputSystem::get().isKeyDown(SDLK_F2))
    {
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "!!!JOYSTICK HELP!!!");
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "~~Go to input settings to change.");
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "~~Hit the buttons");
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "~~You'll figure it out");
    }
    if (Ego::Input::InputSystem::get().isKeyDown(SDLK_F3))
    {
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "!!!KEYBOARD HELP!!!");
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "~~Go to input settings to change.");
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "Default settings");
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "~~TGB control left hand");
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "~~YHN control right hand");
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "~~Keypad to move and jump");
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "~~Number keys for stats");
    }

    return y;
}

float draw_debug(float y)
{
    if (!egoboo_config_t::get().debug_developerMode_enable.getValue())
    {
        return y;
    }

    if (Ego::Input::InputSystem::get().isKeyDown(SDLK_F5))
    {
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "!!!DEBUG MODE-5!!!");
        std::ostringstream os;
        os << "~~CAM"
           << " " << CameraSystem::get().getMainCamera()->getPosition()[kX]
           << " " << CameraSystem::get().getMainCamera()->getPosition()[kY]
           << " " << CameraSystem::get().getMainCamera()->getPosition()[kZ];
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), os.str(), 0, 1.0f);
        if (activeModule().getPlayerList().size() > 0)
        {
            std::shared_ptr<Object> pchr = activeModule().getPlayer(0)->getObject();
            os << "~~PLA0DEF"
               << " " << std::setw(4) << std::setprecision(2) << pchr->getRawDamageResistance(DAMAGE_SLASH)
               << " " << std::setw(4) << std::setprecision(2) << pchr->getRawDamageResistance(DAMAGE_CRUSH)
               << " " << std::setw(4) << std::setprecision(2) << pchr->getRawDamageResistance(DAMAGE_POKE)
               << " " << std::setw(4) << std::setprecision(2) << pchr->getRawDamageResistance(DAMAGE_HOLY)
               << " " << std::setw(4) << std::setprecision(2) << pchr->getRawDamageResistance(DAMAGE_EVIL)
               << " " << std::setw(4) << std::setprecision(2) << pchr->getRawDamageResistance(DAMAGE_FIRE)
               << " " << std::setw(4) << std::setprecision(2) << pchr->getRawDamageResistance(DAMAGE_ICE)
               << " " << std::setw(4) << std::setprecision(2) << pchr->getRawDamageResistance(DAMAGE_ZAP);
            y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), os.str(), 0, 1.0f);
            os.str(std::string());
            os << std::setw(5) << std::setprecision(1) << (pchr->getPosX() / Info<float>::Grid::Size())
               << std::setw(5) << std::setprecision(1) << (pchr->getPosY() / Info<float>::Grid::Size());
            y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), os.str(), 0, 1.0f);
        }

        if (activeModule().getPlayerList().size() > 1)
        {
            std::shared_ptr<Object> pchr = activeModule().getPlayer(1)->getObject();
            std::ostringstream os;
            os << "~~PLA1"
               << " " << std::setw(5) << std::setprecision(1) << (pchr->getPosY() / Info<float>::Grid::Size())
               << " " << std::setw(5) << std::setprecision(1) << (pchr->getPosY() / Info<float>::Grid::Size());
            y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), os.str(), 0, 1.0f);
        }
    }

    if (Ego::Input::InputSystem::get().isKeyDown(SDLK_F6))
    {
        std::ostringstream os;
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "!!!DEBUG MODE-6!!!");

        os.str(std::string()); os << "~~FREEPRT: " << EngineContext::get().particleHandler().getFreeCount();
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), os.str(), 0, 1.0f);

        os.str(std::string()); os << "~~FREECHR: " << OBJECTS_MAX - activeModule().getObjectHandler().getObjectCount();
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), os.str(), 0, 1.0f);

        os.str(std::string()); os << "~~EXPORT:  " << (activeModule().isExportValid() ? "TRUE" : "FALSE");
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), os.str(), 0, 1.0f);

        os.str(std::string()); os << "~~PASS:    " << activeModule().getPassageCount();
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), os.str(), 0, 1.0f);
    }

    if (Ego::Input::InputSystem::get().isKeyDown(SDLK_F7))
    {
        std::shared_ptr<Camera> camera = CameraSystem::get().getMainCamera();

        std::ostringstream os;
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "!!!DEBUG MODE-7!!!");

        os.str(std::string()); os << "CAM <"
            << camera->getViewMatrix()(0, 0) << ", "
            << camera->getViewMatrix()(0, 1) << ", "
            << camera->getViewMatrix()(0, 2) << ", "
            << camera->getViewMatrix()(0, 3) << ">";
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), os.str(), 0, 1.0f);

        os.str(std::string()); os << "CAM <"
            << camera->getViewMatrix()(1, 0) << ", "
            << camera->getViewMatrix()(1, 1) << ", "
            << camera->getViewMatrix()(1, 2) << ", "
            << camera->getViewMatrix()(1, 3) << ">";
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), os.str(), 0, 1.0f);

        os.str(std::string()); os << "CAM <"
            << camera->getViewMatrix()(2, 0) << ", "
            << camera->getViewMatrix()(2, 1) << ", "
            << camera->getViewMatrix()(2, 2) << ", "
            << camera->getViewMatrix()(2, 3) << ">";
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), os.str(), 0, 1.0f);

        os.str(std::string()); os << "CAM <"
            << camera->getViewMatrix()(3, 0) << ", "
            << camera->getViewMatrix()(3, 1) << ", "
            << camera->getViewMatrix()(3, 2) << ", "
            << camera->getViewMatrix()(3, 3) << ">";
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), os.str(), 0, 1.0f);

        os.str(std::string()); os << "CAM center <"
            << camera->getCenter()[0] << ", "
            << camera->getCenter()[1] << ", "
            << camera->getCenter()[2] << ">";
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), os.str(), 0, 1.0f);

        os.str(std::string()); os << "CAM turn " << static_cast<int>(camera->getTurnMode()) << " " << camera->getTurnTime();
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), os.str(), 0, 1.0f);
    }

    return y;
}

float draw_timer(float y)
{
    int fifties, seconds, minutes;

    if (timeron)
    {
        fifties = (timervalue % 50) << 1;
        seconds = ((timervalue / 50) % 60);
        minutes = (timervalue / 3000);
        std::ostringstream os;
        os << "=" << minutes << ":" << std::setw(2) << seconds << ":" << std::setw(2) << fifties << "=";
        y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), os.str(), 0, 1.0f);
    }

    return y;
}

float draw_game_status(float y)
{
#if 0
    if ( egonet_getWaitingForClients() )
    {
        y = uiManager().drawBitmapFontString( 0, y, "Waiting for players... " );
    }
    else if (g_serverState.player_count > 0 )
#endif
    {
        if (GameSessionContext::get().allLocalPlayersDead() || activeModule().canRespawnAnyTime())
        {
            if (activeModule().isRespawnValid() && egoboo_config_t::get().game_difficulty.getValue() < Ego::GameDifficulty::Hard)
            {
                y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "PRESS SPACE TO RESPAWN");
            }
            else
            {
                y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "PRESS ESCAPE TO QUIT");
            }
        }
        else if (activeModule().isBeaten())
        {
            y = uiManager().drawBitmapFontString(Ego::Vector2f(0, y), "VICTORY!  PRESS ESCAPE");
        }
    }
#if 0
    else
    {
        y = uiManager().drawBitmapFontString( 0, y, "ERROR: MISSING PLAYERS" );
    }
#endif

    return y;
}
} // namespace

void draw_blip(float sizeFactor, uint8_t color, float x, float y)
{
    ego_frect_t tx_rect;

    float width, height;

    if (x > 0.0f && y > 0.0f)
    {
        std::shared_ptr<const Ego::Texture> ptex = Ego::TextureManager::get().getTexture("mp_data/blip");

#define BLIPSIZE 6
        Ego::Rectangle<int> bliprect[COLOR_MAX];

        for (int cnt = 0; cnt < COLOR_MAX; cnt++)
        {
            bliprect[cnt]._left = cnt * BLIPSIZE;
            bliprect[cnt]._right = cnt * BLIPSIZE + BLIPSIZE;
            bliprect[cnt]._top = 0;
            bliprect[cnt]._bottom = BLIPSIZE;
        }

        tx_rect.xmin = (float)bliprect[color]._left / (float)ptex->getWidth();
        tx_rect.xmax = (float)bliprect[color]._right / (float)ptex->getWidth();
        tx_rect.ymin = (float)bliprect[color]._top / (float)ptex->getHeight();
        tx_rect.ymax = (float)bliprect[color]._bottom / (float)ptex->getHeight();

        width = sizeFactor * (bliprect[color]._right - bliprect[color]._left);
        height = sizeFactor * (bliprect[color]._bottom - bliprect[color]._top);

        auto sc_rect = Ego::Rectangle2f(Ego::Point2f(x - (width / 2), y - (height / 2)), Ego::Point2f(x + (width / 2), y + (height / 2)));
        uiManager().drawQuad2D(sc_rect, tx_rect, std::make_shared<Ego::GUI::Material>(ptex, Ego::Colour4f::white(), true));
    }
}

float draw_icon_texture(const std::shared_ptr<const Ego::Texture>& ptex, float x, float y, uint8_t sparkle_color, uint32_t sparkle_timer, float size, bool useAlpha)
{
    float width, height;
    ego_frect_t tx_rect;

    if (NULL == ptex)
    {
        tx_rect.xmin = 0.0f;
        tx_rect.xmax = 1.0f;
        tx_rect.ymin = 0.0f;
        tx_rect.ymax = 1.0f;
    }
    else
    {
        tx_rect.xmin = 0.0f;
        tx_rect.xmax = (float)ptex->getSourceWidth() / (float)ptex->getWidth();
        tx_rect.ymin = 0.0f;
        tx_rect.ymax = (float)ptex->getSourceWidth() / (float)ptex->getWidth();
    }

    width = ICON_SIZE;
    height = ICON_SIZE;

    if (size >= 0.0f)
    {
        float factor_wid = (float)size / width;
        float factor_hgt = (float)size / height;
        float factor = std::min(factor_wid, factor_hgt);

        width *= factor;
        height *= factor;
    }

    auto sc_rect = Ego::Rectangle2f(Ego::Point2f(x, y), Ego::Point2f(x + width, y + height));
    uiManager().drawQuad2D(sc_rect, tx_rect, std::make_shared<const Ego::GUI::Material>(ptex, Ego::Colour4f::white(), true));

    if (NOSPARKLE != sparkle_color)
    {
        int position;
        float loc_blip_x, loc_blip_y;

        position = sparkle_timer & SPARKLE_AND;

        loc_blip_x = x + position * (width / SPARKLE_SIZE);
        loc_blip_y = y;
        draw_blip(0.5f, sparkle_color, loc_blip_x, loc_blip_y);

        loc_blip_x = x + width;
        loc_blip_y = y + position * (height / SPARKLE_SIZE);
        draw_blip(0.5f, sparkle_color, loc_blip_x, loc_blip_y);

        loc_blip_x = loc_blip_x - position * (width / SPARKLE_SIZE);
        loc_blip_y = y + height;
        draw_blip(0.5f, sparkle_color, loc_blip_x, loc_blip_y);

        loc_blip_x = x;
        loc_blip_y = loc_blip_y - position * (height / SPARKLE_SIZE);
        draw_blip(0.5f, sparkle_color, loc_blip_x, loc_blip_y);
    }

    return y + height;
}

float draw_game_icon(const std::shared_ptr<const Ego::Texture>& icontype, float x, float y, uint8_t sparkle_color, uint32_t sparkle_timer, float size)
{
    return draw_icon_texture(icontype, x, y, sparkle_color, sparkle_timer, size);
}

void draw_hud()
{
    uiManager().beginRenderUI();
    {
        int y = draw_fps(0);
        y = draw_help(y);
        y = draw_debug(y);
        y = draw_timer(y);
        y = draw_game_status(y);
    }
    uiManager().endRenderUI();
}

void draw_mouse_cursor()
{
    const std::shared_ptr<Ego::Texture>& pcursor = Ego::TextureManager::get().getTexture("mp_data/cursor");

    if (nullptr == pcursor)
    {
        Ego::GraphicsSystemNew::get().setCursorVisibility(true);
    }
    else
    {
        Ego::GraphicsSystemNew::get().setCursorVisibility(false);

        int x, y;
        SDL_GetMouseState(&x, &y);

        uiManager().beginRenderUI();
        uiManager().drawImage(Ego::Point2f(x, y), Ego::Vector2f(pcursor->getWidth(), pcursor->getHeight()), std::make_shared<Ego::GUI::Material>(pcursor, Ego::Colour4f::white(), true));
        uiManager().endRenderUI();
    }
}
