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
///
/// Pass 344 adds the interaction-logic characterization the above passes unlocked: the mouse
/// wheel and navigation/module button handlers in ModuleSelector.cpp. These handlers do their
/// own size_t arithmetic on `_startIndex` and `_modules.size()` independent of the UI manager, so
/// the only extra fixture requirement beyond HeadlessUIManager is a disabled AudioSystem (both
/// the wheel handler and Button::doClick() play a click sound unconditionally on success). Access
/// to ModuleSelector's private `_startIndex`/`_selectedModule`/nav-button members uses the
/// established `#define private public` include-block idiom (see ScriptSystemsFunctions.cpp);
/// the two anonymous ModuleButton children are instead reached through the public
/// Container::iterator() and dynamic_pointer_cast<Button> (ModuleButton is a protected nested
/// class -- which the same idiom also exposes as public -- but naming it isn't necessary since
/// doClick() is inherited, public Button API).

#include "gtest/gtest.h"

#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"

#define protected public
#define private public
#include "egolib/game/GUI/ModuleSelector.hpp"
#undef private
#undef protected

#include "egolib/game/GUI/UIManager.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "HeadlessUIManager.hpp"

namespace {

using Ego::GUI::Button;
using Ego::GUI::ModuleSelector;
using Ego::Events::MouseWheelTurnedEvent;
using Ego::Vector2f;

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

/// @brief Suite-scoped fixture providing the one extra ingredient the interaction handlers need
///        beyond a HeadlessUIManager: a disabled AudioSystem (both the wheel handler and
///        Button::doClick() play a click sound unconditionally on a successful action).
class ModuleSelectorInteractionFixture : public ::testing::Test {
protected:
    static std::unique_ptr<ContentRuntimeBootstrap> s_runtime;

    static void SetUpTestSuite() {
        Ego::Test::configureDataDirectory();

        ContentRuntimeBootstrap::Options opts;
        opts.initializeVirtualFileSystem = true;
        opts.initializeBaseVfsPaths = true;
        opts.initializeLogging = true;
        opts.configureLightweightProfileLoading = true;
        opts.initializeImageManager = true;
        opts.initializePerkHandler = true;
        opts.initializeProfileSystem = true;
        opts.binaryPath = "";
        opts.logPath = "/debug/module-selector-widget-interaction-tests.log";
        opts.logLevel = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);

        setenv("EGOBOO_DISABLE_AUDIO", "1", 1);
        AudioSystem::initialize(EngineContext::get().config(), EngineContext::get().logTarget());
        EngineContext::get().installAudioSystem(AudioSystem::get());
    }

