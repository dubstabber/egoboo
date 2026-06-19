#pragma once

#include "egolib/bbox.h"
#include "egolib/typedef.h"

#include <cstddef>
#include <cstdint>
#include <forward_list>
#include <vector>

namespace Ego
{
namespace Graphics
{

enum class AnimatedModelPrimitiveMode
{
    TriangleStrip,
    TriangleFan
};

struct AnimatedModelDrawVertex
{
    AnimatedModelDrawVertex();

    float s;
    float t;
    int32_t vertexIndex;
};

struct AnimatedModelDrawCommand
{
    AnimatedModelDrawCommand();

    AnimatedModelPrimitiveMode primitiveMode;
    std::vector<AnimatedModelDrawVertex> data;
};

struct AnimatedModelVertex
{
    AnimatedModelVertex();

    Ego::Vector3f pos;
    Ego::Vector3f nrm;
    size_t normalIndex;
};

struct AnimatedModelFrame
{
    AnimatedModelFrame();

    char name[16];
    std::vector<AnimatedModelVertex> vertexList;
    oct_bb_t bb;
    int framelip;
    BIT_FIELD framefx;
};

class AnimatedModel
{
public:
    static constexpr size_t normalCount = 163;

    AnimatedModel();

    size_t getVertexCount() const;
    void setVertexCount(size_t vertices);

    std::vector<AnimatedModelFrame>& getFrames();
    const std::vector<AnimatedModelFrame>& getFrames() const;

    std::forward_list<AnimatedModelDrawCommand>& getDrawCommands();
    const std::forward_list<AnimatedModelDrawCommand>& getDrawCommands() const;

    void scaleModel(float scaleX, float scaleY, float scaleZ);
    void makeEquallyLit();

    static float getLegacyNormal(size_t normal, size_t index);

private:
    size_t _vertices;
    std::vector<AnimatedModelFrame> _frames;
    std::forward_list<AnimatedModelDrawCommand> _commands;
};

} // namespace Graphics
} // namespace Ego
