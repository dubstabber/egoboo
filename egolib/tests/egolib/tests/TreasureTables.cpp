#include "gtest/gtest.h"

#include "TestEnvironment.hpp"
#include "egolib/Logic/TreasureTables.hpp"
#include "egolib/Script/Errors.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/vfs.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace
{

/// Characterization suite for Ego::TreasureTables (egolib/Logic/TreasureTables.cpp).
///
/// These tests PIN current observable behavior, including known quirks/bugs of the
/// reference-following logic in getRandomTreasure. Tests named PIN_BUG_* document a
/// suspected defect; they characterize current behavior — do not fix silently.
class TreasureTablesFixture : public ::testing::Test
{
protected:
    static std::unique_ptr<ContentRuntimeBootstrap> s_runtime;
    static std::filesystem::path s_root;

    static void writeFile(const std::filesystem::path& path, const std::string& contents)
    {
        std::ofstream out(path, std::ios::binary);
        out << contents;
        // A silently failed write would let empty-file behavior masquerade as the
        // behavior these fixtures mean to exercise.
        ASSERT_TRUE(out.good()) << "failed to write fixture file " << path;
    }

    static void SetUpTestSuite()
    {
        Ego::Test::configureDataDirectory();

        ContentRuntimeBootstrap::Options opts;
        opts.initializeVirtualFileSystem = true;
        opts.initializeBaseVfsPaths = true;
        opts.initializeLogging = true;
        // Explicit: the Options struct defaults both of these to true.
        opts.initializePerkHandler = false;
        opts.initializeProfileSystem = false;
        opts.clearBaseVfsPathsOnShutdown = true;
        opts.seedRandom = true;
        opts.randomSeed = 0;
        opts.binaryPath = "";
        opts.logPath = "/debug/treasure-tables-tests.log";
        opts.logLevel = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);

#ifdef _WIN32
        const int pid = _getpid();
#else
        const int pid = static_cast<int>(getpid());
#endif
        s_root = std::filesystem::temp_directory_path()
               / ("egoboo-treasure-" + std::to_string(pid));
        std::filesystem::create_directories(s_root);
        Ego::Test::scheduleTestDirectoryCleanup(s_root);

        writeFile(s_root / "empty.txt", "");
        // NO ':' anywhere — skipToColon is comment-blind, it only looks for colons.
        writeFile(s_root / "comments.txt", "//just a comment with no colon\n");
        writeFile(s_root / "basic.txt", ":ARMOR\n:Advent_Armor\n:END\n");
        writeFile(s_root / "emptytable.txt", ":EMPTYTBL\n:END\n");
        writeFile(s_root / "lowerend.txt", ":T\n:end\n:END\n");
        writeFile(s_root / "refchain.txt", ":A\n:%B\n:END\n:B\n:RealB\n:END\n");
        writeFile(s_root / "selfref.txt", ":A\n:%A\n:END\n");
        writeFile(s_root / "purerefs.txt", ":A\n:%B\n:%C\n:END\n:B\n:ItemB\n:END\n:C\n:ItemC\n:END\n");
        writeFile(s_root / "mixed.txt", ":MIX\n:%SUB\n:ItemA\n:END\n:SUB\n:ItemS\n:END\n");
        writeFile(s_root / "noend.txt", ":A\n:ItemA\n");
        writeFile(s_root / "badname.txt", ":A\n:123bad\n:END\n");
        writeFile(s_root / "dupdecl.txt", ":A\n:X\n:END\n:A\n:Y\n:END\n");
        writeFile(s_root / "endtable.txt", ":END\n:Item\n:END\n");
        writeFile(s_root / "refend.txt", ":A\n:%END\n:END\n");
        writeFile(s_root / "coloncomment.txt", "//note: junk\n:A\n:Item\n:END\n");

        ASSERT_NE(0, vfs_add_mount_point(s_root.string(), Ego::FsPath(""),
                                         Ego::VfsPath("mp_treasure"), 1));
    }

    static void TearDownTestSuite()
    {
        vfs_remove_mount_point(Ego::VfsPath("mp_treasure"));
        s_runtime.reset();
    }
};

