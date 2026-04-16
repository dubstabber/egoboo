/// @file ModuleLoadSmoke.cpp
/// @brief Structural smoke tests for module loading.
///
/// These tests exercise the key data-loading steps of module initialization
/// against test.mod WITHOUT requiring SDL video, audio, or OpenGL.  They
/// mirror the loading sequence in GameModule's constructor:
///   1. VFS mount
///   2. Required file presence
///   3. Wawalite parsing
///   4. Mesh loading
///   5. Spawn-entry parsing
///   6. Local object profile loading (lightweight)
///   7. Spawn-referenced object resolution
///
/// Full interactive GameModule coverage is still not feasible here because it
/// depends on AudioSystem, OpenGL textures, camera state, and the input
/// system. These tests stay at the non-interactive bootstrap/load boundary.
/// That deeper coupling is still a known architectural issue tracked in
/// Phase C of the refactoring plan (document 19).

#include "gtest/gtest.h"

#include "TestEnvironment.hpp"
#include "egolib/FileFormats/map_file.h"
#include "egolib/FileFormats/SpawnFile/spawn_file.h"
#include "egolib/FileFormats/SpawnFile/SpawnFileReaderImpl.hpp"
#include "egolib/FileFormats/wawalite_file.h"
#include "egolib/Logic/TreasureTables.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Module/module_spawn.h"
#include "egolib/game/game.h"
#include "egolib/Image/ImageManager.hpp"
#include "egolib/Logic/PerkHandler.hpp"
#include "egolib/egoboo_setup.h"
#include "egolib/vfs.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include <unordered_set>

// ---------------------------------------------------------------------------
// Shared test fixture that bootstraps VFS and minimal runtime services.
// ---------------------------------------------------------------------------

class ModuleLoadSmokeFixture : public ::testing::Test
{
protected:
    static std::unique_ptr<ContentRuntimeBootstrap> s_runtime;

    static void SetUpTestSuite()
    {
        Ego::Test::configureDataDirectory();

        ContentRuntimeBootstrap::Options opts;
        opts.initializeVirtualFileSystem   = true;
        opts.initializeBaseVfsPaths        = true;
        opts.initializeLogging             = true;
        opts.configureLightweightProfileLoading = true;
        opts.initializeImageManager        = true;
        opts.initializePerkHandler         = true;
        opts.initializeProfileSystem       = true;
        opts.clearModuleVfsPathsOnShutdown = true;
        opts.clearBaseVfsPathsOnShutdown   = true;
        opts.seedRandom     = true;
        opts.randomSeed     = 42;
        opts.binaryPath     = "";
        opts.logPath        = "/debug/module-load-smoke.log";
        opts.logLevel       = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);

        ProfileSystem::get().loadModuleProfiles();
    }

    static void TearDownTestSuite()
    {
        s_runtime.reset();
    }

    static std::shared_ptr<ModuleProfile> findModule(const std::string& dirName)
    {
        for (const auto& mod : ProfileSystem::get().getModuleProfiles())
        {
            if (mod && mod->getFolderName() == dirName)
            {
                return mod;
            }
        }
        return nullptr;
    }

    static void mountModule(const ModuleProfile& mod)
    {
        setup_init_module_vfs_paths(mod.getPath());
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ModuleLoadSmokeFixture::s_runtime;

// ===========================================================================
//  Structural module load smoke tests for test.mod
// ===========================================================================

TEST_F(ModuleLoadSmokeFixture, RequiredFilesExist)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    EXPECT_TRUE(vfs_exists("mp_data/menu.txt"));
    EXPECT_TRUE(vfs_exists("mp_data/spawn.txt"));
    EXPECT_TRUE(vfs_exists("mp_data/level.mpd"));
    EXPECT_TRUE(vfs_exists("mp_data/wawalite.txt"));
}

TEST_F(ModuleLoadSmokeFixture, MeshLoadsSuccessfully)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    map_t map;
    ASSERT_TRUE(map.load("mp_data/level.mpd"));

    EXPECT_GT(map._info.getTileCountX(), 0u);
    EXPECT_GT(map._info.getTileCountY(), 0u);
    EXPECT_GT(map._info.getVertexCount(), 0u);
}

TEST_F(ModuleLoadSmokeFixture, WawaliteLoadsSuccessfully)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    wawalite_data_t data;
    auto result = wawalite_data_read("mp_data/wawalite.txt", &data);
    EXPECT_NE(result, nullptr);
}

TEST_F(ModuleLoadSmokeFixture, SpawnEntriesParseSuccessfully)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    SpawnFileReaderImpl reader;
    auto entries = reader.read("mp_data/spawn.txt");

    EXPECT_EQ(entries.size(), 45u);
}

