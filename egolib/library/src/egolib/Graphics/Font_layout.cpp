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

/// @file egolib/Graphics/Font_layout.cpp
/// @brief Text layout and font-atlas geometry engine, split out of Font.cpp.

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

void Font::layoutLine(const std::vector<uint16_t> &codepoints, size_t pos, int maxWidth, bool useNewlines,
                      const FontAtlas &atlas, size_t *endPos, std::vector<uint16_t> &usedChars,
                      std::vector<SDL_Rect> &positions, int *lineWidth, int *lineHeight) {
    bool useWidth = maxWidth > 0;
    uint16_t lastCodepoint = 0;
    size_t lastWordStartPosInChars = pos;
    size_t lastWordStartPosInUsedChars = 0;
    int x = 0;
    size_t currentPos;

    int maxLineWidth = 0;
    int maxLineHeight = 0;
    for (currentPos = pos; currentPos < codepoints.size(); currentPos++) {
        uint16_t codepoint = codepoints[currentPos];
        if (codepoint != '\n' && !TTF_GlyphIsProvided(_ttfFont, codepoint))
            continue;
        Rectangle2f rect;
        if (codepoint != '\n') {
            auto glyph = atlas.glyphs.find(codepoint);
            SDL_assert(glyph != atlas.glyphs.end());
            rect = glyph->second;
        }
        if (codepoint == ' ') {
            lastWordStartPosInChars = currentPos + 1;
            lastWordStartPosInUsedChars = usedChars.size() + 1;
        }

        if ((useNewlines && codepoint == '\n') || (useWidth && maxWidth - x < rect.get_size().x())) {
            if (codepoint != '\n' && codepoint != ' ' && lastWordStartPosInUsedChars > 0) {
                currentPos = lastWordStartPosInChars;
                auto wordStart = usedChars.begin();
                std::advance(wordStart, lastWordStartPosInUsedChars);
                usedChars.erase(wordStart, usedChars.end());
                auto wordPosStart = positions.begin();
                std::advance(wordPosStart, lastWordStartPosInUsedChars);
                positions.erase(wordPosStart, positions.end());

                maxLineWidth = 0;
                maxLineHeight = 0;
                for (size_t i = 0; i < usedChars.size(); i++) {
                    uint16_t ch = usedChars[i];
                    auto glyph = atlas.glyphs.find(ch);
                    SDL_Rect pos = positions[i];
                    auto r = glyph->second;
                    if (maxLineHeight < r.get_size().y())
                        maxLineHeight = r.get_size().y();
                    if (maxLineWidth < pos.x + r.get_size().x())
                        maxLineWidth = pos.x + r.get_size().x();
                }
            } else if (useNewlines && codepoint == '\n') {
                currentPos++;
            }
            break;
        }
        if (codepoint == '\n')
            continue;

        if (maxLineWidth < x + rect.get_size().x())
            maxLineWidth = x + rect.get_size().x();
        if (maxLineHeight < rect.get_size().y())
            maxLineHeight = rect.get_size().y();

        int minx, advance;
        TTF_GlyphMetrics(_ttfFont, codepoint, &minx, nullptr, nullptr, nullptr, &advance);
        x += getFontKerning(lastCodepoint, codepoint);
        SDL_assert(x >= 0);
        SDL_Rect dst = {x, 0, (int)rect.get_size().x(), (int)rect.get_size().y()};
        if (minx < 0)
            dst.x += minx;
        positions.push_back(dst);
        usedChars.push_back(codepoint);
        x += advance;
        lastCodepoint = codepoint;
    }
    *endPos = currentPos;
    *lineWidth = maxLineWidth;
    *lineHeight = maxLineHeight;
}

