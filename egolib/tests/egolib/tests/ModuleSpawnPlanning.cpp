#include "gtest/gtest.h"
#include "egolib/game/Core/EngineContext.hpp"

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Module/Module_spawn_plan.hpp"
#include "egolib/game/Module/module_spawn.h"
#include "egolib/vfs.h"

#include <cstdlib>
#include <memory>

namespace
{

constexpr int FIRST_DYNAMIC_PROFILE_SLOT = 1 + MAX_IMPORT_PER_PLAYER * MAX_PLAYER;

class ModuleSpawnPlanningFixture : public ::testing::Test
{
protected:
    static std::unique_ptr<ContentRuntimeBootstrap> s_runtime;

    static void SetUpTestSuite()
    {
        Ego::Test::configureDataDirectory();

        ContentRuntimeBootstrap::Options opts;
        opts.initializeVirtualFileSystem = true;
        opts.initializeBaseVfsPaths = true;
        opts.initializeLogging = true;
        opts.configureLightweightProfileLoading = true;
        opts.initializeImageManager = true;
        opts.initializePerkHandler = true;
        opts.initializeProfileSystem = true;
        opts.clearModuleVfsPathsOnShutdown = true;
        opts.clearBaseVfsPathsOnShutdown = true;
        opts.seedRandom = true;
        opts.randomSeed = 7;
        opts.binaryPath = "";
        opts.logPath = "/debug/module-spawn-planning-tests.log";
        opts.logLevel = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);

        setenv("EGOBOO_DISABLE_AUDIO", "1", 1);
        AudioSystem::initialize();
        EngineContext::get().installAudioSystem(AudioSystem::get());
    }

    static void TearDownTestSuite()
    {
        EngineContext::get().clearAudioSystem();
        AudioSystem::uninitialize();
        s_runtime.reset();
    }

    void SetUp() override
    {
        EngineContext::get().profileSystem().reset();
        EngineContext::get().profileSystem().loadModuleProfiles();
    }

