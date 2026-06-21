#pragma once

#include "egolib/Graphics/ModelAnimationMetadata.hpp"

#include <cstddef>
#include <string>
#include <vector>

struct cgltf_data;

namespace Ego
{
namespace Graphics
{
namespace GltfModelDetail
{

struct FramePlan
{
    std::string name;
    std::size_t meshIndex = 0;
    BIT_FIELD framefx = EMPTY_BIT_FIELD;
};

bool parseMetadataExtras(const cgltf_data& data,
                         std::vector<FramePlan>& frames,
                         AnimationMetadataInput& metadata,
                         std::string& error);

} // namespace GltfModelDetail
} // namespace Graphics
} // namespace Ego
