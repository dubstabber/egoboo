#pragma once

#include "egolib/IDSZ.hpp"
#include "egolib/typedef.h"
#include "idlib/non_copyable.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class ObjectHandler;
class GameModule;
class ModuleProfile;
class ego_mesh_t;
struct AnimatedTilesState;
struct fog_instance_t;
struct import_list_t;
struct water_instance_t;
struct WeatherState;
namespace Ego { class Player; class Texture; }

struct LocalPlayerStatus
{
    size_t registeredCount = 0;
    size_t aliveCount = 0;
    size_t deadCount = 0;

    bool allPlayersDead() const
    {
        return deadCount >= registeredCount;
    }
};

struct LocalPlayerPerceptionState
{
    float grogLevel = 0.0f;
    float dazeLevel = 0.0f;
    float seeInvisibleLevel = 0.0f;
    float seeInvisibleMagnitude = 1.0f;
    float seeDarkLevel = 0.0f;
    float seeDarkMagnitude = 1.0f;
    float seeKurseLevel = 0.0f;
};

struct EnemySenseState
{
    EnemySenseState();
    EnemySenseState(TEAM_REF team, const IDSZ2& idsz);

    TEAM_REF team;
    IDSZ2 idsz;
};

LocalPlayerStatus collectLocalPlayerStatus(const std::vector<std::shared_ptr<Ego::Player>>& players);
LocalPlayerPerceptionState collectLocalPlayerPerception(const std::vector<std::shared_ptr<Ego::Player>>& players);

class GameSessionContext : private idlib::non_copyable
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

    ObjectHandler* tryObjectHandler();
    ObjectHandler& objectHandler();
    std::shared_ptr<ego_mesh_t> mesh();
    std::shared_ptr<const Ego::Texture> tileTexture(size_t index);
    std::shared_ptr<const Ego::Texture> waterTexture(uint8_t layer);
    water_instance_t& water();
    WeatherState& weatherState();
    fog_instance_t& fog();
    AnimatedTilesState& animatedTilesState();
    const std::vector<std::shared_ptr<Ego::Player>>& playerList() const;
    size_t localPlayerCount() const;
    const LocalPlayerStatus& localPlayerStatus() const;
    const LocalPlayerPerceptionState& localPlayerPerception() const;
    const EnemySenseState& enemySense() const;
    int respawnCooldown() const;
    bool hasLocalPlayers() const;
    bool allLocalPlayersDead() const;

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
    uint32_t& characterStatClock();
    uint32_t& enchantStatClock();

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