Font::LaidOutText Font::layout(const std::string &text, const LayoutOptions &options) {
    bool useHeight = options.maxHeight > 0;
    int spacing = options.spacing;
    if (spacing <= 0)
        spacing = getLineSpacing();

    std::vector<uint16_t> codepoints = splitUTF8StringToCodepoints(text);

    const FontAtlas *currentAtlas = nullptr;
    for (const FontAtlas &atlas : _atlases) {
        bool containsAllChars = std::all_of(codepoints.begin(), codepoints.end(), [&atlas](uint16_t pos) {
            return pos == ' ' || pos == '\n' || atlas.glyphs.find(pos) != atlas.glyphs.end();
        });

        if (containsAllChars) {
            currentAtlas = &atlas;
            break;
        }
    }

    if (currentAtlas == nullptr) {
        _atlases.push_back(createFontAtlas(codepoints));
        currentAtlas = &(_atlases.at(_atlases.size() - 1));
    }

    LaidOutText laidText(*currentAtlas);

    int maxLineWidth = 0;
    int y = 0;
    int maxHeight = 0;

    for (size_t pos = 0; pos < codepoints.size(); ) {
        std::vector<SDL_Rect> linePos;
        std::vector<uint16_t> lineUsedChars;
        int lineWidth;
        int lineHeight;
        size_t newPos = pos;

        layoutLine(codepoints, pos, options.maxWidth, options.interpretNewlines, laidText.atlas, &newPos,
                   lineUsedChars, linePos, &lineWidth, &lineHeight);

        if (newPos == pos)
            break;

        pos = newPos;

        if (useHeight && options.maxHeight - y < lineHeight)
            break;

        if (maxLineWidth < lineWidth)
            maxLineWidth = lineWidth;

        laidText.codepoints.insert(laidText.codepoints.end(), lineUsedChars.begin(), lineUsedChars.end());

        for (const SDL_Rect &rect : linePos) {
            SDL_Rect newRect = rect;
            newRect.y += y;
            laidText.positions.push_back(Rectangle2f(Point2f(newRect.x, newRect.y),
                                                     Point2f(newRect.x + newRect.w,
                                                             newRect.y + newRect.h)));
        }

        int ourHeight = y + lineHeight;
        y += spacing;
        if (maxHeight < ourHeight)
            maxHeight = ourHeight;
    }

    if (options.textWidth) *(options.textWidth) = maxLineWidth;
    if (options.textHeight) *(options.textHeight) = maxHeight;

    return laidText;
}

std::shared_ptr<Font::LaidTextRenderer> Font::layoutToBuffer(const std::string &text, const LayoutOptions &options) {
    struct TextVertex {
        float x;
        float y;
        float z;
        float u;
        float v;
    };

    LaidOutText laidText = layout(text, options);

    const auto &vertexDesc = VertexFormatFactory::get(idlib::vertex_format::P3FT2F);
    auto buffer = Ego::activeVideoBufferManager().create_vertex_buffer(4 * laidText.codepoints.size(), vertexDesc.get_size());

    TextVertex *vertices = reinterpret_cast<TextVertex *>(buffer->lock());
    float texWidth = laidText.atlas.texture->getWidth();
    float texHeight = laidText.atlas.texture->getHeight();

    for (size_t i = 0; i < laidText.codepoints.size(); i++) {
        uint16_t chr = laidText.codepoints[i];
        auto charPos = laidText.positions[i];
        auto glyphPos = laidText.atlas.glyphs.at(chr);

        float xMin = charPos.get_min().x();
        float xMax = charPos.get_max().x();
        float yMin = charPos.get_min().y();
        float yMax = charPos.get_max().y();

        float uMin = (glyphPos.get_min().x()) / texWidth;
        float uMax = (glyphPos.get_max().x()) / texWidth;
        float vMin = (glyphPos.get_min().y()) / texHeight;
        float vMax = (glyphPos.get_max().y()) / texHeight;

        vertices[i * 4 + 0].x = xMin;
        vertices[i * 4 + 0].y = yMin;
        vertices[i * 4 + 0].z = 0;
        vertices[i * 4 + 0].u = uMin;
        vertices[i * 4 + 0].v = vMin;

        vertices[i * 4 + 1].x = xMax;
        vertices[i * 4 + 1].y = yMin;
        vertices[i * 4 + 1].z = 0;
        vertices[i * 4 + 1].u = uMax;
        vertices[i * 4 + 1].v = vMin;

        vertices[i * 4 + 2].x = xMax;
        vertices[i * 4 + 2].y = yMax;
        vertices[i * 4 + 2].z = 0;
        vertices[i * 4 + 2].u = uMax;
        vertices[i * 4 + 2].v = vMax;

        vertices[i * 4 + 3].x = xMin;
        vertices[i * 4 + 3].y = yMax;
        vertices[i * 4 + 3].z = 0;
        vertices[i * 4 + 3].u = uMin;
        vertices[i * 4 + 3].v = vMax;
    }
    buffer->unlock();
    return std::shared_ptr<LaidTextRenderer>(new LaidTextRenderer(laidText.atlas.texture, vertexDesc, buffer));
}

