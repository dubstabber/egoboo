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

/// @file egolib/Image/SDL_Image_Extensions_functors.c
/// @brief Extensions to SDL image: the Ego::convert and idlib power_of_two/pad/get_pixel/blit/fill/set_pixel functor specializations for SDL_Surface.

#include "egolib/Image/SDL_Image_Extensions.h"
#include "egolib/Image/ImageManager.hpp"

namespace Ego {

std::shared_ptr<SDL_Surface> convert_functor<SDL_Surface>::operator()(const std::shared_ptr<SDL_Surface>& pixels, const pixel_descriptor& pixel_descriptor) const
{
    if (!pixels)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "surface");
    }

    uint32_t alphaMask = pixel_descriptor.get_alpha().get_mask(),
             blueMask = pixel_descriptor.get_blue().get_mask(),
             greenMask = pixel_descriptor.get_green().get_mask(),
             redMask = pixel_descriptor.get_red().get_mask();
    int bpp = pixel_descriptor.get_color_depth().depth();

    uint32_t newFormat = SDL_MasksToPixelFormatEnum(bpp, redMask, greenMask, blueMask, alphaMask);
    if (newFormat == SDL_PIXELFORMAT_UNKNOWN)
    {
        throw idlib::invalid_argument_error(__FILE__, __LINE__, "pixelFormatDescriptor doesn't correspond with a SDL_PixelFormat");
    }
    SDL_Surface *newPixels = SDL_ConvertSurfaceFormat(pixels.get(), newFormat, 0);
    if (!newPixels)
    {
        throw idlib::runtime_error(__FILE__, __LINE__, "unable to convert surface");
    }

    return std::shared_ptr<SDL_Surface>(newPixels, [](SDL_Surface *pSurface) { SDL_FreeSurface(pSurface); });
}

} // namespace Ego

