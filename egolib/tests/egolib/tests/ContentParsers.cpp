/// @file ContentParsers.cpp
/// @brief Characterization tests for legacy content parsers.
///
/// These tests exercise the spawn.txt, menu.txt (ModuleProfile),
/// wawalite.txt, data.txt (ObjectProfile), and level.mpd parsers against the
/// shipped test.mod data.  They verify that the parsers produce expected field
/// values for a known-good module so that future refactoring work does not
/// silently change parse results.
///
/// The tests require a VFS bootstrap and the real data/ directory, similar
/// to the content validator tool.

#include "gtest/gtest.h"
#include "egolib/game/Core/EngineContext.hpp"

#include "TestEnvironment.hpp"
#include "egolib/Audio/IAudioSystem.hpp"
#include "egolib/FileFormats/map_file.h"
#include "egolib/FileFormats/SpawnFile/spawn_file.h"
#include "egolib/FileFormats/SpawnFile/SpawnFileReaderImpl.hpp"
#include "egolib/FileFormats/wawalite_file.h"
#include "egolib/Graphics/AnimatedModel.hpp"
#include "egolib/Graphics/ModelDescriptor.hpp"
#include "egolib/Graphics/ObjectModelAsset.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/Image/ImageManager.hpp"
#include "egolib/Logic/PerkHandler.hpp"
#include "egolib/egoboo_setup.h"
#include "egolib/vfs.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

namespace
{
class RecordingAudioSystem : public IAudioSystem
{
public:
    SoundID loadSound(const std::string& fileName) override
    {
        loadedSoundPaths.push_back(fileName);
        return -1;
    }

    void playMusic(MusicID, uint16_t = 0) override {}
    void playMusic(const std::string&, uint16_t = 0) override {}
    void stopMusic() override {}
    void fadeAllSounds() override {}
    int playSound(const Ego::Vector3f&, SoundID) override { return 0; }
    void playSoundLooped(SoundID, ObjectRef) override {}
    size_t stopObjectLoopingSounds(ObjectRef, SoundID = -1) override { return 0; }
    int playSoundFull(SoundID) override { return 0; }
    SoundID getGlobalSound(GlobalSound) const override { return 0; }
    void setMaxHearingDistance(float) override {}
    void setMusicVolume(int) override {}
    void setSoundEffectVolume(int) override {}
    void update() override {}
    void loadGlobalSounds() override {}
    void loadAllMusic() override {}

    void reset()
    {
        loadedSoundPaths.clear();
    }

    std::vector<std::string> loadedSoundPaths;
};
}

// ---------------------------------------------------------------------------
// Shared test fixture that bootstraps VFS and minimal runtime services.
// ---------------------------------------------------------------------------

class ContentParserFixture : public ::testing::Test
{
protected:
    static std::unique_ptr<ContentRuntimeBootstrap> s_runtime;
    static std::unique_ptr<RecordingAudioSystem> s_audioSystem;

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
        opts.randomSeed     = 0;
        opts.binaryPath     = "";
        opts.logPath        = "/debug/content-parser-tests.log";
        opts.logLevel       = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);
        s_audioSystem = std::make_unique<RecordingAudioSystem>();
        EngineContext::get().installAudioSystem(*s_audioSystem);

        // Discover modules so that test.mod is reachable.
        EngineContext::get().profileSystem().loadModuleProfiles();
    }

    static void TearDownTestSuite()
    {
        EngineContext::get().clearAudioSystem();
        s_audioSystem.reset();
        s_runtime.reset();
    }

    /// Helper: find the ModuleProfile for a given module directory name.
    static std::shared_ptr<ModuleProfile> findModule(const std::string& dirName)
    {
        for (const auto& mod : EngineContext::get().profileSystem().getModuleProfiles())
        {
            if (mod && mod->getFolderName() == dirName)
            {
                return mod;
            }
        }
        return nullptr;
    }

    /// Helper: mount a module's VFS paths so its content is reachable.
    static void mountModule(const ModuleProfile& mod)
    {
        setup_init_module_vfs_paths(mod.getPath());
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ContentParserFixture::s_runtime;
std::unique_ptr<RecordingAudioSystem> ContentParserFixture::s_audioSystem;

// ===========================================================================
//  spawn.txt parser tests
// ===========================================================================

class SpawnParserTest : public ContentParserFixture {};

TEST_F(SpawnParserTest, TestModSpawnEntryCount)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr) << "test.mod not found in module profiles";
    mountModule(*mod);

    SpawnFileReaderImpl reader;
    auto entries = reader.read("mp_data/spawn.txt");

    // test.mod/gamedat/spawn.txt has 45 spawn entries (known from validator).
    EXPECT_EQ(entries.size(), 45u);
}

