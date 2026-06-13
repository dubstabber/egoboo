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

/// @file egolib/Graphics/Font.cpp
/// @brief TTF management
/// @details TrueType font drawing functionality.  Uses the SDL_ttf module
///          to do its business. This depends on SDL_ttf and OpenGL.

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

Font::LaidTextRenderer::LaidTextRenderer(const std::shared_ptr<Texture> &atlas,
                                         const idlib::vertex_descriptor& vertexDescriptor,
                                         const std::shared_ptr<idlib::vertex_buffer> &vertexBuffer) :
    _atlas(atlas),
    _vertexDescriptor(vertexDescriptor),
    _vertexBuffer(vertexBuffer) {}

void Font::LaidTextRenderer::render(int x, int y, const Colour4f &colour) {
    struct MatrixStack {
        MatrixStack() :matrix(Ego::activeRenderer().getProjectionMatrix()) {}
        ~MatrixStack() { Ego::activeRenderer().setProjectionMatrix(matrix); }
        const Matrix4f4f matrix;
    };

    auto &renderer = Ego::activeRenderer();
    MatrixStack stack;

    Vector3f pos(static_cast<float>(x), static_cast<float>(y), 0.0f);
    Matrix4f4f transMat = idlib::translation_matrix(pos);
    Matrix4f4f projMatrix = stack.matrix * transMat;

    renderer.setProjectionMatrix(projMatrix);
    renderer.setBlendingEnabled(true);
    renderer.setColour(colour);
    renderer.getTextureUnit().setActivated(_atlas.get());
    renderer.render(*(_vertexBuffer.get()), _vertexDescriptor, idlib::primitive_type::quadriliterals, 0, _vertexBuffer->number_of_vertices());
}

Font::Font(const std::string &fileName, int pointSize) :
    _ttfFont(),
    _renderedCache(),
    _sizedCache() {
    _ttfFont = TTF_OpenFontRW(vfs_openRWopsRead(fileName), 1, pointSize);

    if (_ttfFont == nullptr) {
        throw idlib::environment_error(__FILE__, __LINE__, "SDL_ttf", TTF_GetError());
    }

    // Create an texture atlas for ASCII characters
    std::vector<uint16_t> ascii;
    for (int i = 0x20; i < 0x7F; i++)
        ascii.push_back(i);
    _atlases.push_back(createFontAtlas(ascii));
}

Font::~Font() {
    TTF_CloseFont(_ttfFont);
}

void Font::getTextSize(const std::string &text, int *width, int *height) {
    bool updateCache = true;
    std::shared_ptr<SizedTextCache> cache = findInSizedCache(text, 0, &updateCache);

    if (updateCache) {
        int ourWidth = 0;
        int ourHeight = 0;

        LayoutOptions options;
        options.textWidth = &ourWidth;
        options.textHeight = &ourHeight;
        options.interpretNewlines = false;

        layout(text, options);

        cache->text = text;
        cache->width = ourWidth;
        cache->height = ourHeight;
        cache->spacing = 0;
    }

    if (width) *width = cache->width;
    if (height) *height = cache->height;

    cache->lastUseInTicks = ::Time::now<::Time::Unit::Ticks>();
}

void Font::getTextBoxSize(const std::string &text, int spacing, int *width, int *height) {
    bool updateCache = true;
    auto cache = findInSizedCache(text, spacing, &updateCache);

    if (updateCache) {
        int ourWidth = 0;
        int ourHeight = 0;

        LayoutOptions options;
        options.textWidth = &ourWidth;
        options.textHeight = &ourHeight;

        layout(text, options);

        cache->text = text;
        cache->width = ourWidth;
        cache->height = ourHeight;
        cache->spacing = spacing;
    }

    if (width) *width = cache->width;
    if (height) *height = cache->height;

    cache->lastUseInTicks = ::Time::now<::Time::Unit::Ticks>();
}

