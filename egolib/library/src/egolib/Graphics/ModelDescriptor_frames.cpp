#include "ModelDescriptor.hpp"

#include "egolib/Graphics/AnimatedModel.hpp"

#include <cassert>

namespace Ego
{

void ModelDescriptor::makeEquallyLit()
{
    _model->makeEquallyLit();
}

const std::shared_ptr<Ego::Graphics::AnimatedModel>& ModelDescriptor::getModel() const
{
    return _model;
}

bool ModelDescriptor::isFrameValid(int action, int frame) const
{
    return _animationMetadata.isFrameValid(action, frame);
}

int ModelDescriptor::getFrameLipToWalkFrame(int lip, int framelip) const
{
    assert(lip >= 0 && lip < LIP_COUNT && framelip >= 0 && framelip < FRAMELIP_COUNT);
    return _animationMetadata.getFrameLipToWalkFrame(lip, framelip);
}

} // namespace Ego
