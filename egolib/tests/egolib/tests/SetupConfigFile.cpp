#include "gtest/gtest.h"

#include "TestEnvironment.hpp"
#include "egolib/FileFormats/ConfigFile/configfile.h"
#include "egolib/egoboo_setup.h"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/vfs.h"

#include <physfs.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{

/// Characterization suite for Ego::Setup::begin()/end() (egolib/egoboo_setup.c) and for the
/// diagnostics of the configuration-file parser (egolib/FileFormats/ConfigFile/configfile.c).
///
/// The contract under test is the one stated in egoboo_setup.h on Setup::begin():
/// "If loading fails, the outcome of this function is equivalent to the case in which that
/// file is empty." EmptySetupFileLoadsAndYieldsDefaults pins that reference behavior; the
/// absent-file and malformed-file tests pin that failures now reach the same outcome.
///
/// State handling: Setup::file/started are private statics with no reset API. Under ctest each
/// gtest case is its own process (gtest_discover_tests), but a raw single-process run of
/// egolib-tests-executable shares them. SetUp() and TearDown() therefore both call
/// Setup::end(), which releases the file and clears `started`, so every case starts from
/// "nothing loaded".
///
/// Known coverage gap: the fix also removed a premature `started = true` that ran before the
/// default-configuration fallback was attempted, so a fallback that itself failed used to
/// return false with `started == true`. Nothing here pins that - reaching it needs a failing
/// std::make_shared<ConfigFile>, which is not injectable. BeginAfterAFailedLoadIsIdempotent
/// covers the fallback SUCCEEDING, not that removal.
class SetupConfigFileFixture : public ::testing::Test
{
protected:
    static std::unique_ptr<ContentRuntimeBootstrap> s_runtime;

