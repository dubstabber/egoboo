/// @file WawaliteReadContract.cpp
/// @brief Characterization tests for read_wawalite_vfs's content-fault contract
///        (egolib/game/game_wawalite.c).
///
/// wawalite_data_read (egolib/FileFormats/wawalite_file.c) has exactly one return statement
/// and reports every failure by throwing: idlib::runtime_error out of the ReadContext it
/// constructs over mp_data/wawalite.txt when the file cannot be opened, and
/// idlib::hll::compilation_error (or its subclass Ego::Script::MissingDelimiterError) from
/// partway through one of nine fixed-position sub-struct ::read() helpers when the file is
/// present but truncated or malformed. read_wawalite_vfs used to call it bare, so both of
/// those failure modes escaped as an uncaught exception instead of reaching the pre-existing
/// `if (!data) return nullptr;` guard right below the call - a guard that was therefore
/// provably unreachable, since wawalite_data_read itself never returns null.
///
/// A second hazard rides along: wawalite_data_read's *profile is the file-scope global
/// `wawalite_data` (egolib/FileFormats/Globals.cpp), reset to
/// `wawalite_data_t::getDefaults()` once at the top of the function and then reset again,
/// per sub-struct, immediately before eight of the nine sub-structs' own fields are
/// (re)assigned - the ninth, wawalite_fog_t::read, only conditionally assigns fields when its
/// own colon-delimited block is present, and relies entirely on the whole-struct reset above
/// for its defaults. A throw partway through one sub-struct's read leaves the fields read
/// before it holding real, non-default values and the remaining sub-structs holding defaults -
/// a genuinely partial state, not just "close to defaults" - unless the caller resets the
/// global again on the way out.
///
/// These fixtures use synthetic, disposable module folders under EGOBOO_USER_DIR (mounted at
/// mp_data by setup_init_module_vfs_paths, which is the same call ModuleLoadPhase::
/// initializeRuntime makes before loadEnvironment reaches read_wawalite_vfs), plus the
/// existing, real test.mod for the positive control - the same fixture ModuleLoadSmoke.cpp
/// already uses to characterize wawalite_data_read/wawalite_finalize directly.

#include "gtest/gtest.h"

#include "TestEnvironment.hpp"
#include "egolib/FileFormats/Globals.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/egoboo_setup.h"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/game.h"
#include "egolib/vfs.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace
{

// Releases a gtest stdout capture even if the code under test escapes via an exception (or an
// ASSERT_* failure) before the test body reaches its own GetCapturedStdout() call. Without this,
// a regression back to read_wawalite_vfs throwing would leave the capture open for the rest of
// the process, silently swallowing all later stdout (including the gtest pass/fail summary).
class ScopedStdoutCapture
{
public:
    ScopedStdoutCapture() { testing::internal::CaptureStdout(); }
    ~ScopedStdoutCapture()
    {
        if (!released)
        {
            testing::internal::GetCapturedStdout();
        }
    }

    std::string release()
    {
        released = true;
        return testing::internal::GetCapturedStdout();
    }

private:
    bool released = false;
};

class WawaliteReadContractFixture : public ::testing::Test
{
protected:
    static std::unique_ptr<ContentRuntimeBootstrap> s_runtime;
    static std::filesystem::path s_userModules;

    static void SetUpTestSuite()
    {
        Ego::Test::configureDataDirectory();

        const char* const userDir = std::getenv("EGOBOO_USER_DIR");
        ASSERT_NE(userDir, nullptr);
        s_userModules = std::filesystem::path(userDir) / "modules";

        // Mirrors ModuleLoadSmokeFixture's bootstrap: EnvironmentUploadsMatchParsedWawaliteState
        // there already proves wawalite_finalize (which read_wawalite_vfs also calls) succeeds
        // against test.mod under exactly this option set, including the profile-system lookup
        // its "NONE" weather line drives.
        ContentRuntimeBootstrap::Options opts;
        opts.initializeVirtualFileSystem        = true;
        opts.initializeBaseVfsPaths             = true;
        opts.initializeLogging                  = true;
        opts.configureLightweightProfileLoading = true;
        opts.initializeImageManager             = true;
        opts.initializePerkHandler              = true;
        opts.initializeProfileSystem            = true;
        opts.clearModuleVfsPathsOnShutdown      = true;
        opts.clearBaseVfsPathsOnShutdown        = true;
        opts.binaryPath                         = "";
        opts.logPath                            = "/debug/wawalite-read-contract.log";
        opts.logLevel                           = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);

        EngineContext::get().profileSystem().loadModuleProfiles();
    }

    static void TearDownTestSuite()
    {
        s_runtime.reset();

        std::error_code ec;
        for (const char* const name :
             {"wawalite-missing.mod", "wawalite-truncated.mod", "wawalite-valid.mod"})
        {
            std::filesystem::remove_all(s_userModules / name, ec);
        }
    }

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

    // Every case below mounts its own module folder onto mp_data (via
    // setup_init_module_vfs_paths), so no per-test unmount is needed - the next mount call
    // clears the module-scoped mount points before adding its own (egoboo_setup.c
    // setup_clear_module_vfs_paths).
    static void mountSyntheticModule(const std::string& folderName)
    {
        std::filesystem::create_directories(s_userModules / folderName / "gamedat");
        setup_init_module_vfs_paths(folderName);
    }

    static void writeWawaliteFile(const std::string& folderName, const std::string& contents)
    {
        const std::filesystem::path path = s_userModules / folderName / "gamedat" / "wawalite.txt";
        std::filesystem::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << contents;
        out.close();
        ASSERT_TRUE(out.good()) << "unable to write test fixture " << path.string();
    }
};

