#include "egolib/game/Graphics/ITextureAtlasManager.hpp"

#include <stdexcept>

namespace Ego {
namespace Graphics {

namespace {
ITextureAtlasManager* g_activeTextureAtlasManager = nullptr;
}

void installActiveTextureAtlasManager(ITextureAtlasManager& textureAtlasManager)
{
    if (g_activeTextureAtlasManager)
    {
        throw std::logic_error("texture atlas manager already installed");
    }
    g_activeTextureAtlasManager = &textureAtlasManager;
}

void clearActiveTextureAtlasManager()
{
    g_activeTextureAtlasManager = nullptr;
}

ITextureAtlasManager* tryActiveTextureAtlasManager()
{
    return g_activeTextureAtlasManager;
}

ITextureAtlasManager& activeTextureAtlasManager()
{
    if (!g_activeTextureAtlasManager)
    {
        throw std::logic_error("no active texture atlas manager");
    }
    return *g_activeTextureAtlasManager;
}

} // namespace Graphics
} // namespace Ego
