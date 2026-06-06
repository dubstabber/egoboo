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

} // namespace Ego
