/// @file FansTxtBounds.cpp
/// @brief Characterization + defect-fixture tests for tile_dictionary_load_vfs's content-fault
///        contract (egolib/FileFormats/MapTileDefinitionsDictionary.{hpp,cpp} and
///        egolib/FileFormats/map_tile_dictionary.c).
///
/// Every range check in MapTileDefinitionsDictionary.cpp used to be an empty
/// "@todo Implement diagnostics" block, so content-controlled counts (vertex count, index-list
/// count, per-definition total index count) flowed unchecked into map_tile_dictionary.c's fixed
/// C arrays (tile_definition_t::vertices[MAP_FAN_VERTICES_MAX], ::command_entries[MAP_FAN_MAX],
/// ::command_verts[MAP_FAN_ENTRIES_MAX]), producing out-of-bounds writes for a fans.txt with
/// too many vertices/commands/indices. A single existing rejection path (definition_count >
/// MAP_FAN_TYPE_MAX inside tile_dictionary_load_vfs itself) was also being silently discarded by
/// its one runtime caller, MeshLoader::operator() (egolib/game/mesh_loader.c), degrading into
/// building the module mesh against a freshly-reset, empty tile dictionary instead of failing
/// cleanly. (cartman_tile_dictionary_load_vfs in cartman/src/cartman/cartman_map.c:1098 still
/// discards the same return value; out of scope here since cartman is CMake-gated behind
/// EGOBOO_BUILD_CARTMAN=OFF and this pass's defect (3) named only mesh_loader.c.)
///
/// IMPORTANT - the vertex "position" field's candidate bound turned out to be wrong: the shipped
/// data/basicdat/fans.txt file (see ShippedFansTxtParsesExpectedShape below) contains vertex
/// positions up to 48, and MapTileDefinitionsDictionary.html documents the field as an unbounded
/// uint32 (decode: x = position % 4, y = (position / 4) % 4 - only the low 4 bits are load-
/// bearing). map_tile_dictionary.c never uses `position` (or the `.ref`/`.grid_ix`/`.grid_iy`
/// fields derived from it) to index any array - it only feeds a bitmask decode that is well-
/// defined for any int - so there is no real upper bound to enforce there. Only negative values
/// (not a valid encoding of the documented unsigned field) are rejected.

#include "gtest/gtest.h"

#include "TestEnvironment.hpp"
#include "egolib/FileFormats/Globals.hpp"
#include "egolib/FileFormats/MapTileDefinitionsDictionary.hpp"
#include "egolib/FileFormats/map_tile_dictionary.h"
#include "egolib/egoboo_setup.h"
#include "egolib/file_common.h"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/mesh.h"
#include "egolib/vfs.h"
#include "idlib/exception.hpp"
#include "idlib/hll.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace
{

constexpr char kScratchRoot[] = "fans-txt-bounds-tests";

class FansTxtBoundsFixture : public ::testing::Test
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

        ContentRuntimeBootstrap::Options opts;
        opts.initializeVirtualFileSystem   = true;
        opts.initializeBaseVfsPaths        = true;
        // The pre-existing ">MAP_FAN_TYPE_MAX" rejection inside tile_dictionary_load_vfs logs via
        // Log::activeTarget(), which throws if no log target is installed - needed for the
        // DefinitionCountOverflow fixtures below.
        opts.initializeLogging             = true;
        opts.initializePerkHandler         = false;
        opts.initializeProfileSystem       = false;
        opts.clearModuleVfsPathsOnShutdown = true;
        opts.clearBaseVfsPathsOnShutdown   = true;
        opts.binaryPath                    = "";
        opts.logPath                       = "/debug/fans-txt-bounds.log";
        opts.logLevel                      = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);
    }

    static void TearDownTestSuite()
    {
        s_runtime.reset();
    }

    void SetUp() override
    {
        vfs_removeDirectoryAndContents(kScratchRoot);
    }

    void TearDown() override
    {
        vfs_removeDirectoryAndContents(kScratchRoot);
        // tile_dict is a process-wide global (egolib/FileFormats/Globals.cpp). Every scenario
        // below leaves it in a defined state on its own (either untouched, cleanly reset-and-
        // unloaded, or loaded from well-formed content), but reset it defensively so a failed
        // assertion mid-test can never leak a corrupted dictionary into a later, unrelated test.
        tile_dict = tile_dictionary_t();
    }

    static std::string writeFixture(const std::string& name, const std::string& content)
    {
        const std::string path = std::string(kScratchRoot) + "/" + name;
        EXPECT_TRUE(vfs_mkdir(kScratchRoot));
        vfs_FILE* file = vfs_openWrite(path);
        EXPECT_NE(file, nullptr);
        vfs_printf(file, "%s", content.c_str());
        vfs_close(file);
        return path;
    }

};