std::unique_ptr<ContentRuntimeBootstrap> TreasureTablesFixture::s_runtime;
std::filesystem::path TreasureTablesFixture::s_root;

// A missing file throws idlib::runtime_error out of vfs_readEntireFile (vfs_bulk.c:56)
// via the Scanner constructor. Note: idlib::runtime_error is NOT a std::exception.
// The message is not pinned.
TEST_F(TreasureTablesFixture, ConstructorThrowsOnMissingFile)
{
    EXPECT_THROW(Ego::TreasureTables("mp_treasure/absent.txt"), idlib::runtime_error);
}

TEST_F(TreasureTablesFixture, EmptyAndCommentOnlyFilesYieldNoTables)
{
    Ego::TreasureTables empty("mp_treasure/empty.txt");
    EXPECT_EQ(empty.getRandomTreasure("%ANYTHING"), "");

    Ego::TreasureTables comments("mp_treasure/comments.txt");
    EXPECT_EQ(comments.getRandomTreasure("%ANYTHING"), "");
}

// Single-element tables consume ZERO randomness: Random::next(low == high) returns low
// before constructing a distribution (Random.hpp:94-97), so this is fully deterministic.
// The item is returned verbatim — readName never maps '_' to ' ' and never lowercases
// (ReadContext.cpp:328-334).
TEST_F(TreasureTablesFixture, SingleItemTableReturnsItemVerbatim)
{
    Ego::TreasureTables tt("mp_treasure/basic.txt");
    EXPECT_EQ(tt.getRandomTreasure("%ARMOR"), "Advent_Armor");
}

// Pins the early-outs at TreasureTables.cpp:41-43 (empty name, missing leading '%'),
// the find-miss at :67-70, and the case-sensitivity of table keys.
TEST_F(TreasureTablesFixture, GuardsReturnEmptyString)
{
    Ego::TreasureTables tt("mp_treasure/basic.txt");
    EXPECT_EQ(tt.getRandomTreasure(""), "");
    EXPECT_EQ(tt.getRandomTreasure("Advent_Armor"), "");
    EXPECT_EQ(tt.getRandomTreasure("ARMOR"), "");
    EXPECT_EQ(tt.getRandomTreasure("%MISSING"), "");
    EXPECT_EQ(tt.getRandomTreasure("%armor"), "");
    EXPECT_EQ(tt.getRandomTreasure("%"), "");
}

// An existing-but-empty table hits the result->second.empty() branch (TreasureTables.cpp:67).
TEST_F(TreasureTablesFixture, EmptyTableReturnsEmptyString)
{
    Ego::TreasureTables tt("mp_treasure/emptytable.txt");
    EXPECT_EQ(tt.getRandomTreasure("%EMPTYTBL"), "");
}

// Only the exact spelling "END" terminates a table (TreasureTables.cpp:103); a lowercase
// "end" is an ordinary treasure item.
TEST_F(TreasureTablesFixture, LowercaseEndIsATreasureItem)
{
    Ego::TreasureTables tt("mp_treasure/lowerend.txt");
    EXPECT_EQ(tt.getRandomTreasure("%T"), "end");
}

// PIN OF A SUSPECTED BUG — characterizes current behavior, do not fix silently.
// TreasureTables.cpp:64 does _treasureTables.find(treasureTableName) — the ORIGINAL
// argument — instead of find(currentEntry), so a referenced table is never consulted.
// Trace for %A = {%B}, %B = {RealB}: iteration 1 inserts %A into visited and draws %B;
// iteration 2 inserts %B but finds %A AGAIN and draws %B again; iteration 3 hits the
// visited set -> "". A fixed implementation would return "RealB" for %A.
// Direct lookup of %B works because then treasureTableName IS the right key.
TEST_F(TreasureTablesFixture, PIN_BUG_ReferencedTableIsNeverConsulted)
{
    Ego::TreasureTables tt("mp_treasure/refchain.txt");
    EXPECT_EQ(tt.getRandomTreasure("%A"), "");
    EXPECT_EQ(tt.getRandomTreasure("%B"), "RealB");
}

