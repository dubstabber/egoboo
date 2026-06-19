#include "egolib/Graphics/ObjectModelAsset.hpp"

#include "egolib/vfs.h"

namespace Ego
{
namespace Graphics
{

namespace
{

struct Candidate
{
    ObjectModelFormat format;
    const char* fileName;
};

static const Candidate MODEL_CANDIDATES[] =
{
    {ObjectModelFormat::Gltf, "tris.gltf"},
    {ObjectModelFormat::Glb,  "tris.glb"},
    {ObjectModelFormat::Md2,  "tris.md2"}
};

} // namespace

ObjectModelAsset::ObjectModelAsset() :
    exists(false),
    format(ObjectModelFormat::Unknown),
    path()
{}

ObjectModelAsset resolveObjectModelAsset(const std::string& objectFolderPath)
{
    for (const Candidate& candidate : MODEL_CANDIDATES)
    {
        const std::string path = objectFolderPath + "/" + candidate.fileName;
        if (vfs_exists(path))
        {
            ObjectModelAsset asset;
            asset.exists = true;
            asset.format = candidate.format;
            asset.path = path;
            return asset;
        }
    }

    return ObjectModelAsset();
}

ObjectModelAsset resolveObjectModelAsset(const std::string& objectFolderPath, ObjectModelFormat format)
{
    for (const Candidate& candidate : MODEL_CANDIDATES)
    {
        if (candidate.format != format)
        {
            continue;
        }

        const std::string path = objectFolderPath + "/" + candidate.fileName;
        if (vfs_exists(path))
        {
            ObjectModelAsset asset;
            asset.exists = true;
            asset.format = candidate.format;
            asset.path = path;
            return asset;
        }
    }

    return ObjectModelAsset();
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

} // namespace Graphics
} // namespace Ego
