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

} // namespace Graphics
} // namespace Ego
