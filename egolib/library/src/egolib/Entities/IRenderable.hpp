#pragma once

#include "egolib/_math.h"                       // Matrix4f4f
#include "egolib/Extensions/ogl_extensions.h"   // GLXvector4f + GL component types
#include "egolib/Graphics/ModelDescriptor.hpp"

#include <memory>

struct GLvertex; // egolib/Graphics/Vertex.hpp — used here only as const GLvertex&

namespace Ego
{
class Texture;
}

class IRenderable
{
public:
    virtual ~IRenderable() = default;

    virtual ObjectRef getObjRef() const = 0;
    virtual bool isHidden() const = 0;
    virtual bool isInsideInventory() const = 0;

    virtual bool isPhongMapped() const = 0;
    virtual bool hasReflection() const = 0;
    virtual bool isDontCullBackfaces() const = 0;

    virtual bool hasModelDescriptor() const = 0;
    virtual const std::shared_ptr<Ego::ModelDescriptor>& getModelDescriptor() const = 0;
    virtual std::shared_ptr<const Ego::Texture> getSkinTexture() const = 0;

    virtual uint8_t getAlpha() const = 0;
    virtual uint8_t getLight() const = 0;
    virtual uint8_t getSheen() const = 0;
    virtual uint8_t getReflectionAlpha() const = 0;
    virtual void getTint(GLXvector4f tint, bool reflection, int type) const = 0;
    virtual int getAmbientColour() const = 0;
    virtual SFP8_T getUOffset() const = 0;
    virtual SFP8_T getVOffset() const = 0;

    virtual const Ego::Matrix4f4f& getMatrix() const = 0;
    virtual const Ego::Matrix4f4f& getReflectionMatrix() const = 0;
    virtual size_t getVertexCount() const = 0;
    virtual const GLvertex& getVertex(size_t index) const = 0;
};