    static void SetUpTestSuite()
    {
        Ego::Test::configureDataDirectory();

        ContentRuntimeBootstrap::Options opts;
        opts.initializeVirtualFileSystem = true;
        opts.initializeBaseVfsPaths = true;
        // REQUIRED: Setup::begin()/end() report through Log::activeTarget(), which throws
        // std::logic_error if logging is uninitialized (egolib/Log/_Include.cpp Log::get()).
        opts.initializeLogging = true;
        // Explicit: the Options struct defaults both of these to true.
        opts.initializePerkHandler = false;
        opts.initializeProfileSystem = false;
        opts.clearBaseVfsPathsOnShutdown = true;
        opts.binaryPath = "";
        opts.logPath = "/debug/setup-config-tests.log";
        // Info, not Warning: Setup::begin()/end() report their success paths at Info level,
        // and the log-content tests below assert on those entries. Log::Target::log() (the
        // path Log::Entry takes) does not apply the level filter that Log::Target::logv()
        // applies, so a Warning target would in fact still print them - but that asymmetry is
        // an unrelated Log quirk and the assertions here must not depend on it.
        opts.logLevel = Log::Level::Info;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);
    }

    static void TearDownTestSuite()
    {
        s_runtime.reset();
    }

    // The PhysFS write dir is the user dir root (vfs.c PHYSFS_setWriteDir), and
    // fs_getUserDirectory() is exactly $EGOBOO_USER_DIR with a trailing slash
    // (Platform/file_linux.c), so a file written as "setup.txt" lands here.
    static std::filesystem::path userSetupPath()
    {
        const char* userDir = std::getenv("EGOBOO_USER_DIR");
        EXPECT_NE(userDir, nullptr) << "EGOBOO_USER_DIR must be set by the test runner";
        return std::filesystem::path(userDir ? userDir : "") / "setup.txt";
    }

    // Order-independence even under raw single-process gtest runs. Two things have to be
    // normalized: the user dir is FIRST in the search path, so a setup.txt left there by a
    // previous case would shadow data/setup.txt and silently change what the next case
    // parses; and Setup::begin() short-circuits while `started` is set, so a leftover loaded
    // configuration would make this case characterize the wrong input. TearDown() clears both
    // as well, but doing it here too means a future test file that calls Setup::begin()
    // without a matching end() cannot poison this suite.
    void SetUp() override
    {
        (void)Ego::Setup::end();
        removeUserSetup();
    }

    void TearDown() override
    {
        // Releases Setup::file and clears Setup::started. Also rewrites setup.txt into the
        // user dir when a file was loaded, which is why the removal below is mandatory.
        (void)Ego::Setup::end();
        removeUserSetup();
    }

    static void removeUserSetup()
    {
        std::error_code ec;
        std::filesystem::remove(userSetupPath(), ec);
    }

    static void writeUserSetup(const std::string& content)
    {
        ASSERT_TRUE(vfs_writeEntireFile("setup.txt", content.data(), content.size()));
        ASSERT_TRUE(std::filesystem::exists(userSetupPath()));
    }

    // Overwrites a freshly constructed configuration with values that are not the compiled-in
    // defaults. Without this, every assertion in expectDefaultConfiguration() would also hold
    // for a download() that wrote nothing at all, because egoboo_config_t's constructor
    // already installs those defaults.
    static void makeSentinelConfiguration(egoboo_config_t& cfg)
    {
        cfg.graphic_resolution_horizontal.setValue(4242);
        cfg.graphic_resolution_vertical.setValue(2424);
        cfg.graphic_colorBuffer_bitDepth.setValue(16);
        cfg.graphic_antialiasing.setValue(7);
        cfg.sound_effects_volume.setValue(13);
        cfg.sound_music_volume.setValue(17);
        cfg.network_hostName.setValue("sentinel host");
        cfg.debug_developerMode_enable.setValue(true);
    }

    // The compiled-in defaults asserted here are the constructor arguments in
    // egoboo_setup.c egoboo_config_t::egoboo_config_t(). Note the header docstrings for
    // several of these defaults are stale; the .c is canonical.
    //
    // Five of them differ from the corresponding values in the shipped data/setup.txt
    // (800 vs 1920, 600 vs 1080, 32 vs 24, antialiasing 2 vs 1, developer mode false vs
    // "true"), so those distinguish "the fallback ran" from "data/setup.txt was parsed".
    // The remaining three (effects volume 90, music volume 70, host name "Egoboo host") are
    // byte-identical to what data/setup.txt sets, so they do NOT make that distinction; they
    // are asserted only as a broad check that the whole variable set was assigned.
    static void expectDefaultConfiguration(const egoboo_config_t& cfg)
    {
        // Discriminating against a parse of the shipped data/setup.txt.
        EXPECT_EQ(cfg.graphic_resolution_horizontal.getValue(), 800);
        EXPECT_EQ(cfg.graphic_resolution_vertical.getValue(), 600);
        EXPECT_EQ(cfg.graphic_colorBuffer_bitDepth.getValue(), 32);
        EXPECT_EQ(static_cast<int>(cfg.graphic_antialiasing.getValue()), 2);
        EXPECT_FALSE(cfg.debug_developerMode_enable.getValue());
        // Not discriminating against data/setup.txt; see above.
        EXPECT_EQ(static_cast<int>(cfg.sound_effects_volume.getValue()), 90);
        EXPECT_EQ(static_cast<int>(cfg.sound_music_volume.getValue()), 70);
        EXPECT_EQ(cfg.network_hostName.getValue(), std::string("Egoboo host"));
    }
};

std::unique_ptr<ContentRuntimeBootstrap> SetupConfigFileFixture::s_runtime;

/// Removes every PhysFS search path entry and restores path, mount point and order on
/// destruction.
///
/// Needed because data/setup.txt exists and the data directory is mounted by vfs_init()
/// independently of setup_init_base_vfs_paths(). Simply deleting the user-dir copy therefore
/// does NOT produce an absent setup.txt - the data-dir copy would still parse fine and the
/// absent-file test would be a false green.
///
/// Restoration must carry the per-entry mount point (PHYSFS_getMountPoint); re-mounting
/// everything at "/" would destroy the mp_data/mp_modules/... mount points that the rest of
/// the suite depends on in a single-process run.
///
/// The PhysFS write directory is set separately (PHYSFS_setWriteDir) and is unaffected, so
/// logging and vfs_openWrite keep working while this guard is in scope.
class ScopedEmptyVfsSearchPath
{
public:
    ScopedEmptyVfsSearchPath()
    {
        char** list = PHYSFS_getSearchPath();
        for (char** it = list; it && *it; ++it)
        {
            const char* mountPoint = PHYSFS_getMountPoint(*it);
            m_saved.emplace_back(*it, mountPoint ? mountPoint : "/");
        }
        PHYSFS_freeList(list);
        for (const auto& entry : m_saved)
        {
            PHYSFS_unmount(entry.first.c_str());
        }
    }

