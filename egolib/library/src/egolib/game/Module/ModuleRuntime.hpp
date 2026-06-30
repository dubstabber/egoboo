#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

class IAudioSystem;
class ICameraSystem;
class IParticleHandler;
class IProfileSystem;
struct egoboo_config_t;
struct import_list_t;

namespace Log { struct Target; }

namespace Ego {
namespace Graphics {
class IBillboardSystem;
} // namespace Graphics
} // namespace Ego

/// @brief Explicit service and session-state providers used by GameModule.
///
/// The providers keep the module implementation independent of EngineContext while still
/// preserving the existing swappable active-service behavior used by focused tests.
struct GameModuleRuntime
{
    std::function<IProfileSystem&()> profileSystem;
    std::function<IAudioSystem&()> audioSystem;
    std::function<IParticleHandler&()> particleHandler;
    std::function<ICameraSystem&()> cameraSystem;
    std::function<Ego::Graphics::IBillboardSystem&()> billboardSystem;
    std::function<egoboo_config_t&()> config;
    std::function<Log::Target&()> logTarget;

    std::function<import_list_t&()> importList;
    std::function<bool&()> overrideSlots;
    std::function<uint32_t&()> worldUpdateCount;
    std::function<uint32_t&()> characterStatClock;
    std::function<uint32_t&()> enchantStatClock;
    std::function<size_t()> localPlayerCount;
    std::function<void(size_t)> publishLocalPlayerCount;
    std::function<void()> resetClocks;
};
