/// @file MD2LoaderHardening.cpp
/// @brief Characterization tests for MD2Model::loadFromFile's content-fault contract.
///
/// MD2Model::loadFromFile (egolib/Graphics/MD2Model.cpp) used to trust every count field in the
/// file header (id_md2_header_t, id_md2.h) directly: num_st, num_tris, num_skins, num_frames,
/// and num_vertices all fed straight into std::vector sizes/resizes with no validation at all.
/// A file with a valid magic number and a negative count wraps to an enormous size_t and throws
/// std::length_error; an absurd positive count throws std::bad_alloc. Both derive from
/// std::logic_error/std::exception's "wrong" branch and escape the model-load handler in
/// ObjectProfile_load.cpp (its idlib::exception and std::runtime_error arms), turning one
/// corrupt model into a fatal abort of the whole module load. The header read itself
/// (vfs_read at the top of the function) was also unchecked, so a truncated file left the
/// header a stack-garbage struct that then drove the same allocations. The vfs_FILE* was closed
/// manually at each of two exit points, so any throw in between leaked the OS file handle.
///
/// The fix: an RAII wrapper around the vfs_FILE* (mirroring the shared_ptr<vfs_FILE> idiom in
/// wawalite_file.c's wawalite_data_write), an explicit check on the header read's return value,
/// and a validated-count/validated-offset guard between the magic-number check and the first
/// container resize - all reporting through the header's own existing nullptr + log_warning
/// contract rather than a new one. The gl-commands loop reads its own per-iteration command word
/// straight from the file on every pass - not one of the five header count fields the guard
/// above covers - and fed it into the exact same kind of resize with the exact same lack of
/// validation, so it gets a matching guard: a checked read, a check against
/// std::numeric_limits<int32_t>::min() (negating it is signed-overflow UB), and a check that the
/// command count fits inside the header's own (now-capped) size_glcmds budget.
///
/// These fixtures are raw MD2 bytes assembled by hand (little-endian int32/int16/float fields,
/// matching id_md2.h's SET_PACKED() layout), written under EGOBOO_USER_DIR/modules and read back
/// through mp_modules (mounted by setup_init_base_vfs_paths, which ContentRuntimeBootstrap's
/// initializeBaseVfsPaths option calls). No full module or profile-system bootstrap is needed -
/// MD2Model::loadFromFile only touches vfs_* and Log::activeTarget().

#include "gtest/gtest.h"

#include "TestEnvironment.hpp"
#include "egolib/Graphics/AnimatedModel.hpp"
#include "egolib/Graphics/MD2Model.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/vfs.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#ifdef __linux__
#include <dirent.h>
#include <sys/resource.h>
#endif

