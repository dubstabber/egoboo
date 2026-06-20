#pragma once

#include "egolib/Graphics/ObjectModelAsset.hpp"

#include <memory>
#include <string>

namespace Ego
{
namespace Graphics
{

class AnimatedModel;

bool canLoadObjectModelFormat(ObjectModelFormat format);
ObjectModelAsset resolveLoadableObjectModelAsset(const std::string& objectFolderPath);
std::shared_ptr<AnimatedModel> loadObjectModelAsset(const ObjectModelAsset& asset);

} // namespace Graphics
} // namespace Ego
