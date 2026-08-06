/// @file ModuleDiscoveryFaultIsolation.cpp
/// @brief Characterization tests for fault isolation in ProfileSystem::loadModuleProfiles.
///
/// Users install modules by dropping folders into the user directory, which
/// setup_init_base_vfs_paths mounts into `mp_modules` alongside the shipped
/// data directory.  A folder whose `gamedat/menu.txt` is missing or malformed
/// makes ModuleProfile::loadFromFile THROW - it never returns nullptr - and
/// before the fault isolation in loadModuleProfiles that throw escaped the
/// whole scan, leaving the caller with a truncated (often empty) module list
/// because `_moduleProfilesLoaded.clear()` had already run.
///
/// These tests pin the part of the contract that is observable from the module
/// list: a broken module is SKIPPED and every other module still loads.  The
/// accompanying "unable to load module" warning is deliberately NOT asserted -
/// Log entries are buffered into the log file and only flushed on shutdown, so
/// reading them back mid-suite would be flaky.
///
/// Note on exception types: the parsers on this path raise idlib exceptions
/// (idlib::runtime_error for an unreadable menu.txt, Ego::Script::
/// MissingDelimiterError for a truncated one).  idlib::exception has no
/// std::exception base, so a `catch (const std::exception&)` in the production
/// code would not catch them - see the comment in ProfileSystem.cpp.

#include "gtest/gtest.h"

#include "TestEnvironment.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace {

/// A well-formed menu.txt, modelled on data/modules/test.mod/gamedat/menu.txt.
/// It must satisfy the full read sequence in ModuleProfile::loadFromFile:
/// name, reference, IDSZ, import amount, export flag, min/max players,
/// respawn, RTS, rank, then SUMMARYLINES summary lines.
const char* const kValidMenuTxt =
    "Module Name :Fault_Isolation_Good\n"
    "Reference Directory :NONE\n"
    "Required reference IDSZ :[NONE]\n"
    "Number of imports ( 0 to 4 ) :0\n"
    "Exporting ( True or False ) :False\n"
    "Minimum players ( 1 to 4 ) :1\n"
    "Maximum players ( 1 to 4 ) :1\n"
    "Respawning ( True or False ) :True\n"
    "RTS control (True, False, ALL) :False\n"
    "Level rating ( * to  ***** ) :*\n"
    "\n"
    "// Module summary\n"
    ":A_module_used_by_the\n"
    ":fault_isolation_test.\n"
    ":_\n"
    ":_\n"
    ":_\n"
    ":_\n"
    ":_\n"
    ":_\n"
    "\n"
    "// IDZs\n"
    ":[TYPE] DEBUG\n";

/// A menu.txt that stops after the required-IDSZ field.  The next read is
/// vfs_get_next_int for the import amount, which calls skipToColon(false) and
/// finds no further colon, so it raises Ego::Script::MissingDelimiterError.
const char* const kTruncatedMenuTxt =
    "Module Name :Fault_Isolation_Truncated\n"
    "Reference Directory :NONE\n"
    "Required reference IDSZ :[NONE]\n";

/// Writes a fixture file, failing the suite rather than silently producing an
/// absent or empty file - a fixture that failed to materialise would turn the
/// "broken module" tests into false greens.
void writeFile(const std::filesystem::path& path, const char* contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << contents;
    out.close();
    ASSERT_TRUE(out.good()) << "unable to write test fixture " << path.string();
    ASSERT_TRUE(std::filesystem::exists(path)) << "missing test fixture " << path.string();
}

/// The number of `.mod` folders shipped in the data directory.  Derived at run
/// time rather than hard-coded: the shipped module count is a volatile number
/// owned by refactoring-documents/06-validator-baseline.md, and duplicating it
/// here would couple this test to a data-submodule change it does not care about.
std::size_t countShippedModules()
{
    const char* const dataDir = std::getenv("EGOBOO_DATA_DIR");
    if (!dataDir) return 0;

    std::size_t count = 0;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(dataDir) / "modules", ec))
    {
        if (entry.is_directory(ec) && entry.path().extension() == ".mod")
        {
            ++count;
        }
    }
    return count;
}

} // namespace

class ModuleDiscoveryFaultIsolationFixture : public ::testing::Test
{
protected:
    static std::unique_ptr<ContentRuntimeBootstrap> s_runtime;
    static std::filesystem::path s_userModules;
    static std::size_t s_shippedModuleCount;