namespace
{

// Releases a gtest stdout capture even if the code under test throws (or an ASSERT_* failure
// fires) before the test body reaches its own GetCapturedStdout() call - see
// WawaliteReadContract.cpp for the same idiom and the reason it exists.
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

//--------------------------------------------------------------------------------------------
// Raw MD2 byte assembly.
//--------------------------------------------------------------------------------------------

void appendI32(std::vector<uint8_t>& buf, int32_t v)
{
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
    buf.insert(buf.end(), p, p + sizeof(v));
}

void appendI16(std::vector<uint8_t>& buf, int16_t v)
{
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
    buf.insert(buf.end(), p, p + sizeof(v));
}

void appendU8(std::vector<uint8_t>& buf, uint8_t v)
{
    buf.push_back(v);
}

void appendFloat(std::vector<uint8_t>& buf, float v)
{
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
    buf.insert(buf.end(), p, p + sizeof(v));
}

void appendZeros(std::vector<uint8_t>& buf, size_t n)
{
    buf.insert(buf.end(), n, 0);
}

constexpr int32_t kMd2Magic = 0x32504449;   // MD2_MAGIC_NUMBER (id_md2.h)
constexpr int32_t kMd2Version = 8;          // MD2_VERSION (id_md2.h)

/// Appends a full 17-field id_md2_header_t (68 bytes), little-endian, in file field order.
void appendHeader(std::vector<uint8_t>& buf, int32_t ident, int32_t version,
                   int32_t skinwidth, int32_t skinheight, int32_t framesize,
                   int32_t num_skins, int32_t num_vertices, int32_t num_st, int32_t num_tris,
                   int32_t size_glcmds, int32_t num_frames,
                   int32_t offset_skins, int32_t offset_st, int32_t offset_tris,
                   int32_t offset_frames, int32_t offset_glcmds, int32_t offset_end)
{
    appendI32(buf, ident);
    appendI32(buf, version);
    appendI32(buf, skinwidth);
    appendI32(buf, skinheight);
    appendI32(buf, framesize);
    appendI32(buf, num_skins);
    appendI32(buf, num_vertices);
    appendI32(buf, num_st);
    appendI32(buf, num_tris);
    appendI32(buf, size_glcmds);
    appendI32(buf, num_frames);
    appendI32(buf, offset_skins);
    appendI32(buf, offset_st);
    appendI32(buf, offset_tris);
    appendI32(buf, offset_frames);
    appendI32(buf, offset_glcmds);
    appendI32(buf, offset_end);
}

/// (a) Valid magic/version, every count safe except num_st, which is negative. num_st is the
/// first field the loader turns into a vector size (std::vector<id_md2_texcoord_t> texCoords),
/// so this isolates that one resize. No payload beyond the header - the loader must reject
/// before it ever seeks for one.
std::vector<uint8_t> makeNegativeNumStFixture()
{
    std::vector<uint8_t> buf;
    appendHeader(buf, kMd2Magic, kMd2Version,
                 /*skinwidth*/ 0, /*skinheight*/ 0, /*framesize*/ 0,
                 /*num_skins*/ 0, /*num_vertices*/ 0, /*num_st*/ -1, /*num_tris*/ 0,
                 /*size_glcmds*/ 0, /*num_frames*/ 0,
                 /*offset_skins*/ 68, /*offset_st*/ 68, /*offset_tris*/ 68,
                 /*offset_frames*/ 68, /*offset_glcmds*/ 68, /*offset_end*/ 68);
    return buf;
}

/// (b) Valid magic/version, every count safe except num_frames, set absurdly high. num_st,
/// num_tris and num_skins are all zero so the earlier resizes in loadFromFile do not fire
/// first - the loader must reject on num_frames specifically.
std::vector<uint8_t> makeAbsurdNumFramesFixture()
{
    std::vector<uint8_t> buf;
    appendHeader(buf, kMd2Magic, kMd2Version,
                 /*skinwidth*/ 0, /*skinheight*/ 0, /*framesize*/ 0,
                 /*num_skins*/ 0, /*num_vertices*/ 0, /*num_st*/ 0, /*num_tris*/ 0,
                 /*size_glcmds*/ 0, /*num_frames*/ std::numeric_limits<int32_t>::max(),
                 /*offset_skins*/ 68, /*offset_st*/ 68, /*offset_tris*/ 68,
                 /*offset_frames*/ 68, /*offset_glcmds*/ 68, /*offset_end*/ 68);
    return buf;
}

/// (c) A header truncated after num_frames (byte 44 of 68) - the six offset fields are simply
/// absent from the file. Every field that IS present is deliberately safe (valid magic/version,
/// every count zero), so the only way this fixture can misbehave is via the missing read check
/// itself, not via an incidentally-oversized count.
std::vector<uint8_t> makeTruncatedHeaderFixture()
{
    std::vector<uint8_t> buf;
    appendI32(buf, kMd2Magic);
    appendI32(buf, kMd2Version);
    appendI32(buf, /*skinwidth*/ 0);
    appendI32(buf, /*skinheight*/ 0);
    appendI32(buf, /*framesize*/ 0);
    appendI32(buf, /*num_skins*/ 0);
    appendI32(buf, /*num_vertices*/ 0);
    appendI32(buf, /*num_st*/ 0);
    appendI32(buf, /*num_tris*/ 0);
    appendI32(buf, /*size_glcmds*/ 0);
    appendI32(buf, /*num_frames*/ 0);
    // Deliberately stop here: 44 bytes total, 24 short of the 68-byte header. The six offset
    // fields are never written.
    return buf;
}

/// (e) A fully well-formed small model (0 skins, 1 texcoord, 1 triangle, 1 frame with 1 vertex,
/// no gl commands) except offset_tris, which points three bytes past the end of the file.
/// Every count is valid, so this is the only fixture that reaches the offset guard.
std::vector<uint8_t> makeOffsetBeyondFileFixture()
{
    // Layout if offsets were honest: header(68) + texcoord(4) + triangle(12) + frame(40 + 4).
    constexpr int32_t offsetSt = 68;
    constexpr int32_t offsetFrames = 84;
    constexpr int32_t fileEnd = 128;

    std::vector<uint8_t> buf;
    appendHeader(buf, kMd2Magic, kMd2Version,
                 /*skinwidth*/ 0, /*skinheight*/ 0, /*framesize*/ 44,
                 /*num_skins*/ 0, /*num_vertices*/ 1, /*num_st*/ 1, /*num_tris*/ 1,
                 /*size_glcmds*/ 0, /*num_frames*/ 1,
                 /*offset_skins*/ offsetSt, /*offset_st*/ offsetSt,
                 /*offset_tris*/ fileEnd + 3, // <-- past EOF, the defect under test
                 /*offset_frames*/ offsetFrames, /*offset_glcmds*/ fileEnd,
                 /*offset_end*/ fileEnd);

    // texcoord (4 bytes)
    appendI16(buf, 0);
    appendI16(buf, 0);
    // triangle (12 bytes)
    for (int i = 0; i < 3; ++i) appendI16(buf, 0);
    for (int i = 0; i < 3; ++i) appendI16(buf, 0);
    // frame header (40 bytes) + 1 vertex (4 bytes)
    for (int i = 0; i < 3; ++i) appendFloat(buf, 1.0f); // scale
    for (int i = 0; i < 3; ++i) appendFloat(buf, 0.0f); // translate
    appendZeros(buf, 16); // name[16]
    appendU8(buf, 0); appendU8(buf, 0); appendU8(buf, 0); appendU8(buf, 0); // vertex + normalIndex

    return buf;
}

/// (f) A well-formed, empty-geometry header (size_glcmds=2, every other count 0) whose single
/// gl-command word is INT32_MIN. `commands > 0 ? commands : -commands` on INT32_MIN is
/// signed-overflow UB (there is no positive int32_t magnitude for it) and in practice still
/// yields INT32_MIN, which sign-extends to an enormous size_t and makes
/// AnimatedModelDrawCommand::data.resize(commandCount) throw std::length_error - a value read
/// fresh from the file each loop iteration, not one of the five header count fields the earlier
/// guard covers, driving a container size the exact same unsafe way. offset_glcmds points
/// directly after the header; the command word is the only byte beyond the header the file
/// contains.
std::vector<uint8_t> makeHostileGlcmdIntMinFixture()
{
    constexpr int32_t offsetGlcmds = 68;

    std::vector<uint8_t> buf;
    appendHeader(buf, kMd2Magic, kMd2Version,
                 /*skinwidth*/ 0, /*skinheight*/ 0, /*framesize*/ 0,
                 /*num_skins*/ 0, /*num_vertices*/ 0, /*num_st*/ 0, /*num_tris*/ 0,
                 /*size_glcmds*/ 2, /*num_frames*/ 0,
                 /*offset_skins*/ offsetGlcmds, /*offset_st*/ offsetGlcmds,
                 /*offset_tris*/ offsetGlcmds, /*offset_frames*/ offsetGlcmds,
                 /*offset_glcmds*/ offsetGlcmds, /*offset_end*/ offsetGlcmds + 4);

    appendI32(buf, std::numeric_limits<int32_t>::min());
    return buf;
}

/// (g) Same shape as (f), but the single gl-command word is a large positive count
/// (100,000,000) that fits comfortably in an int32_t yet asks for far more id_glcmd_packed_t
/// entries than the declared size_glcmds (2 words total: the count word itself plus nothing left
/// over) has room for. Pre-fix this drives
/// AnimatedModelDrawCommand::data.resize(100000000) - roughly a gigabyte of
/// AnimatedModelDrawVertex - the same unbounded-allocation shape the five header counts were
/// guarded against, reached one loop iteration later through a value the header guard never
/// sees.
std::vector<uint8_t> makeHostileGlcmdOversizedCountFixture()
{
    constexpr int32_t offsetGlcmds = 68;

    std::vector<uint8_t> buf;
    appendHeader(buf, kMd2Magic, kMd2Version,
                 /*skinwidth*/ 0, /*skinheight*/ 0, /*framesize*/ 0,
                 /*num_skins*/ 0, /*num_vertices*/ 0, /*num_st*/ 0, /*num_tris*/ 0,
                 /*size_glcmds*/ 2, /*num_frames*/ 0,
                 /*offset_skins*/ offsetGlcmds, /*offset_st*/ offsetGlcmds,
                 /*offset_tris*/ offsetGlcmds, /*offset_frames*/ offsetGlcmds,
                 /*offset_glcmds*/ offsetGlcmds, /*offset_end*/ offsetGlcmds + 4);

    appendI32(buf, 100000000);
    return buf;
}

/// (d) positive control: a minimal well-formed model built the same way as (e) but with honest
/// offsets throughout, so it is a genuine "this is what a tiny legitimate MD2 looks like" fixture
/// distinct from the copied-shipped-asset positive control below.
std::vector<uint8_t> makeMinimalValidFixture()
{
    constexpr int32_t offsetSt = 68;
    constexpr int32_t offsetTris = 72;
    constexpr int32_t offsetFrames = 84;
    constexpr int32_t fileEnd = 128;

    std::vector<uint8_t> buf;
    appendHeader(buf, kMd2Magic, kMd2Version,
                 /*skinwidth*/ 0, /*skinheight*/ 0, /*framesize*/ 44,
                 /*num_skins*/ 0, /*num_vertices*/ 1, /*num_st*/ 1, /*num_tris*/ 1,
                 /*size_glcmds*/ 0, /*num_frames*/ 1,
                 /*offset_skins*/ offsetSt, /*offset_st*/ offsetSt, /*offset_tris*/ offsetTris,
                 /*offset_frames*/ offsetFrames, /*offset_glcmds*/ fileEnd,
                 /*offset_end*/ fileEnd);

    appendI16(buf, 0);
    appendI16(buf, 0);
    for (int i = 0; i < 3; ++i) appendI16(buf, 0);
    for (int i = 0; i < 3; ++i) appendI16(buf, 0);
    for (int i = 0; i < 3; ++i) appendFloat(buf, 1.0f);
    for (int i = 0; i < 3; ++i) appendFloat(buf, 0.0f);
    appendZeros(buf, 16);
    appendU8(buf, 0); appendU8(buf, 0); appendU8(buf, 0); appendU8(buf, 0);

    return buf;
}

#ifdef __linux__
/// Counts entries under /proc/self/fd, i.e. the number of open file descriptors this process
/// currently holds. Linux-only, per the task's "this is the maintained platform" allowance.
size_t countOpenFileDescriptors()
{
    size_t count = 0;
    DIR* dir = opendir("/proc/self/fd");
    if (!dir)
    {
        return 0;
    }
    while (readdir(dir) != nullptr)
    {
        ++count;
    }
    closedir(dir);
    return count;
}
#endif

} // namespace

