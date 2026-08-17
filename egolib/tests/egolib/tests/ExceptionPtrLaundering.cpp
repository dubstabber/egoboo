/// @file ExceptionPtrLaundering.cpp
/// @brief Pins for the exception-ptr-laundering fix: five sites used to do cleanup then
///        `throw std::current_exception();`, which throws the std::exception_ptr OBJECT rather
///        than the exception it points to. No handler anywhere in the tree catches
///        exception_ptr (verified by inspection; the source-scan pin below only proves the
///        thrower-side antipattern string is gone, not the absence of exception_ptr handlers),
///        so any exception crossing one of those sites became uncatchable by every typed
///        idlib/std handler in the taxonomy the content-fault-contract passes built, and
///        surfaced as "unknown exception" wherever a `catch (...)` finally stopped it.
///
/// The fixed sites are egolib/Image/Image.cpp:111,142,229 (clone/convert/pad) and
/// egolib/Renderer/RendererInfo.cpp:50,66 (constructor failure paths); the correct idiom -
/// `std::rethrow_exception(std::current_exception())`, equivalent to plain `throw;` - already
/// existed in-repo at egolib/Graphics/SDL/GraphicsWindow.cpp:358 and egolib/Core/System.cpp.
/// A related but distinct defect was also fixed: ImageManager's constructor
/// (ImageManager.cpp) used to catch(...) around registerImageLoaders(), quit SDL_image, and
/// fall off the end - finishing construction with a silently partial/empty loader list instead
/// of reporting the failure. It now rethrows too.
///
/// Three independent pins:
///  (a) PadFunctorRethrowsTheOriginalExceptionTypeRatherThanAnExceptionPtr drives the actual
///      fixed code at Image.cpp:229 with an input that makes the inner Image construction
///      throw, and asserts the escaping exception is catchable by its real idlib::runtime_error
///      type. Reaching that inner throw through the type-safe public API is not possible - every
///      surface a type-safe caller can construct maps to one of the 8 SDL pixel formats
///      Ego::SDL::getPixelFormat's switch recognises (SDL_Image_Extensions.c:247-280), which in
///      turn cover all 6 enumerators of idlib::pixel_format, so a legitimately-constructed
///      Ego::Image can never fail to reconstruct after clone/convert/pad. To exercise the
///      catch(...) block without relying on OOM, the test corrupts the *source* surface's
///      SDL_PixelFormat fields (BitsPerPixel/masks) in place after construction, then restores
///      them before the surface is destroyed - a technique verified safe here because
///      get_pixel (SDL_Image_Extensions_functors.c:142-203) indexes using the surface's own
///      pitch and only ever narrows BytesPerPixel, so no out-of-bounds read/write is introduced
///      during the window the fields are corrupted - so that pad_functor's *new* surface (built
///      from those corrupted masks) ends up with a real, SDL-recognised-but-unsupported-by-us
///      pixel format (SDL_PIXELFORMAT_RGB565), which is exactly the kind of failure the
///      production catch(...) block exists to propagate. The fields are restored afterwards
///      because classic SDL2 (unlike the sdl2-compat build used on this development machine)
///      can share a single cached, refcounted SDL_PixelFormat struct across same-format
///      surfaces; leaving the corruption in place would risk poisoning any other RGBA8888
///      surface alive in the same process.
///  (b) NoSourceFileThrowsTheExceptionPointerObject is a filesystem source scan (no precedent
///      in this suite for a source-invariant test; ContentFaultMissContracts.cpp's
///      ExceptionHierarchyIsUnrelatedToStdException pins an invariant with static_assert, but
///      that does not fit a "grep the tree" check, so this uses std::filesystem directly rather
///      than shelling out) asserting the literal antipattern string does not reappear anywhere
///      under egolib/library/src or egoboo/src. It is the cheap, broad backstop: even a new
///      site copy-pasting the old idiom trips it immediately.
///  (c) ImageManagerConstructorRethrowsRatherThanSwallowingFailure drives the ImageManager ctor
///      swallow-to-throw fix directly and headlessly, with no mocking: registerImageLoaders'
///      first statement in both branches logs through Log::activeTarget() (ImageManager.cpp:121,
///      166), which throws std::logic_error (Log/_Include.cpp) whenever no log target has been
///      installed or initialized - the default state of a fresh test process, before any
///      IMG_Init call. gtest_discover_tests (egolib/tests/CMakeLists.txt:26) runs each test as
///      a separate process invocation, so that precondition is deterministic here; the test
///      still asserts it directly rather than assuming it, in case a future global
///      ::testing::Environment starts installing a log target for the whole binary.
///
/// Mutation-checked: reverting any of the five `throw std::current_exception();` sites
/// (Image.cpp:111,142,229; RendererInfo.cpp:50,66) back to the antipattern makes (b) fail;
/// reverting Image.cpp:229 specifically also makes (a) fail; reverting the ImageManager ctor's
/// `throw;` back to swallowing makes (c) fail. The getDefaultImage doc-comment fix
/// (ImageManager.hpp) is not independently pinned by a dedicated pass/fail test.