    static void TearDownTestSuite() {
        EngineContext::get().clearAudioSystem();
        AudioSystem::uninitialize();
        s_runtime.reset();
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ModuleSelectorInteractionFixture::s_runtime;

/// @brief Collects @a selector's current children in insertion order: next button, previous
///        button, then one ModuleButton per fitting offset (always 2 under the 640x480 stub,
///        independent of @a selector's module count -- see the geometry note in
///        GuiHeadlessUIManagerStub.cpp).
std::vector<std::shared_ptr<Ego::GUI::Component>> collectChildren(ModuleSelector& selector) {
    auto it = selector.iterator();
    return std::vector<std::shared_ptr<Ego::GUI::Component>>(it.begin(), it.end());
}

TEST_F(ModuleSelectorInteractionFixture, FreshSelectorHasBothNavigationButtonsEnabledRegardlessOfModuleCount) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    // The constructor never initializes nav-button enabled state; Component's own default
    // (_enabled == true) is what a fresh selector actually exposes, even with zero modules to
    // page through. Only notifyModuleListUpdated() (see below) normalizes this.
    auto mods = makeModules(0);
    auto selector = std::make_shared<ModuleSelector>(mods);

    EXPECT_TRUE(selector->_nextModuleButton->isEnabled());
    EXPECT_TRUE(selector->_previousModuleButton->isEnabled());
    EXPECT_EQ(selector->getSelectedModule(), nullptr);
}

TEST_F(ModuleSelectorInteractionFixture, WheelUpWithFewerThanThreeModulesDefeatsTheBlockingGateViaSizeTUnderflow) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    // For N < 3, `_modules.size() - 3` underflows to a huge size_t. The blocking gate at
    // ModuleSelector.cpp:216 (`_startIndex >= _modules.size() - 3`) compares directly against
    // that huge size_t value and is therefore always false: the gate never fires. The very next
    // line then implicitly narrows that same huge size_t down to constrain<int>()'s `upper`
    // parameter; on this project's gnu++17/GCC x86_64 build, that narrowing wraps mod 2^32,
    // reinterpreting the low 32 bits as a NEGATIVE int (-3, -2, -1 for N == 0, 1, 2
    // respectively). That negative upper bound happens to rescue _startIndex back to 0 rather
    // than letting it run away -- but only as an accident of two's-complement truncation, not by
    // design, and it is implementation-defined behavior pinned here as this platform's actual
    // observed result.
    for (size_t n : {size_t{0}, size_t{1}, size_t{2}}) {
        SCOPED_TRACE(::testing::Message() << "N=" << n);

        auto mods = makeModules(n);
        auto selector = std::make_shared<ModuleSelector>(mods);

        EXPECT_TRUE(selector->notifyMouseWheelTurned(MouseWheelTurnedEvent(Vector2f(0.0f, 1.0f))));
        EXPECT_EQ(selector->_startIndex, size_t{0});
        // Spurious: the direct (non-narrowed) size_t comparison a few lines later also sees the
        // huge underflowed value and enables Next despite there being nothing left to page to.
        EXPECT_TRUE(selector->_nextModuleButton->isEnabled());
        EXPECT_FALSE(selector->_previousModuleButton->isEnabled());

        // Contrast: wheel-DOWN at _startIndex == 0 is unconditionally blocked (line 213) for any
        // N, regardless of the size_t underflow story above -- the block is never underflow-prone
        // because it only ever compares _startIndex to the literal 0.
        EXPECT_FALSE(selector->notifyMouseWheelTurned(MouseWheelTurnedEvent(Vector2f(0.0f, -1.0f))));
        EXPECT_EQ(selector->_startIndex, size_t{0});
    }
}

TEST_F(ModuleSelectorInteractionFixture, ZeroDeltaWheelEventIsClaimedAsHandledAndStillRederivesButtonState) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    // A literal zero-delta wheel event (e.g. a diagonal-only trackpad gesture reporting y == 0)
    // passes BOTH blocking gates unconditionally: `0 < 0` is false and `0 > 0` is false. The
    // handler therefore always falls through to the constrain/sound/recompute tail and returns
    // true (claims the event as handled) even though nothing observable changes for N >= 3. For
    // N < 3 the SAME size_t-underflow story pinned above still applies: the spurious Next-enable
    // survives a zero-delta event exactly as it does a real wheel-up.
    for (size_t n : {size_t{0}, size_t{1}, size_t{2}}) {
        SCOPED_TRACE(::testing::Message() << "N=" << n);

        auto mods = makeModules(n);
        auto selector = std::make_shared<ModuleSelector>(mods);

        EXPECT_TRUE(selector->notifyMouseWheelTurned(MouseWheelTurnedEvent(Vector2f(0.0f, 0.0f))));
        EXPECT_EQ(selector->_startIndex, size_t{0});
        EXPECT_TRUE(selector->_nextModuleButton->isEnabled());   // spurious, same underflow as a real wheel-up
        EXPECT_FALSE(selector->_previousModuleButton->isEnabled());
    }

    // For N >= 3 the zero-delta event is a genuine no-op on `_startIndex`, but it is still
    // claimed (returns true) and still re-derives both buttons' enabled state from scratch.
    auto mods = makeModules(4);
    auto selector = std::make_shared<ModuleSelector>(mods);

    EXPECT_TRUE(selector->notifyMouseWheelTurned(MouseWheelTurnedEvent(Vector2f(0.0f, 0.0f))));
    EXPECT_EQ(selector->_startIndex, size_t{0});
    EXPECT_TRUE(selector->_nextModuleButton->isEnabled());       // 0 < 4-3
    EXPECT_FALSE(selector->_previousModuleButton->isEnabled());  // 0 > 0 is false
}