std::shared_ptr<SDL_Surface> Font::layoutToTexture(const std::string &text, const LayoutOptions &options,
                                                   const Colour3f &colour)
{
    LayoutOptions ourOptions = options;
    int surfWidth, surfHeight;
    ourOptions.textWidth = &surfWidth;
    ourOptions.textHeight = &surfHeight;

    LaidOutText laidText = layout(text, ourOptions);

    if (options.textWidth) *(options.textWidth) = surfWidth;
    if (options.textHeight) *(options.textHeight) = surfHeight;

    auto pfd = pixel_descriptor::get<idlib::pixel_format::R8G8B8A8>();

    auto colorByte = Colour3b(colour);

    auto surf = Ego::activeImageManager().createImage(surfWidth, surfHeight, pfd);
    idlib::fill(surf.get(), Colour4b(colorByte, 0));
    SDL::setBlendMode(surf.get(), SDL::BlendMode::NoBlending);

    auto atlasSurf = laidText.atlas.texture->m_source;
    auto oldMod = SDL::getColourMod(atlasSurf.get());
    SDL::setColourMod(atlasSurf.get(), colorByte);

    for (size_t i = 0; i < laidText.codepoints.size(); i++) {
        uint16_t chr = laidText.codepoints[i];
        auto charPos = laidText.positions[i];
        auto glyphPos = laidText.atlas.glyphs.at(chr);
        idlib::blit(atlasSurf.get(),
                    Rectangle2f(glyphPos.get_min(),
                                glyphPos.get_max()),
                    surf.get(),
                    charPos.get_min());
    }

    SDL::setColourMod(atlasSurf.get(), oldMod);

    return surf;
}

Font::FontAtlas Font::createFontAtlas(const std::vector<std::string> &chars) const {
    std::vector<uint16_t> codepoints;
    for (const std::string & chr : chars)
        codepoints.push_back(convertUTF8ToCodepoint(chr));
    return createFontAtlas(codepoints);
}

Font::FontAtlas Font::createFontAtlas(const std::vector<uint16_t> &codepoints) const
{
    static const auto WHITE = Colour4b::white();
    std::vector<std::shared_ptr<SDL_Surface>> images;
    std::vector<Rectangle2f> pos;

    for (uint16_t cp : codepoints)
    {
        auto surf = SDL::render_glyph(_ttfFont, cp, WHITE);
        images.push_back(surf);
    }

    int currentMaxSize = 128;
    int maxTextureSize = Ego::activeRenderer().getInfo()->getMaximumTextureSize();
    std::shared_ptr<SDL_Surface> atlas = nullptr;

    auto pfd = pixel_descriptor::get<idlib::pixel_format::R8G8B8A8>();

    while (currentMaxSize <= maxTextureSize) {
        atlas = Ego::activeImageManager().createImage(currentMaxSize, currentMaxSize, pfd);
        idlib::fill(atlas.get(), Colour4b(Colour3b::white(), 0));
        SDL::setBlendMode(atlas.get(), SDL::BlendMode::NoBlending);

        int x = 0, y = 0;
        int currentHeight = 0;
        bool fits = true;

        for (const auto& surf : images) {
            if (surf == nullptr) {
                pos.push_back(Rectangle2f());
                continue;
            }

            if (currentMaxSize < surf->w || currentMaxSize < surf->h)
            {
                fits = false;
                break;
            }

            if (currentMaxSize - x < surf->w)
            {
                y += currentHeight + 1;
                x = 0;
                currentHeight = 0;
            }

            if (currentHeight < surf->h)
                currentHeight = surf->h;

            if (currentMaxSize - y < currentHeight) {
                fits = false;
                break;
            }

            auto dst = Rectangle2f(Point2f(x, y), Point2f(x + surf->w, y + surf->h));
            idlib::blit(surf.get(), atlas.get(), Point2f(x, y));
            pos.push_back(dst);
            x += surf->w + 1;
        }

        if (fits)
            break;

        Log::activeTarget() << Log::Entry::create(Log::Level::Debug, __FILE__, __LINE__, "unable to fit atlas into a texture of size ", currentMaxSize, ", trying texture of size ", currentMaxSize * 2, " instead", Log::EndOfEntry);
        currentMaxSize <<= 1;
        pos.clear();
        atlas = nullptr;
    }

    if (!atlas) {
        auto e = Log::Entry::create(Log::Level::Error, __FILE__, __LINE__,
                                    "unable to fit a atlas into a texture of size ", maxTextureSize,
                                    Log::EndOfEntry);
        Log::activeTarget() << e;
        throw idlib::environment_error(__FILE__, __LINE__, "font atlas", e.getText());
    }

    FontAtlas retval;
    for (int i = 0; i < images.size(); i++) {
        if (images[i] == nullptr)
            continue;
        retval.glyphs.insert(std::make_pair(codepoints[i], pos[i]));
    }

    retval.texture = Ego::activeRenderer().createTexture();
    retval.texture->load("font atlas", atlas);
    retval.texture->setAddressModeS(idlib::texture_address_mode::clamp);
    retval.texture->setAddressModeT(idlib::texture_address_mode::clamp);
    return retval;
}

} // namespace Ego
