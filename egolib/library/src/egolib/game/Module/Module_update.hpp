#pragma once

#include <cstdint>
#include <memory>
#include <vector>

class IAudioSystem;
class IParticleHandler;
class ObjectHandler;
class Passage;
class ego_mesh_t;
struct damagetile_instance_t;
namespace Ego { class Player; }

namespace module_update
{

/// Invincibility time after taking damage-tile damage.
constexpr uint32_t DAMAGETILETIME = 32;

/// Per-update simulation steps with explicit inputs (bodies in Module_update.cpp).
void checkPassageMusic(const std::vector<std::shared_ptr<Ego::Player>>& players,
                       const std::vector<std::shared_ptr<Passage>>& passages,
                       IAudioSystem& audioSystem);
void updateAllObjects(ObjectHandler& objectHandler, uint32_t currentUpdateFrame,
                      uint32_t& characterStatClock);
void updateDamageTiles(ObjectHandler& objectHandler, const ego_mesh_t& mesh,
                       const damagetile_instance_t& damageTile,
                       IParticleHandler& particleHandler, uint32_t currentUpdateFrame);

} // namespace module_update
