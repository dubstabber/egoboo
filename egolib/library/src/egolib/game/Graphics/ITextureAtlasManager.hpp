#pragma once

#include <memory>

namespace Ego {

class Texture;

namespace Graphics {

/// @brief Service interface for the mesh tile-texture atlas, decoupling callers
///        from the concrete TextureAtlasManager singleton. Published through
///        EngineContext.
class ITextureAtlasManager {
public:
    virtual ~ITextureAtlasManager() = default;

    /// @brief Get the "small" (minified) tile texture for the given image index.
    virtual std::shared_ptr<Ego::Texture> getSmall(int which) const = 0;

    /// @brief Get the "big" tile texture for the given image index.
    virtual std::shared_ptr<Ego::Texture> getBig(int which) const = 0;

    /// @brief Re-upload all atlas textures to the GPU.
    virtual void reupload() = 0;

    /// @brief Decimate all tiled textures of the current mesh into per-tile textures.
    virtual void loadTileSet() = 0;
};

/// @brief Install the active texture-atlas manager.
/// @throw std::logic_error if a texture-atlas manager is already installed.
void installActiveTextureAtlasManager(ITextureAtlasManager& textureAtlasManager);

/// @brief Clear the installed active texture-atlas manager.
void clearActiveTextureAtlasManager();

/// @brief The installed active texture-atlas manager, or @a nullptr if none is installed.
ITextureAtlasManager* tryActiveTextureAtlasManager();

/// @brief The active texture-atlas manager.
/// @throw std::logic_error if no texture-atlas manager is installed.
ITextureAtlasManager& activeTextureAtlasManager();

} // namespace Graphics
} // namespace Ego
