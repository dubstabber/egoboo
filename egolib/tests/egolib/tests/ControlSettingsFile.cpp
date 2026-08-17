#include "gtest/gtest.h"

#include "TestEnvironment.hpp"
#include "egolib/InputControl/ControlSettingsFile.hpp"
#include "egolib/InputControl/InputDevice.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/vfs.h"

#include <SDL.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace
{

// DefaultTarget::writev (Log/DefaultTarget.cpp) writes each record to both the log file (opened
// once for the whole target's lifetime and not flushed until the target is destroyed at
// TearDownTestSuite) and stdout (plain fputs, no fflush - DefaultTarget.cpp:91-92). Capturing
// stdout is therefore the reliable way to observe a single call's log output; reading the log
// file mid-test would race PhysFS's/libc's write buffering. The capture is reliable even though
// fputs itself is buffered because gtest's GetCapturedStdout flushes all streams before reading
// the capture back (external/googletest/googletest/src/gtest-port.cc,
// CapturedStream::GetCapturedString). Same idiom as WawaliteReadContract.cpp's
// ScopedStdoutCapture - always release() (or let the destructor drain) so a later test doesn't
// inherit an open capture.
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

/// Characterization suite for input_settings_load_vfs / input_settings_save_vfs
/// (egolib/InputControl/ControlSettingsFile.cpp).
///
/// These tests PIN current observable behavior, including known quirks/bugs of the
/// colon-driven loader. Bug-pin tests carry source-line-citing comments; they
/// characterize current behavior — do not fix silently.
///
/// No SDL_Init anywhere: SDL_GetKeyFromName/SDL_GetKeyName are static-table lookups;
/// only isButtonPressed touches SDL_GetKeyboardState and it is never called here.
class ControlSettingsFileFixture : public ::testing::Test
{
protected:
    static std::unique_ptr<ContentRuntimeBootstrap> s_runtime;

    static void SetUpTestSuite()
    {
        Ego::Test::configureDataDirectory();

        ContentRuntimeBootstrap::Options opts;
        opts.initializeVirtualFileSystem = true;
        opts.initializeBaseVfsPaths = true;
        // REQUIRED: the save-failure path calls Log::activeTarget(), which throws
        // std::logic_error if logging is uninitialized.
        opts.initializeLogging = true;
        // Explicit: the Options struct defaults both of these to true.
        opts.initializePerkHandler = false;
        opts.initializeProfileSystem = false;
        opts.clearBaseVfsPathsOnShutdown = true;
        opts.binaryPath = "";
        opts.logPath = "/debug/control-settings-tests.log";
        opts.logLevel = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);
    }

    static void TearDownTestSuite()
    {
        s_runtime.reset();
    }

    // Order-independence even under raw single-process gtest runs: reset every mapping
    // and remove any user-dir controls.txt a previous test may have saved (the user dir
    // is FIRST in the search path, so a stale copy would shadow data/controls.txt).
    void SetUp() override
    {
        using Ego::Input::InputDevice;
        for (auto& device : InputDevice::DeviceList)
        {
            for (size_t i = 0; i < static_cast<size_t>(InputDevice::InputButton::COUNT); ++i)
            {
                device.setInputMapping(static_cast<InputDevice::InputButton>(i), SDLK_UNKNOWN);
            }
        }

        std::error_code ec;
        std::filesystem::remove(userControlsPath(), ec);
    }

    // The PhysFS write dir is the user dir root (vfs.c PHYSFS_setWriteDir), and
    // fs_getUserDirectory() is exactly $EGOBOO_USER_DIR with a trailing slash
    // (file_linux.c:192-199), so saved files land here natively.
    static std::filesystem::path userControlsPath()
    {
        const char* userDir = std::getenv("EGOBOO_USER_DIR");
        EXPECT_NE(userDir, nullptr) << "EGOBOO_USER_DIR must be set by the test runner";
        return std::filesystem::path(userDir ? userDir : "") / "controls.txt";
    }

    static void writeVfsFile(const std::string& vfsPath, const std::string& content)
    {
        ASSERT_TRUE(vfs_writeEntireFile(vfsPath, content.data(), content.size()));
    }

    // The only observable surface — there is no raw-keycode getter.
    static std::string name(size_t deviceIndex, Ego::Input::InputDevice::InputButton button)
    {
        return Ego::Input::InputDevice::DeviceList[deviceIndex].getMappedInputName(button);
    }

    // 64 lines of "X\t\t: <value>\n". The labels are decorative — the loader only
    // counts colons, so a constant "X" label works exactly like the real ones.
    static std::string make64(const std::vector<std::string>& values)
    {
        EXPECT_EQ(values.size(), 64u);
        std::string out;
        for (const auto& value : values)
        {
            out += "X\t\t: " + value + "\n";
        }
        return out;
    }

    static std::string readFileBytes(const std::filesystem::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in),
                           std::istreambuf_iterator<char>());
    }

    static size_t countOccurrences(const std::string& haystack, const std::string& needle)
    {
        size_t count = 0;
        for (size_t pos = haystack.find(needle); pos != std::string::npos;
             pos = haystack.find(needle, pos + needle.size()))
        {
            ++count;
        }
        return count;
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ControlSettingsFileFixture::s_runtime;

using Button = Ego::Input::InputDevice::InputButton;

// Fresh user dir has no controls.txt, so "/controls.txt" resolves to the repo's
// data/controls.txt — the boot path (GameEngine_lifecycle.cpp:136). The template's
// "SDLK_UNKNOWN" values are non-empty, so they are looked up (SDL_GetKeyFromName
// returns 0) and stored as unbound rather than skipped.
TEST_F(ControlSettingsFileFixture, LoadShippedTemplateMapsPlayerOneAndLeavesOthersUnbound)
{
    const bool ok = input_settings_load_vfs("/controls.txt");
    EXPECT_TRUE(ok);

    EXPECT_EQ(name(0, Button::MOVE_UP), "Up");
    EXPECT_EQ(name(0, Button::MOVE_RIGHT), "Right");
    EXPECT_EQ(name(0, Button::MOVE_DOWN), "Down");
    EXPECT_EQ(name(0, Button::MOVE_LEFT), "Left");
    EXPECT_EQ(name(0, Button::JUMP), "Space");
    EXPECT_EQ(name(0, Button::USE_LEFT), "T");
    EXPECT_EQ(name(0, Button::GRAB_LEFT), "G");
    EXPECT_EQ(name(0, Button::USE_RIGHT), "Y");
    EXPECT_EQ(name(0, Button::GRAB_RIGHT), "H");
    EXPECT_EQ(name(0, Button::INVENTORY), "B");
    EXPECT_EQ(name(0, Button::STEALTH), "Left Shift");
    EXPECT_EQ(name(0, Button::CAMERA_LEFT), "SDLK_UNKNOWN");
    EXPECT_EQ(name(0, Button::CAMERA_RIGHT), "SDLK_UNKNOWN");
    EXPECT_EQ(name(0, Button::CAMERA_ZOOM_IN), "Keypad +");
    EXPECT_EQ(name(0, Button::CAMERA_ZOOM_OUT), "Keypad -");
    EXPECT_EQ(name(0, Button::CAMERA_CONTROL), "SDLK_UNKNOWN");

    for (size_t device = 1; device < 4; ++device)
    {
        for (size_t i = 0; i < static_cast<size_t>(Button::COUNT); ++i)
        {
            EXPECT_EQ(name(device, static_cast<Button>(i)), "SDLK_UNKNOWN")
                << "device " << device << " button " << i;
        }
    }
}

// The save path reproduces the shipped template byte-for-byte after a template load:
// same prose block, '\t\t: ' separators, "$FILE_VERSION 4\n\n" (fileutil.c:359-364),
// trailing '\n', all key names canonical. NOTE: byte-identity depends on SDL2's
// canonical key-name table; a renamed key in a future SDL is a REAL behavior change to
// controls.txt content — update this test deliberately, do not loosen it.
TEST_F(ControlSettingsFileFixture, SaveAfterTemplateLoadIsByteIdenticalToTemplate)
{
    ASSERT_TRUE(input_settings_load_vfs("/controls.txt"));
    EXPECT_TRUE(input_settings_save_vfs("/controls.txt"));

    const std::string saved = readFileBytes(userControlsPath());
    const std::string shipped =
        readFileBytes(Ego::Test::findRepositoryRoot() / "data" / "controls.txt");
    ASSERT_FALSE(shipped.empty());
    EXPECT_EQ(saved, shipped);
}

// Contract (fixed from a formerly-pinned bug — see the completed-passes log, Pass 365
// "controls-txt-contract"): a missing/unopenable file returns false instead of letting
// ReadContext's idlib::runtime_error (vfs_readEntireFile, vfs_bulk.c:56) escape uncaught out of
// the boot caller (GameEngine_lifecycle.cpp). ControlSettingsFile.cpp's construction guard
// catches exactly this. No mappings are applied before construction, so every device's bindings
// stay exactly as they were before the call — here, the fixture's SetUp() all-unbound default
// plus the one deliberate Q binding the test plants below, proving no reset occurs.
TEST_F(ControlSettingsFileFixture, LoadMissingFileReturnsFalseAndLeavesBindingsUntouched)
{
    using Ego::Input::InputDevice;
    InputDevice::DeviceList[0].setInputMapping(Button::MOVE_UP, SDLK_q);

    EXPECT_NO_THROW({
        const bool ok = input_settings_load_vfs("/controls-missing-xyz.txt");
        EXPECT_FALSE(ok);
    });

    // Untouched: still whatever was bound before the call, not reset to SDLK_UNKNOWN either.
    EXPECT_EQ(name(0, Button::MOVE_UP), "Q");
}

// The construction-failure branch reports through the log (ControlSettingsFile.cpp
// logControlsLoadFailure), matching the QuestLog::loadFromFile idiom for a loader that promises
// never to throw. Log::initialize was pointed at opts.logPath by SetUpTestSuite.
TEST_F(ControlSettingsFileFixture, LoadMissingFileLogsAWarningNamingTheFile)
{
    ScopedStdoutCapture capture;
    const bool ok = input_settings_load_vfs("/controls-missing-xyz.txt");
    const std::string out = capture.release();

    ASSERT_FALSE(ok);
    EXPECT_NE(out.find("WARNING: "), std::string::npos) << out;
    EXPECT_NE(out.find("controls-missing-xyz.txt"), std::string::npos) << out;
}

// PIN OF A SUSPECTED BUG — characterizes current behavior, do not fix silently.
// A short file returns false, but the mappings read so far are already applied — no
// rollback (ControlSettingsFile.cpp:107-109 returns mid-loop, :115 stores as it goes).
// Observability: unlike a missing/unopenable file (LoadMissingFileLogsAWarningNamingTheFile),
// this path is a plain `return false;` outside the try (the try covers only ReadContext
// construction, ControlSettingsFile.cpp:76-84), reached via skipToColon(true)'s optional=true
// END_OF_INPUT branch — not an exception — so logControlsLoadFailure never runs and nothing is
// logged. This is deliberate, not a gap: the exact same quirk already applies to
// input_settings_load_vfs's caller (GameEngine_lifecycle.cpp), which now logs its own single
// summary Warning whenever this function returns false, covering both cases at the boot level.
TEST_F(ControlSettingsFileFixture, LoadShortFileReturnsFalseWithPartialMappingsApplied)
{
    writeVfsFile("/controls-short.txt", "Move Up\t\t: Q\nMove Right\t\t: W\n");

    ScopedStdoutCapture capture;
    const bool ok = input_settings_load_vfs("/controls-short.txt");
    const std::string out = capture.release();
    EXPECT_FALSE(ok);
    EXPECT_EQ(out.find("WARNING: "), std::string::npos) << out;

    EXPECT_EQ(name(0, Button::MOVE_UP), "Q");
    EXPECT_EQ(name(0, Button::MOVE_RIGHT), "W");
    EXPECT_EQ(name(0, Button::MOVE_DOWN), "SDLK_UNKNOWN");
    for (size_t device = 1; device < 4; ++device)
    {
        for (size_t i = 0; i < static_cast<size_t>(Button::COUNT); ++i)
        {
            EXPECT_EQ(name(device, static_cast<Button>(i)), "SDLK_UNKNOWN")
                << "device " << device << " button " << i;
        }
    }
}

// PIN OF A SUSPECTED BUG — characterizes current behavior, do not fix silently.
// The asymmetry: an unknown key NAME silently unbinds the slot (SDL_GetKeyFromName's
// failure value 0 is stored unconditionally, ControlSettingsFile.cpp:112-116), while an
// EMPTY value skips the store and preserves the existing binding (:112 empty check).
TEST_F(ControlSettingsFileFixture, UnknownNameClearsBindingButEmptyValuePreservesIt)
{
    using Ego::Input::InputDevice;
    InputDevice::DeviceList[0].setInputMapping(Button::MOVE_UP, SDLK_a);
    InputDevice::DeviceList[0].setInputMapping(Button::MOVE_RIGHT, SDLK_b);

    std::vector<std::string> values(64, "SDLK_UNKNOWN");
    values[0] = "NotAKeyName";
    values[1] = "";
    writeVfsFile("/controls-edge.txt", make64(values));

    const bool ok = input_settings_load_vfs("/controls-edge.txt");
    EXPECT_TRUE(ok);

    EXPECT_EQ(name(0, Button::MOVE_UP), "SDLK_UNKNOWN");
    EXPECT_EQ(name(0, Button::MOVE_RIGHT), "B");
}

// readToEndOfLine strips only LEADING blanks, then keeps everything up to the newline
// (ReadContext.cpp:201-215), so "Q " reaches SDL_GetKeyFromName verbatim, fails the
// lookup (0), and unbinds the slot.
TEST_F(ControlSettingsFileFixture, TrailingWhitespaceInValueUnbindsKey)
{
    Ego::Input::InputDevice::DeviceList[0].setInputMapping(Button::MOVE_UP, SDLK_z);

    std::vector<std::string> values(64, "");
    values[0] = "Q ";
    writeVfsFile("/controls-trailing.txt", make64(values));

    const bool ok = input_settings_load_vfs("/controls-trailing.txt");
    EXPECT_TRUE(ok);

    EXPECT_EQ(name(0, Button::MOVE_UP), "SDLK_UNKNOWN");
}

// Nothing validates $FILE_VERSION: the loader doc says v3 (ControlSettingsFile.cpp:65),
// the writer emits v4 (:136), and the read loop (:99-118) never checks. The version line
// survives only because it contains no ':'.
TEST_F(ControlSettingsFileFixture, FileVersionIsNeverValidated)
{
    std::vector<std::string> values(64, "");
    values[0] = "Q";
    writeVfsFile("/controls-version.txt", "$FILE_VERSION 999\n" + make64(values));

    const bool ok = input_settings_load_vfs("/controls-version.txt");
    EXPECT_TRUE(ok);

    EXPECT_EQ(name(0, Button::MOVE_UP), "Q");
}

// PIN OF A SUSPECTED BUG — characterizes current behavior, do not fix silently.
// Labels are decorative; the loader is pure colon-counting (skipToColon,
// ReadContext.cpp:156-190). A stray colon in a comment line shifts EVERY subsequent
// binding by one slot: slot k receives the file's mapping line k-1. With 65 colons the
// loader stops after 64, so the last real mapping line is never read. Here: slot 0
// consumes "extra colon here" (unknown name -> unbind), slot 1 gets "Q", ..., slot 63
// (device 3 CAMERA_CONTROL) gets "F5", and "F9" is applied nowhere.
TEST_F(ControlSettingsFileFixture, StrayColonShiftsAllSubsequentBindings)
{
    std::vector<std::string> values(64, "F1");
    values[0] = "Q";
    values[62] = "F5";
    values[63] = "F9";
    writeVfsFile("/controls-shift.txt", "Note: extra colon here\n" + make64(values));

    const bool ok = input_settings_load_vfs("/controls-shift.txt");
    EXPECT_TRUE(ok);

    EXPECT_EQ(name(0, Button::MOVE_UP), "SDLK_UNKNOWN");
    EXPECT_EQ(name(0, Button::MOVE_RIGHT), "Q");
    EXPECT_EQ(name(0, Button::MOVE_DOWN), "F1");
    EXPECT_EQ(name(3, Button::CAMERA_CONTROL), "F5");

    // "F9" was on the 65th colon line — never read, applied nowhere.
    for (size_t device = 0; device < 4; ++device)
    {
        for (size_t i = 0; i < static_cast<size_t>(Button::COUNT); ++i)
        {
            EXPECT_NE(name(device, static_cast<Button>(i)), "F9")
                << "device " << device << " button " << i;
        }
    }
}

// Pins the writer's exact header, the mixed label style ("CAMERA_LEFT" vs
// "Camera Control", ControlSettingsFile.cpp:185-205), and the 'SDLK_UNKNOWN' literal
// emitted for unbound slots (InputDevice.cpp:54-56).
TEST_F(ControlSettingsFileFixture, SaveDefaultDeviceListWritesExactHeaderAndAllUnknown)
{
    EXPECT_TRUE(input_settings_save_vfs("/controls-default.txt"));

    const std::string saved = readFileBytes(
        std::filesystem::path(std::getenv("EGOBOO_USER_DIR")) / "controls-default.txt");
    ASSERT_FALSE(saved.empty());

    EXPECT_EQ(saved.rfind("$FILE_VERSION 4\n\nControls\n========\n", 0), 0u);
    EXPECT_NE(saved.find("\nPLAYER 1\n========\n"), std::string::npos);
    EXPECT_NE(saved.find("\nPLAYER 2\n========\n"), std::string::npos);
    EXPECT_NE(saved.find("\nPLAYER 3\n========\n"), std::string::npos);
    EXPECT_NE(saved.find("\nPLAYER 4\n========\n"), std::string::npos);
    EXPECT_EQ(countOccurrences(saved, "\t\t: SDLK_UNKNOWN\n"), 64u);
    EXPECT_NE(saved.find("CAMERA_LEFT\t\t: SDLK_UNKNOWN\n"), std::string::npos);
    const std::string tail = "Camera Control\t\t: SDLK_UNKNOWN\n";
    ASSERT_GE(saved.size(), tail.size());
    EXPECT_EQ(saved.substr(saved.size() - tail.size()), tail);
}

// The write dir (user dir) is PREPENDED to the search path (vfs_bulk.c:41-42), so a
// saved user copy of controls.txt shadows data/controls.txt on the next load — this is
// also the semantic save->load round trip. Per-test SetUp removes this file so other
// tests still see the shipped template.
TEST_F(ControlSettingsFileFixture, SavedUserFileShadowsDataTemplateOnReload)
{
    using Ego::Input::InputDevice;
    InputDevice::DeviceList[0].setInputMapping(Button::MOVE_UP, SDLK_q);
    EXPECT_TRUE(input_settings_save_vfs("/controls.txt"));

    for (auto& device : InputDevice::DeviceList)
    {
        for (size_t i = 0; i < static_cast<size_t>(Button::COUNT); ++i)
        {
            device.setInputMapping(static_cast<Button>(i), SDLK_UNKNOWN);
        }
    }

    EXPECT_TRUE(input_settings_load_vfs("/controls.txt"));
    EXPECT_EQ(name(0, Button::MOVE_UP), "Q"); // "Up" would mean the template was read
    EXPECT_EQ(name(0, Button::JUMP), "SDLK_UNKNOWN");
}

// PHYSFS_openWrite fails on an existing directory, so save returns false after logging
// a warning through the initialized Log target. NOTE (do NOT test): with logging
// uninitialized this failure path would throw std::logic_error from Log::activeTarget()
// instead of returning false — a latent early-boot crash path. Do NOT provoke failure
// with a missing parent directory instead: vfs_openWrite auto-creates parents
// (_vfs_ensure_write_directory).
TEST_F(ControlSettingsFileFixture, SaveToPathOccupiedByDirectoryReturnsFalse)
{
    ASSERT_TRUE(vfs_mkdir("/controls-collision"));
    EXPECT_FALSE(input_settings_save_vfs("/controls-collision"));
}

// SDL_GetKeyFromName normalizes: single characters lowercase to the keycode, multi-char
// names match the scancode-name table case-insensitively. A user-edited file with "q"
// or "left shift" therefore loads fine, and getMappedInputName (and hence the next
// save) emits the canonical spellings "Q" / "Left Shift" — user files are silently
// rewritten into canonical form on the next save.
TEST_F(ControlSettingsFileFixture, LoadNormalizesKeyNamesToCanonicalSpelling)
{
    std::vector<std::string> values(64, "");
    values[0] = "q";
    values[1] = "left shift";
    writeVfsFile("/controls-canonical.txt", make64(values));

    const bool ok = input_settings_load_vfs("/controls-canonical.txt");
    EXPECT_TRUE(ok);

    EXPECT_EQ(name(0, Button::MOVE_UP), "Q");
    EXPECT_EQ(name(0, Button::MOVE_RIGHT), "Left Shift");
}

} // namespace