TEST_F(ModuleSelectorInteractionFixture, WheelAtStartIndexZeroWithExactlyThreeModulesIsANoOp) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    // N == 3 is the one boundary case where `_modules.size() - 3 == 0` does NOT underflow: the
    // blocking gate correctly fires (`0 >= 0`), so the handler returns false before touching any
    // state at all -- not even the navigation buttons' enabled flags change.
    auto mods = makeModules(3);
    auto selector = std::make_shared<ModuleSelector>(mods);

    const bool nextWasEnabled = selector->_nextModuleButton->isEnabled();
    const bool prevWasEnabled = selector->_previousModuleButton->isEnabled();

    EXPECT_FALSE(selector->notifyMouseWheelTurned(MouseWheelTurnedEvent(Vector2f(0.0f, 1.0f))));
    EXPECT_EQ(selector->_startIndex, size_t{0});
    EXPECT_EQ(selector->_nextModuleButton->isEnabled(), nextWasEnabled);
    EXPECT_EQ(selector->_previousModuleButton->isEnabled(), prevWasEnabled);

    EXPECT_FALSE(selector->notifyMouseWheelTurned(MouseWheelTurnedEvent(Vector2f(0.0f, -1.0f))));
    EXPECT_EQ(selector->_startIndex, size_t{0});
}

TEST_F(ModuleSelectorInteractionFixture, WheelClampsAndTogglesNavigationButtonsWithFourModules) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    auto mods = makeModules(4);
    auto selector = std::make_shared<ModuleSelector>(mods);

    // First wheel-up: `_modules.size() - 3 == 1` does not underflow, so the arithmetic behaves
    // "normally" from here on: _startIndex advances by exactly one notch and Next disables once
    // there is nothing left to page to.
    EXPECT_TRUE(selector->notifyMouseWheelTurned(MouseWheelTurnedEvent(Vector2f(0.0f, 1.0f))));
    EXPECT_EQ(selector->_startIndex, size_t{1});
    EXPECT_FALSE(selector->_nextModuleButton->isEnabled());
    EXPECT_TRUE(selector->_previousModuleButton->isEnabled());

    // A second wheel-up hits the (correctly-firing, non-underflowed) blocking gate: a no-op.
    EXPECT_FALSE(selector->notifyMouseWheelTurned(MouseWheelTurnedEvent(Vector2f(0.0f, 1.0f))));
    EXPECT_EQ(selector->_startIndex, size_t{1});

    // Wheel-down walks back to 0 and re-toggles both buttons.
    EXPECT_TRUE(selector->notifyMouseWheelTurned(MouseWheelTurnedEvent(Vector2f(0.0f, -1.0f))));
    EXPECT_EQ(selector->_startIndex, size_t{0});
    EXPECT_TRUE(selector->_nextModuleButton->isEnabled());
    EXPECT_FALSE(selector->_previousModuleButton->isEnabled());

    // A multi-notch wheel event (e.g. a fast trackpad scroll) is clamped to the same upper bound
    // in one step via constrain<int>(), not accumulated notch-by-notch.
    EXPECT_TRUE(selector->notifyMouseWheelTurned(MouseWheelTurnedEvent(Vector2f(0.0f, 5.0f))));
    EXPECT_EQ(selector->_startIndex, size_t{1});
}

