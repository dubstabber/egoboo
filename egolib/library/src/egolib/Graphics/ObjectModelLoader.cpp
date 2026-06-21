#include "egolib/Graphics/ObjectModelLoader.hpp"

#include "egolib/Graphics/GltfModel.hpp"
#include "egolib/Graphics/MD2Model.hpp"

namespace Ego
{
namespace Graphics
{

bool canLoadObjectModelFormat(ObjectModelFormat format)
{
    switch (format)
    {
        case ObjectModelFormat::Md2:
        case ObjectModelFormat::Gltf:
        case ObjectModelFormat::Glb:
            return true;
        case ObjectModelFormat::Unknown:
        default:
            return false;
    }
}

ObjectModelAsset resolveLoadableObjectModelAsset(const std::string& objectFolderPath)
{
    const ObjectModelAsset preferredAsset = resolveObjectModelAsset(objectFolderPath);
    if (!preferredAsset.exists || canLoadObjectModelFormat(preferredAsset.format))
    {
        return preferredAsset;
    }

    return resolveObjectModelAsset(objectFolderPath, ObjectModelFormat::Md2);
}

ObjectModelLoadResult loadObjectModel(const ObjectModelAsset& asset)
{
    if (!asset.exists)
    {
        return ObjectModelLoadResult();
    }

    switch (asset.format)
    {
        case ObjectModelFormat::Md2:
        {
            ObjectModelLoadResult result;
            result.model = MD2Model::loadFromFile(asset.path);
            return result;
        }
        case ObjectModelFormat::Gltf:
        case ObjectModelFormat::Glb:
            return GltfModel::loadFromFile(asset.path);
        case ObjectModelFormat::Unknown:
        default:
            return ObjectModelLoadResult();
    }
}

std::shared_ptr<AnimatedModel> loadObjectModelAsset(const ObjectModelAsset& asset)
{
    return loadObjectModel(asset).model;
}

} // namespace Graphics
} // namespace Ego