    ~ScopedEmptyVfsSearchPath()
    {
        // Appending in the original order restores the original order.
        for (const auto& entry : m_saved)
        {
            if (!PHYSFS_mount(entry.first.c_str(), entry.second.c_str(), 1))
            {
                // A silent failure here would strip the data directory and the mp_* mount
                // points for the remainder of a single-process run, and the damage would be
                // attributed to whichever later case happened to need them. ADD_FAILURE does
                // not throw, so it is safe in a destructor.
                ADD_FAILURE() << "failed to restore PhysFS search path entry `" << entry.first
                              << "` at mount point `" << entry.second
                              << "`: " << PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());
            }
        }
    }

    ScopedEmptyVfsSearchPath(const ScopedEmptyVfsSearchPath&) = delete;
    ScopedEmptyVfsSearchPath& operator=(const ScopedEmptyVfsSearchPath&) = delete;

private:
    std::vector<std::pair<std::string, std::string>> m_saved;
};

//--------------------------------------------------------------------------------------------
// The reference behavior the header contract points at.
//--------------------------------------------------------------------------------------------

/// An empty setup.txt is a successful load, not a failure: the scanner reads zero bytes,
/// parseFile() hits END_OF_INPUT immediately and returns true, and parse() yields an empty
/// ConfigFile. Every variable is then missing from the file, and Load (Configuration.hpp)
/// assigns the default for a missing key.
TEST_F(SetupConfigFileFixture, EmptySetupFileLoadsAndYieldsDefaults)
{
    writeUserSetup("");

    ASSERT_TRUE(Ego::Setup::begin());

    // A local configuration: never download into egoboo_config_t::get(), which the bootstrap
    // installs as the active configuration for the whole process.
    egoboo_config_t cfg;
    makeSentinelConfiguration(cfg);
    ASSERT_TRUE(Ego::Setup::download(cfg));
    expectDefaultConfiguration(cfg);
}

//--------------------------------------------------------------------------------------------
// Defect 1: an absent setup.txt used to throw out of begin().
//--------------------------------------------------------------------------------------------

/// ConfigFileParser derives from Ego::Script::Scanner, whose constructor calls
/// vfs_readEntireFile and throws idlib::runtime_error when the file cannot be opened
/// (vfs_bulk.c). The parser used to be constructed outside begin()'s try block, so that
/// exception escaped begin() and the default-configuration fallback never ran.
TEST_F(SetupConfigFileFixture, BeginWithNoSetupFileAnywhereFallsBackToDefaults)
{
    ScopedEmptyVfsSearchPath noSearchPath;
    ASSERT_FALSE(vfs_exists("setup.txt"));

    EXPECT_NO_THROW({ EXPECT_TRUE(Ego::Setup::begin()); });

    egoboo_config_t cfg;
    makeSentinelConfiguration(cfg);
    ASSERT_NO_THROW({ EXPECT_TRUE(Ego::Setup::download(cfg)); });
    expectDefaultConfiguration(cfg);
}

//--------------------------------------------------------------------------------------------
// Defect 2: a malformed setup.txt used to dereference a null shared_ptr.
//--------------------------------------------------------------------------------------------

/// "graphic.anti" is a qualified name that ends before its ':'. parseEntry() reports
/// "invalid key-value pair" and returns false, so ConfigFileParser::parse() returns nullptr.
/// begin()'s failure branch then used to build its warning out of file->getFileName() with
/// file == nullptr, inside its own `if (!file)` test.
///
/// Input choice matters: this build has asserts live (CMAKE_BUILD_TYPE is empty, NDEBUG is
/// not defined), and configfile.c asserts unguarded in parseQualifiedName/parseName/parseValue.
/// Inputs such as "{Section}", "1abc : \"x\"" or an unquoted value abort instead of returning
/// nullptr. "graphic.anti" reaches the nullptr path cleanly.
TEST_F(SetupConfigFileFixture, BeginWithMalformedSetupFileFallsBackToDefaults)
{
    writeUserSetup("graphic.anti");

    EXPECT_NO_THROW({ EXPECT_TRUE(Ego::Setup::begin()); });

    egoboo_config_t cfg;
    makeSentinelConfiguration(cfg);
    ASSERT_NO_THROW({ EXPECT_TRUE(Ego::Setup::download(cfg)); });
    expectDefaultConfiguration(cfg);
}