TEST_F(SpawnParserTest, TestModFirstEntryIsPlayer1)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    SpawnFileReaderImpl reader;
    auto entries = reader.read("mp_data/spawn.txt");
    ASSERT_GE(entries.size(), 1u);

    const auto& first = entries[0];
    EXPECT_EQ(first.spawn_comment, "Player1");
    EXPECT_EQ(first.slot, 0);
    EXPECT_TRUE(first.stat);
}

TEST_F(SpawnParserTest, TestModFirstEntryPosition)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    SpawnFileReaderImpl reader;
    auto entries = reader.read("mp_data/spawn.txt");
    ASSERT_GE(entries.size(), 1u);

    const auto& first = entries[0];
    // Positions in spawn.txt are in tile units; the parser may multiply by
    // GRID_FSIZE (128.0).  We verify the raw parsed values match expectations.
    // The file says 37.6 37.6 0.0 for xpos ypos zpos.
    EXPECT_NEAR(first.pos[kX], 37.6f * 128.0f, 1.0f);
    EXPECT_NEAR(first.pos[kY], 37.6f * 128.0f, 1.0f);
    EXPECT_NEAR(first.pos[kZ], 0.0f, 0.1f);
}

TEST_F(SpawnParserTest, TestModAttachmentTypes)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    SpawnFileReaderImpl reader;
    auto entries = reader.read("mp_data/spawn.txt");
    ASSERT_GE(entries.size(), 5u);

    // Entry 0 is the Player1 base — no attachment.
    EXPECT_EQ(entries[0].attach, ATTACH_NONE);
    // Entry 1: "L" = ATTACH_LEFT
    EXPECT_EQ(entries[1].attach, ATTACH_LEFT);
    // Entry 2: "R" = ATTACH_RIGHT
    EXPECT_EQ(entries[2].attach, ATTACH_RIGHT);
    // Entry 3: "I" = ATTACH_INVENTORY
    EXPECT_EQ(entries[3].attach, ATTACH_INVENTORY);
}

// ===========================================================================
//  menu.txt / ModuleProfile parser tests
// ===========================================================================

class ModuleProfileParserTest : public ContentParserFixture {};

TEST_F(ModuleProfileParserTest, TestModProfileExists)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr) << "test.mod not found in module profiles";
}

TEST_F(ModuleProfileParserTest, TestModProfileName)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(mod->getName(), "Test Module");
}

TEST_F(ModuleProfileParserTest, TestModProfileImports)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    // menu.txt says "Number of imports ( 0 to 4 ) :4"
    EXPECT_EQ(mod->getImportAmount(), 4);
}

TEST_F(ModuleProfileParserTest, TestModProfileExporting)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    // "Exporting ( True or False ) :True"
    EXPECT_TRUE(mod->isExportAllowed());
}

TEST_F(ModuleProfileParserTest, TestModProfileMaxPlayers)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    // test.mod supports up to 4 players.
    EXPECT_GE(mod->getMaxPlayers(), 1);
    EXPECT_LE(mod->getMaxPlayers(), 4);
}

// ===========================================================================
//  wawalite.txt parser tests
// ===========================================================================

class WawaliteParserTest : public ContentParserFixture {};

TEST_F(WawaliteParserTest, TestModWawaliteLoads)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    wawalite_data_t data;
    auto result = wawalite_data_read("mp_data/wawalite.txt", &data);
    EXPECT_NE(result, nullptr);
}

TEST_F(WawaliteParserTest, TestModWawaliteGravity)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    wawalite_data_t data;
    auto result = wawalite_data_read("mp_data/wawalite.txt", &data);
    ASSERT_NE(result, nullptr);

    // Physics gravity should be a reasonable value (not zero, not absurd).
    EXPECT_GT(data.phys.gravity, -20.0f);
    EXPECT_LT(data.phys.gravity, 0.0f);
}

// ===========================================================================
//  data.txt / ObjectProfile parser tests
// ===========================================================================

class ObjectProfileParserTest : public ContentParserFixture {};

