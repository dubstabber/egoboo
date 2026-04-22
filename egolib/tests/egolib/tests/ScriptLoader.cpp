#include "gtest/gtest.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

#include "TestEnvironment.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/Script/script.h"
#include "egolib/egoboo_setup.h"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/script_compile.h"
#include "egolib/vfs.h"

namespace
{

class ScriptLoaderFixture : public ::testing::Test
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
        opts.randomSeed = 71;
        opts.binaryPath = "";
        opts.logPath = "/debug/script-loader-tests.log";
        opts.logLevel = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);
    }

    static void TearDownTestSuite()
    {
        s_runtime.reset();
    }

    void SetUp() override
    {
        resetBaseVfsPaths();
        EngineContext::get().profileSystem().reset();
        EngineContext::get().profileSystem().loadModuleProfiles();
        ASSERT_TRUE(setup_init_module_vfs_paths("mp_modules/test.mod"));
    }

    void TearDown() override
    {
        setup_clear_module_vfs_paths();
        setup_clear_base_vfs_paths();
    }

    void resetBaseVfsPaths() const
    {
        setup_clear_module_vfs_paths();
        setup_clear_base_vfs_paths();
        setup_init_base_vfs_paths();
    }

    std::shared_ptr<ObjectProfile> loadFollowerProfile(int slot) const
    {
        const ObjectProfileRef profileRef = EngineContext::get().profileSystem().loadOneProfile("mp_objects/follower.obj", slot);
        EXPECT_NE(profileRef, ObjectProfileRef::Invalid);
        if (profileRef == ObjectProfileRef::Invalid)
        {
            return nullptr;
        }

        return EngineContext::get().profileSystem().getProfile(profileRef);
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ScriptLoaderFixture::s_runtime;

TEST_F(ScriptLoaderFixture, LoadsValidPrimaryScriptWithoutFallingBack)
{
    const auto profile = loadFollowerProfile(6101);
    ASSERT_NE(profile, nullptr);

    const std::string scriptPath = profile->getPathname() + "/script.txt";
    ASSERT_TRUE(vfs_exists(scriptPath));

    script_info_t script;
    parser_state_t& parser = parser_state_t::get();

    EXPECT_TRUE(load_ai_script_vfs(parser, scriptPath, profile.get(), script));
    EXPECT_EQ(script.getName(), scriptPath);
    EXPECT_FALSE(script._instructions.isEmpty());
}

TEST_F(ScriptLoaderFixture, MissingPrimaryScriptFallsBackToDefaultScript)
{
    const auto profile = loadFollowerProfile(6102);
    ASSERT_NE(profile, nullptr);

    const std::string missingPath = "script-loader-tests/missing-script.txt";
    ASSERT_FALSE(vfs_exists(missingPath));

    script_info_t script;
    parser_state_t& parser = parser_state_t::get();

    EXPECT_TRUE(load_ai_script_vfs(parser, missingPath, profile.get(), script));
    EXPECT_EQ(script.getName(), "mp_data/script.txt");
    EXPECT_FALSE(script._instructions.isEmpty());
}

TEST_F(ScriptLoaderFixture, InvalidPrimaryScriptFallsBackToDefaultScript)
{
    const auto profile = loadFollowerProfile(6103);
    ASSERT_NE(profile, nullptr);

    if (!vfs_exists("script-loader-tests"))
    {
        ASSERT_TRUE(vfs_mkdir("script-loader-tests"));
    }
    static constexpr char invalidScript[] = {'b', 'a', 'd', '\0', 'x'};
    const std::string invalidPath = "script-loader-tests/invalid-script.txt";
    ASSERT_TRUE(vfs_writeEntireFile(invalidPath, invalidScript, sizeof(invalidScript)));

    script_info_t script;
    parser_state_t& parser = parser_state_t::get();

    EXPECT_TRUE(load_ai_script_vfs(parser, invalidPath, profile.get(), script));
    EXPECT_EQ(script.getName(), "mp_data/script.txt");
    EXPECT_FALSE(script._instructions.isEmpty());
}

TEST_F(ScriptLoaderFixture, ReturnsFalseWhenPrimaryAndFallbackScriptsBothFail)
{
    const auto profile = loadFollowerProfile(6104);
    ASSERT_NE(profile, nullptr);

    static constexpr char invalidFallback[] = {'n', 'o', '\0', 'p', 'e'};
    ASSERT_TRUE(vfs_writeEntireFile("mp_data/script.txt", invalidFallback, sizeof(invalidFallback)));

    script_info_t script;
    parser_state_t& parser = parser_state_t::get();

    EXPECT_FALSE(load_ai_script_vfs(parser, "script-loader-tests/missing-script.txt", profile.get(), script));
}

} // namespace
