#include "gtest/gtest.h"

#include "egolib/Graphics/AnimatedModel.hpp"
#include "egolib/game/Graphics/DefaultModelVertexBuffer.hpp"

#include <cstdint>
#include <initializer_list>
#include <vector>

namespace
{

Ego::Graphics::AnimatedModelDrawCommand makeCommand(Ego::Graphics::AnimatedModelPrimitiveMode mode,
                                                    std::initializer_list<int32_t> indices)
{
    Ego::Graphics::AnimatedModelDrawCommand command;
    command.primitiveMode = mode;

    float uv = 0.0f;
    for (const int32_t index : indices)
    {
        Ego::Graphics::AnimatedModelDrawVertex vertex;
        vertex.vertexIndex = index;
        vertex.s = uv;
        vertex.t = 1.0f - uv;
        command.data.push_back(vertex);
        uv += 0.125f;
    }

    return command;
}

} // namespace

TEST(ModelDrawCommands, AppendAndPrependPreserveCommandOrder)
{
    Ego::Graphics::AnimatedModel model;

    model.appendDrawCommand(makeCommand(Ego::Graphics::AnimatedModelPrimitiveMode::TriangleStrip, {0, 1, 2}));
    model.appendDrawCommand(makeCommand(Ego::Graphics::AnimatedModelPrimitiveMode::Triangles, {0, 1, 2, 2, 3, 0}));
    model.prependDrawCommand(makeCommand(Ego::Graphics::AnimatedModelPrimitiveMode::TriangleFan, {4, 5, 6}));

    std::vector<Ego::Graphics::AnimatedModelPrimitiveMode> modes;
    for (const Ego::Graphics::AnimatedModelDrawCommand& command : model.getDrawCommands())
    {
        modes.push_back(command.primitiveMode);
    }

    const std::vector<Ego::Graphics::AnimatedModelPrimitiveMode> expected =
    {
        Ego::Graphics::AnimatedModelPrimitiveMode::TriangleFan,
        Ego::Graphics::AnimatedModelPrimitiveMode::TriangleStrip,
        Ego::Graphics::AnimatedModelPrimitiveMode::Triangles
    };
    EXPECT_EQ(modes, expected);
}

TEST(ModelDrawCommands, TriangleCommandsPreserveIndexedVerticesAndUvs)
{
    Ego::Graphics::AnimatedModel model;
    model.appendDrawCommand(makeCommand(Ego::Graphics::AnimatedModelPrimitiveMode::Triangles, {4, 7, 9}));

    const Ego::Graphics::AnimatedModelDrawCommand& command = model.getDrawCommands().front();
    ASSERT_EQ(command.primitiveMode, Ego::Graphics::AnimatedModelPrimitiveMode::Triangles);
    ASSERT_EQ(command.data.size(), 3u);

    EXPECT_EQ(command.data[0].vertexIndex, 4);
    EXPECT_FLOAT_EQ(command.data[0].s, 0.0f);
    EXPECT_FLOAT_EQ(command.data[0].t, 1.0f);

    EXPECT_EQ(command.data[1].vertexIndex, 7);
    EXPECT_FLOAT_EQ(command.data[1].s, 0.125f);
    EXPECT_FLOAT_EQ(command.data[1].t, 0.875f);

    EXPECT_EQ(command.data[2].vertexIndex, 9);
    EXPECT_FLOAT_EQ(command.data[2].s, 0.25f);
    EXPECT_FLOAT_EQ(command.data[2].t, 0.75f);
}

TEST(ModelDrawCommands, VertexBufferCapacityCountsTriangleCommandLength)
{
    Ego::Graphics::AnimatedModel model;
    model.appendDrawCommand(makeCommand(Ego::Graphics::AnimatedModelPrimitiveMode::TriangleStrip, {0, 1, 2}));
    model.appendDrawCommand(makeCommand(Ego::Graphics::AnimatedModelPrimitiveMode::TriangleFan, {0, 1, 2, 3}));
    model.appendDrawCommand(makeCommand(Ego::Graphics::AnimatedModelPrimitiveMode::Triangles, {0, 1, 2, 2, 3, 0}));

    Ego::Graphics::DefaultModelVertexBuffer buffer;
    EXPECT_EQ(buffer.getRequiredVertexBufferCapacity(model), 6u);
}
