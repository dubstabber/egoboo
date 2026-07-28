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

/// @file GuiTextLayoutHeadless.cpp
/// @brief Characterization tests for the headless (no active UIManager) construction path of
///        Ego::GUI::Button and Ego::GUI::Label, and their transitive consumers OptionsButton
///        (by-value Label member) and ScrollableList (two text Buttons).
///
/// Before the tryActiveUIManager() guard was added to Button::setText()/Label::ctor()/
/// Label::setText(), constructing any of these widgets without an installed UIManager threw
/// std::logic_error("no active ui manager") (Component::uiManager() -> activeUIManager()).
/// These are Tier-1 pure headless tests: no fixture, no bootstrap, no UIManager is ever
/// installed. They NEVER call draw()/drawAll() (the only paths that would need a real renderer
/// and a real UIManager) -- see GuiComponentBehavior.cpp's cardinal rule. Every Component is
/// heap-allocated via std::make_shared (Component : enable_shared_from_this).

#include "gtest/gtest.h"

#define protected public
#define private public
#include "egolib/game/GUI/Button.hpp"
#include "egolib/game/GUI/Label.hpp"
#undef private
#undef protected

#include "egolib/game/GUI/OptionsButton.hpp"
#include "egolib/game/GUI/ScrollableList.hpp"
#include "egolib/game/GUI/UIManager.hpp"

#include <memory>

namespace {

using Ego::GUI::Button;
using Ego::GUI::Label;
using Ego::GUI::OptionsButton;
using Ego::GUI::ScrollableList;

// ---------------------------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------------------------

TEST(GuiTextLayoutHeadless, ButtonWithTextConstructsHeadlesslyWithPendingLayout) {
    ASSERT_EQ(Ego::GUI::tryActiveUIManager(), nullptr);   // precondition: no manager installed

    auto button = std::make_shared<Button>("Hello", SDLK_a);

    EXPECT_EQ(button->getText(), "Hello");
    EXPECT_EQ(button->_buttonTextRenderer, nullptr);
    EXPECT_TRUE(button->_textLayoutPending);
}

TEST(GuiTextLayoutHeadless, ButtonSetEmptyTextClearsPendingAndKeepsRendererNull) {
    ASSERT_EQ(Ego::GUI::tryActiveUIManager(), nullptr);

    auto button = std::make_shared<Button>("Hello", SDLK_a);
    ASSERT_TRUE(button->_textLayoutPending);

    button->setText("");

    EXPECT_EQ(button->getText(), "");
    EXPECT_EQ(button->_buttonTextRenderer, nullptr);
    // The empty-text branch always clears the pending flag (unlike the headless non-empty
    // branch), matching Button::setText()'s pre-seam behavior of nulling the renderer outright.
    EXPECT_FALSE(button->_textLayoutPending);
}

// ---------------------------------------------------------------------------------------------
// Label
// ---------------------------------------------------------------------------------------------

TEST(GuiTextLayoutHeadless, LabelWithTextConstructsHeadlesslyWithPendingLayout) {
    ASSERT_EQ(Ego::GUI::tryActiveUIManager(), nullptr);

    auto label = std::make_shared<Label>("multi\nline");

    EXPECT_EQ(label->getText(), "multi\nline");
    EXPECT_EQ(label->getFont(), nullptr);
    // setText()'s setSize() call never runs on the headless (no manager, no font) path, so
    // Component's ctor default bounds (0,0)-(32,32) survive untouched -- (32,32), NOT (0,0).
    EXPECT_FLOAT_EQ(label->getSize().x(), 32.0f);
    EXPECT_FLOAT_EQ(label->getSize().y(), 32.0f);
    EXPECT_TRUE(label->_textLayoutPending);
}

TEST(GuiTextLayoutHeadless, DefaultLabelConstructsHeadlessly) {
    ASSERT_EQ(Ego::GUI::tryActiveUIManager(), nullptr);

    auto label = std::make_shared<Label>();

    EXPECT_EQ(label->getText(), "");
    EXPECT_EQ(label->getFont(), nullptr);
    // Label() delegates to Label(""), and the ctor only calls setText() for non-empty text, so
    // no layout is ever attempted and nothing is pending.
    EXPECT_FALSE(label->_textLayoutPending);
}

// ---------------------------------------------------------------------------------------------
// Transitive consumers
// ---------------------------------------------------------------------------------------------

TEST(GuiTextLayoutHeadless, OptionsButtonConstructsHeadlessly) {
    ASSERT_EQ(Ego::GUI::tryActiveUIManager(), nullptr);

    // OptionsButton holds a by-value Label member constructed with the caller's text; it must
    // not throw even though no UIManager is installed.
    std::shared_ptr<OptionsButton> optionsButton;
    EXPECT_NO_THROW(optionsButton = std::make_shared<OptionsButton>("x"));
    EXPECT_NE(optionsButton, nullptr);
}

TEST(GuiTextLayoutHeadless, ScrollableListConstructsHeadlessly) {
    ASSERT_EQ(Ego::GUI::tryActiveUIManager(), nullptr);

    // ScrollableList's ctor constructs two text Buttons ("+"/"-"); it must not throw either.
    std::shared_ptr<ScrollableList> list;
    EXPECT_NO_THROW(list = std::make_shared<ScrollableList>());
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(list->getComponentCount(), 2u);
}

} // namespace
