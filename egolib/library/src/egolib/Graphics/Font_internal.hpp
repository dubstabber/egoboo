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

/// @file egolib/Graphics/Font_internal.hpp
/// @brief Private nested-type definitions for Font (RenderedTextCache, SizedTextCache,
///        FontAtlas, LaidOutText, LayoutOptions), shared across the Font*.cpp TUs.

#pragma once

#include "egolib/Graphics/Font.hpp"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace Ego {

struct Font::RenderedTextCache : private idlib::non_copyable {
    std::shared_ptr<ILaidTextRenderer> cache;
    uint32_t lastUseInTicks;
    std::string text;
    int width;
    int height;
    int spacing;

    bool isEquivalent(const std::string &t, int w, int h, int s) {
        return w == width && h == height && s == spacing && t == text;
    }

    static bool isLessThan(const std::shared_ptr<RenderedTextCache> &a, const std::shared_ptr<RenderedTextCache> &b) {
        return a->lastUseInTicks < b->lastUseInTicks;
    }
};

struct Font::SizedTextCache : private idlib::non_copyable {
    uint32_t lastUseInTicks;
    std::string text;
    int width;
    int height;
    int spacing;

    bool isEquivalent(const std::string &t, int s) {
        return s == spacing && t == text;
    }

    static bool isLessThan(const std::shared_ptr<SizedTextCache> &a, const std::shared_ptr<SizedTextCache> &b) {
        return a->lastUseInTicks < b->lastUseInTicks;
    }
};

struct Font::FontAtlas {
    std::shared_ptr<Texture> texture;
    std::unordered_map<uint16_t, Rectangle2f> glyphs;
};

struct Font::LaidOutText {
    std::vector<uint16_t> codepoints;
    std::vector<Rectangle2f> positions;
    const FontAtlas &atlas;

    LaidOutText(const FontAtlas &a) :
        atlas(a) {}
};

struct Font::LayoutOptions {
    int maxWidth = 0;  ///< The maximum width in pixels of the constraining box; set to 0 for no constraint.
    int maxHeight = 0; ///< The maximum height in pixels of the constraining box; set to 0 for no constraint.
    int spacing = 0; ///< The space in pixels added between lines; if 0, it's set to @c Font::getFontSpacing()
    bool interpretNewlines = true; ///< If @c false, newline characters do not create a new line.
    int *textWidth = nullptr; ///< Set to the laid text's width
    int *textHeight = nullptr; ///< Set to the laid text's height
};

} // namespace Ego
