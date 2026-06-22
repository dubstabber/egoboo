#pragma once

#include "egolib/Graphics/PixelFormat.hpp"

#include <SDL.h>
#undef main

#include <memory>
#include <string>

namespace Ego
{

class IImageManager
{
public:
    virtual ~IImageManager() = default;

    virtual std::shared_ptr<SDL_Surface> getDefaultImage() const = 0;
    virtual std::shared_ptr<SDL_Surface> createImage(size_t width,
                                                     size_t height,
                                                     size_t pitch,
                                                     const pixel_descriptor& pixelDescriptor,
                                                     void* pixels) const = 0;
    virtual std::shared_ptr<SDL_Surface> createImage(size_t width,
                                                     size_t height,
                                                     const pixel_descriptor& pixelDescriptor) const = 0;
    virtual void save_as_png(const std::shared_ptr<SDL_Surface>& pixels, const std::string& pathname) const = 0;
    virtual bool imageExistsWithKnownExtension(const std::string& basename) const = 0;
    virtual std::shared_ptr<SDL_Surface> loadImageWithKnownExtension(const std::string& basename,
                                                                     std::string* resolvedPath = nullptr) const = 0;
};

/// @brief Install the active image manager.
/// @throw std::logic_error if an image manager is already installed.
void installActiveImageManager(IImageManager& imageManager);

/// @brief Clear the installed active image manager.
void clearActiveImageManager();

/// @brief The installed active image manager, or @a nullptr if none is installed.
IImageManager* tryActiveImageManager();

/// @brief The active image manager.
///
/// Falls back to the concrete singleton when no explicit service has been installed,
/// preserving legacy lower-layer callers that predate EngineContext publication.
IImageManager& activeImageManager();

} // namespace Ego
