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

/// @file egolib/Graphics/Font_utf8.cpp
/// @brief UTF-8 codepoint decoding helpers, split out of Font.cpp.

#include "egolib/Graphics/Font.hpp"
#include "egolib/Graphics/VideoBufferManagerSeam.hpp"
#include "egolib/Time/Time.hpp"

#include "egolib/Core/StringUtilities.hpp"
#include "egolib/Graphics/FontManager.hpp"
#include "egolib/Graphics/VertexFormat.hpp"
#include "egolib/Core/System.hpp"
#include "egolib/Image/ImageManager.hpp"
#include "egolib/Renderer/Renderer.hpp"
#include "egolib/Image/SDL_Image_Extensions.h"
#include "egolib/Log/_Include.hpp"
#include "egolib/vfs.h"
#include "idlib/exception.hpp"
#include "egolib/Graphics/Font_internal.hpp"

namespace Ego {

uint16_t Font::convertUTF8ToCodepoint(const std::string &string, size_t *pos) {
    size_t tmpPos = 0;
    if (pos == nullptr) pos = &tmpPos;

    uint16_t retval = 0;

    unsigned char tmp = string.at(*pos);
    *pos += 1;

    if (tmp < 0x80) {
        retval |= tmp;
    } else if (tmp < 0xC2) {
        throw std::invalid_argument("UTF-8 character invalid");
    } else if (tmp < 0xE0) {
        retval |= (tmp & 0x1F) << 6;

        tmp = string.at(*pos);
        *pos += 1;
        if ((tmp & 0xC0) != 0x80)
            throw std::invalid_argument("UTF-8 character invalid");
        retval |= (tmp & 0x3F);
    } else if (tmp < 0xF0) {
        retval |= (tmp & 0xF) << 12;

        tmp = string.at(*pos);
        *pos += 1;
        if ((tmp & 0xC0) != 0x80 || (retval == 0 && tmp < 0xA0))
            throw std::invalid_argument("UTF-8 character invalid");
        retval |= (tmp & 0x3F) << 6;

        tmp = string.at(*pos);
        *pos += 1;
        if ((tmp & 0xC0) != 0x80)
            throw std::invalid_argument("UTF-8 character invalid");
        retval |= (tmp & 0x3F);
    } else {
        throw std::out_of_range("UTF-8 character does not fit in a 16-bit codepoint");
    }

    return retval;
}

std::vector<uint16_t> Font::splitUTF8StringToCodepoints(const std::string &text) {
    std::vector<uint16_t> retval;
    size_t pos = 0;
    while (pos < text.size())
        retval.push_back(convertUTF8ToCodepoint(text, &pos));
    return retval;
}

} // namespace Ego