TEST_F(ObjectProfileParserTest, TestModFollowerProfileLoads)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    // Load follower.obj in lightweight mode (no 3D model, sounds, particles).
    auto profile = ObjectProfile::loadFromFile(
        "mp_objects/follower.obj", ObjectProfileRef(37), true);
    ASSERT_NE(profile, nullptr) << "follower.obj failed to load";
}

TEST_F(ObjectProfileParserTest, TestModFollowerProfileLoadsNonLightweightWithInstalledAudioService)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    s_audioSystem->reset();
    auto profile = ObjectProfile::loadFromFile(
        "mp_objects/follower.obj", ObjectProfileRef(37), false);
    ASSERT_NE(profile, nullptr) << "follower.obj failed to load in full mode";
    ASSERT_NE(profile->getModel(), nullptr);
    ASSERT_NE(profile->getModel()->getModel(), nullptr);
    EXPECT_TRUE(profile->getModel()->isActionValid(ACTION_DA));
    EXPECT_GT(profile->getModel()->getModel()->getVertexCount(), 0u);
    EXPECT_FALSE(profile->getModel()->getModel()->getFrames().empty());
    EXPECT_FALSE(profile->getModel()->getModel()->getDrawCommands().empty());
    ASSERT_FALSE(s_audioSystem->loadedSoundPaths.empty());
    EXPECT_EQ(s_audioSystem->loadedSoundPaths.front(), "mp_objects/follower.obj/sound0");
}

TEST_F(ObjectProfileParserTest, ObjectModelResolverFindsCurrentMd2Fallback)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    const Ego::Graphics::ObjectModelAsset modelAsset =
        Ego::Graphics::resolveObjectModelAsset("mp_objects/follower.obj");
    ASSERT_TRUE(modelAsset.exists);
    EXPECT_EQ(modelAsset.format, Ego::Graphics::ObjectModelFormat::Md2);
    EXPECT_EQ(modelAsset.path, "mp_objects/follower.obj/tris.md2");
}

TEST_F(ObjectProfileParserTest, ObjectModelResolverPrefersGltfThenGlbThenMd2)
{
#ifdef _WIN32
    const int processId = _getpid();
#else
    const int processId = getpid();
#endif
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("egoboo-model-resolver-" + std::to_string(processId));
    const std::filesystem::path objectDir = root / "synthetic.obj";
    std::filesystem::create_directories(objectDir);
    Ego::Test::scheduleTestDirectoryCleanup(root);

    {
        std::ofstream(objectDir / "tris.md2").put('\0');
    }

    ASSERT_NE(0, vfs_add_mount_point(root.string(), Ego::FsPath(""), Ego::VfsPath("mp_modelresolver"), 1));

    const std::string objectPath = "mp_modelresolver/synthetic.obj";
    Ego::Graphics::ObjectModelAsset modelAsset = Ego::Graphics::resolveObjectModelAsset(objectPath);
    ASSERT_TRUE(modelAsset.exists);
    EXPECT_EQ(modelAsset.format, Ego::Graphics::ObjectModelFormat::Md2);

    {
        std::ofstream(objectDir / "tris.glb").put('\0');
    }
    modelAsset = Ego::Graphics::resolveObjectModelAsset(objectPath);
    ASSERT_TRUE(modelAsset.exists);
    EXPECT_EQ(modelAsset.format, Ego::Graphics::ObjectModelFormat::Glb);

    {
        std::ofstream(objectDir / "tris.gltf").put('\0');
    }
    modelAsset = Ego::Graphics::resolveObjectModelAsset(objectPath);
    ASSERT_TRUE(modelAsset.exists);
    EXPECT_EQ(modelAsset.format, Ego::Graphics::ObjectModelFormat::Gltf);

    const Ego::Graphics::ObjectModelAsset md2Fallback =
        Ego::Graphics::resolveObjectModelAsset(objectPath, Ego::Graphics::ObjectModelFormat::Md2);
    ASSERT_TRUE(md2Fallback.exists);
    EXPECT_EQ(md2Fallback.format, Ego::Graphics::ObjectModelFormat::Md2);

    vfs_remove_mount_point(Ego::VfsPath("mp_modelresolver"));
}

TEST_F(ObjectProfileParserTest, TestModFollowerClassName)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    auto profile = ObjectProfile::loadFromFile(
        "mp_objects/follower.obj", ObjectProfileRef(37), true);
    ASSERT_NE(profile, nullptr);

    // data.txt says "Healer" (first char capitalized by parser).
    EXPECT_EQ(profile->getClassName(), "Healer");
}

