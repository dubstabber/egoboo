#include "gtest/gtest.h"
#include "egolib/game/Core/EngineContext.hpp"

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/GameStates/LoadPlayerElement.hpp"
#include "egolib/vfs.h"

#include <cstdlib>
#include <memory>

namespace
{

constexpr char kQuestTestRoot[] = "quest-tests";

class LoadPlayerElementFixture : public ::testing::Test
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
        opts.randomSeed = 17;
        opts.binaryPath = "";
        opts.logPath = "/debug/load-player-element-tests.log";
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
        vfs_removeDirectoryAndContents(kQuestTestRoot);
        EngineContext::get().profileSystem().reset();
        EngineContext::get().profileSystem().loadModuleProfiles();
        setup_init_module_vfs_paths("mp_modules/test.mod");
    }

    void TearDown() override
    {
        vfs_removeDirectoryAndContents(kQuestTestRoot);
        setup_clear_module_vfs_paths();
    }

    std::shared_ptr<ObjectProfile> loadIsolatedFollowerProfile(const std::string& profilePath, int slot) const
    {
        vfs_removeDirectoryAndContents(profilePath.c_str());
        EXPECT_TRUE(vfs_copyDirectory("mp_objects/follower.obj", profilePath.c_str()));
        EXPECT_TRUE(vfs_exists((profilePath + "/data.txt").c_str()));
        EXPECT_TRUE(vfs_exists((profilePath + "/naming.txt").c_str()));

        const ObjectProfileRef profileRef = EngineContext::get().profileSystem().loadOneProfile(profilePath, slot);
        EXPECT_NE(profileRef, ObjectProfileRef::Invalid);
        if (profileRef == ObjectProfileRef::Invalid)
        {
            return nullptr;
        }

        return EngineContext::get().profileSystem().getProfile(profileRef);
    }
};

std::unique_ptr<ContentRuntimeBootstrap> LoadPlayerElementFixture::s_runtime;

TEST_F(LoadPlayerElementFixture, ConstructorKeepsMissingQuestLoadSilent)
{
    const std::string profilePath = std::string(kQuestTestRoot) + "/load-player-element-missing.obj";
    auto profile = loadIsolatedFollowerProfile(profilePath, 123);
    ASSERT_NE(profile, nullptr);

    EXPECT_FALSE(vfs_exists((profilePath + "/quest.txt").c_str()));

    LoadPlayerElement element(profile);

    EXPECT_FALSE(element.hasQuest(IDSZ2('T', 'E', 'S', 'T'), 0));
    EXPECT_EQ(element.getProfile()->getPathname(), profilePath);
    EXPECT_NE(element.getName(), std::string());
}

} // namespace
