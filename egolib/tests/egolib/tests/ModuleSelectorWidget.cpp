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
/// HISTORY (superseded by Pass 343's Ego::GUI::IUIManager seam + Ego::Test::HeadlessUIManager
/// stub): a fixture that installed a raw-storage, never-constructed fake UIManager (the pattern
/// used at the time by ScopedPlayingStateHarness in ScriptSystemsFunctions.cpp and by
/// CameraTrackingFixture) and then constructed a real ModuleSelector was attempted first and
/// reproducibly SEGFAULTED. Root cause, confirmed with a debugger:
///
///  - ModuleSelector's constructor builds its two navigation Buttons ("->" / "<-",
///    ModuleSelector.cpp ~48-49) in its MEMBER-INITIALIZER LIST, which runs to completion
///    before the constructor BODY executes. The body then calls `uiManager().getScreenWidth()
///    /getScreenHeight()` (ModuleSelector.cpp ~51-53).
///  - If no manager is installed at all: the Buttons construct fine via the headless-deferred
///    path (see GuiTextLayoutHeadless.cpp), but the constructor body's `uiManager()` call
///    throws std::logic_error before any navigation/selection logic runs (see the test below
///    that pins exactly this).
///  - If a manager IS installed, Button::setText() takes the seam's manager-present branch,
///    which unconditionally calls `ui->getFont(...)` then `font->layoutText(...)`. A raw,
///    never-constructed UIManager's `_fonts` array is unformed, so the returned Font pointer is
///    garbage or null, and dereferencing it inside `Font::layoutText()` segfaults deep in
///    SDL_ttf. This was never a defect in ModuleSelector or the seam itself -- it was purely an
///    artifact of the raw-storage fake being an UNFORMED object, not a real UIManager.
///
/// Pass 343 retired the raw-storage-fake idiom in favor of Ego::Test::HeadlessUIManager, a
/// properly-constructed (if GL/SDL_ttf-free) Ego::GUI::IUIManager implementation. With a real
/// object installed, ModuleSelector's full constructor -- navigation Buttons AND the
/// screen-size-derived layout arithmetic -- now runs to completion headlessly; see
/// GuiHeadlessUIManagerStub.cpp for that coverage (including the mouse-wheel/navigation-button
/// geometry this file's introduction originally judged uncharacterizable). What remains here is
/// the no-manager-at-all case: ModuleSelector still requires an active UI manager to construct,
/// and its two navigation Buttons are safely destroyed during the resulting exception unwind
/// (standard C++ guarantee, but worth pinning: a headless caller with no manager installed gets
/// a clean, catchable failure rather than a crash).

#include "gtest/gtest.h"

#include <memory>
#include <stdexcept>
#include <vector>

#include "egolib/game/GUI/ModuleSelector.hpp"
#include "egolib/game/GUI/UIManager.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "HeadlessUIManager.hpp"

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

TEST(ModuleSelectorWidget, ConstructionSucceedsWithAHeadlessUIManagerInstalled) {
    ASSERT_EQ(Ego::GUI::tryActiveUIManager(), nullptr);   // precondition: no manager installed

    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    auto mods = makeModules(2);

    // Unlike the raw-storage, never-constructed fake this file's introduction documented as
    // segfaulting, a properly-constructed HeadlessUIManager lets ModuleSelector's constructor
    // -- navigation Buttons AND the constructor body's screen-size arithmetic -- run to
    // completion. See GuiHeadlessUIManagerStub.cpp for the resulting geometry assertions.
    std::shared_ptr<ModuleSelector> selector;
    EXPECT_NO_THROW(selector = std::make_shared<ModuleSelector>(mods));
    EXPECT_NE(selector, nullptr);
}

} // namespace