TEST_F(ModuleSelectorInteractionFixture, NotifyModuleListUpdatedResetsAndNormalizesUnlikeTheWheelPathForFewModules) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    {
        auto mods = makeModules(4);
        auto selector = std::make_shared<ModuleSelector>(mods);
        ASSERT_TRUE(selector->notifyMouseWheelTurned(MouseWheelTurnedEvent(Vector2f(0.0f, 1.0f))));
        ASSERT_EQ(selector->_startIndex, size_t{1});

        selector->notifyModuleListUpdated();
        EXPECT_EQ(selector->_startIndex, size_t{0});
        EXPECT_TRUE(selector->_nextModuleButton->isEnabled());    // 4 > 3
        EXPECT_FALSE(selector->_previousModuleButton->isEnabled());
    }

    // Asymmetry pin: notifyModuleListUpdated()'s `_modules.size() > 3` is a safe, non-underflowing
    // comparison and correctly disables Next for N == 2. The wheel path's re-derivation of the
    // SAME button state (`_startIndex < _modules.size() - 3`) computes that bound via unsigned
    // subtraction FIRST, so it underflows for the same N == 2 and enables Next anyway. The two
    // expressions look textually equivalent but disagree for N < 3.
    {
        auto mods = makeModules(2);
        auto selector = std::make_shared<ModuleSelector>(mods);

        selector->notifyModuleListUpdated();
        EXPECT_FALSE(selector->_nextModuleButton->isEnabled());
        EXPECT_FALSE(selector->_previousModuleButton->isEnabled());
    }
    {
        auto mods = makeModules(2);
        auto selector = std::make_shared<ModuleSelector>(mods);

        EXPECT_TRUE(selector->notifyMouseWheelTurned(MouseWheelTurnedEvent(Vector2f(0.0f, 1.0f))));
        EXPECT_TRUE(selector->_nextModuleButton->isEnabled());   // spuriously enabled, see above
    }
}

TEST_F(ModuleSelectorInteractionFixture, NotifyModuleListUpdatedAgreesWithTheWheelPathFormulaExactlyAtTheThreeModuleBoundary) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    // N == 3 is the one boundary where `_modules.size() - 3 == 0` does NOT underflow, so the
    // wheel path's own recompute formula (`_startIndex < _modules.size() - 3`, i.e. `0 < 0`) and
    // notifyModuleListUpdated()'s safe formula (`_modules.size() > 3`, i.e. `3 > 3`) AGREE: both
    // are false. This is unlike the N < 3 divergence pinned just above. Note the wheel handler
    // itself can never actually execute its recompute line at N == 3 through the public wheel
    // API: its own blocking gate (`_startIndex >= _modules.size() - 3`, i.e. `_startIndex >= 0`)
    // is unconditionally true for any size_t _startIndex, so notifyModuleListUpdated() is the
    // only place this exact boundary is publicly observable.
    auto mods = makeModules(3);
    auto selector = std::make_shared<ModuleSelector>(mods);

    selector->notifyModuleListUpdated();
    EXPECT_FALSE(selector->_nextModuleButton->isEnabled());
    EXPECT_FALSE(selector->_previousModuleButton->isEnabled());
    // Confirms the wheel path's own formula independently agrees at this exact N.
    EXPECT_FALSE(selector->_startIndex < selector->_modules.size() - 3);
}

TEST_F(ModuleSelectorInteractionFixture, PreviousButtonClickUnderflowsStartIndexToSizeMax) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    // Unlike the wheel handler, the previous-button click lambda (ModuleSelector.cpp:82-87) does
    // an entirely unguarded `_startIndex--`. From the fresh _startIndex == 0, this underflows to
    // SIZE_MAX rather than being blocked the way the wheel handler's down-gate is.
    auto mods = makeModules(4);
    auto selector = std::make_shared<ModuleSelector>(mods);

    selector->_previousModuleButton->doClick();

    EXPECT_EQ(selector->_startIndex, std::numeric_limits<size_t>::max());
    EXPECT_TRUE(selector->_previousModuleButton->isEnabled());   // SIZE_MAX > 0
    EXPECT_TRUE(selector->_nextModuleButton->isEnabled());       // unconditionally forced true
}

