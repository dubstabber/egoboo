#include "egolib/Graphics/AnimatedModel.hpp"

#include "egolib/_math.h"

#include <algorithm>
#include <cmath>

namespace Ego
{
namespace Graphics
{

AnimatedModelDrawVertex::AnimatedModelDrawVertex() :
    s(0.0f),
    t(0.0f),
    vertexIndex(0)
{}

AnimatedModelDrawCommand::AnimatedModelDrawCommand() :
    primitiveMode(AnimatedModelPrimitiveMode::TriangleStrip),
    data()
{}

AnimatedModelVertex::AnimatedModelVertex() :
    pos(0.0f, 0.0f, 0.0f),
    nrm(0.0f, 0.0f, 0.0f),
    envU(0.0f)
{}

AnimatedModelFrame::AnimatedModelFrame() :
    vertexList(),
    bb(),
    framelip(0),
    framefx(EMPTY_BIT_FIELD)
{
    name[0] = '\0';
}

AnimatedModel::AnimatedModel() :
    _vertices(0),
    _frames(),
    _commands()
{}

size_t AnimatedModel::getVertexCount() const
{
    return _vertices;
}

void AnimatedModel::setVertexCount(size_t vertices)
{
    _vertices = vertices;
}

std::vector<AnimatedModelFrame>& AnimatedModel::getFrames()
{
    return _frames;
}

const std::vector<AnimatedModelFrame>& AnimatedModel::getFrames() const
{
    return _frames;
}

std::forward_list<AnimatedModelDrawCommand>& AnimatedModel::getDrawCommands()
{
    return _commands;
}

const std::forward_list<AnimatedModelDrawCommand>& AnimatedModel::getDrawCommands() const
{
    return _commands;
}

void AnimatedModel::scaleModel(const float scaleX, const float scaleY, const float scaleZ)
{
    for (AnimatedModelFrame& frame : _frames)
    {
        bool boundingBoxFound = false;

        for (AnimatedModelVertex& vertex : frame.vertexList)
        {
            vertex.pos[kX] *= scaleX;
            vertex.pos[kY] *= scaleY;
            vertex.pos[kZ] *= scaleZ;

            std::copysign(vertex.nrm[kX], scaleX);
            std::copysign(vertex.nrm[kY], scaleY);
            std::copysign(vertex.nrm[kZ], scaleZ);

            vertex.nrm = Ego::normalize(vertex.nrm).get_vector();

            const oct_vec_v2_t opos(vertex.pos);
            if (!boundingBoxFound)
            {
                frame.bb = oct_bb_t(opos);
                boundingBoxFound = true;
            }
            else
            {
                frame.bb.join(opos);
            }
        }
    }
}

void AnimatedModel::makeEquallyLit()
{
    // Pin the environment-map U coordinate to the zero-normal entry (azimuth 0),
    // matching the legacy behaviour of mapping every vertex to the zero normal.
    // The lighting normal (nrm) is intentionally left untouched.
    for (AnimatedModelFrame& frame : _frames)
    {
        for (AnimatedModelVertex& vertex : frame.vertexList)
        {
            vertex.envU = 0.0f;
        }
    }
}

} // namespace Graphics
} // namespace Ego
