#pragma once

#include <SDL.h>

#include <filesystem>
#include <stdexcept>
#include <string>

namespace Ego::Test
{

inline std::filesystem::path findRepositoryRoot()
{
    std::filesystem::path candidate = std::filesystem::path(__FILE__).parent_path();
    while (!candidate.empty())
    {
        if (std::filesystem::is_directory(candidate / "data")
         && std::filesystem::is_directory(candidate / "egolib"))
        {
            return candidate;
        }

        const std::filesystem::path parent = candidate.parent_path();
        if (parent == candidate)
        {
            break;
        }
        candidate = parent;
    }

    throw std::runtime_error("unable to locate repository root for Egoboo tests");
}

inline void configureDataDirectory()
{
    const std::string dataDir = (findRepositoryRoot() / "data").string();
    if (SDL_setenv("EGOBOO_DATA_DIR", dataDir.c_str(), 1) != 0)
    {
        throw std::runtime_error("unable to set EGOBOO_DATA_DIR for Egoboo tests");
    }
}

} // namespace Ego::Test
