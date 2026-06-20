#include "egolib/Graphics/ObjectModelAsset.hpp"

#include "egolib/vfs.h"

#include <string>
#include <vector>

namespace Ego
{
namespace Graphics
{

namespace
{

static const std::vector<ObjectModelFormat> MODEL_SEARCH_ORDER =
{
    ObjectModelFormat::Gltf,
    ObjectModelFormat::Glb,
    ObjectModelFormat::Md2
};

} // namespace

ObjectModelAsset::ObjectModelAsset() :
    exists(false),
    format(ObjectModelFormat::Unknown),
    path()
{}

ObjectModelAsset resolveObjectModelAsset(const std::string& objectFolderPath)
{
    for (const ObjectModelFormat format : getObjectModelSearchOrder())
    {
        const std::string path = objectFolderPath + "/" + getObjectModelFileName(format);
        if (vfs_exists(path))
        {
            ObjectModelAsset asset;
            asset.exists = true;
            asset.format = format;
            asset.path = path;
            return asset;
        }
    }

    return ObjectModelAsset();
}

ObjectModelAsset resolveObjectModelAsset(const std::string& objectFolderPath, ObjectModelFormat format)
{
    const char* fileName = getObjectModelFileName(format);
    if ('\0' == fileName[0])
    {
        return ObjectModelAsset();
    }

    const std::string path = objectFolderPath + "/" + fileName;
    if (vfs_exists(path))
    {
        ObjectModelAsset asset;
        asset.exists = true;
        asset.format = format;
        asset.path = path;
        return asset;
    }

    return ObjectModelAsset();
}

const std::vector<ObjectModelFormat>& getObjectModelSearchOrder()
{
    return MODEL_SEARCH_ORDER;
}

const char* getObjectModelFileName(ObjectModelFormat format)
{
    switch (format)
    {
        case ObjectModelFormat::Gltf: return "tris.gltf";
        case ObjectModelFormat::Glb:  return "tris.glb";
        case ObjectModelFormat::Md2:  return "tris.md2";
        case ObjectModelFormat::Unknown:
        default:
            return "";
    }
}

const char* getObjectModelFormatName(ObjectModelFormat format)
{
    switch (format)
    {
        case ObjectModelFormat::Gltf: return "glTF";
        case ObjectModelFormat::Glb:  return "GLB";
        case ObjectModelFormat::Md2:  return "MD2";
        case ObjectModelFormat::Unknown:
        default:
            return "unknown";
    }
}

std::string describeObjectModelSearchOrder()
{
    std::string description;
    for (const ObjectModelFormat format : getObjectModelSearchOrder())
    {
        if (!description.empty())
        {
            description += ", ";
        }
        description += getObjectModelFileName(format);
    }
    return description;
}

} // namespace Graphics
} // namespace Ego