class MD2LoaderHardeningFixture : public ::testing::Test
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

        // MD2Model::loadFromFile only needs vfs_* and Log::activeTarget() - no profile system,
        // image manager, or perk handler required. initializePerkHandler/initializeProfileSystem
        // default to true, so both are turned off explicitly here.
        ContentRuntimeBootstrap::Options opts;
        opts.initializeVirtualFileSystem   = true;
        opts.initializeBaseVfsPaths        = true;
        opts.initializeLogging             = true;
        opts.initializePerkHandler         = false;
        opts.initializeProfileSystem       = false;
        opts.clearBaseVfsPathsOnShutdown   = true;
        opts.binaryPath                    = "";
        opts.logPath                       = "/debug/md2-loader-hardening.log";
        opts.logLevel                      = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);
    }

    static void TearDownTestSuite()
    {
        s_runtime.reset();

        std::error_code ec;
        std::filesystem::remove_all(s_userModules / "md2-hardening", ec);
    }

    /// Writes raw bytes to EGOBOO_USER_DIR/modules/md2-hardening/<name> and returns the VFS
    /// path (mp_modules/...) it can be read back through.
    static std::string writeFixture(const std::string& name, const std::vector<uint8_t>& bytes)
    {
        const std::filesystem::path path = s_userModules / "md2-hardening" / name;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        out.close();
        return "mp_modules/md2-hardening/" + name;
    }
};

