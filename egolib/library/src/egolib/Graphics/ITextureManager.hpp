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

/// @brief Ownership-move seam for the active texture manager (mirrors
///        Ego::activeGraphicsSystem / Ego::activeRenderer). EngineContext owns the
///        install/clear and delegates here, so lower-layer callers can reach the
///        installed texture manager without depending on the app-layer EngineContext.

/// @brief Install the active texture manager.
/// @param textureManager the texture manager to install
/// @throw std::logic_error if a texture manager is already installed
void installActiveTextureManager(ITextureManager& textureManager);

/// @brief Clear the installed active texture manager.
void clearActiveTextureManager();

/// @brief The installed active texture manager, or @a nullptr if none is installed.
ITextureManager* tryActiveTextureManager();

/// @brief The active texture manager.
/// @throw std::logic_error if none is installed
ITextureManager& activeTextureManager();

} // namespace Ego