namespace idlib {

using Rectangle2f = Ego::Rectangle2f;
using Point2f = Ego::Point2f;
using Colour3b = Ego::Colour3b;
using Colour4b = Ego::Colour4b;

std::shared_ptr<SDL_Surface> power_of_two_functor<SDL_Surface>::operator()(const std::shared_ptr<SDL_Surface>& pixels) const {
    // Alias old width and old height.
  int oldWidth = pixels->w,
     oldHeight = pixels->h;

// Compute new width and new height.
  int newWidth = Ego::Math::powerOfTwo(oldWidth),
     newHeight = Ego::Math::powerOfTwo(oldHeight);

// Only if the new dimension differ from the old dimensions, perform the scaling.
  if (newWidth != oldWidth || newHeight != oldHeight) {
    padding padding;
    padding.left = 0;
    padding.top = 0;
    padding.right = newWidth - oldWidth;
    padding.bottom = newHeight - oldHeight;
    return pad(pixels, padding);
  } else {
    return Ego::SDL::cloneSurface(pixels);
  }
}

std::shared_ptr<SDL_Surface> pad_functor<SDL_Surface>::operator()(const std::shared_ptr<SDL_Surface>& pixels, const padding& padding) const
{
    if (!pixels)
        throw idlib::argument_null_error(__FILE__, __LINE__, "pixels");

    if (padding.left < 0)
        throw idlib::invalid_argument_error(__FILE__, __LINE__, "left < 0");
    if (padding.top < 0)
        throw idlib::invalid_argument_error(__FILE__, __LINE__, "top < 0");
    if (padding.right < 0)
        throw idlib::invalid_argument_error(__FILE__, __LINE__, "right < 0");
    if (padding.bottom < 0)
        throw idlib::invalid_argument_error(__FILE__, __LINE__, "bottom < 0");

    if (!padding.left && !padding.top && !padding.right && !padding.bottom)
    {
        return Ego::SDL::cloneSurface(pixels);
    }

    // Alias old surface.
    const auto& oldSurface = pixels;

    // Alias old width and old height.
    size_t oldWidth = pixels->w,
           oldHeight = pixels->h;

    // Compute new width and new height.
    size_t newWidth = oldWidth + padding.left + padding.right,
           newHeight = oldHeight + padding.top + padding.bottom;

    // Create the copy.
    auto newSurface = std::shared_ptr<SDL_Surface>(SDL_CreateRGBSurface(SDL_SWSURFACE, newWidth, newHeight, oldSurface->format->BitsPerPixel,
                                                                        oldSurface->format->Rmask,
                                                                        oldSurface->format->Gmask,
                                                                        oldSurface->format->Bmask,
                                                                        oldSurface->format->Amask),
                                                                        [](SDL_Surface *pSurface) { SDL_FreeSurface(pSurface); });
    if (!newSurface)
    {
        throw idlib::runtime_error(__FILE__, __LINE__, "SDL_CreateRGBSurface failed");
    }
    // Fill the copy with transparent black.
    SDL_FillRect(newSurface.get(), nullptr, SDL_MapRGBA(newSurface->format, 0, 0, 0, 0));
    // Copy the old surface into the new surface.
    for (size_t y = 0; y < oldHeight; ++y)
    {
        for (size_t x = 0; x < oldWidth; ++x)
        {
            auto p = idlib::get_pixel(oldSurface.get(), { x, y });
            idlib::set_pixel(newSurface.get(), p, { padding.left + x, padding.top + y });
        }
    }
    return newSurface;
}

Colour4b get_pixel_functor<SDL_Surface>::operator()(const SDL_Surface *surface, const Point2f& point) const
{
    if (!surface)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "surface");
    }

    int32_t x = std::round(point.x()),
            y = std::round(point.y());
    if (x < 0) throw idlib::argument_out_of_bounds_error(__FILE__, __LINE__, "x");
    if (x >= surface->w) throw idlib::argument_out_of_bounds_error(__FILE__, __LINE__, "x");
    if (y < 0) throw idlib::argument_out_of_bounds_error(__FILE__, __LINE__, "y");
    if (y >= surface->h) throw idlib::argument_out_of_bounds_error(__FILE__, __LINE__, "y");

    int bpp = surface->format->BytesPerPixel;
    // Here p is the address to the pixel we want to get.
    uint8_t *p = (uint8_t *)surface->pixels + y * surface->pitch + x * bpp;

    switch (bpp)
    {
    case 1:
    {
        uint32_t v = *p;
        uint8_t r, g, b, a;
        SDL_GetRGBA(v, surface->format, &r, &g, &b, &a);
        return Colour4b(r, g, b, a);
    }
    case 2:
    {
        uint32_t v = *reinterpret_cast<uint16_t*>(p);
        uint8_t r, g, b, a;
        SDL_GetRGBA(v, surface->format, &r, &g, &b, &a);
        return Colour4b(r, g, b, a);
    }
    case 3:
    {
        if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
        {
            uint32_t v = p[0] << 16 | p[1] << 8 | p[2];
            uint8_t r, g, b, a;
            SDL_GetRGBA(v, surface->format, &r, &g, &b, &a);
            return Colour4b(r, g, b, a);
        }
        else
        {
            uint32_t v = p[0] | p[1] << 8 | p[2] << 16;
            uint8_t r, g, b, a;
            SDL_GetRGBA(v, surface->format, &r, &g, &b, &a);
            return Colour4b(r, g, b, a);
        }
    }
    case 4:
    {
        uint32_t v = *reinterpret_cast<uint32_t*>(p);
        uint8_t r, g, b, a;
        SDL_GetRGBA(v, surface->format, &r, &g, &b, &a);
        return Colour4b(r, g, b, a);
    }
    default:
        throw idlib::unhandled_switch_case_error(__FILE__, __LINE__, "unreachable code reached"); /* shouldn't happen, but avoids warnings */
    }
}

void blit_functor<SDL_Surface>::operator()(SDL_Surface *source, SDL_Surface *target) const
{
    if (!source)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "source");
    }
    if (!target)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "target");
    }
    SDL_BlitSurface(source, nullptr, target, nullptr);
}

void blit_functor<SDL_Surface>::operator()(SDL_Surface *source, const Rectangle2f& source_rectangle, SDL_Surface *target) const
{
    if (!source)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "source");
    }
    if (!target)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "target");
    }
    SDL_Rect sdl_source_rectangle;
    sdl_source_rectangle.x = source_rectangle.get_min().x();
    sdl_source_rectangle.y = source_rectangle.get_min().y();
    sdl_source_rectangle.w = source_rectangle.get_size().x();
    sdl_source_rectangle.h = source_rectangle.get_size().y();
    SDL_BlitSurface(source, &sdl_source_rectangle, target, nullptr);
}

void blit_functor<SDL_Surface>::operator()(SDL_Surface *source, SDL_Surface *target, const Point2f& target_position) const
{
    if (!source)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "source");
    }
    if (!target)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "target");
    }
    SDL_Rect sdl_target_rectangle;
    sdl_target_rectangle.x = target_position.x();
    sdl_target_rectangle.y = target_position.y();
    sdl_target_rectangle.w = 0;
    sdl_target_rectangle.h = 0;
    SDL_BlitSurface(source, nullptr, target, &sdl_target_rectangle);
}

