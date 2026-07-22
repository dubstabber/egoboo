#pragma once

#include "egolib/game/Module/AnimatedTiles.hpp"
#include "egolib/game/Module/Fog.hpp"
#include "egolib/game/Module/ModuleRuntime.hpp"
#include "egolib/game/Module/Water.hpp"
#include "egolib/game/Module/Weather.hpp"
#include "egolib/game/Module/damagetile_instance.h"
#include "egolib/Renderer/DeferredTexture.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class IProfileSystem;
class ModuleProfile;
class ObjectHandler;
class Passage;
class Team;
class ego_mesh_t;

namespace module_loading
{

struct ModuleLoadContext
{
    GameModuleRuntime& runtime;
    const std::shared_ptr<ModuleProfile>& moduleProfile;
    uint32_t seed;
    std::vector<Team>& teamList;
    std::array<Ego::DeferredTexture, 4>& tileTextures;
    std::array<Ego::DeferredTexture, 2>& waterTextures;
    water_instance_t& water;
    damagetile_instance_t& damageTile;
    WeatherState& weatherState;
    fog_instance_t& fog;
    AnimatedTilesState& animatedTilesState;
    std::shared_ptr<ego_mesh_t>& mesh;
    ObjectHandler& objectHandler;
    std::vector<std::shared_ptr<Passage>>& passages;
};

/// Content-loading steps with explicit inputs (bodies in Module_loading.cpp).
void loadProfiles(GameModuleRuntime& runtime, const ModuleProfile& moduleProfile);
void loadPassages(ego_mesh_t& mesh, ObjectHandler& objectHandler, std::vector<std::shared_ptr<Passage>>& passages);
void loadTeamAlliances(std::vector<Team>& teamList);
void logSlotUsage(IProfileSystem& profileSystem, const std::string& savename);

class ModuleLoadPhase
{
public:
    explicit ModuleLoadPhase(ModuleLoadContext context);

    void run();

private:
    void initializeRuntime();
    void initializeTeamsAndTextures();
    void initializeSharedAssets();
    void loadEnvironment();
    void loadContent();
    void finalizeInitialization();

private:
    ModuleLoadContext _context;
};

} // namespace module_loading
