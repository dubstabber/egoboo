#pragma once

#include <string>

namespace Ego
{
namespace Graphics
{

enum class ObjectModelFormat
{
    Unknown,
    Gltf,
    Glb,
    Md2
};

struct ObjectModelAsset
{
    ObjectModelAsset();

    bool exists;
    ObjectModelFormat format;
    std::string path;
};

ObjectModelAsset resolveObjectModelAsset(const std::string& objectFolderPath);
ObjectModelAsset resolveObjectModelAsset(const std::string& objectFolderPath, ObjectModelFormat format);
const char* getObjectModelFormatName(ObjectModelFormat format);

} // namespace Graphics
} // namespace Ego
