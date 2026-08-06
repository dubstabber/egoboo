/// @file ContentFaultMissContracts.cpp
/// @brief Characterization tests for the two content-fault miss contracts that replaced
///        exception band-aids: ModuleProfile::moduleHasIDSZ and ObjectProfile::loadFromFile.
///
/// Both functions are handed paths that come out of content.  moduleHasIDSZ gets a module
/// folder name read from an object's message table (scr_IfModuleHasIDSZ), and loadFromFile
/// gets an object folder discovered on disk.  A name that does not resolve, or a file that
/// does not parse, is ordinary input for them - not a precondition violation - so both
/// report the miss rather than throwing.
///
/// The reason this needs pinning rather than being obvious: the parsers underneath raise
/// idlib exceptions (idlib::runtime_error for an unreadable file, by way of
/// vfs_readEntireFile at vfs_bulk.c:56 and the Ego::Script::Scanner constructor;
/// idlib::hll::compilation_error and its subclass Ego::Script::MissingDelimiterError for a
/// malformed one).  idlib::exception is declared with no base class at all
/// (idlib/exception/exception.hpp:64), so `catch (const std::exception&)` is blind to every
/// one of them.  ExceptionHierarchyIsUnrelatedToStdException below asserts exactly that,
/// so the next reader learns the rule from a failing assumption rather than from a silent
/// miss somewhere else.
///
/// moduleHasIDSZ used to throw for both failure modes and the script helper
/// activeModuleHasIdszWithValidMessage wrapped it in `catch (...)`, which also swallowed
/// std::bad_alloc and any programming error inside it.  The contract now lives in the
/// function and the blanket handler is gone.
///
/// The scratch fixtures go under EGOBOO_USER_DIR, which setup_init_base_vfs_paths mounts
/// into `mp_modules`.  Nothing here writes to the data submodule.

#include "gtest/gtest.h"

#include "TestEnvironment.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/Script/Errors.hpp"
#include "egolib/fileutil.h"
#include "egolib/vfs.h"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"

#include "idlib/debug.hpp"  // idlib::debug_assertion_failed_error
#include "idlib/exception.hpp"
#include "idlib/hll.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <type_traits>

namespace {

/// A well-formed menu.txt that declares the `[TYPE]` expansion.  It must satisfy the fixed
/// read sequence in moduleHasIDSZ: ten header colons, then SUMMARYLINES (8) summary colons,
/// then the expansion block.
const char* const kValidMenuTxt =
    "Module Name :Miss_Contract_Good\n"
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
    ":miss_contract_test.\n"
    ":_\n"
    ":_\n"
    ":_\n"
    ":_\n"
    ":_\n"
    ":_\n"
    "\n"
    "// IDZs\n"
    ":[TYPE] DEBUG\n";

/// A menu.txt that stops after the required-IDSZ field, i.e. after three of the ten header
/// colons.  The fourth skipToColon(false) finds no further colon and raises
/// Ego::Script::MissingDelimiterError, an idlib::hll::compilation_error subclass.
const char* const kTruncatedMenuTxt =
    "Module Name :Miss_Contract_Truncated\n"
    "Reference Directory :NONE\n"
    "Required reference IDSZ :[NONE]\n";

/// A well-formed header followed by an expansion block whose FIRST entry is a five-character
/// IDSZ.  ReadContext::readIDSZ reads four characters and then requires `]`, finds `F`, and
/// raises idlib::hll::compilation_error ("unexpected character while scanning IDSZ").  The
/// `[TYPE]` on the next line is therefore never reached - the surprising clause of the
/// contract, and the reason it is spelled out in the header.
const char* const kMalformedExpansionMenuTxt =
    "Module Name :Miss_Contract_Malformed\n"
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
    ":Malformed_expansion\n"
    ":block.\n"
    ":_\n"
    ":_\n"
    ":_\n"
    ":_\n"
    ":_\n"
    ":_\n"
    "\n"
    "// IDZs\n"
    ":[STAFF]\n"
    ":[TYPE] DEBUG\n";

/// A data.txt that stops in the middle of the header.  ObjectProfile::loadDataFile opens a
/// ReadContext over it successfully and then runs out of input while parsing, so this drives
/// the compilation_error arm rather than the runtime_error one.
const char* const kTruncatedDataTxt =
    "Slot number    : 0\n"
    "Class name     : Miss_Contract_Broken\n";

/// Writes a fixture file, failing the suite rather than silently producing an absent or
/// empty file - a fixture that failed to materialise would turn every negative assertion
/// below into a false green.
void writeFile(const std::filesystem::path& path, const char* contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << contents;
    out.close();
    ASSERT_TRUE(out.good()) << "unable to write test fixture " << path.string();
    ASSERT_TRUE(std::filesystem::exists(path)) << "missing test fixture " << path.string();
}

} // namespace

