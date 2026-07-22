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
/// @file egolib/game/Module/Module.hpp
/// @details Code handling a game module
/// @author Johan Jansen

#pragma once

#include "egolib/Physics/ICollisionWorld.hpp"  // GameModule implements ICollisionWorld
#include "egolib/Entities/IObjectWorld.hpp"     // GameModule implements IObjectWorld
#include "egolib/Mesh/ITerrainQuery.hpp"        // GameModule implements ITerrainQuery
#include "egolib/game/Module/IModuleCommands.hpp" // GameModule implements IModuleCommands
#include "egolib/game/Module/IModuleEnvironment.hpp" // GameModule implements IModuleEnvironment
#include "egolib/game/Module/IModuleStatus.hpp" // GameModule implements IModuleStatus
#include "egolib/game/Module/ModuleRuntime.hpp"
#include "egolib/game/mesh.h"
#include "egolib/game/Module/AnimatedTiles.hpp"
#include "egolib/game/Module/Fog.hpp"
#include "egolib/game/Module/Water.hpp"
#include "egolib/game/Module/Weather.hpp"
#include "egolib/game/Module/module_spawn.h"
#include "egolib/game/Module/damagetile_instance.h"

//@todo This is an ugly hack to work around cyclic dependency and private header guards
#ifndef GAME_ENTITIES_PRIVATE
    #define GAME_ENTITIES_PRIVATE 1
    #include "egolib/Entities/ObjectHandler.hpp"
    #undef GAME_ENTITIES_PRIVATE
#else
    #include "egolib/Entities/ObjectHandler.hpp"
#endif

// Forward declarations.
class ModuleProfile;
class Passage;
class Team;
namespace Ego { class Player; }
namespace Ego { namespace Input { class InputDevice; } }

