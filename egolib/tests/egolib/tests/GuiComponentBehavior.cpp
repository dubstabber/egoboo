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

/// @file GuiComponentBehavior.cpp
/// @brief Characterization tests for the egolib-gui base classes: Ego::GUI::Component,
///        Ego::GUI::Container, and the Ego::GUI::LayoutColumns/LayoutRows arrangers.
///
/// These pin the *observable behavior* of the generic widget toolkit's two base classes
/// (every widget derives from Component; every multi-widget screen derives from Container,
/// and every GameState IS-A Container) so that the recently-carved `egolib-gui` layer and the
/// next restructuring wave (the GameStates/menu link-split) can be refactored without silently
/// changing geometry, hit-testing, z-order, state gating, or input propagation.
///
/// Engine-free by construction: these tests NEVER call draw()/drawAll(), which are the only
/// paths that reach the renderer via activeUIManager() (and would throw with no UIManager
/// installed). Construction, geometry, state-flag, membership, and notify*/layout paths are
/// all engine-free. Component derives from std::enable_shared_from_this and destroy() /
/// the shared_ptr cast operator call shared_from_this(), so every instance is heap-allocated
/// via std::make_shared. Positions are float-exact: point_2s == Point2f (idlib `single` == float).

#include "gtest/gtest.h"

#include "egolib/game/GUI/Component.hpp"
#include "egolib/game/GUI/Container.hpp"
#include "egolib/game/GUI/Layout.hpp"
#include "egolib/integrations/math.hpp"   // Ego::Point2f / Vector2f / Rectangle2f
#include "idlib/exception.hpp"            // idlib::argument_null_error

#include <memory>
#include <vector>

namespace {

using Ego::GUI::Component;
using Ego::GUI::Container;
using Ego::GUI::DrawingContext;
using Ego::GUI::LayoutColumns;
using Ego::GUI::LayoutRows;
using Ego::Point2f;
using Ego::Vector2f;
using Ego::Rectangle2f;
namespace Events = Ego::Events;

/// A concrete leaf Component that records the input events it receives and never touches the
/// renderer. `consume` controls whether it reports an event as handled (stopping propagation).
class TestComponent : public Component {
public:
    int mousePressedCount = 0;
    int mouseReleasedCount = 0;
    int mouseClickedCount = 0;
    int pointerMovedCount = 0;
    int keyPressedCount = 0;
    int keyReleasedCount = 0;
    int keyTypedCount = 0;
    int wheelTurnedCount = 0;
    Point2f lastPressedPosition{0.0f, 0.0f};
    Point2f lastReleasedPosition{0.0f, 0.0f};
    Point2f lastPointerMovedPosition{0.0f, 0.0f};
    int lastKeyPressed = 0;
    Vector2f lastWheelDelta{0.0f, 0.0f};
    bool consume = false;

    void draw(DrawingContext&) override { /* no-op: never reaches the renderer */ }