    std::shared_ptr<ModuleProfile> mountTestModule()
    {
        for (const auto& module : EngineContext::get().profileSystem().getModuleProfiles())
        {
            if (module && module->getFolderName() == "test.mod")
            {
                setup_init_module_vfs_paths(module->getPath());
                return module;
            }
        }
        return nullptr;
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ModuleSpawnPlanningFixture::s_runtime;

spawn_file_info_t makeSpawnEntry(int slot, const std::string& spawnComment)
{
    spawn_file_info_t entry;
    entry.slot = slot;
    entry.spawn_comment = spawnComment;
    return entry;
}

} // namespace

TEST_F(ModuleSpawnPlanningFixture, DynamicEntriesReuseReservedSlotForRepeatedNames)
{
    auto module = mountTestModule();
    ASSERT_NE(module, nullptr);

    Ego::TreasureTables treasureTables("mp_data/randomtreasure.txt");
    std::vector<spawn_file_info_t> entries = {
        makeSpawnEntry(-1, "Follower"),
        makeSpawnEntry(-1, "Follower"),
        makeSpawnEntry(51, "Mouse"),
    };

    const auto plan = module_spawn_plan::buildSpawnPlan(
        entries,
        treasureTables,
        [](ObjectProfileRef) { return false; });

    ASSERT_EQ(plan.entries.size(), 3u);
    EXPECT_EQ(plan.entries[0].spawn_comment, "follower.obj");
    EXPECT_EQ(plan.entries[1].spawn_comment, "follower.obj");
    EXPECT_EQ(plan.entries[2].spawn_comment, "mouse.obj");
    EXPECT_EQ(plan.entries[0].slot, plan.entries[1].slot);
    EXPECT_EQ(plan.entries[0].slot, FIRST_DYNAMIC_PROFILE_SLOT);
    EXPECT_EQ(plan.entries[2].slot, 51);
}

TEST_F(ModuleSpawnPlanningFixture, DynamicAllocationSkipsLoadedSlots)
{
    auto module = mountTestModule();
    ASSERT_NE(module, nullptr);

    Ego::TreasureTables treasureTables("mp_data/randomtreasure.txt");
    std::vector<spawn_file_info_t> entries = {
        makeSpawnEntry(-1, "Bumper"),
    };

    const auto plan = module_spawn_plan::buildSpawnPlan(
        entries,
        treasureTables,
        [](ObjectProfileRef slot)
        {
            return slot == ObjectProfileRef(FIRST_DYNAMIC_PROFILE_SLOT);
        });

    ASSERT_EQ(plan.entries.size(), 1u);
    EXPECT_EQ(plan.entries[0].spawn_comment, "bumper.obj");
    EXPECT_EQ(plan.entries[0].slot, FIRST_DYNAMIC_PROFILE_SLOT + 1);
}

TEST_F(ModuleSpawnPlanningFixture, PlannedEntriesPreserveOriginalSpawnOrder)
{
    auto module = mountTestModule();
    ASSERT_NE(module, nullptr);

    Ego::TreasureTables treasureTables("mp_data/randomtreasure.txt");
    std::vector<spawn_file_info_t> entries = {
        makeSpawnEntry(77, "Mouse"),
        makeSpawnEntry(-1, "Follower"),
        makeSpawnEntry(78, "Bumper"),
    };

    const auto plan = module_spawn_plan::buildSpawnPlan(
        entries,
        treasureTables,
        [](ObjectProfileRef) { return false; });

    ASSERT_EQ(plan.entries.size(), 3u);
    EXPECT_EQ(plan.entries[0].spawn_comment, "mouse.obj");
    EXPECT_EQ(plan.entries[1].spawn_comment, "follower.obj");
    EXPECT_EQ(plan.entries[2].spawn_comment, "bumper.obj");
}

TEST_F(ModuleSpawnPlanningFixture, ActivateSpawnLoadObjectLoadsMountedProfileIntoRequestedSlot)
{
    auto module = mountTestModule();
    ASSERT_NE(module, nullptr);

    spawn_file_info_t entry = makeSpawnEntry(90, "follower.obj");
    ASSERT_FALSE(EngineContext::get().profileSystem().isLoaded(ObjectProfileRef(90)));

    EXPECT_TRUE(activate_spawn_file_load_object(entry));
    EXPECT_TRUE(EngineContext::get().profileSystem().isLoaded(ObjectProfileRef(90)));

    const auto& profile = EngineContext::get().profileSystem().getProfile(ObjectProfileRef(90));
    ASSERT_NE(profile, nullptr);
    EXPECT_EQ(profile->getPathname(), "mp_objects/follower.obj");
}

TEST_F(ModuleSpawnPlanningFixture, ActivateSpawnLoadObjectReturnsFalseWhenProfileIsMissing)
{
    auto module = mountTestModule();
    ASSERT_NE(module, nullptr);

    spawn_file_info_t entry = makeSpawnEntry(91, "missing-profile.obj");

    EXPECT_FALSE(activate_spawn_file_load_object(entry));
    EXPECT_FALSE(EngineContext::get().profileSystem().isLoaded(ObjectProfileRef(91)));
    EXPECT_EQ(entry.slot, 91);
}

TEST_F(ModuleSpawnPlanningFixture, ActivateSpawnLoadObjectIsNoOpWhenSlotIsAlreadyLoaded)
{
    auto module = mountTestModule();
    ASSERT_NE(module, nullptr);

    ASSERT_EQ(EngineContext::get().profileSystem().loadOneProfile("mp_objects/follower.obj", 92), ObjectProfileRef(92));

    spawn_file_info_t entry = makeSpawnEntry(92, "bumper.obj");

    EXPECT_FALSE(activate_spawn_file_load_object(entry));

    const auto& profile = EngineContext::get().profileSystem().getProfile(ObjectProfileRef(92));
    ASSERT_NE(profile, nullptr);
    EXPECT_EQ(profile->getPathname(), "mp_objects/follower.obj");
}
