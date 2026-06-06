#pragma once

#include <memory>
#include <string>

namespace Ego {

class Texture;

/// @brief Service interface for texture loading/management, decoupling callers
///        from the concrete TextureManager singleton. Published through
///        EngineContext.
class ITextureManager {
public:
    virtual ~ITextureManager() = default;

    /// @brief Get (lazily loading) the texture for the given file path.
    virtual const std::shared_ptr<Texture>& getTexture(const std::string& filePath) = 0;

    /// @brief Process any pending deferred texture loads (called per frame).
    virtual void updateDeferredLoading() = 0;

    /// @brief Re-upload all managed textures to the GPU.
    virtual void reupload() = 0;

    /// @brief Release all managed textures.
    virtual void release_all() = 0;
};

} // namespace Ego
