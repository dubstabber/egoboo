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

/// Installs a deliberately uncompilable `mp_data/script.txt` for as long as it is in scope,
/// so a test can deny the default-script fallback, and removes it again afterwards.
///
/// The removal is not tidiness. `mp_data` resolves through the user directory first
/// (setup_init_base_vfs_paths mounts it ahead of the data directory), and vfs_writeEntireFile
/// writes to the PhysFS write directory, which is the user directory. A copy left behind there
/// therefore SHADOWS the real default AI script at data/basicdat/mp_data/script.txt for every
/// later run against that user directory - including runs of other programs, notably
/// egoboo-content-validator. That is not hypothetical: it silently moved the project's
/// documented validator baseline, because ten objects whose own script fails to compile fall
/// back to the default and are reported as `script_fallback` warnings when the default is
/// valid, but as `script_compile_failure` errors when it is not.
///
/// Under ctest the damage is contained, because each test process gets its own EGOBOO_USER_DIR
/// which is removed afterwards. It escapes when egolib-tests-executable is run directly, which
/// is an ordinary thing to do while debugging a single case.
///
/// vfs_delete_file removes the file from the write directory only, so the read-only copy in the
/// data directory is untouched and the fallback resolves to it again once this goes out of scope.
class ScopedUncompilableDefaultScript
{
public:
    ScopedUncompilableDefaultScript()
    {
        // Embedded NUL, so the lexer fails: 'n', 'o', '\0', 'p', 'e'.
        static constexpr char invalidFallback[] = {'n', 'o', '\0', 'p', 'e'};
        m_installed = vfs_writeEntireFile(defaultScriptPath, invalidFallback, sizeof(invalidFallback));
        EXPECT_TRUE(m_installed) << "could not install the uncompilable default script";
    }

    ~ScopedUncompilableDefaultScript()
    {
        if (!m_installed) return;
        // ADD_FAILURE rather than an assertion: this runs in a destructor.
        if (!vfs_delete_file(defaultScriptPath))
        {
            ADD_FAILURE() << "could not remove `" << defaultScriptPath
                          << "` from the user directory; it will shadow the real default script";
        }
    }

    ScopedUncompilableDefaultScript(const ScopedUncompilableDefaultScript&) = delete;
    ScopedUncompilableDefaultScript& operator=(const ScopedUncompilableDefaultScript&) = delete;

private:
    static constexpr const char* defaultScriptPath = "mp_data/script.txt";
    bool m_installed = false;
};

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

    const ScopedUncompilableDefaultScript noFallback;

    script_info_t script;
    parser_state_t& parser = parser_state_t::get();

    EXPECT_FALSE(load_ai_script_vfs(parser, "script-loader-tests/missing-script.txt", profile.get(), script));

    // Nothing runnable was produced, so nothing may be left behind. A partially
    // compiled stream never reaches parse_jumps, so its conditionals have no
    // resolved fail-jump and would fall through into their own bodies if executed.
    EXPECT_TRUE(script._instructions.isEmpty());
}

// A script that fails to compile PART WAY THROUGH must never leave the caller holding
// the instructions emitted before the error. Those instructions never reached
// parse_jumps, so their conditionals have no resolved fail-jump and would fall through
// into their own bodies if executed.
//
// The fallback normally hides this by overwriting the stream, so this test breaks BOTH
// sources: a primary that compiles part way and then throws on a five-character IDSZ
// (exactly the defect that cost the wizard classes their scripts, [STAFF] vs [STAF]),
// and a corrupt default. Without the transactional publish in script_compile.c the
// caller would be left holding the partial stream here.
TEST_F(ScriptLoaderFixture, PartiallyCompiledScriptIsNeverPublishedToTheCaller)
{
    const auto profile = loadFollowerProfile(6105);
    ASSERT_NE(profile, nullptr);

    if (!vfs_exists("script-loader-tests"))
    {
        ASSERT_TRUE(vfs_mkdir("script-loader-tests"));
    }

    // A compilable prefix — including a conditional whose body would run if its
    // fail-jump were never resolved — followed by the invalid IDSZ.
    const std::string partial =
        "IfSpawned\n"
        "  tmpargument = 0\n"
        "  SetState\n"
        "IfUsed\n"
        "  tmpargument = [STAFF]\n"
        "  IfTargetHasID\n"
        "    SetState\n";
    const std::string partialPath = "script-loader-tests/partial-script.txt";
    ASSERT_TRUE(vfs_writeEntireFile(partialPath, partial.data(), partial.size()));

    // Deny the fallback so it cannot mask what the primary left behind.
    const ScopedUncompilableDefaultScript noFallback;

    script_info_t script;
    parser_state_t& parser = parser_state_t::get();

    EXPECT_FALSE(load_ai_script_vfs(parser, partialPath, profile.get(), script));
    EXPECT_TRUE(script._instructions.isEmpty());
}

} // namespace
