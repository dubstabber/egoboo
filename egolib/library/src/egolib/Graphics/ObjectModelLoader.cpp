#include "egolib/Graphics/ObjectModelLoader.hpp"

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
            return true;
        case ObjectModelFormat::Gltf:
        case ObjectModelFormat::Glb:
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

std::shared_ptr<AnimatedModel> loadObjectModelAsset(const ObjectModelAsset& asset)
{
    if (!asset.exists)
    {
        return nullptr;
    }

    switch (asset.format)
    {
        case ObjectModelFormat::Md2:
            return MD2Model::loadFromFile(asset.path);
        case ObjectModelFormat::Gltf:
        case ObjectModelFormat::Glb:
        case ObjectModelFormat::Unknown:
        default:
            return nullptr;
    }
}

} // namespace Graphics
} // namespace Ego