#include "gtest/gtest.h"

#include "TestEnvironment.hpp"
#include "egolib/Image/Image.hpp"
#include "egolib/Image/ImageManager.hpp"
#include "egolib/Log/_Include.hpp"

#include "idlib/exception.hpp"

#include <SDL.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{

TEST(ExceptionPtrLaundering, PadFunctorRethrowsTheOriginalExceptionTypeRatherThanAnExceptionPtr)
{
    // A genuinely valid, recognised-format source image: SDL_PIXELFORMAT_RGBA8888 is one of
    // the 8 SDL pixel formats Ego::SDL::getPixelFormat's switch recognises
    // (SDL_Image_Extensions.c:247-280), so Ego::Image's constructor succeeds normally here -
    // no video/renderer subsystem needed, SDL_CreateRGBSurfaceWithFormat works without any
    // prior SDL_Init call.
    SDL_Surface *raw = SDL_CreateRGBSurfaceWithFormat(0, 4, 4, 32, SDL_PIXELFORMAT_RGBA8888);
    ASSERT_NE(raw, nullptr) << SDL_GetError();

    auto image = std::make_shared<Ego::Image>(raw); // Image takes ownership of raw.

    // Corrupt the *source* surface's format in place: claim it is 16 bpp RGB565 (a real SDL
    // pixel format, just not one Ego::SDL::getPixelFormat recognises) while leaving the actual
    // pixel buffer and pitch (allocated for 32 bpp) untouched. pad_functor<Image> builds its
    // *new* surface from these fields (BitsPerPixel/Rmask/Gmask/Bmask/Amask), so the new
    // surface is created honestly by SDL as SDL_PIXELFORMAT_RGB565 - a real, self-consistent
    // surface that Ego::Image simply refuses to wrap. get_pixel() only narrows BytesPerPixel
    // (4 -> 2) when reading the old surface, so it stays within the original, larger row pitch.
    // The fields are saved and restored below (rather than left corrupted) because classic
    // SDL2 builds can share a single cached, refcounted SDL_PixelFormat struct across surfaces
    // of the same format enum - leaving it corrupted could poison an unrelated RGBA8888
    // surface elsewhere in the process; on this development machine's sdl2-compat build each
    // surface gets a distinct format struct, but the restore keeps the test correct either way.
    SDL_PixelFormat *format = image->getSurface()->format;
    const SDL_PixelFormat originalFormat = *format;
    format->BitsPerPixel = 16;
    format->BytesPerPixel = 2;
    format->Rmask = 0xF800;
    format->Gmask = 0x07E0;
    format->Bmask = 0x001F;
    format->Amask = 0x0000;

    idlib::padding padding{1, 1, 1, 1};

    bool caughtRuntimeError = false;
    try
    {
        idlib::pad_functor<Ego::Image>{}(image, padding);
        FAIL() << "expected pad_functor<Image> to throw: the corrupted source format should "
                  "make SDL construct a new surface tagged SDL_PIXELFORMAT_RGB565, which "
                  "Ego::SDL::getPixelFormat's default case rejects";
    }
    catch (const idlib::runtime_error& e)
    {
        // This is the real exception type raised by Ego::SDL::getPixelFormat's default case
        // (SDL_Image_Extensions.c:277-278). Catching it here means Image.cpp:229's
        // `catch (...) { ...; throw; }` let it through unmangled.
        caughtRuntimeError = true;
        EXPECT_NE(e.message().find("pixel format"), std::string::npos) << e.message();
    }
    catch (...)
    {
        // If this fires, the escaping exception is no longer an idlib::runtime_error - the
        // classic symptom of the exception_ptr-laundering bug (Image.cpp:229 used to do
        // `throw std::current_exception();`, which throws the std::exception_ptr object
        // itself, unrelated to idlib::runtime_error, and uncatchable by name anywhere).
        FAIL() << "expected the escaping exception to be idlib::runtime_error, not some other "
                  "(or unrecognised) type - pad_functor<Image> appears to be laundering it "
                  "through std::exception_ptr again";
    }
    EXPECT_TRUE(caughtRuntimeError);

    // Restore the source surface's format before it is destroyed, so this test does not leave
    // a corrupted (and possibly shared/cached) SDL_PixelFormat behind for later code in the
    // same process to observe.
    *format = originalFormat;
}