TEST_F(ModuleSelectorInteractionFixture, PreviousButtonUnderflowCombinedWithModuleButtonOffsetWrapsSelectionToModuleZero) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    // Under the 640x480 stub, exactly 2 ModuleButtons exist (offsets 0 and 1) regardless of how
    // many modules are fabricated; N == 1 here so only offset 0 refers to a real module.
    auto mods = makeModules(1);
    auto selector = std::make_shared<ModuleSelector>(mods);
    selector->_previousModuleButton->doClick();
    ASSERT_EQ(selector->_startIndex, std::numeric_limits<size_t>::max());

    auto children = collectChildren(*selector);
    ASSERT_EQ(children.size(), 4u);
    auto moduleButtonOffset0 = std::dynamic_pointer_cast<Button>(children[2]);
    auto moduleButtonOffset1 = std::dynamic_pointer_cast<Button>(children[3]);
    ASSERT_NE(moduleButtonOffset0, nullptr);
    ASSERT_NE(moduleButtonOffset1, nullptr);

    // Offset 0: guard computes `_startIndex + 0 >= _modules.size()`, i.e. `SIZE_MAX >= 1` -- true,
    // so the click is blocked and getSelectedModule() stays null.
    moduleButtonOffset0->doClick();
    EXPECT_EQ(selector->getSelectedModule(), nullptr);

    // Offset 1: the guard's `_startIndex + 1` wraps (unsigned overflow, well-defined) to 0, which
    // is `< 1`, so the guard does NOT block -- and `_modules[_startIndex + 1]` also wraps to
    // `_modules[0]`, "selecting" module 0 despite the nonsensical start index.
    moduleButtonOffset1->doClick();
    EXPECT_EQ(selector->getSelectedModule(), mods[0]);
}

TEST_F(ModuleSelectorInteractionFixture, NextButtonClickLadderTogglesNextButtonAcrossSevenModules) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    // Unlike the wheel handler's `_startIndex < _modules.size() - 3`, the next-button click
    // lambda (ModuleSelector.cpp:72-77) re-derives its own enabled state as `_modules.size() >
    // _startIndex + 3`: `_startIndex + 3` can only grow, so this form never underflows and is
    // safe for any N (contrast with the wheel path's N < 3 spurious-enable bug above).
    auto mods = makeModules(7);
    auto selector = std::make_shared<ModuleSelector>(mods);

    selector->_nextModuleButton->doClick();
    EXPECT_EQ(selector->_startIndex, size_t{1});
    EXPECT_TRUE(selector->_nextModuleButton->isEnabled());       // 7 > 4
    EXPECT_TRUE(selector->_previousModuleButton->isEnabled());   // forced true unconditionally

    selector->_nextModuleButton->doClick();
    EXPECT_EQ(selector->_startIndex, size_t{2});
    EXPECT_TRUE(selector->_nextModuleButton->isEnabled());       // 7 > 5

    selector->_nextModuleButton->doClick();
    EXPECT_EQ(selector->_startIndex, size_t{3});
    EXPECT_TRUE(selector->_nextModuleButton->isEnabled());       // 7 > 6

    selector->_nextModuleButton->doClick();
    EXPECT_EQ(selector->_startIndex, size_t{4});
    EXPECT_FALSE(selector->_nextModuleButton->isEnabled());      // 7 > 7 is false
}

TEST_F(ModuleSelectorInteractionFixture, PerButtonSelectionGuardBlocksOutOfRangeOffsetsAndSelectionStaysNullUntilAValidClick) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    auto mods = makeModules(1);
    auto selector = std::make_shared<ModuleSelector>(mods);
    EXPECT_EQ(selector->getSelectedModule(), nullptr);

    auto children = collectChildren(*selector);
    ASSERT_EQ(children.size(), 4u);
    auto moduleButtonOffset0 = std::dynamic_pointer_cast<Button>(children[2]);
    auto moduleButtonOffset1 = std::dynamic_pointer_cast<Button>(children[3]);
    ASSERT_NE(moduleButtonOffset0, nullptr);
    ASSERT_NE(moduleButtonOffset1, nullptr);

    // Offset 1 refers to a module that doesn't exist (_startIndex(0) + 1 >= _modules.size()(1)):
    // the click is blocked and getSelectedModule() is unaffected.
    moduleButtonOffset1->doClick();
    EXPECT_EQ(selector->getSelectedModule(), nullptr);

    // Offset 0 is in range and selects the one fabricated module.
    moduleButtonOffset0->doClick();
    EXPECT_EQ(selector->getSelectedModule(), mods[0]);
}

} // namespace