// %A = {%A}: iteration 1 inserts %A and draws %A; iteration 2 hits the visited set
// (circle-detection path, TreasureTables.cpp:55-58) -> "". Note this outcome is
// fix-invariant: a corrected :64 lookup would also yield "" here via the same cycle
// detection, so this pins the cycle handling rather than the lookup bug.
TEST_F(TreasureTablesFixture, SelfReferenceReturnsEmpty)
{
    Ego::TreasureTables tt("mp_treasure/selfref.txt");
    EXPECT_EQ(tt.getRandomTreasure("%A"), "");
}

// PIN OF A SUSPECTED BUG — characterizes current behavior, do not fix silently.
// Under the TreasureTables.cpp:64 bug every draw comes from %A's own table {%B,%C};
// every draw is a reference, and the visited set forces termination within at most
// 4 loop iterations. ItemB/ItemC are unreachable through %A. The outcome is
// deterministic ("" every time) despite RNG path variance in which references get drawn.
TEST_F(TreasureTablesFixture, PIN_BUG_PureReferenceTableAlwaysEmpty)
{
    Ego::TreasureTables tt("mp_treasure/purerefs.txt");
    for (int i = 0; i < 50; ++i)
    {
        EXPECT_EQ(tt.getRandomTreasure("%A"), "");
    }
}

// PIN OF A SUSPECTED BUG — characterizes current behavior, do not fix silently.
// For %MIX = {%SUB, ItemA}, %SUB = {ItemS} the reachable outcome set under the
// TreasureTables.cpp:64 bug is exactly {"", "ItemA"} — "ItemS" is unreachable. A FIXED
// implementation would return "ItemS" ~50% per call (draw %SUB with p=1/2, then SUB's
// single element), so 100 clean iterations fail loudly on a silent fix
// (P(no ItemS in 100 fixed draws) = 2^-100). Assertions are set-membership only —
// never assert exact draw sequences (uniform_int_distribution output is
// implementation-defined).
TEST_F(TreasureTablesFixture, PIN_BUG_MixedTableNeverYieldsReferencedItem)
{
    Ego::TreasureTables tt("mp_treasure/mixed.txt");
    for (int i = 0; i < 100; ++i)
    {
        const std::string r = tt.getRandomTreasure("%MIX");
        EXPECT_TRUE(r == "" || r == "ItemA") << "unexpected result: `" << r << "`";
        EXPECT_NE(r, "ItemS");
    }
}

// A table without ":END" throws Ego::Script::MissingDelimiterError from the inner
// skipToColon(false) at TreasureTables.cpp:124 via ReadContext.cpp:173 DURING scanning.
// parse()'s own "expected <end of table>" (TreasureTables.cpp:179, a plain
// std::runtime_error) is unreachable from the constructor — read() either emits a
// well-formed token stream or throws at scanner level first.
TEST_F(TreasureTablesFixture, MissingEndThrowsMissingDelimiterError)
{
    EXPECT_THROW(Ego::TreasureTables("mp_treasure/noend.txt"),
                 Ego::Script::MissingDelimiterError);
}

// readName0 rejects digit-first names ("invalid name", ReadContext.cpp:295-298) with
// idlib::hll::compilation_error. This input keeps the pin distinct from
// MissingEndThrowsMissingDelimiterError (MissingDelimiterError IS-A compilation_error).
TEST_F(TreasureTablesFixture, InvalidElementNameThrowsCompilationError)
{
    EXPECT_THROW(Ego::TreasureTables("mp_treasure/badname.txt"),
                 idlib::hll::compilation_error);
}

