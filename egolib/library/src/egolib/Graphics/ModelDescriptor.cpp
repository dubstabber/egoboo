#include "ModelDescriptor.hpp"
#include "egolib/Graphics/AnimatedModel.hpp"
#include "egolib/Graphics/ObjectModelLoader.hpp"

namespace Ego
{

ModelDescriptor::ModelDescriptor(const std::string &folderPath) :
    _name(folderPath),      //Make up a name for the model...  IMPORT\TEMP0000.OBJ
    _animationMetadata(),
    _model(nullptr)
{
    Graphics::ObjectModelAsset modelAsset = Graphics::resolveLoadableObjectModelAsset(folderPath);
    if (!modelAsset.exists)
    {
        const Graphics::ObjectModelAsset preferredAsset = Graphics::resolveObjectModelAsset(folderPath);
        if (preferredAsset.exists)
        {
            throw std::runtime_error(std::string("Unsupported model format: ") +
                                     preferredAsset.path +
                                     " (" +
                                     Graphics::getObjectModelFormatName(preferredAsset.format) +
                                     ")");
        }
        throw std::runtime_error("File not found: " + folderPath +
                                 " (expected one of: " +
                                 Graphics::describeObjectModelSearchOrder() +
                                 ")");
    }

    const Graphics::ObjectModelLoadResult loadedModel = Graphics::loadObjectModel(modelAsset);
    _model = loadedModel.model;
    if(!_model) {
        throw std::runtime_error(std::string("Unable to load model file: ") +
                                 modelAsset.path +
                                 " (" +
                                 Graphics::getObjectModelFormatName(modelAsset.format) +
                                 ")");
    }

    // Post-load normalization is format-specific: MD2 keeps the legacy scale and
    // frame-name parse, while glTF/GLB supplies format-native metadata.
    switch (modelAsset.format)
    {
        case Graphics::ObjectModelFormat::Md2:
            /// @details Egoboo legacy models were designed with 1 tile = 32x32 units, but internally Egoboo uses
            ///      1 tile = 128x128 units. Previously, this was handled by sprinkling a bunch of
            ///      commands that multiplied various quantities by 4 or by 4.125 throughout the code.
            ///      It was very counterintuitive, and caused me no end of headaches...  Of course the
            ///      solution is to scale the model!
            _model->scaleModel(-3.5f, 3.5f, 3.5f);

            _animationMetadata.initializeFromLegacyFrames(*_model, _name, folderPath + "/copy.txt");
            break;

        case Graphics::ObjectModelFormat::Gltf:
        case Graphics::ObjectModelFormat::Glb:
            if (!loadedModel.animationMetadata.has_value())
            {
                throw std::runtime_error("Missing animation metadata: " + modelAsset.path);
            }
            _animationMetadata.initializeFromActionData(*_model, *loadedModel.animationMetadata);
            break;

        case Graphics::ObjectModelFormat::Unknown:
            break;
    }
}

const std::string& ModelDescriptor::getName() const
{
    return _name;
}

bool ModelDescriptor::isActionValid(int action) const
{
    return _animationMetadata.isActionValid(action);
}


ModelAction ModelDescriptor::getAction(int action) const
{
    return _animationMetadata.getAction(action);
}

BIT_FIELD ModelDescriptor::getMadFX(int action) const
{
    return _animationMetadata.getMadFX(*_model, action);
}

ModelAction ModelDescriptor::randomizeAction(ModelAction action, int slot) const
{
    return _animationMetadata.randomizeAction(action, slot);
}

ModelAction ModelDescriptor::charToAction(char cTmp)
{
    return Graphics::ModelAnimationMetadata::charToAction(cTmp);
}

} //Ego
