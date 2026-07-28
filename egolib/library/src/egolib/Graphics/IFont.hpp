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

/// @file egolib/Graphics/IFont.hpp
/// @details Interfaces abstracting the consumed surface of Ego::Font and
///          Ego::Font::LaidTextRenderer, letting GUI/gamestate code that only draws or lays
///          out text depend on a seam instead of the concrete, GL/SDL_ttf-bound Font. This is
///          a namespace-scope interface (rather than nested inside IFont) so it is
///          forward-declarable on its own, unlike the nested Font::LaidTextRenderer it mirrors.

#pragma once

#include <memory>
#include <string>
#include "egolib/integrations/color.hpp"

namespace Ego { class Texture; }

namespace Ego {

/// @brief Interface for a container and renderer of laid out text.
/// @remark Mirrors the consumed surface of Font::LaidTextRenderer.
class ILaidTextRenderer {
public:
    virtual ~ILaidTextRenderer() = default;

    /**
     * @brief
     *  Renders the laid out text.
     * @param x,y
     *  The position on screen to render the text at
     * @param colour
     *  The colour of the rendered text; default is white
     */
    virtual void render(int x, int y, const Colour4f &colour = Colour4f::white()) = 0;
};

/// @brief Interface for a TrueType font, exposing only the surface consumed outside of the
///        Font implementation itself.
class IFont {
public:
    virtual ~IFont() = default;

    /**
     * @brief
     *  Get the size of the given text that only has one line.
     * @param text
     *  the text to draw
     * @param[out] width
     *  the width of the text box (may be nullptr)
     * @param[out] height
     *  the height of the text box (may be nullptr)
     */
    virtual void getTextSize(const std::string &text, int *width, int *height) = 0;

    /**
     * @brief
     *  Draw text that only has one line to a texture.
     * @param tex
     *  the texture object to draw to
     * @param text
     *  the text to draw
     * @param colour
     *  the colour of the text (default white)
     */
    virtual void drawTextToTexture(Texture *tex, const std::string &text,
                                   const Colour3f &color = Colour3f::white()) = 0;

    /**
     * @brief
     *  Draw text that only has one line to the screen.
     * @param text
     *  the text to draw
     * @param x
     *  the x position on screen
     * @param y
     *  the y position on screen
     * @param colour
     *  the colour of the text (default white)
     */
    virtual void drawText(const std::string &text, int x, int y,
                          const Colour4f &colour = Colour4f::white()) = 0;

    /**
     * @brief
     *  Draw text that potentially has multiple lines to the screen.
     * @param text
     *  the text to draw
     * @param x
     *  the x position on screen
     * @param y
     *  the y position on screen
     * @param width
     *  the maximum width before text wrapping (0 is no limit)
     * @param height
     *  the maximum height before text truncating (0 is no limit)
     * @param spacing
     *  the spacing (in pixels) between each line
     * @param colour
     *  the colour of the text (default white)
     */
    virtual void drawTextBox(const std::string &text, int x, int y, int width, int height, int spacing,
                             const Colour4f &colour = Colour4f::white()) = 0;

    /**
     * @brief
     *  Create a render cache to render text that only has one line.
     * @param text
     *  The text to cache
     * @param[out] textWidth,textHeight
     *  These are set to the size of the laid text.
     * @sa
     *  drawText
     */
    virtual std::shared_ptr<ILaidTextRenderer> layoutText(const std::string &text, int *textWidth, int *textHeight) = 0;

    /**
     * @brief
     *  Create a render cache to render text that may have multiple lines.
     * @param text
     *  The text to cache
     * @param width, height
     *  Constraints on the laid text; use 0 for no constraint
     * @param spacing
     *  the spacing (in pixels) between each line
     * @param[out] textWidth,textHeight
     *  These are set to the size of the laid text.
     * @sa
     *  drawTextBox
     */
    virtual std::shared_ptr<ILaidTextRenderer> layoutTextBox(const std::string &text, int width, int height, int spacing,
                                                             int *textWidth, int *textHeight) = 0;

    /**
     * @brief
     *  Get the suggested line spacing for this font.
     * @return
     *  number of pixels suggested for line spacing
     */
    virtual int getLineSpacing() const = 0;
};

} // namespace Ego
