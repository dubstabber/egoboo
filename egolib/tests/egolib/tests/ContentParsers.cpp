/// @file ContentParsers.cpp
/// @brief Characterization tests for legacy content parsers.
///
/// These tests exercise the spawn.txt, menu.txt (ModuleProfile), and
/// wawalite.txt parsers against the shipped test.mod data.  They verify
/// that the parsers produce expected field values for a known-good module
/// so that future refactoring work does not silently change parse results.
///
/// The tests require a VFS bootstrap and the real data/ directory, similar
/// to the content validator tool.

#include "gtest/gtest.h"

#include "egolib/FileFormats/SpawnFile/spawn_file.h"
#include "egolib/FileFormats/SpawnFile/SpawnFileReaderImpl.hpp"
#include "egolib/FileFormats/wawalite_file.h"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/Image/ImageManager.hpp"
#include "egolib/Logic/PerkHandler.hpp"
#include "egolib/egoboo_setup.h"
#include "egolib/vfs.h"

#include <cstdlib>
#include <memory>

// ---------------------------------------------------------------------------
// Shared test fixture that bootstraps VFS and minimal runtime services.
// ---------------------------------------------------------------------------

class ContentParserFixture : public ::testing::Test
{
protected:
    static std::unique_ptr<ContentRuntimeBootstrap> s_runtime;

    static void SetUpTestSuite()
    {
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

        // Discover modules so that test.mod is reachable.
        ProfileSystem::get().loadModuleProfiles();
    }

    static void TearDownTestSuite()
    {
        s_runtime.reset();
    }

    /// Helper: find the ModuleProfile for a given module directory name.
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

    /// Helper: mount a module's VFS paths so its content is reachable.
    static void mountModule(const ModuleProfile& mod)
    {
        setup_init_module_vfs_paths(mod.getPath());
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ContentParserFixture::s_runtime;

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