TEST_F(ModuleLoadSmokeFixture, LocalObjectProfilesLoadInLightweightMode)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    // Enumerate local objects under the module.
    size_t localObjectCount = 0;
    size_t loadSuccessCount = 0;

    SearchContext ctx(Ego::VfsPath(mod->getPath() + "/objects"),
                      Ego::Extension("obj"),
                      VFS_SEARCH_DIR);
    while (ctx.hasData())
    {
        ++localObjectCount;
        std::string objDir = ctx.getData().string();

        // Remap to the mounted mp_objects path.
        std::string objName = objDir.substr(objDir.find_last_of('/') + 1);
        std::string virtualPath = "mp_objects/" + objName;

        auto profile = ObjectProfile::loadFromFile(virtualPath, ObjectProfileRef(1), true);
        if (profile)
        {
            ++loadSuccessCount;
        }

        ctx.nextData();
    }

    // test.mod has at least follower.obj.
    EXPECT_GE(localObjectCount, 1u);
    EXPECT_EQ(loadSuccessCount, localObjectCount)
        << "not all local object profiles loaded successfully";
}

TEST_F(ModuleLoadSmokeFixture, SpawnReferencedObjectsResolveAgainstMpObjects)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    SpawnFileReaderImpl reader;
    auto entries = reader.read("mp_data/spawn.txt");

    Ego::TreasureTables treasureTables("mp_data/randomtreasure.txt");

    size_t resolvable = 0;
    size_t nonImportEntries = 0;
    std::unordered_set<std::string> checkedNames;

    for (auto& entry : entries)
    {
        // Skip import-slot entries (player characters loaded from disk).
        if (entry.slot >= 0 && entry.slot <= static_cast<int>(mod->getImportAmount()) * MAX_IMPORT_PER_PLAYER)
        {
            continue;
        }

        convert_spawn_file_load_name(entry, treasureTables);

        if (entry.spawn_comment.empty())
        {
            continue;
        }

        if (checkedNames.count(entry.spawn_comment))
        {
            ++nonImportEntries;
            // Already checked this name — count as resolvable if it was last time.
            if (vfs_exists(("mp_objects/" + entry.spawn_comment + "/data.txt").c_str()))
            {
                ++resolvable;
            }
            continue;
        }
        checkedNames.insert(entry.spawn_comment);
        ++nonImportEntries;

        std::string dataPath = "mp_objects/" + entry.spawn_comment + "/data.txt";
        if (vfs_exists(dataPath.c_str()))
        {
            ++resolvable;
        }
    }

    // test.mod is a "passing" module in the validator baseline — all spawn
    // references should resolve.  Allow zero failures.
    EXPECT_EQ(resolvable, nonImportEntries)
        << "some spawn-referenced objects could not be found under mp_objects";
}

TEST_F(ModuleLoadSmokeFixture, SpawnEntriesReserveImportSlotsBeforeConcreteProfiles)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    SpawnFileReaderImpl reader;
    auto entries = reader.read("mp_data/spawn.txt");

    const int importSlotLimit = static_cast<int>(mod->getImportAmount()) * MAX_IMPORT_PER_PLAYER;
    size_t importEntries = 0;
    size_t concreteEntries = 0;
    int maxImportSlot = -1;
    int minConcreteSlot = std::numeric_limits<int>::max();

    for (const auto& entry : entries)
    {
        if (entry.slot >= 0 && entry.slot < importSlotLimit)
        {
            ++importEntries;
            maxImportSlot = std::max(maxImportSlot, entry.slot);
            continue;
        }

        if (entry.slot > importSlotLimit)
        {
            ++concreteEntries;
            minConcreteSlot = std::min(minConcreteSlot, entry.slot);
        }
    }

    auto firstConcrete = std::find_if(entries.begin(), entries.end(),
        [importSlotLimit](const auto& entry)
        {
            return entry.slot > importSlotLimit;
        });

    ASSERT_NE(firstConcrete, entries.end());
    EXPECT_EQ(importEntries, static_cast<size_t>(importSlotLimit));
    EXPECT_EQ(maxImportSlot, importSlotLimit - 1);
    EXPECT_GT(concreteEntries, 0u);
    EXPECT_EQ(minConcreteSlot, importSlotLimit + 1);
    EXPECT_EQ(firstConcrete->slot, importSlotLimit + 1);
    EXPECT_EQ(firstConcrete->spawn_comment, "Follower");
}