class ContentFaultMissContractsFixture : public ::testing::Test
{
protected:
    static std::unique_ptr<ContentRuntimeBootstrap> s_runtime;
    static std::filesystem::path s_userModules;

    /// The module folder that holds the scratch object profiles.  It deliberately has no
    /// gamedat/menu.txt, so module discovery skips it; only the object paths under it matter.
    static constexpr const char* kObjectHost = "zzmc-objects.mod";

    static void SetUpTestSuite()
    {
        // Sets EGOBOO_DATA_DIR and a per-PID EGOBOO_USER_DIR under the system temp directory,
        // and registers that root for atexit removal.
        Ego::Test::configureDataDirectory();

        const char* const userDir = std::getenv("EGOBOO_USER_DIR");
        ASSERT_NE(userDir, nullptr);
        const std::filesystem::path userModules = std::filesystem::path(userDir) / "modules";
        s_userModules = userModules;

        // Create the scratch content BEFORE the VFS mount points are set up.
        writeFile(userModules / "zzmc-good.mod" / "gamedat" / "menu.txt", kValidMenuTxt);
        writeFile(userModules / "zzmc-truncated.mod" / "gamedat" / "menu.txt", kTruncatedMenuTxt);
        writeFile(userModules / "zzmc-malformed.mod" / "gamedat" / "menu.txt", kMalformedExpansionMenuTxt);
        // A module folder with a gamedat but no menu.txt at all: the likeliest real bad drop.
        std::filesystem::create_directories(userModules / "zzmc-nomenu.mod" / "gamedat");

        // A real, well-formed module literally named "NONE", declaring [TYPE]. This exists so
        // that ModuleHasIdszShortCircuitsWithoutReadingAnything is not vacuous: without it,
        // deleting the `szModName == "NONE"` guard would still yield false (the read would fail
        // and the miss contract would report false), and the assertion would pass against a
        // function that had lost the short-circuit entirely.
        writeFile(userModules / "NONE" / "gamedat" / "menu.txt", kValidMenuTxt);

        // Object profiles, hosted under a module folder because `mp_modules` is the mount that
        // is available without a module being current (setup_init_module_vfs_paths is what
        // populates `mp_objects`, and it needs an active module).
        writeFile(userModules / kObjectHost / "objects" / "zzmc-broken.obj" / "data.txt", kTruncatedDataTxt);
        // An object folder with no data.txt at all.
        std::filesystem::create_directories(userModules / kObjectHost / "objects" / "zzmc-nodata.obj");

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
        opts.logPath    = "/debug/content-fault-miss-contracts.log";
        opts.logLevel   = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);
    }

    static void TearDownTestSuite()
    {
        s_runtime.reset();

        // EGOBOO_USER_DIR is per-PID, not per-suite, and it is mounted into `mp_modules`.
        // Under ctest each case gets its own process, but a manual single-process run of the
        // whole test binary would otherwise leave these drops in place for every later fixture
        // that scans modules. The atexit cleanup from configureDataDirectory is the backstop.
        std::error_code ec;
        for (const char* const name : {"zzmc-good.mod", "zzmc-truncated.mod",
                                       "zzmc-malformed.mod", "zzmc-nomenu.mod", "NONE",
                                       kObjectHost})
        {
            std::filesystem::remove_all(s_userModules / name, ec);
        }
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ContentFaultMissContractsFixture::s_runtime;
std::filesystem::path ContentFaultMissContractsFixture::s_userModules;

/// The premise of this whole family, asserted rather than asserted-in-a-comment: neither
/// hierarchy is reachable from the other, so a handler for one catches nothing of the other.
TEST(ContentFaultExceptionHierarchy, ExceptionHierarchyIsUnrelatedToStdException)
{
    static_assert(!std::is_base_of<std::exception, idlib::exception>::value,
                  "idlib::exception has no std::exception base - a catch (const std::exception&) "
                  "cannot see anything the engine's parsers raise. If this ever becomes false, "
                  "the paired idlib/std handlers across egolib can be simplified.");
    static_assert(!std::is_base_of<std::exception, idlib::runtime_error>::value,
                  "idlib::runtime_error derives from idlib::exception, not from std::runtime_error");
    static_assert(!std::is_base_of<std::exception, idlib::hll::compilation_error>::value,
                  "idlib::hll::compilation_error derives from idlib::exception");
    static_assert(std::is_base_of<idlib::exception, idlib::runtime_error>::value, "");
    static_assert(std::is_base_of<idlib::exception, idlib::hll::compilation_error>::value, "");

    // The shape of the idlib tree that constrains how narrow any handler can be. idlib files
    // its argument-validation and unhandled-switch errors UNDER runtime_error, so a
    // catch (const idlib::runtime_error&) - such as the one in ModuleProfile::moduleHasIDSZ -
    // unavoidably covers them too. That is documented at the handler; it is asserted here so
    // the claim cannot quietly go stale.
    static_assert(std::is_base_of<idlib::runtime_error, idlib::unhandled_switch_case_error>::value,
                  "unhandled_switch_case_error derives from idlib::runtime_error");
    static_assert(std::is_base_of<idlib::runtime_error, idlib::invalid_argument_error>::value,
                  "invalid_argument_error derives from idlib::runtime_error");
    static_assert(std::is_base_of<idlib::runtime_error, idlib::argument_null_error>::value,
                  "argument_null_error derives from idlib::invalid_argument_error, so from runtime_error");
    static_assert(std::is_base_of<idlib::runtime_error, idlib::environment_error>::value,
                  "environment_error derives from idlib::runtime_error - this is what Font's ctor raises");
    // The one that is easy to miss, and the reason the handler comment does not claim the pair
    // keeps assertions out: idlib's assertion type is filed under runtime_error too, so
    // catch (const idlib::runtime_error&) covers it whenever _DEBUG is defined and
    // IDLIB_DEBUG_ASSERT expands to a throw (debug_assert.hpp). It is inert in this build
    // configuration, and MSVC Debug configurations define _DEBUG automatically.
    static_assert(std::is_base_of<idlib::runtime_error, idlib::debug_assertion_failed_error>::value,
                  "debug_assertion_failed_error derives from idlib::runtime_error, so a "
                  "catch (const idlib::runtime_error&) cannot exclude it");
    static_assert(std::is_base_of<idlib::hll::compilation_error, Ego::Script::MissingDelimiterError>::value,
                  "MissingDelimiterError derives from idlib::hll::compilation_error");

    // The two arms in moduleHasIDSZ are equivalent in reach to catching idlib::exception today,
    // because runtime_error and compilation_error are its only two direct subclasses. There is
    // no portable way to assert "only two", so the value of the pair is forward-looking: a new
    // branch off idlib::exception will not be swallowed by either arm. Nothing to assert here -
    // this note exists so the next reader does not mistake the pair for a narrowing it is not.
    SUCCEED();
}

//--------------------------------------------------------------------------------------------
// ModuleProfile::moduleHasIDSZ
//--------------------------------------------------------------------------------------------

/// Positive control. Without it the negative cases below would all pass against a function
/// that unconditionally returned false.
TEST_F(ContentFaultMissContractsFixture, ModuleHasIdszFindsADeclaredExpansion)
{
    EXPECT_TRUE(ModuleProfile::moduleHasIDSZ("zzmc-good.mod", IDSZ2('T', 'Y', 'P', 'E')));
}

/// A readable, well-formed menu.txt that simply does not declare the IDSZ.
TEST_F(ContentFaultMissContractsFixture, ModuleHasIdszReportsAGenuineMiss)
{
    EXPECT_FALSE(ModuleProfile::moduleHasIDSZ("zzmc-good.mod", IDSZ2('Z', 'Z', 'Z', 'Z')));
}

/// The idlib::runtime_error arm: a module folder with no menu.txt at all. The EXPECT_THROW
/// establishes that the underlying read really does raise an idlib exception on this exact
/// path, so the EXPECT_FALSE below is a statement about the contract and not about a file
/// that happened to be readable.
TEST_F(ContentFaultMissContractsFixture, ModuleHasIdszTreatsAnUnreadableMenuAsAMiss)
{
    EXPECT_THROW(ReadContext ctxt("mp_modules/zzmc-nomenu.mod/gamedat/menu.txt"), idlib::runtime_error);

    EXPECT_NO_THROW({
        EXPECT_FALSE(ModuleProfile::moduleHasIDSZ("zzmc-nomenu.mod", IDSZ2('T', 'Y', 'P', 'E')));
    });
}

/// A name that does not correspond to any folder at all - the shape scr_IfModuleHasIDSZ sees
/// when an object's message table names a module the player does not have installed.
TEST_F(ContentFaultMissContractsFixture, ModuleHasIdszTreatsAnUnknownModuleAsAMiss)
{
    EXPECT_NO_THROW({
        EXPECT_FALSE(ModuleProfile::moduleHasIDSZ("missing.mod", IDSZ2('T', 'Y', 'P', 'E')));
    });
}

/// The idlib::hll::compilation_error arm: a menu.txt too short for the fixed header sequence.
TEST_F(ContentFaultMissContractsFixture, ModuleHasIdszTreatsATruncatedMenuAsAMiss)
{
    EXPECT_NO_THROW({
        EXPECT_FALSE(ModuleProfile::moduleHasIDSZ("zzmc-truncated.mod", IDSZ2('T', 'Y', 'P', 'E')));
    });
}

/// The documented corollary: the scan is linear, so a malformed expansion line hides every
/// expansion after it. `[TYPE]` is present in this fixture but sits behind `:[STAFF]`.
TEST_F(ContentFaultMissContractsFixture, ModuleHasIdszStopsScanningAtAMalformedExpansion)
{
    EXPECT_NO_THROW({
        EXPECT_FALSE(ModuleProfile::moduleHasIDSZ("zzmc-malformed.mod", IDSZ2('T', 'Y', 'P', 'E')));
    });
}

/// The two answers the contract gives without touching the file system. IDSZ2::None is
/// IDSZ2('N','O','N','E'), not a zero value, and most shipped modules declare it as their
/// required reference IDSZ - so the `true` here is live behaviour, not a corner case.
///
/// Both assertions are arranged so that deleting the short-circuit they name would fail them.
/// The first names a module that does not exist, where the miss contract would answer false and
/// the short-circuit answers true. The second is asked against a REAL module folder named
/// "NONE" (written by SetUpTestSuite) that does declare [TYPE]: without the sentinel check the
/// read would succeed and report true.
TEST_F(ContentFaultMissContractsFixture, ModuleHasIdszShortCircuitsWithoutReadingAnything)
{
    EXPECT_TRUE(ModuleProfile::moduleHasIDSZ("no-such-module-at-all.mod", IDSZ2::None));

    // Guard the guard. The fixture carries kValidMenuTxt, the same content that
    // ModuleHasIdszFindsADeclaredExpansion pins as yielding true for [TYPE]; asserting here that
    // the file really is present and readable is what makes the assertion below non-vacuous.
    // (It cannot be checked by calling moduleHasIDSZ with some alias of the name - the sentinel
    // compares the raw string, and PhysFS rejects a "." path component.)
    ASSERT_TRUE(vfs_exists("mp_modules/NONE/gamedat/menu.txt"))
        << "the NONE fixture module must exist, otherwise the sentinel assertion below would "
           "pass by way of the miss contract instead of the short-circuit";
    ASSERT_NO_THROW(ReadContext ctxt("mp_modules/NONE/gamedat/menu.txt"));

    EXPECT_FALSE(ModuleProfile::moduleHasIDSZ("NONE", IDSZ2('T', 'Y', 'P', 'E')));
}

//--------------------------------------------------------------------------------------------
// ModuleProfile::moduleAddIDSZ
//--------------------------------------------------------------------------------------------

/// The failure path of the vfs_copyFile guard added alongside the miss contract. Before that
/// contract, moduleHasIDSZ *threw* for a module whose menu.txt cannot be read, so moduleAddIDSZ
/// never reached its write branch for such a module. Now moduleHasIDSZ reports a miss, the
/// `if (!moduleHasIDSZ(...))` guard opens, and only the copy check stops the function from
/// creating a user-directory menu.txt holding nothing but the appended expansion line - which
/// ProfileSystem::loadModuleProfiles would then fail to parse, manufacturing exactly the broken
/// module the surrounding work exists to survive.
TEST_F(ContentFaultMissContractsFixture, ModuleAddIdszReportsFailureWhenTheSourceMenuCannotBeCopied)
{
    const std::string target = "/modules/zzmc-nomenu.mod/gamedat/menu.txt";
    ASSERT_FALSE(vfs_exists(target)) << "precondition: the fixture must start with no target file";

    EXPECT_NO_THROW({
        EXPECT_FALSE(ModuleProfile::moduleAddIDSZ("zzmc-nomenu.mod", IDSZ2('T', 'S', 'Z', '2')));
    });

    // The point of the guard: no fabricated menu.txt is left behind.
    EXPECT_FALSE(vfs_exists(target));
}

//--------------------------------------------------------------------------------------------
// ObjectProfile::loadFromFile
//--------------------------------------------------------------------------------------------

/// A truncated data.txt opens fine and then fails mid-parse, raising
/// idlib::hll::compilation_error out of the ReadContext helpers. loadFromFile must report
/// that object as unloadable instead of letting the throw abort the caller's whole scan -
/// ProfileSystem::loadOneProfile and the content validator both treat nullptr as "skip".
TEST_F(ContentFaultMissContractsFixture, LoadFromFileIsolatesATruncatedDataFile)
{
    const std::string objectPath = std::string("mp_modules/") + kObjectHost + "/objects/zzmc-broken.obj";

    // The file exists and opens; only the parse fails. This distinguishes the compilation_error
    // arm from the runtime_error arm exercised by the next test.
    EXPECT_NO_THROW(ReadContext ctxt(objectPath + "/data.txt"));

    std::shared_ptr<ObjectProfile> profile;
    EXPECT_NO_THROW(profile = ObjectProfile::loadFromFile(objectPath, ObjectProfileRef(1), true));
    EXPECT_EQ(profile, nullptr);
}

/// The idlib::runtime_error arm of the same handler: an object folder with no data.txt.
TEST_F(ContentFaultMissContractsFixture, LoadFromFileIsolatesAMissingDataFile)
{
    const std::string objectPath = std::string("mp_modules/") + kObjectHost + "/objects/zzmc-nodata.obj";

    EXPECT_THROW(ReadContext ctxt(objectPath + "/data.txt"), idlib::runtime_error);

    std::shared_ptr<ObjectProfile> profile;
    EXPECT_NO_THROW(profile = ObjectProfile::loadFromFile(objectPath, ObjectProfileRef(1), true));
    EXPECT_EQ(profile, nullptr);
}