void Font::drawTextToTexture(Texture *tex, const std::string &text, const Colour3f &colour) {
    LayoutOptions options;

    auto surface = layoutToTexture(text, options, colour);

    std::string name = "Font text '" + text + "'";
    tex->load(name, surface);
    tex->setAddressModeS(idlib::texture_address_mode::clamp);
    tex->setAddressModeT(idlib::texture_address_mode::clamp);
}

void Font::drawTextBoxToTexture(Texture *tex, const std::string &text, int width, int height, int spacing,
                                const Colour3f &colour) {
    LayoutOptions options;
    options.maxWidth = width;
    options.maxHeight = height;
    options.spacing = spacing;

    auto surface = layoutToTexture(text, options, colour);

    std::string name = "Font textbox '" + text + "'";
    tex->load(name, surface);
    tex->setAddressModeS(idlib::texture_address_mode::clamp);
    tex->setAddressModeT(idlib::texture_address_mode::clamp);
}

void Font::drawText(const std::string &text, int x, int y, const Colour4f &colour) {
    if (text.empty()) return;

    bool updateCache = true;
    auto cache = findInRenderedCache(text, 0, 0, 0, &updateCache);

    if (updateCache) {
        cache->cache = layoutText(text, nullptr, nullptr);
        cache->text = text;
        cache->width = 0;
        cache->height = 0;
        cache->spacing = 0;
    }

    cache->cache->render(x, y, colour);

    cache->lastUseInTicks = ::Time::now<::Time::Unit::Ticks>();
}

void Font::drawTextBox(const std::string &text, int x, int y, int width, int height, int spacing, const Colour4f &colour) {
    if (text.empty()) return;

    bool updateCache = true;
    auto cache = findInRenderedCache(text, width, height, spacing, &updateCache);

    if (updateCache) {
        cache->cache = layoutTextBox(text, width, height, spacing, nullptr, nullptr);
        cache->text = text;
        cache->width = width;
        cache->height = height;
        cache->spacing = spacing;
    }

    cache->cache->render(x, y, colour);

    cache->lastUseInTicks = ::Time::now<::Time::Unit::Ticks>();
}

std::shared_ptr<Font::LaidTextRenderer> Font::layoutText(const std::string &text, int *textWidth, int *textHeight) {
    LayoutOptions options;
    options.textWidth = textWidth;
    options.textHeight = textHeight;
    options.interpretNewlines = false;

    return layoutToBuffer(text, options);
}

std::shared_ptr<Font::LaidTextRenderer> Font::layoutTextBox(const std::string &text, int width, int height, int spacing,
                                                            int *textWidth, int *textHeight) {
    LayoutOptions options;
    options.textWidth = textWidth;
    options.textHeight = textHeight;
    options.maxWidth = width;
    options.maxHeight = height;
    options.spacing = spacing;

    return layoutToBuffer(text, options);
}

int Font::getLineSpacing() const {
    return TTF_FontLineSkip(_ttfFont);
}

int Font::getFontHeight() const {
    return TTF_FontHeight(_ttfFont);
}

std::shared_ptr<Font::RenderedTextCache> Font::findInRenderedCache(const std::string &text, int width, int height,
                                                                   int spacing, bool *update) {
    if (MAX_CACHE_SIZE == 0) {
        *update = true;
        return std::make_shared<RenderedTextCache>();
    }

    if (MAX_CACHE_SIZE == 1) {
        *update = true;
        if (_renderedCache.empty())
            _renderedCache.emplace_back(std::make_shared<RenderedTextCache>());
        return _renderedCache[0];
    }

    std::shared_ptr<Font::RenderedTextCache> cache;
    bool updateCache = true;

    auto cacheIterator = std::find_if(_renderedCache.begin(), _renderedCache.end(),
                                      [&](const std::shared_ptr<Font::RenderedTextCache> &ptr) {
        return ptr->isEquivalent(text, width, height, spacing);
    });

    if (cacheIterator != _renderedCache.end()) {
        cache = *cacheIterator;
        updateCache = false;
    } else if (_renderedCache.size() < MAX_CACHE_SIZE) {
        cache = std::make_shared<RenderedTextCache>();
        _renderedCache.push_back(cache);
    } else {
        std::sort(_renderedCache.begin(), _renderedCache.end(), RenderedTextCache::isLessThan);
        cache = _renderedCache.at(0);
    }

    if (update) *update = updateCache;
    return cache;
}