/// A second malformed shape: the value's closing quote is missing, which is what a setup.txt
/// truncated mid-write looks like. parseValue() reports "invalid value" and returns false.
TEST_F(SetupConfigFileFixture, BeginWithTruncatedValueFallsBackToDefaults)
{
    writeUserSetup("graphic.resolution.horizontal : \"128");

    EXPECT_NO_THROW({ EXPECT_TRUE(Ego::Setup::begin()); });

    egoboo_config_t cfg;
    makeSentinelConfiguration(cfg);
    ASSERT_NO_THROW({ EXPECT_TRUE(Ego::Setup::download(cfg)); });
    expectDefaultConfiguration(cfg);
}

//--------------------------------------------------------------------------------------------
// The success path must be unchanged.
//--------------------------------------------------------------------------------------------

/// The user directory is first in the PhysFS search path, so a setup.txt written there
/// shadows data/setup.txt. Keys present in the file are decoded; keys absent from it get the
/// compiled-in default (Load in Configuration.hpp assigns the default for a missing key -
/// it does not leave the previous in-memory value).
TEST_F(SetupConfigFileFixture, BeginWithValidUserSetupFileLoadsItsValues)
{
    writeUserSetup("graphic.resolution.horizontal : \"1280\"\n"
                   "graphic.resolution.vertical : \"720\"\n"
                   "network.hostName : \"probe host\"\n"
                   "debug.developerMode.enable : \"true\"\n");

    ASSERT_TRUE(Ego::Setup::begin());

    egoboo_config_t cfg;
    makeSentinelConfiguration(cfg);
    ASSERT_TRUE(Ego::Setup::download(cfg));
    EXPECT_EQ(cfg.graphic_resolution_horizontal.getValue(), 1280);
    EXPECT_EQ(cfg.graphic_resolution_vertical.getValue(), 720);
    EXPECT_EQ(cfg.network_hostName.getValue(), std::string("probe host"));
    EXPECT_TRUE(cfg.debug_developerMode_enable.getValue());
    // Absent from the file: the compiled-in default survives.
    EXPECT_EQ(cfg.graphic_colorBuffer_bitDepth.getValue(), 32);
    EXPECT_EQ(static_cast<int>(cfg.sound_music_volume.getValue()), 70);
}

/// end() with a loaded file writes it back through ConfigFileUnParser, which serializes the
/// entries the ConfigFile currently holds - sorted by qualified name - and nothing else. end()
/// does NOT itself serialize the configuration variables: the full 63-variable file the real
/// game writes comes from GameEngine::uninitialize() calling config_synch -> Setup::upload ->
/// egoboo_config_t::store BEFORE ~SystemService reaches end(). Nothing here does that, so the
/// single parsed entry is exactly what comes back out.
TEST_F(SetupConfigFileFixture, EndWithLoadedFileWritesTheUserSetupFile)
{
    writeUserSetup("graphic.resolution.horizontal : \"1280\"\n");
    ASSERT_TRUE(Ego::Setup::begin());
    removeUserSetup();

    EXPECT_TRUE(Ego::Setup::end());

    ASSERT_TRUE(std::filesystem::exists(userSetupPath()));
    std::ifstream stream(userSetupPath(), std::ios::binary);
    const std::string written((std::istreambuf_iterator<char>(stream)),
                              std::istreambuf_iterator<char>());
    EXPECT_EQ(written, "graphic.resolution.horizontal : \"1280\"\n");
}

//--------------------------------------------------------------------------------------------
// Defect 3: end() used to dereference a null file.
//--------------------------------------------------------------------------------------------

/// SystemService::~SystemService calls Setup::end() unconditionally, and both a failed
/// begin() and a previous end() leave Setup::file == nullptr. end() used to hand that null
/// straight to ConfigFileUnParser::unparse, which immediately calls _source->getFileName().
/// Contract now: nothing loaded means nothing was saved, so end() reports false.
TEST_F(SetupConfigFileFixture, EndWithNoLoadedFileReportsNoSaveAndDoesNotCrash)
{
    EXPECT_NO_THROW({ EXPECT_FALSE(Ego::Setup::end()); });
    // Still no file, and still no crash on a repeat.
    EXPECT_NO_THROW({ EXPECT_FALSE(Ego::Setup::end()); });
    EXPECT_FALSE(std::filesystem::exists(userSetupPath()));
}

