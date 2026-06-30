#pragma once

#include "egolib/game/Core/ISessionState.hpp"
#include "egolib/typedef.h"
#include "idlib/non_copyable.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class GameModule;
class ModuleProfile;
class ego_mesh_t;
struct AnimatedTilesState;
struct fog_instance_t;
struct import_list_t;
struct water_instance_t;
struct WeatherState;
namespace Ego { class Player; class Texture; }

LocalPlayerStatus collectLocalPlayerStatus(const std::vector<std::shared_ptr<Ego::Player>>& players);
LocalPlayerPerceptionState collectLocalPlayerPerception(const std::vector<std::shared_ptr<Ego::Player>>& players);

class GameSessionContext : public ISessionState,
                           private idlib::non_copyable
{
public:
    static GameSessionContext& get();

    GameSessionContext();
    ~GameSessionContext();

    bool hasActiveModule() const;

    GameModule* tryActiveModule();
    const GameModule* tryActiveModule() const;

    GameModule& activeModule();
    const GameModule& activeModule() const;

    bool beginModule(const std::shared_ptr<ModuleProfile>& module);
    bool beginModule(const std::shared_ptr<ModuleProfile>& module, uint32_t seed);
    void quitModule();
    bool finishModule();

    std::shared_ptr<ego_mesh_t> mesh();
    std::shared_ptr<const Ego::Texture> tileTexture(size_t index);
    std::shared_ptr<const Ego::Texture> waterTexture(uint8_t layer);
    water_instance_t& water();
    WeatherState& weatherState();
    fog_instance_t& fog();
    AnimatedTilesState& animatedTilesState();
    const std::vector<std::shared_ptr<Ego::Player>>& playerList() const override;
    size_t localPlayerCount() const override;
    const LocalPlayerStatus& localPlayerStatus() const override;
    const LocalPlayerPerceptionState& localPlayerPerception() const override;
    const EnemySenseState& enemySense() const override;
    int respawnCooldown() const override;
    bool hasLocalPlayers() const override;
    bool allLocalPlayersDead() const override;

    void publishLocalPlayerCount(size_t count);
    void publishLocalPlayerStatus(const LocalPlayerStatus& status);
    void publishLocalPlayerPerception(const LocalPlayerPerceptionState& state);
    void publishEnemySense(const EnemySenseState& state);
    void publishRespawnCooldown(int ticks);
    void tickRespawnCooldown();
    void resetLocalPlayerState();
    void resetLocalPlayerPerception();
    void resetEnemySense();
    void resetRespawnCooldown();

    import_list_t& importList();
    const import_list_t& importList() const;

    bool& overrideSlots();
    uint32_t& worldUpdateCount();
    uint32_t worldUpdateCount() const override;
    uint32_t& characterStatClock();
    uint32_t characterStatClock() const override;
    uint32_t& enchantStatClock();
    uint32_t enchantStatClock() const override;

    void resetClocks();

private:
    std::unique_ptr<GameModule> _activeModule;
    std::unique_ptr<import_list_t> _importList;
    bool _overrideSlots;
    uint32_t _worldUpdateCount;
    uint32_t _characterStatClock;
    uint32_t _enchantStatClock;
    size_t _preModuleLocalPlayerCount;
    LocalPlayerStatus _localPlayerStatus;
    LocalPlayerPerceptionState _localPlayerPerception;
    EnemySenseState _enemySense;
    int _respawnCooldown;
    bool _hasPublishedLocalPlayerStatus;
};