std::unique_ptr<ContentRuntimeBootstrap> WawaliteReadContractFixture::s_runtime;
std::filesystem::path WawaliteReadContractFixture::s_userModules;

// A version line, then a truncated seed line: profile->seed (a plain vfs_get_next_bool) reads
// "TRUE" successfully (a real, non-default value - the default is 0/false), and then
// wawalite_water_t::read's own vfs_get_next_int for layer_count calls skipToColon(false),
// finds no further colon before end of input, and raises Ego::Script::MissingDelimiterError
// (an idlib::hll::compilation_error subclass). wawalite_water_t::read had already reset its
// own sub-struct to defaults immediately before that throw, so the resulting global holds a
// real `seed` alongside default everything-else - the partial-state hazard this fix addresses.
constexpr const char* kTruncatedWawalite = "$FILE_VERSION 2\n\n:TRUE\n";

//--------------------------------------------------------------------------------------------
// (a) Absent mp_data/wawalite.txt.
//--------------------------------------------------------------------------------------------

TEST_F(WawaliteReadContractFixture, MissingWawaliteFileReturnsNullWithoutThrowing)
{
    mountSyntheticModule("wawalite-missing.mod");
    ASSERT_FALSE(vfs_exists("mp_data/wawalite.txt"));

    wawalite_data_t* result = nullptr;
    EXPECT_NO_THROW({ result = read_wawalite_vfs(); });
    EXPECT_EQ(result, nullptr);
}

//--------------------------------------------------------------------------------------------
// (b) + (c) A truncated wawalite.txt: no throw, nullptr, warning logged, and the file-scope
// global restored to a clean default state rather than left holding the partial write.
//--------------------------------------------------------------------------------------------

TEST_F(WawaliteReadContractFixture, TruncatedWawaliteFileReturnsNullWithoutThrowing)
{
    mountSyntheticModule("wawalite-truncated.mod");
    writeWawaliteFile("wawalite-truncated.mod", kTruncatedWawalite);

    wawalite_data_t* result = nullptr;
    EXPECT_NO_THROW({ result = read_wawalite_vfs(); });
    EXPECT_EQ(result, nullptr);
}

TEST_F(WawaliteReadContractFixture, TruncatedWawaliteFileLogsAWarning)
{
    mountSyntheticModule("wawalite-truncated.mod");
    writeWawaliteFile("wawalite-truncated.mod", kTruncatedWawalite);

    ScopedStdoutCapture capture;
    wawalite_data_t* result = read_wawalite_vfs();
    const std::string out = capture.release();

    EXPECT_EQ(result, nullptr);
    EXPECT_NE(out.find("WARNING: "), std::string::npos) << out;
    EXPECT_NE(out.find("failed to load environment file"), std::string::npos) << out;
    EXPECT_NE(out.find("mp_data/wawalite.txt"), std::string::npos) << out;
}

// Mutation-checked: with the `wawalite_data = wawalite_data_t::getDefaults();` restore removed
// from read_wawalite_vfs's catch blocks, `seed` stays 1 (the real value the truncated fixture
// above assigns before the throw) instead of reverting to the default-constructed 0, and this
// assertion fails.
TEST_F(WawaliteReadContractFixture, FailedLoadRestoresGlobalStateToCleanDefaults)
{
    mountSyntheticModule("wawalite-truncated.mod");
    writeWawaliteFile("wawalite-truncated.mod", kTruncatedWawalite);

    // Poison the global before the failing call so a fix that merely happened to leave `seed`
    // alone (rather than actively restoring it) cannot pass this by accident.
    wawalite_data.seed = 4242;

    wawalite_data_t* result = read_wawalite_vfs();

    ASSERT_EQ(result, nullptr);
    EXPECT_EQ(wawalite_data.seed, wawalite_data_t::getDefaults().seed);
    EXPECT_EQ(wawalite_data.water.layer_count, wawalite_data_t::getDefaults().water.layer_count);
}

//--------------------------------------------------------------------------------------------
// (d) A valid wawalite.txt still parses successfully: the positive control. (Read-only - this
// does not exercise write_wawalite_vfs, so it is not a write/read round trip.)
//--------------------------------------------------------------------------------------------

TEST_F(WawaliteReadContractFixture, ValidWawaliteFileParsesRealModuleValues)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    setup_init_module_vfs_paths(mod->getPath());
    ASSERT_TRUE(vfs_exists("mp_data/wawalite.txt"));

    wawalite_data_t* result = nullptr;
    EXPECT_NO_THROW({ result = read_wawalite_vfs(); });
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result, &wawalite_data);

    // A handful of values straight out of data/modules/test.mod/gamedat/wawalite.txt, to prove
    // this is a genuine parse and not a default-shaped null-turned-success.
    EXPECT_FLOAT_EQ(wawalite_data.water.douse_level, 45.0f);
    EXPECT_FLOAT_EQ(wawalite_data.water.surface_level, 25.0f);
    EXPECT_FLOAT_EQ(wawalite_data.phys.gravity, -1.0f);
}

// A failed load must not corrupt a later, unrelated, well-formed one.
TEST_F(WawaliteReadContractFixture, RecoversAfterFailedLoadWithSubsequentGoodLoad)
{
    mountSyntheticModule("wawalite-truncated.mod");
    writeWawaliteFile("wawalite-truncated.mod", kTruncatedWawalite);
    ASSERT_EQ(read_wawalite_vfs(), nullptr);

    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    setup_init_module_vfs_paths(mod->getPath());

    wawalite_data_t* result = nullptr;
    EXPECT_NO_THROW({ result = read_wawalite_vfs(); });
    ASSERT_NE(result, nullptr);
    EXPECT_FLOAT_EQ(wawalite_data.water.douse_level, 45.0f);
}

} // namespace
