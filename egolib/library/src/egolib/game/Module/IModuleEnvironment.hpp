#pragma once

/// @file egolib/game/Module/IModuleEnvironment.hpp
/// @brief Active read-only module-environment seam.

#include <cstddef>
#include <cstdint>
#include <memory>

class ego_mesh_t;
struct AnimatedTilesState;
struct fog_instance_t;
struct water_instance_t;
struct WeatherState;
namespace Ego { class Texture; }

/// @brief The active module environment surface for callers that need terrain,
///        environmental state, or module tile/water textures without depending
///        on GameSessionContext.
class IModuleEnvironment
{
public:
    virtual ~IModuleEnvironment() = default;

    virtual std::shared_ptr<ego_mesh_t> mesh() = 0;
    virtual std::shared_ptr<const Ego::Texture> tileTexture(size_t index) = 0;
    virtual std::shared_ptr<const Ego::Texture> waterTexture(uint8_t layer) = 0;
    virtual water_instance_t& water() = 0;
    virtual WeatherState& weatherState() = 0;
    virtual fog_instance_t& fog() = 0;
    virtual AnimatedTilesState& animatedTilesState() = 0;
};

/// @brief Install @a environment as the active module environment. Passing
///        nullptr is equivalent to clearModuleEnvironment().
void installModuleEnvironment(IModuleEnvironment* environment);

/// @brief Clear the installed active module environment.
void clearModuleEnvironment();

/// @brief The installed module environment, or nullptr if none is installed.
IModuleEnvironment* tryActiveModuleEnvironment();

/// @brief The installed active module environment.
/// @throw std::logic_error if no module environment is installed.
IModuleEnvironment& activeModuleEnvironment();