std::unique_ptr<ContentRuntimeBootstrap> FansTxtBoundsFixture::s_runtime;
std::filesystem::path FansTxtBoundsFixture::s_userModules;

// Mounts a scratch directory containing a synthetic fans.txt onto mp_data, *ahead of* the real
// basicdat mount, so "mp_data/fans.txt" (the fixed path MeshLoader::operator() reads) resolves to
// the synthetic file instead of the shipped one - the same "a fans.txt dropped in the user
// directory shadows mp_data" mechanism the defect's origin pass (354) described. RAII so the
// mount is always undone (an ASSERT_*/FAIL() early-return inside the guarded scope still runs the
// destructor), unlike a plain "unmount at the end of the test" call would.
//
// Verified: egoboo_setup.c's own module-gamedat mount (setup_init_module_vfs_paths) is NOT
// sufficient for this - it calls vfs_add_mount_point(..., append=1) both for that mount and for
// the base basicdat mount, and PHYSFS_mount's `appendToPath` genuinely appends (searched last), so
// the module mount never wins against an already-mounted, already-present shipped file. This
// guard instead calls vfs_add_mount_point directly with append=0 (prepend, PhysFS searches it
// first), which is unambiguous regardless of mount order.
class ScopedShadowingFansMount
{
public:
    ScopedShadowingFansMount(const std::filesystem::path& userRoot, const std::string& subDir, const std::string& content)
    {
        const std::filesystem::path dir = userRoot / subDir;
        std::filesystem::create_directories(dir);

        std::ofstream out(dir / "fans.txt", std::ios::binary | std::ios::trunc);
        out << content;
        out.close();
        EXPECT_TRUE(out.good());

        vfs_add_mount_point(dir.string(), Ego::FsPath(""), Ego::VfsPath("mp_data"), 0 /* prepend */);
    }

    ~ScopedShadowingFansMount()
    {
        // Tears down *every* physical directory mounted at mp_data (this scratch one included),
        // so the base basicdat mount has to be re-added afterward - mirroring exactly what
        // setup_init_base_vfs_paths() itself does for mp_data (egoboo_setup.c:339) - to avoid
        // disturbing the suite-wide mount other tests in this file rely on (mp_data/fans.txt in
        // ShippedFansTxtParsesExpectedShape).
        vfs_remove_mount_point(Ego::VfsPath("mp_data"));
        vfs_add_mount_point(fs_getDataDirectory(), Ego::FsPath("basicdat"), Ego::VfsPath("mp_data"), 1);
    }
};

//--------------------------------------------------------------------------------------------
// Minimal DDL builders. vfs_get_next_int/vfs_get_next_float both call ctxt.skipToColon(false)
// before reading a literal, so any text may precede a ':' and no separators are required between
// a literal and the next ':' - readIntegerLiteral/readRealLiteral stop as soon as they hit a
// non-digit. A colon-prefixed token is therefore self-contained.
//--------------------------------------------------------------------------------------------

std::string tok(long long value)
{
    return ":" + std::to_string(value) + " ";
}

// One vertex block: position, u, v (readRealLiteral accepts plain digits with no decimal point).
std::string vertexTok(long long position, long long u = 0, long long v = 0)
{
    return tok(position) + tok(u) + tok(v);
}

//--------------------------------------------------------------------------------------------
// (a) MANDATORY FIRST STEP: pin the shipped fans.txt's parsed shape.
//--------------------------------------------------------------------------------------------

