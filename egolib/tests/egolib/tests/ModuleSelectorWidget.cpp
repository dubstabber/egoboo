//********************************************************************************************
//*
//*    This file is part of Egoboo.
//*
//*    Egoboo is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Egoboo is distributed in the hope that it will be useful, but
//*    WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Egoboo.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************

/// @file ModuleSelectorWidget.cpp
/// @brief Characterization coverage for Ego::GUI::ModuleSelector.
///
/// PLANNING NOTE (deviation from the original design): a fixture that installs a raw-storage,
/// never-constructed fake UIManager (the pattern used by ScopedPlayingStateHarness in
/// ScriptSystemsFunctions.cpp and by CameraTrackingFixture) and then constructs a real
/// ModuleSelector was attempted first and reproducibly SEGFAULTS. Root cause, confirmed with
/// a debugger:
///
///  - ModuleSelector's constructor builds its two navigation Buttons ("->" / "<-",
///    ModuleSelector.cpp ~48-49) in its MEMBER-INITIALIZER LIST, which runs to completion
///    before the constructor BODY executes. The body then calls the throwing
///    `uiManager().getScreenWidth()/getScreenHeight()` (ModuleSelector.cpp ~51-53). Because
///    member-initializers and the body run inside one continuous constructor call, the
///    installed-UIManager state is FIXED for both: there is no way for a test to have "no
///    manager" while the Buttons construct and "a manager" by the time the body runs.
///  - If no manager is installed at all: the Buttons construct fine via this pass's new
///    headless-deferred path (see GuiTextLayoutHeadless.cpp), but the constructor body's
///    `uiManager()` call throws std::logic_error before any navigation/selection logic runs
///    (see the one test below that pins exactly this).
///  - If ANY manager is installed -- even the raw, never-constructed fake this recipe called
///    for -- Button::setText() takes the seam's manager-present branch, which is BYTE-FOR-BYTE
///    the pre-existing eager path (Button.cpp: `ui->getFont(...)`, then an unconditional
///    `font->layoutText(...)`). A raw/never-constructed UIManager's `_fonts` array is
///    unformed, so the returned Font pointer is garbage or null, and dereferencing it inside
///    `Font::layoutText()` segfaults deep in SDL_ttf. This is NOT a regression introduced by
///    the tryActiveUIManager() seam: the pre-seam code called `uiManager().getFont(...)`
///    completely unconditionally whenever text was non-empty, so the exact same fake manager
///    would have crashed identically before this pass. It reproduces because
///    Font::layoutText()/layoutTextBox() are unconditionally GL-bound (Font_layout.cpp:
///    `Ego::activeRenderer()`, `Ego::activeVideoBufferManager()`) -- no test in this suite
///    installs a working Renderer or video buffer manager, and building one (idlib's
///    video_buffer_manager is a thin 2-method interface, but Font's internal FontAtlas/Texture
///    plumbing is not) was judged disproportionate to this pass and too tightly coupled to
///    Font's private implementation to be a good characterization investment.
///
/// Net effect: ModuleSelector cannot be constructed via its public constructor in this
/// headless (no live OpenGL renderer) test tier, in ANY manager-installed state, so the
/// mouse-wheel/navigation-button arithmetic that a real instance would exercise is not
/// characterizable here. What IS safely characterizable -- and pinned below -- is that
/// ModuleSelector requires an active UI manager to construct at all, and that its two
/// navigation Buttons are safely destroyed during the resulting exception unwind (standard
/// C++ guarantee, but worth pinning: it means a headless caller gets a clean, catchable
/// failure rather than a crash, as long as no manager was ever installed).

#include "gtest/gtest.h"

#include <memory>
#include <stdexcept>
#include <vector>

#include "egolib/game/GUI/ModuleSelector.hpp"
#include "egolib/game/GUI/UIManager.hpp"
#include "egolib/Profiles/_Include.hpp"

namespace {

using Ego::GUI::ModuleSelector;

/// @brief Fabricate @a n stand-in ModuleProfile instances (default ctor: pure member-init, no
///        VFS/texture IO).
std::vector<std::shared_ptr<ModuleProfile>> makeModules(size_t n) {
    std::vector<std::shared_ptr<ModuleProfile>> mods;
    mods.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        mods.push_back(std::make_shared<ModuleProfile>());
    }
    return mods;
}

TEST(ModuleSelectorWidget, ConstructionRequiresAnActiveUIManager) {
    ASSERT_EQ(Ego::GUI::tryActiveUIManager(), nullptr);   // precondition: no manager installed

    auto mods = makeModules(2);

    // The two navigation Buttons construct headlessly without issue (this pass's seam); the
    // throw comes from the constructor BODY's uiManager().getScreenWidth() call, which runs
    // strictly after them. The exception propagates cleanly out of make_shared, and the
    // already-constructed Button members are destroyed by normal stack unwinding -- this is a
    // safe, catchable failure, not a crash.
    EXPECT_THROW(std::make_shared<ModuleSelector>(mods), std::logic_error);
}

} // namespace