    bool notifyMouseButtonPressed(const Events::MouseButtonPressedEvent& e) override {
        ++mousePressedCount;
        lastPressedPosition = e.get_position();
        return consume;
    }
    bool notifyMouseButtonReleased(const Events::MouseButtonReleasedEvent& e) override {
        ++mouseReleasedCount;
        lastReleasedPosition = e.get_position();
        return consume;
    }
    bool notifyMouseButtonClicked(const Events::MouseButtonClickedEvent&) override {
        ++mouseClickedCount;
        return consume;
    }
    bool notifyMousePointerMoved(const Events::MousePointerMovedEvent& e) override {
        ++pointerMovedCount;
        lastPointerMovedPosition = e.get_position();
        return consume;
    }
    bool notifyKeyboardKeyPressed(const Events::KeyboardKeyPressedEvent& e) override {
        ++keyPressedCount;
        lastKeyPressed = e.get_key();
        return consume;
    }
    bool notifyKeyboardKeyReleased(const Events::KeyboardKeyReleasedEvent&) override {
        ++keyReleasedCount;
        return consume;
    }
    bool notifyKeyboardKeyTyped(const Events::KeyboardKeyTypedEvent&) override {
        ++keyTypedCount;
        return consume;
    }
    bool notifyMouseWheelTurned(const Events::MouseWheelTurnedEvent& e) override {
        ++wheelTurnedCount;
        lastWheelDelta = e.get_delta();
        return consume;
    }
};

/// A Component overriding only the pure-virtual draw(), so every notify* falls through to the
/// InputListener defaults — used to pin those defaults (all return false).
class BareComponent : public Component {
public:
    void draw(DrawingContext&) override { /* no-op */ }
};

/// A concrete Container whose only abstract obligation (drawContainer) is a no-op.
class TestContainer : public Container {
public:
    // Container leaves Component::draw() pure-virtual; provide an engine-free no-op so the
    // class is concrete. We never call draw()/drawAll() (those reach activeUIManager()).
    void draw(DrawingContext&) override { /* no-op: never reaches the renderer */ }
    void drawContainer(DrawingContext&) override { /* no-op: never reaches the renderer */ }
};

void expectPoint(const Point2f& p, float x, float y) {
    EXPECT_FLOAT_EQ(p.x(), x);
    EXPECT_FLOAT_EQ(p.y(), y);
}
void expectVec(const Vector2f& v, float x, float y) {
    EXPECT_FLOAT_EQ(v.x(), x);
    EXPECT_FLOAT_EQ(v.y(), y);
}

/// Collect the contained components in iteration (z) order via the public thread-safe iterator()
/// — the same traversal drawAll() uses (front-of-vector first).
std::vector<Component*> zOrder(Container& c) {
    std::vector<Component*> out;
    for (const std::shared_ptr<Component>& comp : c.iterator()) out.push_back(comp.get());
    return out;
}

// ---------------------------------------------------------------------------------------------
// Component geometry
// ---------------------------------------------------------------------------------------------

TEST(GuiComponent, DefaultBoundsAre32x32AtOrigin) {
    auto c = std::make_shared<TestComponent>();
    expectPoint(c->getPosition(), 0.0f, 0.0f);
    expectVec(c->getSize(), 32.0f, 32.0f);
    EXPECT_FLOAT_EQ(c->getX(), 0.0f);
    EXPECT_FLOAT_EQ(c->getY(), 0.0f);
    EXPECT_FLOAT_EQ(c->getWidth(), 32.0f);
    EXPECT_FLOAT_EQ(c->getHeight(), 32.0f);
    expectPoint(c->getBounds().get_min(), 0.0f, 0.0f);
    expectPoint(c->getBounds().get_max(), 32.0f, 32.0f);
}

TEST(GuiComponent, SetPositionTranslatesKeepingSize) {
    auto c = std::make_shared<TestComponent>();
    c->setPosition(Point2f(10.0f, 20.0f));
    expectPoint(c->getPosition(), 10.0f, 20.0f);
    expectVec(c->getSize(), 32.0f, 32.0f);            // size unchanged
    expectPoint(c->getBounds().get_max(), 42.0f, 52.0f);
}

TEST(GuiComponent, SetXSetYMoveOnlyOneAxis) {
    auto c = std::make_shared<TestComponent>();
    c->setX(5.0f);
    expectPoint(c->getPosition(), 5.0f, 0.0f);
    c->setY(7.0f);
    expectPoint(c->getPosition(), 5.0f, 7.0f);
}

TEST(GuiComponent, SetSizeKeepsMinCorner) {
    auto c = std::make_shared<TestComponent>();
    c->setPosition(Point2f(10.0f, 20.0f));
    c->setSize(Vector2f(100.0f, 50.0f));
    expectPoint(c->getPosition(), 10.0f, 20.0f);      // min corner preserved
    expectVec(c->getSize(), 100.0f, 50.0f);
    expectPoint(c->getBounds().get_max(), 110.0f, 70.0f);
}

TEST(GuiComponent, SetWidthSetHeightResizeIndependently) {
    auto c = std::make_shared<TestComponent>();
    c->setWidth(64.0f);
    expectVec(c->getSize(), 64.0f, 32.0f);
    c->setHeight(128.0f);
    expectVec(c->getSize(), 64.0f, 128.0f);
}

TEST(GuiComponent, SetCenterPositionCentersBothAxes) {
    auto c = std::make_shared<TestComponent>();          // 32x32
    c->setCenterPosition(Point2f(100.0f, 100.0f));       // t = (-16,-16)
    expectPoint(c->getPosition(), 84.0f, 84.0f);
    // The geometric center is exactly the requested point.
    expectPoint(Point2f(c->getX() + c->getWidth() / 2.0f, c->getY() + c->getHeight() / 2.0f),
                100.0f, 100.0f);
}

TEST(GuiComponent, SetCenterPositionOnlyHorizontalLeavesYUntouched) {
    auto c = std::make_shared<TestComponent>();          // 32x32 at (0,0)
    c->setCenterPosition(Point2f(100.0f, 100.0f), /*onlyHorizontal*/ true); // t = (-16, 0)
    expectPoint(c->getPosition(), 84.0f, 100.0f);
}

TEST(GuiComponent, ContainsIsClosedOnBothBounds) {
    auto c = std::make_shared<TestComponent>();          // bounds (0,0)-(32,32)
    EXPECT_TRUE(c->contains(Point2f(16.0f, 16.0f)));     // interior
    EXPECT_TRUE(c->contains(Point2f(0.0f, 0.0f)));       // min corner inclusive
    EXPECT_TRUE(c->contains(Point2f(32.0f, 32.0f)));     // max corner inclusive
    EXPECT_TRUE(c->contains(Point2f(0.0f, 32.0f)));      // other corners inclusive
    EXPECT_TRUE(c->contains(Point2f(32.0f, 0.0f)));
    EXPECT_FALSE(c->contains(Point2f(-0.5f, 16.0f)));    // just left
    EXPECT_FALSE(c->contains(Point2f(32.5f, 16.0f)));    // just right
    EXPECT_FALSE(c->contains(Point2f(16.0f, 33.0f)));    // just below
}

// ---------------------------------------------------------------------------------------------
// Component state flags
// ---------------------------------------------------------------------------------------------

TEST(GuiComponent, DefaultStateEnabledVisibleNotDestroyed) {
    auto c = std::make_shared<TestComponent>();
    EXPECT_TRUE(c->isEnabled());
    EXPECT_TRUE(c->isVisible());
    EXPECT_FALSE(c->isDestroyed());
}

TEST(GuiComponent, HidingAlsoDisables) {
    // isEnabled() == _enabled && !_destroyed && _visible : visibility gates enabled.
    auto c = std::make_shared<TestComponent>();
    c->setVisible(false);
    EXPECT_FALSE(c->isVisible());
    EXPECT_FALSE(c->isEnabled());   // hidden components are also reported as not-enabled
}

TEST(GuiComponent, DisablingDoesNotHide) {
    auto c = std::make_shared<TestComponent>();
    c->setEnabled(false);
    EXPECT_FALSE(c->isEnabled());
    EXPECT_TRUE(c->isVisible());    // still visible
}

TEST(GuiComponent, DestroyForcesEnabledAndVisibleFalse) {
    auto c = std::make_shared<TestComponent>();   // no parent
    c->destroy();
    EXPECT_TRUE(c->isDestroyed());
    EXPECT_FALSE(c->isEnabled());
    EXPECT_FALSE(c->isVisible());
}

// ---------------------------------------------------------------------------------------------
// Derived position / bounds through a parent chain
// ---------------------------------------------------------------------------------------------

TEST(GuiComponent, DerivedPositionEqualsOwnPositionWithoutParent) {
    auto c = std::make_shared<TestComponent>();
    c->setPosition(Point2f(7.0f, 9.0f));
    expectPoint(c->getDerivedPosition(), 7.0f, 9.0f);
}

TEST(GuiComponent, DerivedPositionAccumulatesThroughTwoLevels) {
    auto grandparent = std::make_shared<TestContainer>();
    auto parent = std::make_shared<TestContainer>();
    auto leaf = std::make_shared<TestComponent>();
    grandparent->setPosition(Point2f(100.0f, 200.0f));
    parent->setPosition(Point2f(10.0f, 20.0f));
    leaf->setPosition(Point2f(1.0f, 2.0f));
    grandparent->addComponent(parent);
    parent->addComponent(leaf);

    expectPoint(parent->getDerivedPosition(), 110.0f, 220.0f);
    expectPoint(leaf->getDerivedPosition(), 111.0f, 222.0f);

    // getDerivedBounds() translates own bounds by the PARENT's derived position.
    expectPoint(leaf->getDerivedBounds().get_min(), 111.0f, 222.0f);
    expectPoint(leaf->getDerivedBounds().get_max(), 143.0f, 254.0f);
}

// ---------------------------------------------------------------------------------------------
// Container membership
// ---------------------------------------------------------------------------------------------

TEST(GuiContainer, AddComponentSetsParentAndCount) {
    auto container = std::make_shared<TestContainer>();
    auto child = std::make_shared<TestComponent>();
    EXPECT_EQ(container->getComponentCount(), 0u);
    container->addComponent(child);
    EXPECT_EQ(container->getComponentCount(), 1u);
    EXPECT_EQ(child->getParent(), container.get());
}

TEST(GuiContainer, RemoveComponentClearsParentAndCount) {
    auto container = std::make_shared<TestContainer>();
    auto child = std::make_shared<TestComponent>();
    container->addComponent(child);
    container->removeComponent(child);
    EXPECT_EQ(container->getComponentCount(), 0u);
    EXPECT_EQ(child->getParent(), nullptr);
}

TEST(GuiContainer, AddOrRemoveNullThrows) {
    auto container = std::make_shared<TestContainer>();
    EXPECT_THROW(container->addComponent(nullptr), idlib::argument_null_error);
    EXPECT_THROW(container->removeComponent(nullptr), idlib::argument_null_error);
}

TEST(GuiContainer, ClearComponentsEmptiesButLeavesChildParentSet) {
    // Characterization of an asymmetry: clearComponents() empties the list but, unlike
    // removeComponent(), does NOT reset each child's parent pointer.
    auto container = std::make_shared<TestContainer>();
    auto child = std::make_shared<TestComponent>();
    container->addComponent(child);
    container->clearComponents();
    EXPECT_EQ(container->getComponentCount(), 0u);
    EXPECT_EQ(child->getParent(), container.get());   // parent pointer NOT cleared
}

TEST(GuiContainer, SetComponentListReplacesContents) {
    auto container = std::make_shared<TestContainer>();
    auto a = std::make_shared<TestComponent>();
    auto b = std::make_shared<TestComponent>();
    auto c = std::make_shared<TestComponent>();
    container->setComponentList({a, b, c});
    EXPECT_EQ(container->getComponentCount(), 3u);
    EXPECT_EQ(a->getParent(), container.get());
    EXPECT_EQ(c->getParent(), container.get());

    auto d = std::make_shared<TestComponent>();
    container->setComponentList({d});
    EXPECT_EQ(container->getComponentCount(), 1u);
    EXPECT_EQ(zOrder(*container).front(), d.get());
}

TEST(GuiContainer, BringComponentToFrontMovesToBackOfDrawOrder) {
    auto container = std::make_shared<TestContainer>();
    auto a = std::make_shared<TestComponent>();
    auto b = std::make_shared<TestComponent>();
    auto c = std::make_shared<TestComponent>();
    container->addComponent(a);
    container->addComponent(b);
    container->addComponent(c);
    EXPECT_EQ(zOrder(*container), (std::vector<Component*>{a.get(), b.get(), c.get()}));

    container->bringComponentToFront(a);   // a re-added at the back (drawn last / on top)
    EXPECT_EQ(zOrder(*container), (std::vector<Component*>{b.get(), c.get(), a.get()}));
    EXPECT_EQ(container->getComponentCount(), 3u);
}

TEST(GuiContainer, DestroyRemovesComponentFromParent) {
    auto container = std::make_shared<TestContainer>();
    auto child = std::make_shared<TestComponent>();
    container->addComponent(child);
    child->destroy();
    EXPECT_TRUE(child->isDestroyed());
    EXPECT_EQ(container->getComponentCount(), 0u);   // auto-removed
    EXPECT_EQ(child->getParent(), nullptr);
}

// ---------------------------------------------------------------------------------------------
// Container input propagation
// ---------------------------------------------------------------------------------------------

TEST(GuiContainer, MouseButtonPropagatesInReverseAddOrder) {
    // Last-added component (on top) consumes first.
    auto container = std::make_shared<TestContainer>();
    auto first = std::make_shared<TestComponent>();
    auto last = std::make_shared<TestComponent>();
    first->consume = true;
    last->consume = true;
    container->addComponent(first);
    container->addComponent(last);

    bool handled = container->notifyMouseButtonPressed(
        Events::MouseButtonPressedEvent(Point2f(5.0f, 5.0f), 1));
    EXPECT_TRUE(handled);
    EXPECT_EQ(last->mousePressedCount, 1);    // top-most consumed
    EXPECT_EQ(first->mousePressedCount, 0);   // never reached
}

TEST(GuiContainer, PropagationContinuesPastNonConsumer) {
    auto container = std::make_shared<TestContainer>();
    auto first = std::make_shared<TestComponent>();
    auto last = std::make_shared<TestComponent>();
    first->consume = true;     // bottom consumes
    last->consume = false;     // top declines
    container->addComponent(first);
    container->addComponent(last);

    bool handled = container->notifyMouseButtonPressed(
        Events::MouseButtonPressedEvent(Point2f(5.0f, 5.0f), 1));
    EXPECT_TRUE(handled);
    EXPECT_EQ(last->mousePressedCount, 1);    // visited first, declined
    EXPECT_EQ(first->mousePressedCount, 1);   // then visited and consumed
}

TEST(GuiContainer, NoConsumerReturnsFalseButVisitsAll) {
    auto container = std::make_shared<TestContainer>();
    auto a = std::make_shared<TestComponent>();
    auto b = std::make_shared<TestComponent>();
    a->consume = false;
    b->consume = false;
    container->addComponent(a);
    container->addComponent(b);

    bool handled = container->notifyMouseButtonPressed(
        Events::MouseButtonPressedEvent(Point2f(5.0f, 5.0f), 1));
    EXPECT_FALSE(handled);
    EXPECT_EQ(a->mousePressedCount, 1);
    EXPECT_EQ(b->mousePressedCount, 1);
}

TEST(GuiContainer, DisabledChildIsSkippedDuringPropagation) {
    auto container = std::make_shared<TestContainer>();
    auto enabled = std::make_shared<TestComponent>();
    auto disabled = std::make_shared<TestComponent>();
    enabled->consume = true;
    disabled->consume = true;
    container->addComponent(enabled);
    container->addComponent(disabled);   // added last => normally consumes first
    disabled->setEnabled(false);         // ...but is skipped

    bool handled = container->notifyMouseButtonPressed(
        Events::MouseButtonPressedEvent(Point2f(5.0f, 5.0f), 1));
    EXPECT_TRUE(handled);
    EXPECT_EQ(disabled->mousePressedCount, 0);   // skipped
    EXPECT_EQ(enabled->mousePressedCount, 1);    // received instead
}

TEST(GuiContainer, MouseButtonPositionTranslatedByContainerPosition) {
    auto container = std::make_shared<TestContainer>();
    auto child = std::make_shared<TestComponent>();
    child->consume = true;
    container->addComponent(child);
    container->setPosition(Point2f(10.0f, 20.0f));

    container->notifyMouseButtonPressed(
        Events::MouseButtonPressedEvent(Point2f(100.0f, 100.0f), 1));
    expectPoint(child->lastPressedPosition, 90.0f, 80.0f);   // event - container position
}

TEST(GuiContainer, MouseReleasePositionTranslatedByContainerPosition) {
    auto container = std::make_shared<TestContainer>();
    auto child = std::make_shared<TestComponent>();
    child->consume = true;
    container->addComponent(child);
    container->setPosition(Point2f(10.0f, 20.0f));

    container->notifyMouseButtonReleased(
        Events::MouseButtonReleasedEvent(Point2f(100.0f, 100.0f), 1));
    expectPoint(child->lastReleasedPosition, 90.0f, 80.0f);
}

TEST(GuiContainer, PointerMovedTranslatedAndReverseOrdered) {
    auto container = std::make_shared<TestContainer>();
    auto first = std::make_shared<TestComponent>();
    auto last = std::make_shared<TestComponent>();
    last->consume = true;
    container->addComponent(first);
    container->addComponent(last);
    container->setPosition(Point2f(5.0f, 5.0f));

    bool handled = container->notifyMousePointerMoved(
        Events::MousePointerMovedEvent(Point2f(50.0f, 60.0f)));
    EXPECT_TRUE(handled);
    EXPECT_EQ(last->pointerMovedCount, 1);
    EXPECT_EQ(first->pointerMovedCount, 0);
    expectPoint(last->lastPointerMovedPosition, 45.0f, 55.0f);   // 50-5, 60-5
}

// ---------------------------------------------------------------------------------------------
// Layout arrangers (pure geometry)
// ---------------------------------------------------------------------------------------------

std::vector<std::shared_ptr<Component>> makeTwoDefaultComponents() {
    return { std::make_shared<TestComponent>(), std::make_shared<TestComponent>() };
}

TEST(GuiLayout, ColumnsStackVerticallyWhenTheyFit) {
    auto comps = makeTwoDefaultComponents();           // 32x32 each
    LayoutColumns(Point2f(10.0f, 10.0f), /*bottom*/ 100.0f, /*hGap*/ 5.0f, /*vGap*/ 5.0f)(comps);
    expectPoint(comps[0]->getPosition(), 10.0f, 10.0f);
    expectPoint(comps[1]->getPosition(), 10.0f, 47.0f);   // y += maxHeight(32)+vGap(5)
}

TEST(GuiLayout, ColumnsWrapToNextColumnWhenOutOfVerticalSpace) {
    auto comps = makeTwoDefaultComponents();
    LayoutColumns(Point2f(10.0f, 10.0f), /*bottom*/ 50.0f, /*hGap*/ 5.0f, /*vGap*/ 5.0f)(comps);
    expectPoint(comps[0]->getPosition(), 10.0f, 10.0f);
    expectPoint(comps[1]->getPosition(), 47.0f, 10.0f);   // x += maxWidth(32)+hGap(5), y reset
}

TEST(GuiLayout, RowsArrangeHorizontallyWhenTheyFit) {
    auto comps = makeTwoDefaultComponents();
    LayoutRows(Point2f(10.0f, 10.0f), /*right*/ 100.0f, /*hGap*/ 5.0f, /*vGap*/ 5.0f)(comps);
    expectPoint(comps[0]->getPosition(), 10.0f, 10.0f);
    expectPoint(comps[1]->getPosition(), 47.0f, 10.0f);   // x += maxWidth(32)+hGap(5)
}

TEST(GuiLayout, RowsWrapToNextRowWhenOutOfHorizontalSpace) {
    auto comps = makeTwoDefaultComponents();
    LayoutRows(Point2f(10.0f, 10.0f), /*right*/ 50.0f, /*hGap*/ 5.0f, /*vGap*/ 5.0f)(comps);
    expectPoint(comps[0]->getPosition(), 10.0f, 10.0f);
    expectPoint(comps[1]->getPosition(), 10.0f, 47.0f);   // y += maxHeight(32)+vGap(5), x reset
}

TEST(GuiLayout, EmptyComponentListIsANoOp) {
    std::vector<std::shared_ptr<Component>> empty;
    EXPECT_NO_THROW(LayoutColumns(Point2f(0.0f, 0.0f), 100.0f, 0.0f, 0.0f)(empty));
    EXPECT_NO_THROW(LayoutRows(Point2f(0.0f, 0.0f), 100.0f, 0.0f, 0.0f)(empty));
}

TEST(GuiLayout, ColumnsThirdComponentOverlapsSecondOnSecondWrap) {
    // Quirk characterization: the column wrap resets x to leftTop.x()+horizontalIncrement (a SINGLE
    // increment), NOT to leftTop.x()+horizontalIncrement*columnIndex (Layout.cpp:36). So once a
    // second wrap occurs the third column lands on top of the second instead of advancing. This pins
    // the CURRENT behavior so that a future "fix" toward the documented cumulative formula is a
    // conscious, test-updating change rather than a silent one.
    std::vector<std::shared_ptr<Component>> comps = {
        std::make_shared<TestComponent>(), std::make_shared<TestComponent>(),
        std::make_shared<TestComponent>() };                        // each 32x32
    LayoutColumns(Point2f(10.0f, 10.0f), /*bottom*/ 50.0f, /*hGap*/ 0.0f, /*vGap*/ 0.0f)(comps);
    expectPoint(comps[0]->getPosition(), 10.0f, 10.0f);             // fits in column 0
    expectPoint(comps[1]->getPosition(), 42.0f, 10.0f);             // wraps to column 1
    expectPoint(comps[2]->getPosition(), 42.0f, 10.0f);             // wraps AGAIN, overlaps comps[1]
}

// ---------------------------------------------------------------------------------------------
// Input propagation — additions that pin the seams a nesting/layer refactor is most likely to break
// ---------------------------------------------------------------------------------------------

TEST(GuiContainer, MousePositionTranslatedPerNestingLevelByOwnPosition) {
    // The single most load-bearing contract for the GameStates/menu carve (GameState IS-A Container,
    // nested screens are common): each container subtracts its OWN getPosition() — not its derived
    // position — so translations compose level by level. A refactor swapping getPosition() for
    // getDerivedPosition() in Container::notifyMouse* would change the leaf payload and this catches it.
    auto outer = std::make_shared<TestContainer>();
    auto inner = std::make_shared<TestContainer>();
    auto leaf = std::make_shared<TestComponent>();
    leaf->consume = true;
    outer->setPosition(Point2f(10.0f, 20.0f));
    inner->setPosition(Point2f(5.0f, 5.0f));
    outer->addComponent(inner);
    inner->addComponent(leaf);

    bool handled = outer->notifyMouseButtonPressed(
        Events::MouseButtonPressedEvent(Point2f(100.0f, 100.0f), 1));
    EXPECT_TRUE(handled);
    EXPECT_EQ(leaf->mousePressedCount, 1);
    // outer subtracts (10,20) -> inner sees (90,80); inner subtracts (5,5) -> leaf sees (85,75).
    // (If derived position were used, inner would subtract (15,25) and leaf would see (75,55).)
    expectPoint(leaf->lastPressedPosition, 85.0f, 75.0f);
}

TEST(GuiContainer, DispatchesToEnabledChildrenRegardlessOfBounds) {
    // Container performs NO hit-testing: a child receives the event even when the (translated)
    // position lies far outside the child's bounds. A maintainer must not "helpfully" add a
    // contains() gate here — that would be an observable behavior change.
    auto container = std::make_shared<TestContainer>();
    auto child = std::make_shared<TestComponent>();   // bounds (0,0)-(32,32)
    child->consume = true;
    container->addComponent(child);
    bool handled = container->notifyMouseButtonPressed(
        Events::MouseButtonPressedEvent(Point2f(1000.0f, 1000.0f), 1));   // far outside child
    EXPECT_TRUE(handled);
    EXPECT_EQ(child->mousePressedCount, 1);
    expectPoint(child->lastPressedPosition, 1000.0f, 1000.0f);
}

TEST(GuiContainer, KeyboardKeyPressedReverseOrderFirstConsumerWinsAndNotTranslated) {
    auto container = std::make_shared<TestContainer>();
    auto first = std::make_shared<TestComponent>();
    auto last = std::make_shared<TestComponent>();
    first->consume = true;
    last->consume = true;
    container->addComponent(first);
    container->addComponent(last);
    container->setPosition(Point2f(10.0f, 20.0f));   // must NOT affect the key payload

    bool handled = container->notifyKeyboardKeyPressed(Events::KeyboardKeyPressedEvent(42));
    EXPECT_TRUE(handled);
    EXPECT_EQ(last->keyPressedCount, 1);     // top-most consumed
    EXPECT_EQ(first->keyPressedCount, 0);    // never reached
    EXPECT_EQ(last->lastKeyPressed, 42);     // payload forwarded unchanged (no position translation)
}

TEST(GuiContainer, MouseWheelTurnedReverseOrderFirstConsumerWinsAndNotTranslated) {
    auto container = std::make_shared<TestContainer>();
    auto first = std::make_shared<TestComponent>();
    auto last = std::make_shared<TestComponent>();
    first->consume = true;
    last->consume = true;
    container->addComponent(first);
    container->addComponent(last);
    container->setPosition(Point2f(10.0f, 20.0f));

    bool handled = container->notifyMouseWheelTurned(
        Events::MouseWheelTurnedEvent(Vector2f(3.0f, 4.0f)));
    EXPECT_TRUE(handled);
    EXPECT_EQ(last->wheelTurnedCount, 1);
    EXPECT_EQ(first->wheelTurnedCount, 0);
    expectVec(last->lastWheelDelta, 3.0f, 4.0f);   // delta forwarded unchanged
}

TEST(GuiContainer, KeyReleasedTypedAndButtonClickedAreNotForwardedToChildren) {
    // Container overrides exactly five notify* methods; KeyboardKeyReleased / KeyboardKeyTyped /
    // MouseButtonClicked are NOT among them, so they fall through to InputListener's default
    // (return false) and are NOT forwarded to children. `consume = true` makes this meaningful:
    // were they forwarded, the child count would be 1 and the call would return true.
    auto container = std::make_shared<TestContainer>();
    auto child = std::make_shared<TestComponent>();
    child->consume = true;
    container->addComponent(child);

    EXPECT_FALSE(container->notifyKeyboardKeyReleased(Events::KeyboardKeyReleasedEvent(1)));
    EXPECT_FALSE(container->notifyKeyboardKeyTyped(Events::KeyboardKeyTypedEvent(1)));
    EXPECT_FALSE(container->notifyMouseButtonClicked(
        Events::MouseButtonClickedEvent(Point2f(5.0f, 5.0f), 1)));
    EXPECT_EQ(child->keyReleasedCount, 0);
    EXPECT_EQ(child->keyTypedCount, 0);
    EXPECT_EQ(child->mouseClickedCount, 0);
}

TEST(GuiComponent, UnoverriddenInputHandlersReturnFalseByDefault) {
    // InputListener defaults: every notify* returns false unless overridden. A flipped default would
    // silently make Container propagation short-circuit and report events as handled.
    auto c = std::make_shared<BareComponent>();
    EXPECT_FALSE(c->notifyMouseButtonPressed(Events::MouseButtonPressedEvent(Point2f(0.0f, 0.0f), 1)));
    EXPECT_FALSE(c->notifyMouseButtonReleased(Events::MouseButtonReleasedEvent(Point2f(0.0f, 0.0f), 1)));
    EXPECT_FALSE(c->notifyMouseButtonClicked(Events::MouseButtonClickedEvent(Point2f(0.0f, 0.0f), 1)));
    EXPECT_FALSE(c->notifyMousePointerMoved(Events::MousePointerMovedEvent(Point2f(0.0f, 0.0f))));
    EXPECT_FALSE(c->notifyMouseWheelTurned(Events::MouseWheelTurnedEvent(Vector2f(0.0f, 0.0f))));
    EXPECT_FALSE(c->notifyKeyboardKeyPressed(Events::KeyboardKeyPressedEvent(0)));
    EXPECT_FALSE(c->notifyKeyboardKeyReleased(Events::KeyboardKeyReleasedEvent(0)));
    EXPECT_FALSE(c->notifyKeyboardKeyTyped(Events::KeyboardKeyTypedEvent(0)));
}

// ---------------------------------------------------------------------------------------------
// Component lifecycle / z-order additions
// ---------------------------------------------------------------------------------------------

TEST(GuiComponent, BringToFrontWithoutParentIsANoOp) {
    auto c = std::make_shared<TestComponent>();   // no parent
    EXPECT_NO_THROW(c->bringToFront());           // guards on !_parent before shared_from_this()
}

TEST(GuiContainer, ComponentBringToFrontDelegatesToParent) {
    auto container = std::make_shared<TestContainer>();
    auto a = std::make_shared<TestComponent>();
    auto b = std::make_shared<TestComponent>();
    auto c = std::make_shared<TestComponent>();
    container->addComponent(a);
    container->addComponent(b);
    container->addComponent(c);
    a->bringToFront();   // delegates to parent->bringComponentToFront(self)
    EXPECT_EQ(zOrder(*container), (std::vector<Component*>{b.get(), c.get(), a.get()}));
}

TEST(GuiComponent, DestroyDominatesLaterEnableAndShow) {
    // destroy() only sets _destroyed; the getters mask via the !_destroyed gate, so re-enabling or
    // re-showing a destroyed component has no observable effect.
    auto c = std::make_shared<TestComponent>();
    c->destroy();
    c->setEnabled(true);
    c->setVisible(true);
    EXPECT_TRUE(c->isDestroyed());
    EXPECT_FALSE(c->isEnabled());
    EXPECT_FALSE(c->isVisible());
}

TEST(GuiContainer, SetComponentListLeavesDroppedChildParentSet) {
    // Same clearComponents() asymmetry, via setComponentList: replacing the list does not reset the
    // dropped child's parent pointer.
    auto container = std::make_shared<TestContainer>();
    auto old = std::make_shared<TestComponent>();
    container->setComponentList({old});
    auto fresh = std::make_shared<TestComponent>();
    container->setComponentList({fresh});
    EXPECT_EQ(container->getComponentCount(), 1u);
    EXPECT_EQ(old->getParent(), container.get());   // dropped child retains its (stale) parent
}

} // namespace
