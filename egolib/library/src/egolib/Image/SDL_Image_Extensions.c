//********************************************************************************************
//*
//*    This file is part of the SDL extensions library. This library is
//*    distributed with Egoboo.
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

/// @file egolib/Image/SDL_Image_Extensions.c
/// @brief Extensions to SDL image.

#include "egolib/Image/SDL_Image_Extensions.h"
#include "egolib/Image/ImageManager.hpp"

namespace Ego { namespace SDL {

uint32_t getEnumeratedPixelFormat(const pixel_descriptor& pixel_descriptor)
{
    uint32_t alphaMask = pixel_descriptor.get_alpha().get_mask(),
             blueMask = pixel_descriptor.get_blue().get_mask(),
             greenMask = pixel_descriptor.get_green().get_mask(),
             redMask = pixel_descriptor.get_red().get_mask();
    int bitsPerPixel = pixel_descriptor.get_color_depth().depth();

    uint32_t pixelFormatEnum_sdl = SDL_MasksToPixelFormatEnum(bitsPerPixel, redMask, greenMask, blueMask, alphaMask);
    if (SDL_PIXELFORMAT_UNKNOWN == pixelFormatEnum_sdl)
    {
        throw idlib::runtime_error(__FILE__, __LINE__, "pixel format descriptor has no corresponding SDL pixel format");
    }
    return pixelFormatEnum_sdl;
}

std::shared_ptr<const SDL_PixelFormat> getPixelFormat(const pixel_descriptor& pixel_descriptor)
{
    std::shared_ptr<const SDL_PixelFormat> pixelFormat_sdl = std::shared_ptr<const SDL_PixelFormat>
        (
            SDL_AllocFormat(getEnumeratedPixelFormat(pixel_descriptor)),
            [](SDL_PixelFormat *pixelFormat) { if (pixelFormat) { SDL_FreeFormat(pixelFormat); } }
    );
    if (!pixelFormat_sdl)
    {
        throw idlib::environment_error(__FILE__, __LINE__, "SDL", "internal error");
    }
    return pixelFormat_sdl;
}

std::shared_ptr<SDL_Surface> cloneSurface(const std::shared_ptr<const SDL_Surface>& surface)
{
    static_assert(SDL_VERSION_ATLEAST(2, 0, 0), "SDL 2.x required");
    if (!surface)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "surface");
    }
    // TODO: The signature SDL_ConvertSurface(SDL_Surface *, const SDL_PixelFormat *, uint32_t) might be considered as a bug.
    //       It should be SDL_ConvertSurface(const SDL_Surface *, const SDL_PixelFormat *, uint32_t).
    auto clone = std::shared_ptr<SDL_Surface>(SDL_ConvertSurface((SDL_Surface *)surface.get(), surface->format, 0), [](SDL_Surface *pSurface) { SDL_FreeSurface(pSurface); });
    if (!clone)
    {
        throw idlib::runtime_error(__FILE__, __LINE__, "SDL_ConvertSurface failed");
    }
    return clone;
}

bool testAlpha(SDL_Surface *surface)
{
    // test to see whether an image requires alpha blending

    if (!surface)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "surface");
    }

    // Alias.
    SDL_PixelFormat *format = surface->format;

    // (1)
    // If the surface has a per-surface color key,
    // it is partially transparent.
    uint32_t colorKey;
    int rslt = SDL_GetColorKey(surface, &colorKey);
    if (rslt < -1)
    {
        // If a value smaller than -1 is returned, an error occured.
        throw std::invalid_argument("SDL_GetColorKey failed");
    }
    else if (rslt >= 0)
    {
        // If a value greater or equal than 0 is returned, the surface has a color key.
        return true;
    } /*else if (rslt == -1) {
      // If a value of -1 is returned, the surface has no color key: Continue.
    }*/

    // (2)
    // If the image is alpha modded with a non-opaque alpha value,
    // it is partially transparent.
    uint8_t alpha;
    SDL_GetSurfaceAlphaMod(surface, &alpha);
    if (0xff != alpha)
    {
        return true;
    }

    // (3)
    // If the image is palettized and has non-opaque colors in the color,
    // it is partially transparent.
    if (nullptr != format->palette)
    {
        for (int i = 0; i < format->palette->ncolors; ++i)
        {
            SDL_Color& color = format->palette->colors[i];
            if (0xff != color.a)
            {
                return true;
            }
        }
        return false;
    }

    // (The image is not palettized.)
    // (4)
    // If the image has no alpha channel,
    // then it is NOT partially transparent.
    if (0x00 == format->Amask)
    {
        return false;
    }

    // (The image is not palettized and has an alpha channel.)
    // If the image has an alpha channel and has non-opaque pixels,
    // then it is partially transparent.
    uint32_t bitMask = format->Rmask | format->Gmask | format->Bmask | format->Amask;
    int bytesPerPixel = format->BytesPerPixel;
    int width = surface->w;
    int height = surface->h;
    int pitch = surface->pitch;

    const char *row_ptr = static_cast<const char *>(surface->pixels);
    for (int y = 0; y < height; ++y)
    {
        const char *char_ptr = row_ptr;
        for (int x = 0; x < width; ++x)
        {
            const uint32_t *ui32_ptr = reinterpret_cast<const uint32_t*>(char_ptr);
            uint32_t pixel = (*ui32_ptr) & bitMask;
            uint8_t r, g, b, a;
            SDL_GetRGBA(pixel, format, &r, &g, &b, &a);

            if (0xFF != a)
            {
                return true;
            }
            char_ptr += bytesPerPixel;
        }
        row_ptr += pitch;
    }

    return false;
}