void blit_functor<SDL_Surface>::operator()(SDL_Surface *source, const Rectangle2f& source_rectangle, SDL_Surface *target, const Point2f& target_position) const
{
    if (!source)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "source");
    }
    if (!target)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "target");
    }
    SDL_Rect sdl_source_rectangle;
    sdl_source_rectangle.x = source_rectangle.get_min().x();
    sdl_source_rectangle.y = source_rectangle.get_min().y();
    sdl_source_rectangle.w = source_rectangle.get_size().x();
    sdl_source_rectangle.h = source_rectangle.get_size().y();
    SDL_Rect sdl_target_rectangle;
    sdl_target_rectangle.x = target_position.x();
    sdl_target_rectangle.y = target_position.y();
    sdl_target_rectangle.w = 0;
    sdl_target_rectangle.h = 0;
    SDL_BlitSurface(source, &sdl_source_rectangle, target, &sdl_target_rectangle);
}

void fill_functor<SDL_Surface>::operator()(SDL_Surface *surface, const Colour3b& color) const
{
    if (!surface)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "surface");
    }
    SDL_FillRect(surface, nullptr, Ego::SDL::make_rgb(surface, color));
}

void fill_functor<SDL_Surface>::operator()(SDL_Surface *surface, const Colour4b& color) const
{
    if (!surface)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "surface");
    }
    SDL_FillRect(surface, nullptr, Ego::SDL::make_rgba(surface, color));
}

void fill_functor<SDL_Surface>::operator()(SDL_Surface *surface, const Colour3b& color, const Rectangle2f& rectangle) const
{
    if (!surface)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "surface");
    }
    SDL_Rect sdl_rectangle;
    sdl_rectangle.x = rectangle.get_min().x();
    sdl_rectangle.y = rectangle.get_min().y();
    sdl_rectangle.w = rectangle.get_size().x();
    sdl_rectangle.h = rectangle.get_size().y();
    SDL_FillRect(surface, &sdl_rectangle, Ego::SDL::make_rgb(surface, color));
}

void fill_functor<SDL_Surface>::operator()(SDL_Surface *surface, const Colour4b& color, const Rectangle2f& rectangle) const
{
    if (!surface)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "surface");
    }
    SDL_Rect sdl_rectangle;
    sdl_rectangle.x = rectangle.get_min().x();
    sdl_rectangle.y = rectangle.get_min().y();
    sdl_rectangle.w = rectangle.get_size().x();
    sdl_rectangle.h = rectangle.get_size().y();
    SDL_FillRect(surface, &sdl_rectangle, Ego::SDL::make_rgba(surface, color));
}


void set_pixel_functor<SDL_Surface>::operator()(SDL_Surface *surface, const Colour3b& color, const Point2f& point) const
{
    if (!surface)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "surface");
    }
    uint32_t coded_color = Ego::SDL::make_rgb(surface, color);
    (*this)(surface, coded_color, point);
}

void set_pixel_functor<SDL_Surface>::operator()(SDL_Surface *surface, const Colour4b& color, const Point2f& point) const
{
    if (!surface)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "surface");
    }
    uint32_t coded_color = Ego::SDL::make_rgba(surface, color);
    (*this)(surface, coded_color, point);
}

void set_pixel_functor<SDL_Surface>::operator()(SDL_Surface *surface, uint32_t color, const Point2f& point) const
{
    if (!surface)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "surface");
    }
    int32_t x = std::round(point.x()),
            y = std::round(point.y());
    if (x < 0) return;
    if (x >= surface->w) return;
    if (y < 0) return;
    if (y >= surface->h) return;

    // Get Bytes per pixel.
    int bpp = surface->format->BytesPerPixel;

    // Here p is the address to the pixel we want to set.
    uint8_t *p = (uint8_t *)surface->pixels + y * surface->pitch + x * bpp;

    switch (bpp)
    {
    case 1:
        *p = color;
        break;

    case 2:
        *reinterpret_cast<uint16_t*>(p) = color;
        break;

    case 3:
#if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
        p[0] = (color >> 16) & 0xff;
        p[1] = (color >> 8) & 0xff;
        p[2] = color & 0xff;
#else
        p[0] = color & 0xff;
        p[1] = (color >> 8) & 0xff;
        p[2] = (color >> 16) & 0xff;
#endif
        break;

    case 4:
        *reinterpret_cast<uint32_t*>(p) = color;
        break;

    default:
        throw idlib::unhandled_switch_case_error(__FILE__, __LINE__, "unreachable code reached"); /* shouldn't happen, but avoids warnings */
    }
}

} // namespace idlib
