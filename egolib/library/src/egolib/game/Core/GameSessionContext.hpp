#pragma once

#include "idlib/non_copyable.hpp"

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
};
