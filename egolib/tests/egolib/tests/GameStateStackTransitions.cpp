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

/// @file GameStateStackTransitions.cpp
/// @brief Characterization tests for GameEngine::advanceGameStateStack: the push/pop/clear
///        state-stack flow, re-entry beginState() semantics, and the main-menu-factory
///        fallback when the stack drains.
///
/// Engine-free by construction: GameEngine's default constructor touches no installed
/// services, and these tests never call updateOneFrame()/renderOneFrame() (which reach
/// SDL, the texture manager, and the renderer). Only the extracted stack-advance step and
/// the public push/set/factory surface are exercised.

#include "gtest/gtest.h"

#include <cstddef>
#include <iterator>
#include <memory>
#include <stdexcept>

#define private public
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/GameStates/GameState.hpp"
#undef private

namespace {

/// A minimal GameState that records lifecycle calls. It never draws (drawContainer would
/// reach the renderer through the UI manager) and is heap-allocated via std::make_shared
/// (Component derives from std::enable_shared_from_this).
class StubGameState : public GameState
{
public:
    void update() override { ++updateCount; }

    void beginState() override { ++beginCount; }

    void draw(Ego::GUI::DrawingContext&) override {}

    int beginCount = 0;
    int updateCount = 0;

protected:
    void drawContainer(Ego::GUI::DrawingContext&) override {}
};

size_t stackDepth(const GameEngine& engine)
{
    return static_cast<size_t>(std::distance(engine._gameStateStack.begin(), engine._gameStateStack.end()));
}

TEST(GameStateStackTransitions, PushMakesStateCurrentAndBeginsItOnce)
{
    GameEngine engine;
    auto first = std::make_shared<StubGameState>();

    engine.pushGameState(first);

    EXPECT_EQ(engine._currentGameState, first);
    EXPECT_EQ(first->beginCount, 1);
    EXPECT_EQ(stackDepth(engine), 1u);
}

TEST(GameStateStackTransitions, AdvanceLeavesLiveCurrentStateUntouched)
{
    GameEngine engine;
    auto first = std::make_shared<StubGameState>();
    engine.pushGameState(first);

    engine.advanceGameStateStack();

    EXPECT_EQ(engine._currentGameState, first);
    EXPECT_EQ(first->beginCount, 1);
    EXPECT_EQ(stackDepth(engine), 1u);
}

TEST(GameStateStackTransitions, AdvancePopsEndedStateAndRebeginsThePreviousState)
{
    GameEngine engine;
    auto first = std::make_shared<StubGameState>();
    auto second = std::make_shared<StubGameState>();
    engine.pushGameState(first);
    engine.pushGameState(second);

    second->endState();
    engine.advanceGameStateStack();

    EXPECT_EQ(engine._currentGameState, first);
    EXPECT_EQ(first->beginCount, 2);
    EXPECT_EQ(second->beginCount, 1);
    EXPECT_EQ(stackDepth(engine), 1u);
}

TEST(GameStateStackTransitions, AdvanceDrainsChainedEndedStatesAndRebeginsEachOnReEntry)
{
    GameEngine engine;
    auto first = std::make_shared<StubGameState>();
    auto second = std::make_shared<StubGameState>();
    auto third = std::make_shared<StubGameState>();
    engine.pushGameState(first);
    engine.pushGameState(second);
    engine.pushGameState(third);

    third->endState();
    second->endState();
    engine.advanceGameStateStack();

    EXPECT_EQ(engine._currentGameState, first);
    EXPECT_EQ(first->beginCount, 2);
    // Legacy quirk pinned deliberately: an already-ended state is re-begun on re-entry
    // before the loop notices it is still ended and pops it again.
    EXPECT_EQ(second->beginCount, 2);
    EXPECT_EQ(third->beginCount, 1);
    EXPECT_EQ(stackDepth(engine), 1u);
}

TEST(GameStateStackTransitions, AdvanceFallsBackToMainMenuFactoryWhenStackDrains)
{
    GameEngine engine;
    std::shared_ptr<StubGameState> menu;
    int factoryCalls = 0;
    engine.setMainMenuStateFactory([&]() -> std::shared_ptr<GameState> {
        ++factoryCalls;
        menu = std::make_shared<StubGameState>();
        return menu;
    });

    auto first = std::make_shared<StubGameState>();
    engine.pushGameState(first);
    first->endState();

    engine.advanceGameStateStack();

    EXPECT_EQ(factoryCalls, 1);
    ASSERT_NE(menu, nullptr);
    EXPECT_EQ(engine._currentGameState, menu);
    EXPECT_EQ(menu->beginCount, 1);
    EXPECT_EQ(stackDepth(engine), 1u);
}

TEST(GameStateStackTransitions, AdvanceWithoutInstalledFactoryThrowsWhenStackDrains)
{
    GameEngine engine;
    auto first = std::make_shared<StubGameState>();
    engine.pushGameState(first);
    first->endState();

    EXPECT_THROW(engine.advanceGameStateStack(), std::logic_error);
}

TEST(GameStateStackTransitions, SetGameStateReplacesStackLazilyOnNextAdvance)
{
    GameEngine engine;
    auto first = std::make_shared<StubGameState>();
    auto second = std::make_shared<StubGameState>();
    engine.pushGameState(first);
    engine.pushGameState(second);

    auto replacement = std::make_shared<StubGameState>();
    engine.setGameState(replacement);

    // The replacement becomes current immediately, but the old states stay on the
    // stack until the next advance applies the deferred clear request.
    EXPECT_EQ(engine._currentGameState, replacement);
    EXPECT_EQ(replacement->beginCount, 1);
    EXPECT_TRUE(engine._clearGameStateStackRequested);
    EXPECT_EQ(stackDepth(engine), 3u);

    engine.advanceGameStateStack();

    EXPECT_EQ(engine._currentGameState, replacement);
    EXPECT_FALSE(engine._clearGameStateStackRequested);
    EXPECT_EQ(stackDepth(engine), 1u);
    EXPECT_EQ(engine._gameStateStack.front(), replacement);
    // Applying the clear request does not re-begin the surviving state.
    EXPECT_EQ(replacement->beginCount, 1);
}

TEST(GameStateStackTransitions, ClearRequestAppliesBeforeEndedFallthrough)
{
    GameEngine engine;
    std::shared_ptr<StubGameState> menu;
    int factoryCalls = 0;
    engine.setMainMenuStateFactory([&]() -> std::shared_ptr<GameState> {
        ++factoryCalls;
        menu = std::make_shared<StubGameState>();
        return menu;
    });

    auto first = std::make_shared<StubGameState>();
    engine.pushGameState(first);

    auto replacement = std::make_shared<StubGameState>();
    engine.setGameState(replacement);
    replacement->endState();

    engine.advanceGameStateStack();

    // The clear discarded the pre-existing state before the ended-state fallthrough ran,
    // so draining the replacement falls back to the factory instead of the old state.
    EXPECT_EQ(factoryCalls, 1);
    ASSERT_NE(menu, nullptr);
    EXPECT_EQ(engine._currentGameState, menu);
    EXPECT_EQ(first->beginCount, 1);
    EXPECT_EQ(stackDepth(engine), 1u);
}

} // namespace
