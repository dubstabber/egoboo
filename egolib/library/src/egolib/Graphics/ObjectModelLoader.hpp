#pragma once

#include "egolib/Graphics/ObjectModelAsset.hpp"
#include "egolib/Graphics/ModelAnimationMetadata.hpp"

#include <memory>
#include <optional>
#include <string>

namespace Ego
{
namespace Graphics
{

class AnimatedModel;

struct ObjectModelLoadResult
{
    std::shared_ptr<AnimatedModel> model;
    std::optional<AnimationMetadataInput> animationMetadata;
};

bool canLoadObjectModelFormat(ObjectModelFormat format);
ObjectModelAsset resolveLoadableObjectModelAsset(const std::string& objectFolderPath);
ObjectModelLoadResult loadObjectModel(const ObjectModelAsset& asset);
std::shared_ptr<AnimatedModel> loadObjectModelAsset(const ObjectModelAsset& asset);

} // namespace Graphics
} // namespace Ego