/// end() after a successful begin() releases the configuration, so a second end() finds
/// nothing to save rather than writing the file a second time.
TEST_F(SetupConfigFileFixture, SecondEndAfterASuccessfulEndReportsNoSave)
{
    writeUserSetup("");
    ASSERT_TRUE(Ego::Setup::begin());
    ASSERT_TRUE(Ego::Setup::end());

    EXPECT_NO_THROW({ EXPECT_FALSE(Ego::Setup::end()); });
}

//--------------------------------------------------------------------------------------------
// Defect 4: `started` must agree with "a configuration file is loaded".
//--------------------------------------------------------------------------------------------

/// begin() short-circuits to true when `started` is set. Because end() releases the file, it
/// must also clear `started`, otherwise the second begin() below returns true with no file
/// loaded and download() throws std::logic_error("setup file `setup.txt` not loaded").
TEST_F(SetupConfigFileFixture, BeginAfterEndReloadsTheConfiguration)
{
    writeUserSetup("graphic.resolution.horizontal : \"1280\"\n");

    ASSERT_TRUE(Ego::Setup::begin());
    ASSERT_TRUE(Ego::Setup::end());
    // end() rewrote the user file; make the reload read something distinguishable.
    writeUserSetup("graphic.resolution.horizontal : \"1024\"\n");

    ASSERT_TRUE(Ego::Setup::begin());

    egoboo_config_t cfg;
    makeSentinelConfiguration(cfg);
    ASSERT_NO_THROW({ EXPECT_TRUE(Ego::Setup::download(cfg)); });
    EXPECT_EQ(cfg.graphic_resolution_horizontal.getValue(), 1024);
}

/// A failed load followed by a successful default fallback still counts as started, and the
/// loaded default configuration is downloadable.
TEST_F(SetupConfigFileFixture, BeginAfterAFailedLoadIsIdempotent)
{
    writeUserSetup("graphic.anti");

    ASSERT_TRUE(Ego::Setup::begin());
    ASSERT_TRUE(Ego::Setup::begin());

    egoboo_config_t cfg;
    makeSentinelConfiguration(cfg);
    ASSERT_NO_THROW({ EXPECT_TRUE(Ego::Setup::download(cfg)); });
    expectDefaultConfiguration(cfg);
}

//--------------------------------------------------------------------------------------------
// Parser diagnostics are logged, not printed to stderr.
//--------------------------------------------------------------------------------------------

/// configfile.c used to report parse failures with fprintf(stderr, ...). They now go through
/// the log target, which DefaultTarget::writev writes to stdout (and to the log file) with a
/// "WARNING: " prefix. begin()'s own fallback warning is checked here too, since it is the
/// entry that used to crash while being built.
TEST_F(SetupConfigFileFixture, MalformedSetupFileDiagnosticsAreLoggedNotPrintedToStderr)
{
    writeUserSetup("graphic.anti");

    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
    const bool started = Ego::Setup::begin();
    const std::string out = testing::internal::GetCapturedStdout();
    const std::string err = testing::internal::GetCapturedStderr();

    EXPECT_TRUE(started);
    EXPECT_NE(out.find("setup.txt: invalid key-value pair"), std::string::npos) << out;
    EXPECT_NE(out.find("WARNING: "), std::string::npos) << out;
    EXPECT_NE(out.find("unable to load setup file `setup.txt` - reverting to default configuration"),
              std::string::npos) << out;
    EXPECT_EQ(err.find("invalid key-value pair"), std::string::npos) << err;
}

/// The success path still emits exactly its Info entry, with the wording unchanged.
TEST_F(SetupConfigFileFixture, SuccessfulLoadStillLogsTheLoadedEntry)
{
    writeUserSetup("");

    testing::internal::CaptureStdout();
    const bool started = Ego::Setup::begin();
    const std::string out = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(started);
    EXPECT_NE(out.find("setup file `setup.txt` loaded"), std::string::npos) << out;
    EXPECT_EQ(out.find("reverting to default configuration"), std::string::npos) << out;
}

} // namespace