TEST_F(ObjectProfileParserTest, TestModFollowerGender)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    auto profile = ObjectProfile::loadFromFile(
        "mp_objects/follower.obj", ObjectProfileRef(37), true);
    ASSERT_NE(profile, nullptr);

    // data.txt says "FEMALE".
    EXPECT_EQ(profile->getGender(), GenderProfile::Female);
}

TEST_F(ObjectProfileParserTest, TestModFollowerPhysicalAttributes)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    auto profile = ObjectProfile::loadFromFile(
        "mp_objects/follower.obj", ObjectProfileRef(37), true);
    ASSERT_NE(profile, nullptr);

    // Size: 1.10
    EXPECT_NEAR(profile->getSize(), 1.10f, 0.01f);
    // Shadow size: 25
    EXPECT_NEAR(profile->getShadowSize(), 25.0f, 0.5f);
    // Bump height: 90
    EXPECT_NEAR(profile->getBumpHeight(), 90.0f, 0.5f);
    // Weight: 90
    EXPECT_EQ(profile->getWeight(), 90);
    // Jump power: 10.0
    EXPECT_NEAR(profile->getJumpPower(), 10.0f, 0.01f);
    // Max ammo: 0
    EXPECT_EQ(profile->getMaxAmmo(), 0);
}

TEST_F(ObjectProfileParserTest, TestModFollowerFlags)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    auto profile = ObjectProfile::loadFromFile(
        "mp_objects/follower.obj", ObjectProfileRef(37), true);
    ASSERT_NE(profile, nullptr);

    // data.txt flags for follower.obj:
    EXPECT_FALSE(profile->isItem());
    EXPECT_FALSE(profile->isMount());
    EXPECT_FALSE(profile->isStackable());
    EXPECT_FALSE(profile->isInvincible());
    EXPECT_FALSE(profile->isPlatform());
    EXPECT_TRUE(profile->isNameKnown());
    EXPECT_TRUE(profile->canGrabMoney());
}

TEST_F(ObjectProfileParserTest, GlobalIronBallTransferBlendingFlag)
{
    auto profile = ObjectProfile::loadFromFile(
        "mp_data/globalobjects/misc/ironball.obj", ObjectProfileRef(51), true);
    ASSERT_NE(profile, nullptr);

    EXPECT_FALSE(profile->transferBlending());
}

// ===========================================================================
//  level.mpd parser tests
// ===========================================================================

class MapFileParserTest : public ContentParserFixture {};

TEST_F(MapFileParserTest, TestModLevelMpdLoads)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    map_t map;
    bool loaded = map.load("mp_data/level.mpd");
    EXPECT_TRUE(loaded) << "level.mpd failed to parse";
}

TEST_F(MapFileParserTest, TestModLevelMpdDimensions)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    map_t map;
    ASSERT_TRUE(map.load("mp_data/level.mpd"));

    // The map must have non-zero tile dimensions.
    EXPECT_GT(map._info.getTileCountX(), 0u);
    EXPECT_GT(map._info.getTileCountY(), 0u);
    EXPECT_GT(map._info.getVertexCount(), 0u);

    // The total tile count should be tileX * tileY.
    EXPECT_EQ(map._info.getTileCount(),
              map._info.getTileCountX() * map._info.getTileCountY());
}

TEST_F(MapFileParserTest, TestModLevelMpdTileMemory)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    map_t map;
    ASSERT_TRUE(map.load("mp_data/level.mpd"));

    // After a successful load, tile and vertex vectors must match the info.
    EXPECT_EQ(map._mem.tiles.size(),
              static_cast<size_t>(map._info.getTileCount()));
    EXPECT_EQ(map._mem.vertices.size(),
              static_cast<size_t>(map._info.getVertexCount()));
}

TEST_F(MapFileParserTest, TestModLevelMpdSaneBounds)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    map_t map;
    ASSERT_TRUE(map.load("mp_data/level.mpd"));

    // Tile dimensions should be within engine limits.
    EXPECT_LE(map._info.getTileCountX(), MAP_TILE_MAX_X);
    EXPECT_LE(map._info.getTileCountY(), MAP_TILE_MAX_Y);
    EXPECT_LE(map._info.getVertexCount(), MAP_VERTICES_MAX);
}
