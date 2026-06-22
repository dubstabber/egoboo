#pragma once

#include <memory>
#include <string>

namespace Ego {

class Font;

/// @brief Service interface for loading fonts, decoupling callers from the
///        concrete FontManager singleton. Published through EngineContext.
class IFontManager {
public:
    virtual ~IFontManager() = default;

    virtual std::shared_ptr<Font> loadFont(const std::string& fileName, int pointSize) = 0;
};

/// @brief Install the active font manager.
/// @throw std::logic_error if a font manager is already installed.
void installActiveFontManager(IFontManager& fontManager);

/// @brief Clear the installed active font manager.
void clearActiveFontManager();

/// @brief The installed active font manager, or @a nullptr if none is installed.
IFontManager* tryActiveFontManager();

/// @brief The active font manager.
/// @throw std::logic_error if no font manager is installed.
IFontManager& activeFontManager();

} // namespace Ego
