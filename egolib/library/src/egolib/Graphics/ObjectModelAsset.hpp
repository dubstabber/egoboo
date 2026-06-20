#pragma once

#include <string>
#include <vector>

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
const std::vector<ObjectModelFormat>& getObjectModelSearchOrder();
const char* getObjectModelFileName(ObjectModelFormat format);
const char* getObjectModelFormatName(ObjectModelFormat format);
std::string describeObjectModelSearchOrder();

} // namespace Graphics
} // namespace Ego