TEST_F(FansTxtBoundsFixture, ShippedFansTxtParsesExpectedShape)
{
    ASSERT_TRUE(vfs_exists("mp_data/fans.txt"));

    tile_dictionary_t dict;
    ASSERT_TRUE(tile_dictionary_load_vfs("mp_data/fans.txt", dict));

    EXPECT_TRUE(dict.loaded);
    // 26 real definitions -> fantype_offset = 2 * 2^floor(log2(26)) = 2*16 = 32,
    // definition_count = 2*32 = 64 (exactly MAP_FAN_TYPE_MAX - the shipped file sits right at the
    // boundary of the pre-existing ">MAP_FAN_TYPE_MAX" rejection without tripping it).
    EXPECT_EQ(dict.offset, 32u);
    EXPECT_EQ(dict.def_count, 64u);

    // Spot-check a handful of entries by (vertex count, command count):
    //   type 0 "Two Faced Ground":   4 vertices, 1 command
    //   type 5 "Eighteen Faced Pillar": 16 vertices, 4 commands - exactly at the new upper
    //     bounds this pass adds (16 == MAP_FAN_VERTICES_MAX, 4 == MAP_FAN_MAX), so this single
    //     entry also proves the new checks use a strict "> max" (reject), not ">= max".
    //   type 6 "Blank": 0 vertices, 0 commands
    //   type 24 "Twelve Faced Stair (WE)": 14 vertices, 3 commands
    EXPECT_EQ(dict.def_lst[0].numvertices, 4);
    EXPECT_EQ(dict.def_lst[0].command_count, 1);
    EXPECT_EQ(dict.def_lst[5].numvertices, 16);
    EXPECT_EQ(dict.def_lst[5].command_count, 4);
    EXPECT_EQ(dict.def_lst[6].numvertices, 0);
    EXPECT_EQ(dict.def_lst[6].command_count, 0);
    EXPECT_EQ(dict.def_lst[24].numvertices, 14);
    EXPECT_EQ(dict.def_lst[24].command_count, 3);

    // type 3 "Eight Faced Ground"'s 9th vertex is "Ref: 48" - a real, shipped vertex position
    // above the falsified "0..15" candidate bound (see the file-level comment above). This must
    // still parse and store cleanly; a fix that clamps/rejects "position > 15" would break this.
    ASSERT_EQ(dict.def_lst[3].numvertices, 9);
    EXPECT_EQ(dict.def_lst[3].vertices[8].ref, 48);

    // The "big" duplicate at [type + offset] mirrors the "small" one (map_tile_dictionary.c's
    // Dupe comments).
    EXPECT_EQ(dict.def_lst[5 + 32].numvertices, 16);
    EXPECT_EQ(dict.def_lst[5 + 32].command_count, 4);
}

//--------------------------------------------------------------------------------------------
// (b) Defect fixtures: oversized counts that would overflow map_tile_dictionary.c's fixed
// arrays. Each fixture supplies FULL, well-formed data past the offending count (not just the
// count itself) so that, pre-fix, parsing completes without an incidental "ran out of input"
// exception from ReadContext - the only thing that should turn these away is the new bound.
//--------------------------------------------------------------------------------------------

// tile_definition_t::vertices is vertex_t[MAP_FAN_VERTICES_MAX] (16). 17 real vertices, 0 index
// lists.
TEST_F(FansTxtBoundsFixture, RejectsOversizedVertexCount)
{
    std::string content = tok(1) /* numberOfDefinitions */;
    content += tok(17) /* numberOfVertices */;
    for (int i = 0; i < 17; ++i)
    {
        content += vertexTok(0, 0, 0);
    }
    content += tok(0) /* numberOfIndexLists */;

    const std::string path = writeFixture("oversized-vertices.txt", content);

    tile_dictionary_t dict;
    try
    {
        tile_dictionary_load_vfs(path, dict);
        FAIL() << "expected idlib::hll::compilation_error for a 17-vertex tile definition";
    }
    catch (const idlib::hll::compilation_error& e)
    {
        EXPECT_EQ(e.get_location().file_name(), path);
        EXPECT_NE(e.to_string().find("vertices"), std::string::npos) << e.to_string();
    }
}

// tile_definition_t::command_entries is uint8_t[MAP_FAN_MAX] (4). 5 real (empty) index lists.
TEST_F(FansTxtBoundsFixture, RejectsOversizedIndexListCount)
{
    std::string content = tok(1) /* numberOfDefinitions */;
    content += tok(0) /* numberOfVertices */;
    content += tok(5) /* numberOfIndexLists */;
    for (int i = 0; i < 5; ++i)
    {
        content += tok(0) /* numberOfIndices */;
    }

    const std::string path = writeFixture("oversized-index-lists.txt", content);

    tile_dictionary_t dict;
    try
    {
        tile_dictionary_load_vfs(path, dict);
        FAIL() << "expected idlib::hll::compilation_error for a 5-index-list tile definition";
    }
    catch (const idlib::hll::compilation_error& e)
    {
        EXPECT_EQ(e.get_location().file_name(), path);
        EXPECT_NE(e.to_string().find("index list"), std::string::npos) << e.to_string();
    }
}

// tile_definition_t::command_verts is uint16_t[MAP_FAN_ENTRIES_MAX] (32), and the contiguous
// write index accumulates ACROSS a definition's index lists (map_tile_dictionary.c's
// `contiguousIndex`). Two index lists of 20 indices each (40 total) individually stay well under
// 32, so this specifically exercises the cross-list running total, not a per-list cap.
TEST_F(FansTxtBoundsFixture, RejectsOversizedTotalIndexCount)
{
    std::string content = tok(1) /* numberOfDefinitions */;
    content += tok(0) /* numberOfVertices */;
    content += tok(2) /* numberOfIndexLists */;
    for (int list = 0; list < 2; ++list)
    {
        content += tok(20) /* numberOfIndices */;
        for (int i = 0; i < 20; ++i)
        {
            content += tok(0) /* index */;
        }
    }

    const std::string path = writeFixture("oversized-total-indices.txt", content);

    tile_dictionary_t dict;
    try
    {
        tile_dictionary_load_vfs(path, dict);
        FAIL() << "expected idlib::hll::compilation_error for 40 total indices across 2 lists";
    }
    catch (const idlib::hll::compilation_error& e)
    {
        EXPECT_EQ(e.get_location().file_name(), path);
        EXPECT_NE(e.to_string().find("indices"), std::string::npos) << e.to_string();
    }
}