std::shared_ptr<Font::SizedTextCache> Font::findInSizedCache(const std::string &text, int spacing, bool *update) {
    if (MAX_CACHE_SIZE == 0) {
        *update = true;
        return std::make_shared<SizedTextCache>();
    }

    if (MAX_CACHE_SIZE == 1) {
        *update = true;
        if (_sizedCache.empty())
            _sizedCache.emplace_back(std::make_shared<SizedTextCache>());
        return _sizedCache[0];
    }

    std::shared_ptr<Font::SizedTextCache> cache;
    bool updateCache = true;

    auto cacheIterator = std::find_if(_sizedCache.begin(), _sizedCache.end(),
                                      [&](const std::shared_ptr<Font::SizedTextCache> &ptr) {
        return ptr->isEquivalent(text, spacing);
    });

    if (cacheIterator != _sizedCache.end()) {
        cache = *cacheIterator;
        updateCache = false;
    } else if (_sizedCache.size() < MAX_CACHE_SIZE) {
        cache = std::make_shared<SizedTextCache>();
        _sizedCache.push_back(cache);
    } else {
        std::sort(_sizedCache.begin(), _sizedCache.end(), SizedTextCache::isLessThan);
        cache = _sizedCache.at(0);
    }

    if (update) *update = updateCache;
    return cache;
}

#define TTF_VERSION_NUM SDL_VERSIONNUM(SDL_TTF_MAJOR_VERSION, SDL_TTF_MINOR_VERSION, SDL_TTF_PATCHLEVEL)
#define TTF_VERSION_2_0_14 SDL_VERSIONNUM(2, 0, 14)
#define TTF_VERSION_2_0_13 SDL_VERSIONNUM(2, 0, 13)
    
// SDL_ttf 2.0.14 has the correct function we need, so just use that.
#if TTF_VERSION_NUM >= TTF_VERSION_2_0_14

int Font::getFontKerning(uint16_t prev, uint16_t curr) const {
    return TTF_GetFontKerningSizeGlyphs(_ttfFont, prev, curr);
}

#else

// This function is used when we're running with SDL_ttf 2.0.12
int Font::getFontKerning_2_0_12(TTF_Font *font, uint16_t prev, uint16_t curr) {
    static auto realKerningFunc =
#if TTF_VERSION_NUM == TTF_VERSION_2_0_13
        reinterpret_cast<int(*)(TTF_Font *, int, int)>(TTF_GetFontKerningSize);
#else
        TTF_GetFontKerningSize;
#endif
    
    // Abuse the fact that TTF_GlyphIsProvided just calls FT_Get_Char_Index and returns the value
    // instead of returning if the value is not equal to 0
    int prev_index = TTF_GlyphIsProvided(font, prev);
    int next_index = TTF_GlyphIsProvided(font, curr);
    
    return realKerningFunc(font, prev_index, next_index);
}

int Font::getFontKerning(uint16_t prev, uint16_t curr) const {
    static int(*kerningFunc)(TTF_Font *, uint16_t, uint16_t) = nullptr;
    
    if (!kerningFunc) {
        const SDL_version *ttfVer = TTF_Linked_Version();
        if (SDL_VERSIONNUM(ttfVer->major, ttfVer->minor, ttfVer->patch) == TTF_VERSION_2_0_13) {
#if TTF_VERSION_NUM == TTF_VERSION_2_0_13
            kerningFunc = TTF_GetFontKerningSize;
#else
            kerningFunc = reinterpret_cast<int(*)(TTF_Font *, uint16_t, uint16_t)>(TTF_GetFontKerningSize);
#endif
        } else {
            kerningFunc = getFontKerning_2_0_12;
        }
    }
    
    return kerningFunc(_ttfFont, prev, curr);
}
#endif

#undef USING_FIXED_API
#undef IS_KERNING_FIXED

} // namespace Ego