/// Broad backstop for all five sites (and any future copy-paste of the antipattern): no source
/// file under the two runtime trees this campaign touches may contain the literal string
/// `throw std::current_exception`. Deliberately does not match the correct idiom
/// `std::rethrow_exception(std::current_exception())` (no "throw " immediately precedes
/// "std::current_exception" there), which remains in use at several sites (e.g.
/// GraphicsWindow.cpp:358, Core/System.cpp) and must keep compiling.
TEST(ExceptionPtrLaundering, NoSourceFileThrowsTheExceptionPointerObject)
{
    namespace fs = std::filesystem;

    const fs::path repoRoot = Ego::Test::findRepositoryRoot();
    const std::vector<fs::path> scanRoots = {
        repoRoot / "egolib" / "library" / "src",
        repoRoot / "egoboo" / "src",
    };
    const std::vector<std::string> scannedExtensions = { ".c", ".cpp", ".h", ".hpp", ".inl" };
    const std::string needle = "throw std::current_exception";

    std::vector<std::string> offenders;
    for (const auto& root : scanRoots)
    {
        ASSERT_TRUE(fs::is_directory(root)) << "expected scan root to exist: " << root.string();
        for (auto it = fs::recursive_directory_iterator(root); it != fs::recursive_directory_iterator(); ++it)
        {
            if (!it->is_regular_file())
            {
                continue;
            }
            const fs::path& path = it->path();
            const std::string extension = path.extension().string();
            if (std::find(scannedExtensions.begin(), scannedExtensions.end(), extension) == scannedExtensions.end())
            {
                continue;
            }

            std::ifstream file(path);
            std::string line;
            std::size_t lineNumber = 0;
            while (std::getline(file, line))
            {
                ++lineNumber;
                if (line.find(needle) != std::string::npos)
                {
                    offenders.push_back(path.string() + ":" + std::to_string(lineNumber));
                }
            }
        }
    }

    std::string offenderList;
    for (const auto& offender : offenders)
    {
        offenderList += "\n  " + offender;
    }
    EXPECT_TRUE(offenders.empty())
        << "found exception-ptr-laundering site(s) - replace `throw std::current_exception();` "
           "with plain `throw;`:" << offenderList;
}

/// Drives the ImageManager constructor's swallow-to-throw fix directly and headlessly, with no
/// mocking of IMG_Init or the loader registration itself.
TEST(ExceptionPtrLaundering, ImageManagerConstructorRethrowsRatherThanSwallowingFailure)
{
    // Precondition for the trigger below: registerImageLoaders' very first statement in either
    // branch (ImageManager.cpp:121, :166) logs through Log::activeTarget(), which throws
    // std::logic_error (Log/_Include.cpp) whenever no log target has been installed
    // (Log::installActiveTarget) or initialized (Log::initialize) - the default state of a
    // fresh test process, and happens before any IMG_Init call. gtest_discover_tests invokes a
    // fresh process per test (egolib/tests/CMakeLists.txt:26) and nothing in this binary's
    // start-up installs a log target unconditionally (ScriptSystemEnvironment.cpp installs the
    // script system only), so this should hold under the ctest gate; assert it directly rather
    // than assume it, so this test fails loudly (instead of silently passing for the wrong
    // reason) if some other test in the same process ever leaves a log target installed.
    ASSERT_THROW(Log::activeTarget(), std::logic_error)
        << "expected no log target installed/initialized yet in this process - a prior test "
           "sharing this process left one behind, which makes this test's trigger for "
           "ImageManager's constructor failure path unreliable";

    ASSERT_FALSE(Ego::ImageManager::is_initialized());

    // registerImageLoaders() throws std::logic_error before ever calling IMG_Init, so the
    // constructor's `catch (...) { if (0 != IMG_Init(0)) IMG_Quit(); throw; }` cleanup is a
    // no-op here and the exception should escape as std::logic_error, not get swallowed.
    EXPECT_THROW(Ego::ImageManager::initialize(), std::logic_error);

    // Pre-fix, the constructor swallowed the failure (quit SDL_image, then fell off the end),
    // so the singleton would finish "successfully" initialized with a silently empty loader
    // list. Post-fix, the exception thrown by `new ImageManager()` propagates out of
    // idlib::singleton::initialize() before `instance.store(o)` ever runs, so the singleton
    // must still be uninitialized.
    EXPECT_FALSE(Ego::ImageManager::is_initialized());
}

} // namespace