// Smoke test against the shipped data file. basicdat is mounted as mp_data by
// setup_init_base_vfs_paths (egoboo_setup.c:294) — no module mount needed.
// %RANDOM_ARMOR (data/basicdat/randomtreasure.txt:283-290) is the only reference-free
// table of the 16, so it dodges the reference-following bug entirely: every draw is a
// direct member and never "".
TEST_F(TreasureTablesFixture, RealDataFileSmoke_RandomArmorMembership)
{
    const std::vector<std::string> members = {
        "Advent_Armor", "Elf_Armor", "Gnome_Armor",
        "Paladin_Armor", "Rogue_Armor", "Soldier_Armor",
    };

    Ego::TreasureTables tt("mp_data/randomtreasure.txt");
    for (int i = 0; i < 20; ++i)
    {
        const std::string r = tt.getRandomTreasure("%RANDOM_ARMOR");
        EXPECT_NE(r, "");
        EXPECT_NE(std::find(members.cbegin(), members.cend(), r), members.cend())
            << "unexpected result: `" << r << "`";
    }
}

// Declaring the same table twice MERGES the element lists: parse() uses
// _treasureTables[current->text] (TreasureTables.cpp:165), which returns the existing
// vector for the second ":A ... :END" block and appends into it. Both X and Y must be
// drawable; with the suite's fixed RNG seed the draw sequence is deterministic, and
// even unseeded P(one of them never appears in 50 draws) = 2^-50.
TEST_F(TreasureTablesFixture, DuplicateTableDeclarationMergesElementLists)
{
    Ego::TreasureTables tt("mp_treasure/dupdecl.txt");
    bool sawX = false, sawY = false;
    for (int i = 0; i < 50; ++i)
    {
        const std::string r = tt.getRandomTreasure("%A");
        EXPECT_TRUE(r == "X" || r == "Y") << "unexpected result: `" << r << "`";
        sawX = sawX || (r == "X");
        sawY = sawY || (r == "Y");
    }
    EXPECT_TRUE(sawX);
    EXPECT_TRUE(sawY);
}

// "END" is only special in ELEMENT position: read()'s table-declaration readName
// (TreasureTables.cpp:121) has no END check, so ":END" declares an ordinary table
// named "%END".
TEST_F(TreasureTablesFixture, EndAsTableDeclarationCreatesEndTable)
{
    Ego::TreasureTables tt("mp_treasure/endtable.txt");
    EXPECT_EQ(tt.getRandomTreasure("%END"), "Item");
}

// The other half of the END asymmetry: readElement (TreasureTables.cpp:94-99) checks
// text == "END" only in the non-'%' branch, so ":%END" is an ordinary Reference token,
// not a table terminator. %A = {%END} then drains through the TreasureTables.cpp:64
// lookup bug: draw "%END", re-visit, "" — deterministically.
TEST_F(TreasureTablesFixture, PercentEndIsOrdinaryReference)
{
    Ego::TreasureTables tt("mp_treasure/refend.txt");
    EXPECT_EQ(tt.getRandomTreasure("%A"), "");
}

// The scanner is completely comment-blind, and a colon INSIDE a comment restructures
// the parse: in "//note: junk\n:A\n:Item\n:END\n" the outer skipToColon
// (TreasureTables.cpp:119) finds the comment's colon, readName (which skips leading
// blanks) makes "junk" the table name, and the ":A" and ":Item" lines become ELEMENTS
// of %junk. %A never exists. This is why the shipped randomtreasure.txt comments must
// stay colon-free.
TEST_F(TreasureTablesFixture, CommentWithColonHijacksParse)
{
    Ego::TreasureTables tt("mp_treasure/coloncomment.txt");
    const std::string r = tt.getRandomTreasure("%junk");
    EXPECT_TRUE(r == "A" || r == "Item") << "unexpected result: `" << r << "`";
    EXPECT_EQ(tt.getRandomTreasure("%A"), "");
}

} // namespace
