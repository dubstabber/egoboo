#include "egolib/Graphics/ITextureManager.hpp"

#include <stdexcept>

namespace Ego {

namespace {
ITextureManager* g_activeTextureManager = nullptr;
}

void installActiveTextureManager(ITextureManager& textureManager)
{
    if (g_activeTextureManager)
    {
        throw std::logic_error("texture manager already installed");
    }
    g_activeTextureManager = &textureManager;
}

void clearActiveTextureManager()
{
    g_activeTextureManager = nullptr;
}

ITextureManager* tryActiveTextureManager()
{
    return g_activeTextureManager;
}

ITextureManager& activeTextureManager()
{
    if (!g_activeTextureManager)
    {
        throw std::logic_error("no active texture manager");
    }
    return *g_activeTextureManager;
}

} // namespace Ego