std::unique_ptr<ContentRuntimeBootstrap> MD2LoaderHardeningFixture::s_runtime;
std::filesystem::path MD2LoaderHardeningFixture::s_userModules;

//--------------------------------------------------------------------------------------------
// (a) valid magic + negative num_st -> nullptr, no throw, warning logged.
//--------------------------------------------------------------------------------------------

TEST_F(MD2LoaderHardeningFixture, NegativeTexCoordCountIsRejectedWithoutThrowing)
{
    const std::string path = writeFixture("negative-num-st.md2", makeNegativeNumStFixture());

    std::shared_ptr<Ego::Graphics::AnimatedModel> model;
    EXPECT_NO_THROW({ model = MD2Model::loadFromFile(path); });
    EXPECT_EQ(model, nullptr);
}

TEST_F(MD2LoaderHardeningFixture, NegativeTexCoordCountLogsAWarningNamingTheFile)
{
    const std::string path = writeFixture("negative-num-st.md2", makeNegativeNumStFixture());

    ScopedStdoutCapture capture;
    std::shared_ptr<Ego::Graphics::AnimatedModel> model = MD2Model::loadFromFile(path);
    const std::string out = capture.release();

    EXPECT_EQ(model, nullptr);
    EXPECT_NE(out.find("WARNING: "), std::string::npos) << out;
    EXPECT_NE(out.find("does not have valid header or identifier"), std::string::npos) << out;
    EXPECT_NE(out.find(path), std::string::npos) << out;
}

