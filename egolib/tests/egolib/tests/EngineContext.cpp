#include "gtest/gtest.h"

#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameEngine.hpp"

#include <memory>
#include <stdexcept>

namespace
{

class EngineContextFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        EngineContext::get().clearEngine();
    }

    void TearDown() override
    {
        EngineContext::get().clearEngine();
    }
};

TEST_F(EngineContextFixture, EngineThrowsWhenNoEngineIsInstalled)
{
    EngineContext& context = EngineContext::get();

    EXPECT_EQ(context.tryEngine(), nullptr);
    EXPECT_THROW(context.engine(), std::logic_error);
}

TEST_F(EngineContextFixture, SetEnginePublishesInstalledEngine)
{
    EngineContext& context = EngineContext::get();
    auto installed = std::make_unique<GameEngine>();
    GameEngine* installedPtr = installed.get();

    context.setEngine(std::move(installed));

    EXPECT_EQ(context.tryEngine(), installedPtr);
    EXPECT_EQ(&context.engine(), installedPtr);
    EXPECT_EQ(context.renderedFrameCount(), 0u);
}

TEST_F(EngineContextFixture, SetEngineRejectsNullAndDoubleInstall)
{
    EngineContext& context = EngineContext::get();

    EXPECT_THROW(context.setEngine(nullptr), std::logic_error);

    auto first = std::make_unique<GameEngine>();
    GameEngine* firstPtr = first.get();
    context.setEngine(std::move(first));

    EXPECT_THROW(context.setEngine(std::make_unique<GameEngine>()), std::logic_error);
    EXPECT_EQ(context.tryEngine(), firstPtr);
}

TEST_F(EngineContextFixture, ClearEngineRemovesInstalledEngine)
{
    EngineContext& context = EngineContext::get();
    context.setEngine(std::make_unique<GameEngine>());

    context.clearEngine();

    EXPECT_EQ(context.tryEngine(), nullptr);
    EXPECT_THROW(context.engine(), std::logic_error);
}

} // namespace
