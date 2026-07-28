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

/// @file GuiHeadlessUIManagerStub.cpp
/// @brief Proof tests for Ego::Test::HeadlessUIManager (Pass 343): a properly-constructed,
///        GL/SDL_ttf-free Ego::GUI::IUIManager implementation installed through the same
///        Ego::GUI::activeUIManager() seam the running engine uses. Demonstrates that GUI
///        widgets can construct and lay out text -- and, in ModuleSelector's case, construct
///        AT ALL -- headlessly, through the seam, rather than only via the pending-layout
///        no-manager path characterized in GuiTextLayoutHeadless.cpp.

#include "gtest/gtest.h"

#include "HeadlessUIManager.hpp"

#define protected public
#define private public
#include "egolib/game/GUI/Button.hpp"
#include "egolib/game/GUI/Label.hpp"
#undef private
#undef protected

#include "egolib/game/GUI/ModuleSelector.hpp"
#include "egolib/Profiles/_Include.hpp"

#include <memory>
#include <vector>

namespace {

using Ego::GUI::Button;
using Ego::GUI::Label;
using Ego::GUI::ModuleSelector;

/// @brief Fabricate @a n stand-in ModuleProfile instances (default ctor: pure member-init, no
///        VFS/texture IO). Mirrors ModuleSelectorWidget.cpp's helper of the same name.
std::vector<std::shared_ptr<ModuleProfile>> makeModules(size_t n) {
    std::vector<std::shared_ptr<ModuleProfile>> mods;
    mods.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        mods.push_back(std::make_shared<ModuleProfile>());
    }
    return mods;
}

TEST(GuiHeadlessUIManagerStub, LabelLaysOutTextThroughTheInstalledStub) {
    ASSERT_EQ(Ego::GUI::tryActiveUIManager(), nullptr);   // precondition: no manager installed

    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    auto label = std::make_shared<Label>("Hello");

    EXPECT_FALSE(label->_textLayoutPending);
    ASSERT_NE(label->getFont(), nullptr);
    // HeadlessFont metrics: 8px/char wide, 16px tall; "Hello" is 5 characters.
    EXPECT_FLOAT_EQ(label->getSize().x(), 40.0f);
    EXPECT_FLOAT_EQ(label->getSize().y(), 16.0f);
}

TEST(GuiHeadlessUIManagerStub, ButtonLaysOutTextThroughTheInstalledStub) {
    ASSERT_EQ(Ego::GUI::tryActiveUIManager(), nullptr);

    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    auto button = std::make_shared<Button>("OK", SDLK_UNKNOWN);

    EXPECT_FALSE(button->_textLayoutPending);
    ASSERT_NE(button->_buttonTextRenderer, nullptr);
    // HeadlessFont metrics: 8px/char wide, 16px tall; "OK" is 2 characters.
    EXPECT_EQ(button->_buttonTextWidth, 16);
    EXPECT_EQ(button->_buttonTextHeight, 16);
}

TEST(GuiHeadlessUIManagerStub, ModuleSelectorConstructsUnderTheInstalledStub) {
    ASSERT_EQ(Ego::GUI::tryActiveUIManager(), nullptr);

    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    // Before this pass, constructing a ModuleSelector with ANY installed UI manager -- even a
    // properly-formed one -- was not exercised headlessly: only the raw, never-constructed fake
    // (which segfaults, see ModuleSelectorWidget.cpp) or no manager at all (which throws) were
    // available. A HeadlessUIManager is the first properly-constructed manager this suite can
    // install, so ModuleSelector's full constructor -- navigation Buttons, screen-size derived
    // layout arithmetic -- now runs to completion.
    auto mods = makeModules(2);
    std::shared_ptr<ModuleSelector> selector;
    EXPECT_NO_THROW(selector = std::make_shared<ModuleSelector>(mods));
    ASSERT_NE(selector, nullptr);

    // Geometry derived from the stub's 640x480 screen size (ModuleSelector.cpp ctor):
    //   MODULE_BUTTON_SIZE = constrain(640 / 6, 138, 256) = 138
    //   setSize(Vector2f(30, 0) + Vector2f(640, 480) * 0.5f) = (350, 240)
    EXPECT_FLOAT_EQ(selector->getWidth(), 350.0f);
    EXPECT_FLOAT_EQ(selector->getHeight(), 240.0f);
    EXPECT_EQ(selector->getSelectedModule(), nullptr);
}

TEST(GuiHeadlessUIManagerStub, GuardRestoresNoActiveManagerOnDestruction) {
    ASSERT_EQ(Ego::GUI::tryActiveUIManager(), nullptr);

    Ego::Test::HeadlessUIManager uiManager;
    {
        Ego::Test::ScopedActiveUIManager guard(uiManager);
        ASSERT_EQ(Ego::GUI::tryActiveUIManager(), &uiManager);
    }

    // The RAII guard clears the seam on destruction, so a subsequent test's no-manager
    // precondition (e.g. GuiTextLayoutHeadless.cpp, ModuleSelectorWidget.cpp) is never leaked
    // into by this test, regardless of test execution order within the same binary.
    EXPECT_EQ(Ego::GUI::tryActiveUIManager(), nullptr);
}

} // namespace