#ifdef __linux__
// Handle-leak check for the RAII fix: a rejected load must not increase the process's open
// file-descriptor count. Pre-fix, this exact fixture makes texCoords' constructor throw
// std::length_error while `f` is still open with no handler between here and the manual
// vfs_close(f) at the end of the function, so the vfs_FILE* (and the OS file handle underneath
// it) leaks on every call.
TEST_F(MD2LoaderHardeningFixture, RejectedLoadDoesNotLeakTheFileHandle)
{
    const std::string path = writeFixture("negative-num-st.md2", makeNegativeNumStFixture());

    // One warm-up call first: PhysFS/vfs bookkeeping (mount tables, etc.) can itself open
    // descriptors lazily on first use, which would otherwise show up as a false "leak" here.
    MD2Model::loadFromFile(path);

    const size_t before = countOpenFileDescriptors();
    ASSERT_GT(before, 0u);

    std::shared_ptr<Ego::Graphics::AnimatedModel> model;
    EXPECT_NO_THROW({ model = MD2Model::loadFromFile(path); });
    EXPECT_EQ(model, nullptr);

    const size_t after = countOpenFileDescriptors();
    EXPECT_EQ(after, before);
}
#endif

//--------------------------------------------------------------------------------------------
// (b) valid magic + absurd num_frames -> nullptr, no throw.
//--------------------------------------------------------------------------------------------

TEST_F(MD2LoaderHardeningFixture, AbsurdFrameCountIsRejectedWithoutThrowing)
{
    const std::string path = writeFixture("absurd-num-frames.md2", makeAbsurdNumFramesFixture());

#ifdef __linux__
    // Pre-fix, this fixture drives frames.resize(INT32_MAX) - roughly 2 billion
    // AnimatedModelFrame objects, each holding its own std::vector - which requests on the
    // order of hundreds of gigabytes from the allocator. On this machine that request would
    // very likely succeed at the virtual-memory-reservation stage (Linux overcommit) and only
    // fail once construction starts touching pages, which risks a slow run or an OOM kill
    // instead of a clean, fast std::bad_alloc. Capping this process's own address space with
    // RLIMIT_AS first forces the allocator to fail immediately and safely, without ever
    // touching bulk memory - verified empirically in isolation before this test was written.
    struct rlimit original{};
    ASSERT_EQ(getrlimit(RLIMIT_AS, &original), 0);
    struct rlimit limited = original;
    limited.rlim_cur = 512ull * 1024 * 1024; // 512 MiB: far above this test binary's own
                                              // footprint, far below what the corrupt header asks for.
    ASSERT_EQ(setrlimit(RLIMIT_AS, &limited), 0);
#endif

    std::shared_ptr<Ego::Graphics::AnimatedModel> model;
    EXPECT_NO_THROW({ model = MD2Model::loadFromFile(path); });
    EXPECT_EQ(model, nullptr);

#ifdef __linux__
    ASSERT_EQ(setrlimit(RLIMIT_AS, &original), 0);
#endif
}

//--------------------------------------------------------------------------------------------
// (c) header truncated mid-struct -> nullptr, no throw.
//--------------------------------------------------------------------------------------------

TEST_F(MD2LoaderHardeningFixture, TruncatedHeaderIsRejectedWithoutThrowing)
{
    const std::string path = writeFixture("truncated-header.md2", makeTruncatedHeaderFixture());

    std::shared_ptr<Ego::Graphics::AnimatedModel> model;
    EXPECT_NO_THROW({ model = MD2Model::loadFromFile(path); });
    EXPECT_EQ(model, nullptr);
}

//--------------------------------------------------------------------------------------------
// (e) a hostile offset that would seek/read past the end of the file -> nullptr, no throw.
//--------------------------------------------------------------------------------------------