TEST_F(ModuleLoadSmokeFixture, SpawnResolutionProducesExpectedUniqueConcreteObjects)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    SpawnFileReaderImpl reader;
    auto entries = reader.read("mp_data/spawn.txt");
    Ego::TreasureTables treasureTables("mp_data/randomtreasure.txt");

    const int importSlotLimit = static_cast<int>(mod->getImportAmount()) * MAX_IMPORT_PER_PLAYER;
    std::unordered_set<std::string> resolvedObjects;

    for (auto& entry : entries)
    {
        if (entry.slot >= 0 && entry.slot <= importSlotLimit)
        {
            continue;
        }

        convert_spawn_file_load_name(entry, treasureTables);
        if (!entry.spawn_comment.empty())
        {
            resolvedObjects.insert(entry.spawn_comment);
        }
    }

    const std::unordered_set<std::string> expectedObjects = {
        "bullwolf.obj",
        "bumper.obj",
        "choco.obj",
        "follower.obj",
        "hellrover.obj",
        "hotdog.obj",
        "mouse.obj",
        "strider.obj",
        "tinker_plane.obj",
    };

    EXPECT_EQ(resolvedObjects, expectedObjects);
}

TEST_F(ModuleLoadSmokeFixture, EnvironmentUploadsMatchParsedWawaliteState)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    wawalite_data_t expected;
    ASSERT_NE(wawalite_data_read("mp_data/wawalite.txt", &expected), nullptr);
    ASSERT_TRUE(wawalite_finalize(&expected));

    water_instance_t water;
    WeatherState weather;
    fog_instance_t fog;
    AnimatedTilesState animatedTiles;

    water.upload(expected.water);
    weather.upload(expected.weather);
    fog.upload(expected.fog);
    animatedTiles.upload(expected.animtile);

    EXPECT_EQ(water._layer_count, expected.water.layer_count);
    EXPECT_FLOAT_EQ(water._surface_level, expected.water.surface_level);
    EXPECT_FLOAT_EQ(water._douse_level, expected.water.douse_level);
    EXPECT_EQ(water._layers[0]._frame_add, expected.water.layer[0].frame_add);
    EXPECT_EQ(water._layers[1]._frame_add, expected.water.layer[1].frame_add);

    EXPECT_EQ(weather.timer_reset, expected.weather.timer_reset);
    EXPECT_EQ(weather.time, expected.weather.timer_reset);
    EXPECT_EQ(weather.over_water, expected.weather.over_water);
    EXPECT_EQ(weather.part_gpip, expected.weather.part_gpip);

    EXPECT_FLOAT_EQ(fog._top, expected.fog.top);
    EXPECT_FLOAT_EQ(fog._bottom, expected.fog.bottom);
    EXPECT_EQ(fog._red, expected.fog.red);
    EXPECT_EQ(fog._grn, expected.fog.grn);
    EXPECT_EQ(fog._blu, expected.fog.blu);

    EXPECT_EQ(animatedTiles.elements[0].update_and, static_cast<int>(expected.animtile.update_and));
    EXPECT_EQ(animatedTiles.elements[0].frame_and, expected.animtile.frame_and);
    EXPECT_EQ(animatedTiles.elements[1].frame_and,
              static_cast<uint16_t>((expected.animtile.frame_and << 1) | 1));
}

TEST_F(ModuleLoadSmokeFixture, EndToEndLoadSequenceSucceeds)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    // Step 1: mesh
    map_t map;
    ASSERT_TRUE(map.load("mp_data/level.mpd"));

    // Step 2: wawalite
    wawalite_data_t wawa;
    ASSERT_NE(wawalite_data_read("mp_data/wawalite.txt", &wawa), nullptr);

    // Step 3: spawn entries
    auto entries = SpawnFileReaderImpl().read("mp_data/spawn.txt");
    ASSERT_FALSE(entries.empty());

    // Step 4: load local profiles (lightweight)
    size_t profilesLoaded = 0;
    SearchContext ctx(Ego::VfsPath(mod->getPath() + "/objects"),
                      Ego::Extension("obj"),
                      VFS_SEARCH_DIR);
    while (ctx.hasData())
    {
        std::string objDir = ctx.getData().string();
        std::string objName = objDir.substr(objDir.find_last_of('/') + 1);
        std::string virtualPath = "mp_objects/" + objName;

        auto profile = ObjectProfile::loadFromFile(virtualPath, ObjectProfileRef(1), true);
        if (profile)
        {
            ++profilesLoaded;
        }
        ctx.nextData();
    }
    EXPECT_GE(profilesLoaded, 1u);

    // Step 5: verify spawn reference resolution
    Ego::TreasureTables treasureTables("mp_data/randomtreasure.txt");
    size_t unresolvedCount = 0;
    for (auto& entry : entries)
    {
        if (entry.slot >= 0 && entry.slot <= static_cast<int>(mod->getImportAmount()) * MAX_IMPORT_PER_PLAYER)
        {
            continue;
        }
        convert_spawn_file_load_name(entry, treasureTables);
        if (!entry.spawn_comment.empty())
        {
            std::string dataPath = "mp_objects/" + entry.spawn_comment + "/data.txt";
            if (!vfs_exists(dataPath.c_str()))
            {
                ++unresolvedCount;
            }
        }
    }
    EXPECT_EQ(unresolvedCount, 0u) << "end-to-end: unresolved spawn references found";
}