    static void SetUpTestSuite()
    {
        // Sets EGOBOO_DATA_DIR and a per-PID EGOBOO_USER_DIR under the system
        // temp directory, and registers that root for atexit removal - so the
        // scratch modules created below are cleaned up automatically.
        Ego::Test::configureDataDirectory();

        s_shippedModuleCount = countShippedModules();
        ASSERT_GT(s_shippedModuleCount, 0u) << "no shipped modules found under EGOBOO_DATA_DIR";

        const char* const userDir = std::getenv("EGOBOO_USER_DIR");
        ASSERT_NE(userDir, nullptr);
        const std::filesystem::path userModules = std::filesystem::path(userDir) / "modules";
        s_userModules = userModules;

        // Create the scratch modules BEFORE the VFS mount points are set up.
        //
        // Failure mode 1: a truncated menu.txt.
        writeFile(userModules / "zz-broken.mod" / "gamedat" / "menu.txt", kTruncatedMenuTxt);
        // Failure mode 2: a module folder with no menu.txt at all.  This is the
        // likeliest real-world bad drop, and it raises idlib::runtime_error out
        // of vfs_readEntireFile via the ReadContext/Scanner constructor.
        std::filesystem::create_directories(userModules / "zz-empty.mod" / "gamedat");
        // A well-formed module that sorts after both broken ones, so it can only
        // load if discovery survived the failures.
        writeFile(userModules / "zz-good.mod" / "gamedat" / "menu.txt", kValidMenuTxt);

        ContentRuntimeBootstrap::Options opts;
        opts.initializeVirtualFileSystem   = true;
        opts.initializeBaseVfsPaths        = true;
        opts.initializeLogging             = true;
        opts.initializeImageManager        = true;
        opts.initializePerkHandler         = true;
        opts.initializeProfileSystem       = true;
        opts.clearModuleVfsPathsOnShutdown = true;
        opts.clearBaseVfsPathsOnShutdown   = true;
        opts.binaryPath = "";
        opts.logPath    = "/debug/module-discovery-fault-isolation.log";
        opts.logLevel   = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);
    }

    static void TearDownTestSuite()
    {
        s_runtime.reset();

        // EGOBOO_USER_DIR is per-PID, not per-suite, and it is mounted into
        // `mp_modules`. Under ctest each test case gets its own process so this
        // cannot reach a neighbour, but a manual single-process run of the test
        // binary would otherwise leave the broken drops in place for every later
        // fixture that scans modules. The atexit cleanup registered by
        // configureDataDirectory remains the backstop.
        std::error_code ec;
        for (const char* const name : {"zz-broken.mod", "zz-empty.mod", "zz-good.mod"})
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
};

std::unique_ptr<ContentRuntimeBootstrap> ModuleDiscoveryFaultIsolationFixture::s_runtime;
std::filesystem::path ModuleDiscoveryFaultIsolationFixture::s_userModules;
std::size_t ModuleDiscoveryFaultIsolationFixture::s_shippedModuleCount = 0;

/// The load must complete instead of propagating the parser exception.
/// Before fault isolation this threw Ego::Script::MissingDelimiterError.
TEST_F(ModuleDiscoveryFaultIsolationFixture, BrokenModuleDoesNotAbortDiscovery)
{
    EXPECT_NO_THROW(EngineContext::get().profileSystem().loadModuleProfiles());
}

/// The broken modules must be absent from the list rather than half-inserted.
/// The positive half of the assertion matters as much as the negative half:
/// two `EXPECT_EQ(..., nullptr)` alone would also pass against an empty list,
/// so the scratch modules that survived discovery are counted explicitly.
TEST_F(ModuleDiscoveryFaultIsolationFixture, BrokenModulesAreSkipped)
{
    ASSERT_NO_THROW(EngineContext::get().profileSystem().loadModuleProfiles());

    EXPECT_EQ(findModule("zz-broken.mod"), nullptr);
    EXPECT_EQ(findModule("zz-empty.mod"), nullptr);

    // Exactly one of the three `zz-` scratch modules must have survived: the
    // two broken ones skipped, the well-formed one kept.
    std::vector<std::string> survivingScratchModules;
    for (const auto& mod : EngineContext::get().profileSystem().getModuleProfiles())
    {
        if (mod && mod->getFolderName().rfind("zz-", 0) == 0)
        {
            survivingScratchModules.push_back(mod->getFolderName());
        }
    }
    ASSERT_EQ(survivingScratchModules.size(), 1u);
    EXPECT_EQ(survivingScratchModules.front(), "zz-good.mod");
}

/// The point of the pass: a well-formed module still loads even though an
/// earlier-enumerated module failed to parse.
TEST_F(ModuleDiscoveryFaultIsolationFixture, GoodModuleLoadsDespiteBrokenNeighbours)
{
    ASSERT_NO_THROW(EngineContext::get().profileSystem().loadModuleProfiles());

    const auto good = findModule("zz-good.mod");
    ASSERT_NE(good, nullptr);
    // The string-literal reader decodes `_` as a space, so the name written as
    // `Fault_Isolation_Good` in menu.txt comes back space-separated.
    EXPECT_EQ(good->getName(), "Fault Isolation Good");
}

/// The shipped module set must be unaffected by a bad drop in the user directory.
TEST_F(ModuleDiscoveryFaultIsolationFixture, ShippedModulesStillLoad)
{
    ASSERT_NO_THROW(EngineContext::get().profileSystem().loadModuleProfiles());

    EXPECT_NE(findModule("test.mod"), nullptr);

    // Every shipped module plus zz-good.mod.  The expected count is derived from
    // the data directory rather than hard-coded, and the comparison is >= rather
    // than ==, because the resolved user directory may legitimately contain other
    // folders.  This is the assertion that would fail before the fix: the escaping
    // parser exception left the list truncated at the first broken drop.
    EXPECT_GE(EngineContext::get().profileSystem().getModuleProfiles().size(),
              s_shippedModuleCount + 1u);
}