// A negative vertex position is not a valid encoding of the documented unsigned "position" field
// (see the file-level comment). There is deliberately no upper-bound assertion here - see
// ShippedFansTxtParsesExpectedShape's Ref:48 check above for why "position > 15" would be wrong.
TEST_F(FansTxtBoundsFixture, RejectsNegativeVertexPosition)
{
    std::string content = tok(1) /* numberOfDefinitions */;
    content += tok(1) /* numberOfVertices */;
    content += vertexTok(-1, 0, 0);
    content += tok(0) /* numberOfIndexLists */;

    const std::string path = writeFixture("negative-vertex-position.txt", content);

    tile_dictionary_t dict;
    try
    {
        tile_dictionary_load_vfs(path, dict);
        FAIL() << "expected idlib::hll::compilation_error for a negative vertex position";
    }
    catch (const idlib::hll::compilation_error& e)
    {
        EXPECT_EQ(e.get_location().file_name(), path);
        EXPECT_NE(e.to_string().find("position"), std::string::npos) << e.to_string();
    }
}

// 33 minimal (0 vertices, 0 index lists) definitions -> fantype_offset =
// 2*2^floor(log(33)/log(2.0f)) = 2*32 = 64, definition_count = 2*64 = 128 >
// MAP_FAN_TYPE_MAX(64). This is the PRE-EXISTING rejection at map_tile_dictionary.c:55-59
// (tile_dictionary_load_vfs returns false) - not a new check from this pass. Exercised directly
// here as the input condition for the MeshLoaderHonorsTileDictionaryFailure test below
// (defect 3).
//
// Note this boundary is NOT exactly at fantype_count == 32: map_tile_dictionary.c computes the
// power-of-two step via `std::floor(std::log(fantype_count) / std::log(2.0f))`, mixing a double
// std::log(int) with a float std::log(2.0f). The float rounding of log(2.0f) makes
// log(32)/log(2.0f) evaluate to ~4.999999986 instead of exactly 5.0, so floor() yields 4 (not 5)
// at fantype_count == 32 - one short of what exact real-number math would give. 33 definitions
// clears that off-by-a-hair margin.
TEST_F(FansTxtBoundsFixture, DefinitionCountOverflowIsRejectedByExistingCheck)
{
    std::string content = tok(33) /* numberOfDefinitions */;
    for (int i = 0; i < 33; ++i)
    {
        content += tok(0) /* numberOfVertices */;
        content += tok(0) /* numberOfIndexLists */;
    }

    const std::string path = writeFixture("definition-count-overflow.txt", content);

    tile_dictionary_t dict;
    EXPECT_FALSE(tile_dictionary_load_vfs(path, dict));
    EXPECT_FALSE(dict.loaded);
}

//--------------------------------------------------------------------------------------------
// (c) mesh_loader.c contract: MeshLoader::operator() must honor tile_dictionary_load_vfs's
// return value instead of discarding it.
//--------------------------------------------------------------------------------------------

// Verified: the sibling failure paths at mesh_loader.c:95-110 throw idlib::runtime_error with
// messages "unable to load mesh of module `X`" (map.load failure) and "unable to convert mesh of
// module `X`" (convert failure). Pre-fix, an oversized tile dictionary falls all the way through
// to the first of those - map.load("mp_data/level.mpd") fails (this synthetic module has no
// level.mpd) and throws THAT message instead, which is why this test asserts on the message
// content and not merely "did it throw".
TEST_F(FansTxtBoundsFixture, MeshLoaderHonorsTileDictionaryFailure)
{
    std::string content = tok(33) /* numberOfDefinitions */;
    for (int i = 0; i < 33; ++i)
    {
        content += tok(0) /* numberOfVertices */;
        content += tok(0) /* numberOfIndexLists */;
    }
    ScopedShadowingFansMount mount(s_userModules.parent_path(), "meshloader-oversized-dict", content);
    ASSERT_TRUE(vfs_exists("mp_data/fans.txt"));
    ASSERT_FALSE(vfs_exists("mp_data/level.mpd"));

    MeshLoader loader;
    try
    {
        loader("meshloader-oversized-dict.mod");
        FAIL() << "expected idlib::runtime_error for an oversized tile dictionary";
    }
    catch (const idlib::runtime_error& e)
    {
        EXPECT_NE(e.message().find("tile dictionary"), std::string::npos) << e.message();
    }
}

} // namespace
