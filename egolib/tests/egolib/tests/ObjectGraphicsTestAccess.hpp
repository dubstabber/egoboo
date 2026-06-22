#pragma once

#include "egolib/game/Graphics/ObjectGraphics.hpp"

namespace Ego
{
namespace Graphics
{

class ObjectGraphicsTestAccess
{
public:
    struct AnimationState
    {
        ModelAction currentAnimation = ACTION_DA;
        ModelAction nextAnimation = ACTION_DA;
        bool canBeInterrupted = false;
        bool freezeAtLastFrame = false;
        bool loopAnimation = false;
        float animationRate = 1.0f;
        uint16_t sourceFrameIndex = 0;
        uint16_t targetFrameIndex = 0;
        uint8_t animationProgressInteger = 0;
        float animationProgress = 0.0f;
    };

    static AnimationState animationState(const ObjectGraphics& graphics)
    {
        return AnimationState{
            graphics._currentAnimation,
            graphics._nextAnimation,
            graphics._canBeInterrupted,
            graphics._freezeAtLastFrame,
            graphics._loopAnimation,
            graphics._animationRate,
            graphics._sourceFrameIndex,
            graphics._targetFrameIndex,
            graphics._animationProgressInteger,
            graphics._animationProgress
        };
    }

    static void setAnimationState(ObjectGraphics& graphics, const AnimationState& state)
    {
        graphics._currentAnimation = state.currentAnimation;
        graphics._nextAnimation = state.nextAnimation;
        graphics._canBeInterrupted = state.canBeInterrupted;
        graphics._freezeAtLastFrame = state.freezeAtLastFrame;
        graphics._loopAnimation = state.loopAnimation;
        graphics._animationRate = state.animationRate;
        graphics._sourceFrameIndex = state.sourceFrameIndex;
        graphics._targetFrameIndex = state.targetFrameIndex;
        graphics._animationProgressInteger = state.animationProgressInteger;
        graphics._animationProgress = state.animationProgress;
    }

    static ModelAction nextAnimation(const ObjectGraphics& graphics)
    {
        return graphics._nextAnimation;
    }

    static bool freezeAtLastFrame(const ObjectGraphics& graphics)
    {
        return graphics._freezeAtLastFrame;
    }

    static uint16_t sourceFrameIndex(const ObjectGraphics& graphics)
    {
        return graphics._sourceFrameIndex;
    }

    static uint16_t targetFrameIndex(const ObjectGraphics& graphics)
    {
        return graphics._targetFrameIndex;
    }

    static uint8_t animationProgressInteger(const ObjectGraphics& graphics)
    {
        return graphics._animationProgressInteger;
    }

    static float animationProgress(const ObjectGraphics& graphics)
    {
        return graphics._animationProgress;
    }

    static void setCurrentAnimation(ObjectGraphics& graphics, ModelAction action)
    {
        graphics._currentAnimation = action;
    }

    static void setNextAnimation(ObjectGraphics& graphics, ModelAction action)
    {
        graphics._nextAnimation = action;
    }

    static void setCanBeInterrupted(ObjectGraphics& graphics, bool canBeInterrupted)
    {
        graphics._canBeInterrupted = canBeInterrupted;
    }

    static void setFreezeAtLastFrame(ObjectGraphics& graphics, bool freezeAtLastFrame)
    {
        graphics._freezeAtLastFrame = freezeAtLastFrame;
    }

    static void setLoopAnimation(ObjectGraphics& graphics, bool loopAnimation)
    {
        graphics._loopAnimation = loopAnimation;
    }

    static void setAnimationRate(ObjectGraphics& graphics, float animationRate)
    {
        graphics._animationRate = animationRate;
    }

    static void setSourceFrameIndex(ObjectGraphics& graphics, uint16_t frameIndex)
    {
        graphics._sourceFrameIndex = frameIndex;
    }

    static VertexListCache vertexCache(const ObjectGraphics& graphics)
    {
        return graphics._vertexCache;
    }

    static void setTargetFrameIndex(ObjectGraphics& graphics, uint16_t frameIndex)
    {
        graphics._targetFrameIndex = frameIndex;
    }

    static void setAnimationProgressInteger(ObjectGraphics& graphics, uint8_t progress)
    {
        graphics._animationProgressInteger = progress;
    }

    static void setAnimationProgress(ObjectGraphics& graphics, float progress)
    {
        graphics._animationProgress = progress;
    }

    static void popVertex(ObjectGraphics& graphics)
    {
        graphics._vertexList.pop_back();
    }

    static void setVertexZ(ObjectGraphics& graphics, size_t index, GLfloat z)
    {
        graphics._vertexList[index].pos[ZZ] = z;
    }

    static void updateAnimationRate(ObjectGraphics& graphics)
    {
        graphics.updateAnimationRate();
    }

    static void incrementFrame(ObjectGraphics& graphics)
    {
        graphics.incrementFrame();
    }
};

} // namespace Graphics
} // namespace Ego
