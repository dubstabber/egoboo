#include "egolib/Graphics/IFontManager.hpp"

#include <stdexcept>

namespace Ego {

namespace {
IFontManager* g_activeFontManager = nullptr;
}

void installActiveFontManager(IFontManager& fontManager)
{
    if (g_activeFontManager)
    {
        throw std::logic_error("font manager already installed");
    }
    g_activeFontManager = &fontManager;
}

void clearActiveFontManager()
{
    g_activeFontManager = nullptr;
}

IFontManager* tryActiveFontManager()
{
    return g_activeFontManager;
}

IFontManager& activeFontManager()
{
    if (!g_activeFontManager)
    {
        throw std::logic_error("no active font manager");
    }
    return *g_activeFontManager;
}

} // namespace Ego
