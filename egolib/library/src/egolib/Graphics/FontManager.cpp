//********************************************************************************************
//*
//*    This file is part of Egoboo.
//*
//*    Egoboo is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Egoboo is distributed in the hope that it will be useful, but
//*    WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Egoboo.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************

/// @file egolib/Graphics/FontManager.cpp
/// @brief TTF management
/// @details TrueType font drawing functionality.  Uses the SDL_ttf module
///          to do its business. This depends on SDL_ttf and OpenGL.

#include "egolib/Graphics/FontManager.hpp"

#include "egolib/Log/_Include.hpp"

#include <array>

namespace Ego {

FontManager::FontManager() {
    Log::activeTarget() << Log::Entry::create(Log::Level::Info, __FILE__, __LINE__, "[font manager]: SDL_ttf v", SDL_TTF_MAJOR_VERSION, ".", SDL_TTF_MINOR_VERSION, ".", SDL_TTF_PATCHLEVEL, Log::EndOfEntry);
    if (TTF_Init() < 0) {
        auto e = Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "[font manager]: unable to initialized SDL_ttf v", SDL_TTF_MAJOR_VERSION, ".", SDL_TTF_MINOR_VERSION, ".", SDL_TTF_PATCHLEVEL, ": ",
                                    SDL_GetError(), Log::EndOfLine);
        Log::activeTarget() << e;
        throw idlib::environment_error(__FILE__, __LINE__, "font manager", e.getText());
    }
}

FontManager::~FontManager() {
    TTF_Quit();
}

std::shared_ptr<Font> FontManager::loadFont(const std::string &fileName, int pointSize) {
    try {
        return std::shared_ptr<Font>(new Font(fileName, pointSize));
    } catch (const idlib::exception& e) {
        Log::activeTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__,
                                         "[font manager]: failed to load font `", fileName,
                                         "` at ", pointSize, "pt: ", e.to_string(),
                                         ". Trying bundled fallbacks.", Log::EndOfEntry);

        static const std::array<const char *, 4> fallbackFonts = {
            "mp_data/DejaVuSansMono.ttf",
            "mp_data/Bo_Chen.ttf",
            "mp_data/IMMORTAL.ttf",
            "mp_data/Egobooish.ttf"
        };

        for (const char *fallbackFont : fallbackFonts) {
            if (fileName == fallbackFont) {
                continue;
            }

            try {
                auto font = std::shared_ptr<Font>(new Font(fallbackFont, pointSize));
                Log::activeTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__,
                                                 "[font manager]: using fallback font `",
                                                 fallbackFont, "` for `", fileName, "`",
                                                 Log::EndOfEntry);
                return font;
            } catch (const idlib::exception& fallbackError) {
                Log::activeTarget() << Log::Entry::create(Log::Level::Debug, __FILE__, __LINE__,
                                                 "[font manager]: fallback font `", fallbackFont,
                                                 "` also failed: ", fallbackError.to_string(),
                                                 Log::EndOfEntry);
            } catch (const std::exception& fallbackError) {
                Log::activeTarget() << Log::Entry::create(Log::Level::Debug, __FILE__, __LINE__,
                                                 "[font manager]: fallback font `", fallbackFont,
                                                 "` also failed: ", fallbackError.what(),
                                                 Log::EndOfEntry);
            }
        }

        throw;
    } catch (const std::exception& e) {
        Log::activeTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__,
                                         "[font manager]: failed to load font `", fileName,
                                         "` at ", pointSize, "pt: ", e.what(),
                                         ". Trying bundled fallbacks.", Log::EndOfEntry);

        static const std::array<const char *, 4> fallbackFonts = {
            "mp_data/DejaVuSansMono.ttf",
            "mp_data/Bo_Chen.ttf",
            "mp_data/IMMORTAL.ttf",
            "mp_data/Egobooish.ttf"
        };

        for (const char *fallbackFont : fallbackFonts) {
            if (fileName == fallbackFont) {
                continue;
            }

            try {
                auto font = std::shared_ptr<Font>(new Font(fallbackFont, pointSize));
                Log::activeTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__,
                                                 "[font manager]: using fallback font `",
                                                 fallbackFont, "` for `", fileName, "`",
                                                 Log::EndOfEntry);
                return font;
            // Mirrors the pair in the idlib arm above. Reaching this arm means the *primary*
            // font failed with a std::exception, but a fallback can still fail with an idlib one:
            // Font's constructor throws idlib::environment_error (an idlib::runtime_error, and so
            // an idlib::exception, which has no std::exception base) when TTF_OpenFontRW returns
            // null, which is the dominant failure for a missing or corrupt .ttf. Without this arm
            // that fallback exception escaped loadFont, replacing the original failure and
            // skipping the remaining fallbacks.
            } catch (const idlib::exception& fallbackError) {
                Log::activeTarget() << Log::Entry::create(Log::Level::Debug, __FILE__, __LINE__,
                                                 "[font manager]: fallback font `", fallbackFont,
                                                 "` also failed: ", fallbackError.to_string(),
                                                 Log::EndOfEntry);
            } catch (const std::exception& fallbackError) {
                Log::activeTarget() << Log::Entry::create(Log::Level::Debug, __FILE__, __LINE__,
                                                 "[font manager]: fallback font `", fallbackFont,
                                                 "` also failed: ", fallbackError.what(),
                                                 Log::EndOfEntry);
            }
        }

        throw;
    }
}
} // namespace Ego