Colour3b getColourMod(SDL_Surface *surface)
{
    if (!surface)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "surface");
    }
    uint8_t r, g, b;
    SDL_GetSurfaceColorMod(surface, &r, &g, &b);
    return Colour3b(r, g, b);
}

void setColourMod(SDL_Surface *surface, const Colour3b& colourMod)
{
    if (!surface)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "surface");
    }
    SDL_SetSurfaceColorMod(surface, colourMod.get_r(), colourMod.get_g(), colourMod.get_b());
}

BlendMode blendModeToInternal(SDL_BlendMode blendMode)
{
    switch (blendMode)
    {
    case SDL_BLENDMODE_NONE:
        return BlendMode::NoBlending;
    case SDL_BLENDMODE_BLEND:
        return BlendMode::AlphaBlending;
    case SDL_BLENDMODE_ADD:
        return BlendMode::AdditiveBlending;
    case SDL_BLENDMODE_MOD:
        return BlendMode::ModulativeBlending;
    default:
        throw idlib::unhandled_switch_case_error(__FILE__, __LINE__);
    };
}

SDL_BlendMode blendModeToExternal(BlendMode blendMode)
{
    switch (blendMode)
    {
    case BlendMode::NoBlending:
        return SDL_BLENDMODE_NONE;
    case BlendMode::AlphaBlending:
        return SDL_BLENDMODE_BLEND;
    case BlendMode::AdditiveBlending:
        return SDL_BLENDMODE_ADD;
    case BlendMode::ModulativeBlending:
        return SDL_BLENDMODE_MOD;
    default:
        throw idlib::unhandled_switch_case_error(__FILE__, __LINE__);
    };
}

BlendMode getBlendMode(SDL_Surface *surface)
{
    if (!surface)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "surface");
    }
    SDL_BlendMode blendMode;
    SDL_GetSurfaceBlendMode(surface, &blendMode);
    return blendModeToInternal(blendMode);
}

void setBlendMode(SDL_Surface *surface, BlendMode blendMode)
{
    if (!surface)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "surface");
    }
    SDL_SetSurfaceBlendMode(surface, blendModeToExternal(blendMode));
}

idlib::pixel_format getPixelFormat(SDL_Surface *surface)
{
    if (!surface)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "surface");
    }
    switch (surface->format->format)
    {
    case SDL_PIXELFORMAT_RGB888:
        return idlib::pixel_format::R8G8B8;
    case SDL_PIXELFORMAT_RGBA8888:
        return idlib::pixel_format::R8G8B8A8;
    case SDL_PIXELFORMAT_BGR888:
        return idlib::pixel_format::B8G8R8;
    case SDL_PIXELFORMAT_BGRA8888:
        return idlib::pixel_format::B8G8R8A8;
    case SDL_PIXELFORMAT_ABGR8888:
        return idlib::pixel_format::A8B8G8R8;
    case SDL_PIXELFORMAT_ARGB8888:
        return idlib::pixel_format::A8R8G8B8;
    case SDL_PIXELFORMAT_RGB24:
        if (idlib::get_byte_order() == idlib::byte_order::big_endian)
            return idlib::pixel_format::R8G8B8;
        else
            return idlib::pixel_format::B8G8R8;
    case SDL_PIXELFORMAT_BGR24:
        if (idlib::get_byte_order() == idlib::byte_order::big_endian)
            return idlib::pixel_format::B8G8R8;
        else
            return idlib::pixel_format::R8G8B8;
    default:
        throw idlib::runtime_error(__FILE__, __LINE__, "unsupported/unknown pixel format");
    };
}

uint32_t make_rgb(SDL_Surface *surface, const Colour3b& colour)
{
    if (!surface)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "surface");
    }
    return SDL_MapRGB(surface->format, colour.get_r(), colour.get_g(), colour.get_b());
}

uint32_t make_rgba(SDL_Surface *surface, const Colour4b& colour)
{
    if (!surface)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "surface");
    }
    return SDL_MapRGBA(surface->format, colour.get_r(), colour.get_g(), colour.get_b(), colour.get_a());
}

std::shared_ptr<SDL_Surface> render_glyph(TTF_Font *sdl_font, uint16_t code_point, const Colour4b& color)
{
    SDL_Color sdl_color = { color.get_red(), color.get_green(), color.get_blue(), color.get_alpha() };
    SDL_Surface *sdl_surface = nullptr;
    if (TTF_GlyphIsProvided(sdl_font, code_point))
    {
        sdl_surface = TTF_RenderGlyph_Blended(sdl_font, code_point, sdl_color);
    }
    // Note: According to C++ documentation, the deleter is invoked if the std::shared_ptr constructor
    //       raises an exception.
    return std::shared_ptr<SDL_Surface>(sdl_surface, [](SDL_Surface *sdl_surface) { SDL_FreeSurface(sdl_surface); });
}

} } // namespace Ego::SDL
