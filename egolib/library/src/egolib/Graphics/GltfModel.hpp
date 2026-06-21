#pragma once

#include "egolib/Graphics/ObjectModelLoader.hpp"

#include <string>

namespace Ego
{
namespace Graphics
{

class GltfModel
{
public:
    GltfModel() = delete;

    static ObjectModelLoadResult loadFromFile(const std::string& fileName);
};

} // namespace Graphics
} // namespace Ego
