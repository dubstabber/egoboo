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
    TriangleFan,
    Triangles
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
    /// Precomputed environment-map U coordinate (the azimuth of nrm, in turns).
    /// Set by the loader so the render path needs no format-specific normal table:
    /// the MD2 loader derives it from the legacy normal palette, a glTF loader from
    /// the asset's real per-vertex normals.
    float envU;
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
    AnimatedModel();

    size_t getVertexCount() const;
    void setVertexCount(size_t vertices);

    std::vector<AnimatedModelFrame>& getFrames();
    const std::vector<AnimatedModelFrame>& getFrames() const;

    const std::forward_list<AnimatedModelDrawCommand>& getDrawCommands() const;
    void prependDrawCommand(AnimatedModelDrawCommand command);
    void appendDrawCommand(AnimatedModelDrawCommand command);

    void scaleModel(float scaleX, float scaleY, float scaleZ);
    void makeEquallyLit();

private:
    size_t _vertices;
    std::vector<AnimatedModelFrame> _frames;
    std::forward_list<AnimatedModelDrawCommand> _commands;
};

} // namespace Graphics
} // namespace Ego
