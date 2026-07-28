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

/// @file egolib/game/GUI/IUIManager.hpp
/// @details Interface abstracting the consumed surface of Ego::GUI::UIManager, letting GUI
///          widgets and gamestates that only draw through the UI manager depend on a seam
///          instead of the concrete, GL-bound UIManager. Ego::GUI::UIManager is the only
///          production implementation; a headless test stub is the other. This is a
///          namespace-scope header (rather than being nested inside UIManager.hpp) so it can
///          be included on its own without dragging in the concrete UIManager.

#pragma once

#include "egolib/integrations/color.hpp"  // Colour4f
#include "egolib/integrations/video.hpp"  // idlib::vertex_descriptor, vertex_buffer
#include "egolib/integrations/math.hpp"   // Point2f, Rectangle2f, Vector2f
#include "egolib/Math/Standard.hpp"        // ego_frect_t
#include "egolib/Graphics/IFont.hpp"       // Ego::IFont

#include <cstdint>
#include <memory>
#include <string>

// Forward declarations.
namespace Ego {
namespace GUI {
class Material;
}
}

namespace Ego {
namespace GUI {

/// @brief Interface for the GUI system's utilities and shared resources, exposing only the
///        surface consumed outside of the UIManager implementation itself.
class IUIManager {
public:
    virtual ~IUIManager() = default;

    enum UIFontType : uint8_t {
        FONT_DEFAULT,
        FONT_FLOATING_TEXT,
        FONT_DEBUG,
        FONT_GAME,
        NR_OF_UI_FONTS
    };

    /**
    * @return
    *   The Font loaded and cached by the UIManager
    **/
    virtual std::shared_ptr<IFont> getFont(const UIFontType type) const = 0;

    /**
    * @todo: REMOVE these functions
    **/
    std::shared_ptr<IFont> getDefaultFont() const { return getFont(FONT_DEFAULT); }
    std::shared_ptr<IFont> getFloatingTextFont() const { return getFont(FONT_FLOATING_TEXT); }
    std::shared_ptr<IFont> getDebugFont() const { return getFont(FONT_DEBUG); }
    std::shared_ptr<IFont> getGameFont() const { return getFont(FONT_GAME); }

    /**
     * @return
     *   Current screen resolution width
     */
    virtual int getScreenWidth() const = 0;

    /**
     * @return
     *   Current screen resolution height
     */
    virtual int getScreenHeight() const = 0;

    /**
     * @brief
     *  Used by the ComponentContainer before rendering GUI components
     */
    virtual void beginRenderUI() = 0;

    /**
     * @brief
     *   Tell the rendering system we are finished drawing GUI components
     */
    virtual void endRenderUI() = 0;

    /**
     * @brief
     *  Convinience function to draw a 2D image
     */
    virtual void drawImage(const Point2f& position, const Vector2f& size, const std::shared_ptr<const Material>& material) = 0;

    /**
    * @brief
    *   dumps the current screen (GL context) to a new bitmap file
    *   right now it dumps it to whatever the current directory is
    * @return
    *   true if successful, false otherwise
    **/
    virtual bool dumpScreenshot() = 0;

    /**
    * @brief
    *   Renders a text string using bitmap font
    * @param start
    *   the screen position at which to render the string
    * @param text
    *   the text string to render
    * @param maxWidth
    *   Maximum x width of the string, if it is bigger the function
    *   will wrap to the next line
    * @param alpha
    *   Value between 1.0f (opaque) to 0.0f (transparent)
    * @return
    *   Y screen coordinate of the line below where the text was rendered
    **/
    virtual float drawBitmapFontString(const Vector2f& start, const std::string &text, const uint32_t maxWidth = 0, const float alpha = 1.0f) = 0;

    /**
    * @brief
    *   Fill a solid coloured rectangle
    * @param rectangle
    *   the rectangle
    * @param useAlpha
    *   enable or disable alpha channel
    * @param tint
    *   colour of the rectangle (including alpha channel)
    **/
    virtual void fillRectangle(const Rectangle2f& rectangle, const bool useAlpha, const Colour4f& tint = Colour4f::white()) = 0;

    /**
     * @brief Render a 2D quad.
     * @param source the source rectangle (texture coordinates)
     * @param target the target rectangle (screen coordinates)
     * @param material the material
     */
    virtual void drawQuad2D(const Rectangle2f& scr_rect, const Rectangle2f& tx_rect, const std::shared_ptr<const Material>& material) = 0;
    virtual void drawQuad2D(const Rectangle2f& scr_rect, const ego_frect_t& tx_rect, const std::shared_ptr<const Material>& material) = 0;

    /// Draw a 2D quad.
    /// @param target the target rectangle in screen coordinates
    /// @param source the source rectangle in texture coordinates
    virtual void drawQuad2d(const Rectangle2f& target, const Rectangle2f& source) = 0;
    /// Draw a 2D quadriliteral.
    /// @param target the target rectangle in screen coordinates
    /// @remark The texture coordinate rectangle is ((0,0),(1,1)) if the material is textured.
    virtual void drawQuad2d(const Rectangle2f& target) = 0;

    /**
     * @brief
     *  The vertex descriptor used to render generic GUI components (e.g. ProgressBar).
     */
    virtual const idlib::vertex_descriptor& componentVertexDescriptor() const = 0;

    /**
     * @brief
     *  The vertex buffer used to render generic GUI components (e.g. ProgressBar).
     * @remark May be @a nullptr in a headless implementation.
     */
    virtual const std::shared_ptr<idlib::vertex_buffer>& componentVertexBuffer() const = 0;
};

/// @brief Install the active UI manager (the engine's UIManager).
/// @remark GUI-layer seam letting lower-layer GUI widgets reach the active UIManager (for drawing)
///         without an upward dependency on the app-layer EngineContext. GameEngine installs its
///         _uiManager here at creation and clears it at teardown. EngineContext::uiManager() keeps
///         its own GameEngine-derived path (the script-function test fixtures manipulate the engine's
///         _uiManager directly), so both resolve the same instance in the running engine.
void installActiveUIManager(IUIManager& uiManager);

/// @brief Clear the installed active UI manager.
void clearActiveUIManager();

/// @brief The installed active UI manager, or @a nullptr if none is installed.
IUIManager* tryActiveUIManager();

/// @brief The active UI manager.
/// @throw std::logic_error if none is installed
IUIManager& activeUIManager();

} // namespace GUI
} // namespace Ego