TEST_F(MD2LoaderHardeningFixture, OffsetPastEndOfFileIsRejectedWithoutThrowing)
{
    const std::string path = writeFixture("offset-past-eof.md2", makeOffsetBeyondFileFixture());

    std::shared_ptr<Ego::Graphics::AnimatedModel> model;
    EXPECT_NO_THROW({ model = MD2Model::loadFromFile(path); });
    EXPECT_EQ(model, nullptr);
}

//--------------------------------------------------------------------------------------------
// (f)/(g) a hostile per-iteration gl-command word -> nullptr, no throw. `commands` is read fresh
// from the file inside the glcmds loop, not one of the five header count fields the earlier
// guard validates, so it needs (and, post-fix, has) its own guard against the same escape.
//--------------------------------------------------------------------------------------------

TEST_F(MD2LoaderHardeningFixture, GlcmdCommandWordIntMinIsRejectedWithoutThrowing)
{
    const std::string path = writeFixture("glcmd-int-min.md2", makeHostileGlcmdIntMinFixture());

    std::shared_ptr<Ego::Graphics::AnimatedModel> model;
    EXPECT_NO_THROW({ model = MD2Model::loadFromFile(path); });
    EXPECT_EQ(model, nullptr);
}

TEST_F(MD2LoaderHardeningFixture, GlcmdOversizedCommandCountIsRejectedWithoutThrowing)
{
    const std::string path = writeFixture("glcmd-oversized-count.md2", makeHostileGlcmdOversizedCountFixture());

#ifdef __linux__
    // Same RLIMIT_AS rationale as AbsurdFrameCountIsRejectedWithoutThrowing: pre-fix this
    // fixture requests on the order of a gigabyte from the allocator, which Linux overcommit
    // could otherwise let succeed at the reservation stage and only fail once pages are
    // touched, risking a slow run or an OOM kill instead of a clean, fast std::bad_alloc.
    struct rlimit original{};
    ASSERT_EQ(getrlimit(RLIMIT_AS, &original), 0);
    struct rlimit limited = original;
    limited.rlim_cur = 512ull * 1024 * 1024; // 512 MiB: far above this test binary's own
                                              // footprint, far below what the corrupt command word asks for.
    ASSERT_EQ(setrlimit(RLIMIT_AS, &limited), 0);
#endif

    std::shared_ptr<Ego::Graphics::AnimatedModel> model;
    EXPECT_NO_THROW({ model = MD2Model::loadFromFile(path); });
    EXPECT_EQ(model, nullptr);

#ifdef __linux__
    ASSERT_EQ(setrlimit(RLIMIT_AS, &original), 0);
#endif
}

//--------------------------------------------------------------------------------------------
// (d) positive controls: well-formed files still load. Without these, every negative test above
// would pass against a function that had simply stopped loading any MD2 at all.
//--------------------------------------------------------------------------------------------

TEST_F(MD2LoaderHardeningFixture, MinimalHandBuiltModelStillLoads)
{
    const std::string path = writeFixture("minimal-valid.md2", makeMinimalValidFixture());

    std::shared_ptr<Ego::Graphics::AnimatedModel> model;
    EXPECT_NO_THROW({ model = MD2Model::loadFromFile(path); });
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->getVertexCount(), 1u);
    ASSERT_EQ(model->getFrames().size(), 1u);
    EXPECT_EQ(model->getFrames().front().vertexList.size(), 1u);
}

TEST_F(MD2LoaderHardeningFixture, SmallestShippedModelStillLoads)
{
    // The stronger control: a genuine shipped asset rather than a hand-built one.
    // acidrain.obj/tris.md2 is the smallest tris.md2 in data/ (184 bytes; 1 skin, 1 texcoord,
    // 0 triangles, 1 frame with 1 vertex, one terminating gl command).
    const std::filesystem::path src = Ego::Test::findRepositoryRoot() / "data"
        / "basicdat" / "globalobjects" / "magic" / "acidrain.obj" / "tris.md2";
    ASSERT_TRUE(std::filesystem::is_regular_file(src)) << "fixture template model missing: " << src.string();

    std::ifstream in(src, std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ASSERT_EQ(bytes.size(), 184u);

    const std::string path = writeFixture("shipped-smallest.md2", bytes);

    std::shared_ptr<Ego::Graphics::AnimatedModel> model;
    EXPECT_NO_THROW({ model = MD2Model::loadFromFile(path); });
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->getVertexCount(), 1u);
    ASSERT_EQ(model->getFrames().size(), 1u);
}