/// The module data that the game needs.
class GameModule : private idlib::non_copyable,
                   public Ego::Physics::ICollisionWorld,
                   public Ego::Entities::IObjectWorld,
                   public Ego::Mesh::ITerrainQuery,
                   public IModuleCommands,
                   public IModuleEnvironment,
                   public IModuleStatus
{
public:
    static constexpr float PITDEPTH = -60;  ///< Depth to kill character

    /**
     * @brief
     *  Prepeares a module to be played
     */
    GameModule(const std::shared_ptr<ModuleProfile> &module, const uint32_t seed, GameModuleRuntime runtime);

    /**
     * Destructor.
     */
    ~GameModule();

    void shutdownRuntime();

    /**
     * @return
     *  name of the module
     */
    const std::string& getName() const {return _name;}

    /**
     * @return
     *   number of players that can join this module
     **/
    uint8_t getImportAmount() const;

    uint8_t getPlayerAmount() const;

    bool isImportValid() const;

    /**
     * @return
     *  @a true if the players have won
     */
    bool isBeaten() const override {return _isBeaten;}

    /**
     * @brief
     *  Make the players win the module.
     *  If they press ESC the game ends and the end screen is shown instead of going to the pause menu
     */
    void beatModule() override {_isBeaten = true;}

    /**
     * @return
     *  @a true if the players are allowed to respawn upon death
     */
    bool isRespawnValid() const override {return _isRespawnValid;}

    /**
     * @return
     *  @a true if the players are allowed to export (save) their progress in this module upon exit
     */
    bool isExportValid() const override {return _exportValid;}

    void setExportValid(bool valid) override {_exportValid = valid;}

    bool canRespawnAnyTime() const override;

    void setRespawnValid(bool valid) override {_isRespawnValid = valid;}

    /// @author ZZ
    /// @details This function returns the owner of a item in a shop
    ObjectRef getShopOwner(const float x, const float y) override;

    /**
     * @brief
     *  Mark all shop passages having this owner as no longer a shop
     */
    void removeShopOwner(ObjectRef owner) override;

    /**
     * @return
     *  number of passages currently loaded
     */
    int getPassageCount();
    int passageCount() const override;

    /**
     * @brief
     *   Get Passage by index number
     * @return
     *  @a nullptr if the id is invalid else the Passage located in the ordered index number
     */
    std::shared_ptr<Passage> getPassageByID(int id) override;

    /**
     * @brief
     *  Get folder path to the Profile of this module
     */
    const std::string& getPath() const override;

    uint8_t getMaxPlayers() const;
    uint8_t getMinPlayers() const;

    const std::shared_ptr<ModuleProfile>& getModuleProfile() const {return _moduleProfile;}
    const std::shared_ptr<ModuleProfile>& moduleProfile() const override {return getModuleProfile();}

    void setImportPlayers(const std::list<std::string> &players) override {_playerNameList = players;}

    const std::list<std::string>& getImportPlayers() const {return _playerNameList;}
    const std::list<std::string>& importPlayers() const override {return getImportPlayers();}

    /**
    * @brief
    *   Get list of all teams in this Module. Teams determine who like each other and who don't
    **/
    std::vector<Team>& getTeamList() override {return _teamList;}

    ObjectRef getTeamLeaderRef(TEAM_REF teamRef) const override;

    ObjectRef getTeamCallerForHelpRef(TEAM_REF teamRef) const override;

    uint16_t getTeamMorale(TEAM_REF teamRef) const override;

    void giveTeamExperience(TEAM_REF teamRef, int amount, XPType type) const override;

    /**
    * @return
    *   Get the ObjectHandler associated with this Module instance
    **/
    ObjectHandler& getObjectHandler() override {return _gameObjects;}

    Object* tryObject(ObjectRef objectRef) override
    {
        return _gameObjects.exists(objectRef) ? _gameObjects.get(objectRef) : nullptr;
    }

    const Object* tryObject(ObjectRef objectRef) const override
    {
        return _gameObjects.exists(objectRef) ? _gameObjects.get(objectRef) : nullptr;
    }

    bool hasObject(ObjectRef objectRef) const override
    {
        return _gameObjects.exists(objectRef);
    }

    /**
    * @return
    *   true if the specified position is inside the level
    **/
    bool isInside(const float x, const float y) const override;

    /// @brief Ego::Physics::ICollisionWorld: the tile index at @a point (delegates to the mesh).
    Index1D getTileIndex(const Ego::Vector2f& point) const override;

    /// @brief Ego::Mesh::ITerrainQuery: the tile index at grid coordinate @a tile.
    Index1D getTileIndex(const Index2D& tile) const override;

    /// @name Ego::Physics::ICollisionWorld terrain queries (all delegate to the mesh / water).
    /// @{
    bool gridIsValid(const Index1D& tile) const override;
    uint8_t getTwist(const Index1D& tile) const override;
    uint8_t getFanTwist(const Index1D& tile) const override;
    uint32_t testFX(const Index1D& tile, uint32_t flags) const override;
    bool tileHasBits(const Index2D& tile, uint32_t flags) const override;
    bool isFanOff(const Index1D& tile) const override;
    float getElevation(const Ego::Vector2f& p, bool waterwalk) const override;
    float getElevation(const Ego::Vector2f& p) const override;
    bool isWater() const override;
    float getEdgeX() const override;
    float getEdgeY() const override;
    size_t getTileCountX() const override;
    size_t getTileCountY() const override;
    /// @}

    bool isInsidePitBounds(float x, float y) const override;

    bool setTileType(Index1D tileIndex, uint16_t tileType) override;

    bool tryGetTileTypeAtPosition(const Ego::Vector2f& position, uint16_t& tileType) const override;

    bool setTileTypeAtPosition(const Ego::Vector2f& position, uint16_t tileType) override;

    /**
    * Porting hack, TODO: remove
    **/
    std::shared_ptr<ego_mesh_t> getMeshPointer() { return _mesh; }
    std::shared_ptr<ego_mesh_t> mesh() override { return getMeshPointer(); }

    /**
     * @brief
     *  Spawn an Object into the game.
     * @return
     *  The reference of the object that was spawned or ObjectRef::Invalid on failure.
     */
     ObjectRef spawnObjectRef(const Ego::Vector3f& pos, ObjectProfileRef profile, const TEAM_REF team, const int skin,
                              const Facing& facing, const std::string& name, const ObjectRef override) override;

     std::shared_ptr<const Ego::Texture> getTileTexture(const size_t index);
     std::shared_ptr<const Ego::Texture> tileTexture(size_t index) override { return getTileTexture(index); }
     std::shared_ptr<const Ego::Texture> getWaterTexture(const uint8_t layer);
     std::shared_ptr<const Ego::Texture> waterTexture(uint8_t layer) override { return getWaterTexture(layer); }

     water_instance_t& getWater();
     water_instance_t& water() override { return getWater(); }
     WeatherState& getWeatherState() { return _weatherState; }
     WeatherState& weatherState() override { return getWeatherState(); }
     fog_instance_t& getFog() { return _fog; }
     fog_instance_t& fog() override { return getFog(); }
     AnimatedTilesState& getAnimatedTilesState() { return _animatedTilesState; }
     AnimatedTilesState& animatedTilesState() override { return getAnimatedTilesState(); }

    std::shared_ptr<Ego::Player>& getPlayer(size_t index);
    std::shared_ptr<Ego::Player> tryGetPlayer(size_t index) override;
    std::shared_ptr<const Ego::Player> tryGetPlayer(size_t index) const;

    const std::vector<std::shared_ptr<Ego::Player>>& getPlayerList() const;

    bool addPlayer(ObjectRef objectRef, const Ego::Input::InputDevice& device);

    /**
    * @brief
    *   Enables that falling into a pit will instantly kill characters.
    *   This is mutual exclusive with setPitsTeleport() and will disable
    *   teleporting in pits.
    **/
    void enablePitsKill() override;

    /**
    * @brief
    *   Enables that falling into a pit will teleport you back to a specified
    *   location. This is mutual exclusive to setPitsKill() and will disable
    *   killing in pits.
    **/
    void enablePitsTeleport(const Ego::Vector3f &location) override;

    /**
    * @brief
    *   This function sets up character data, loaded from "SPAWN.TXT"
    **/
    void spawnAllObjects();

    /// @details This function does several iterations of character movements and such
    ///    to keep the game in sync.
    void update() override;

private:
    bool addPlayer(ObjectRef objectRef,
                   const Ego::Input::InputDevice& device,
                   bool identifySpawnOnSuccess);

    void updateModuleServices();
    void updateModuleSimulation();
    void finalizeModuleUpdate();

    /// @author ZF
    /// @details This function checks all passages if there is a player in it, if it is, it plays a specified
    /// song set in by the AI script functions
    void checkPassageMusic();

    /**
    * @brief
    *   Update all active objects in the module
    **/
    void updateAllObjects();

    /**
    * @brief
    *   This function kills any character in a deep pit...
    **/
    void updatePits();

    /**
    * @brief
    *   This makes tiles flagged as damage tiles hurt any characters standing on 
    *   top of them
    **/
    void updateDamageTiles();

    /**
    * @brief
    *   This function sets all of the character's starting tilt values
    **/
    void tiltCharactersToTerrain();

    /**
    * @brief
    *   Spawns and setup a object from a spawn.txt entry
    **/
    ObjectRef spawnObjectFromFileEntry(const spawn_file_info_t& psp_info, ObjectRef parentRef);

private:
    static constexpr uint32_t PIT_CLOCK_RATE = 20;  ///< How many game ticks between each pit check
    static constexpr uint32_t DAMAGETILETIME = 32;  ///< Invincibility time

    GameModuleRuntime _runtime;
    bool _runtimeShutdown;
    const std::shared_ptr<ModuleProfile> _moduleProfile;
    std::vector<std::shared_ptr<Passage>> _passages;    ///< All passages in this module
    std::vector<Team> _teamList;
    ObjectHandler _gameObjects;
    std::list<std::string> _playerNameList;     ///< List of all import players
    std::vector<std::shared_ptr<Ego::Player>> _playerList;

    std::string  _name;                       ///< Module load names
    bool _exportValid;                          ///< Allow to export when module is reset?
    bool  _exportReset;                       ///< Remember original export mode if the module is restarted
    bool _isRespawnValid;                      ///< Can players respawn with Spacebar?
    bool _isBeaten;                               ///< Have the players won?
    uint32_t  _seed;                          ///< The module seed

    // special terrain and wawalite-related data structs
    water_instance_t _water;
    damagetile_instance_t _damageTile;
    WeatherState _weatherState;
    fog_instance_t _fog;
    AnimatedTilesState _animatedTilesState;

	/// @brief The mesh of the module.
	std::shared_ptr<ego_mesh_t> _mesh;

    std::array<Ego::DeferredTexture, 4> _tileTextures;
    std::array<Ego::DeferredTexture, 2> _waterTextures;

    //Pit Info
    uint32_t _pitsClock;
    bool _pitsKill;              ///< Do they kill?
    bool _pitsTeleport;          ///< Do they teleport?
    Ego::Vector3f _pitsTeleportPos;   ///< If they teleport, then where to?
};
