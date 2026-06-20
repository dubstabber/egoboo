#pragma once

#include "egolib/Graphics/ModelAnimationTypes.hpp"

#include <array>
#include <cstddef>
#include <string>

namespace Ego
{
namespace Graphics
{

class AnimatedModel;

class ModelAnimationMetadata
{
public:
    static const size_t FRAMELIP_COUNT = 16;

    ModelAnimationMetadata();

    void initializeFromLegacyFrames(AnimatedModel& model, const std::string& modelName, const std::string& copyFilePath);

    ModelAction getAction(int action) const;
    BIT_FIELD getMadFX(const AnimatedModel& model, int action) const;
    bool isActionValid(int action) const;
    bool isFrameValid(int action, int frame) const;
    ModelAction randomizeAction(ModelAction action, int slot = 0) const;

    int getFrameLipToWalkFrame(int lip, int framelip) const;
    int getFirstFrame(int action) const;
    int getLastFrame(int action) const;

    static ModelAction charToAction(char cTmp);
    static ModelAction stringToAction(const std::string& action);

private:
    void reset();
    void ripActions(AnimatedModel& model, const std::string& modelName);
    void parseFrameDescriptors(AnimatedModel& model, const std::string& modelName, const char* cFrameName, int frame);
    void healActions(const std::string& filePath);
    void actionCopyCorrect(ModelAction actiona, ModelAction actionb);
    void initializeWalkFrame(int lip, ModelAction action);
    void initializeFrameLip(AnimatedModel& model, ModelAction action);

private:
    uint16_t _framelipToWalkframe[LIP_COUNT][FRAMELIP_COUNT];
    std::array<ModelAction, ACTION_COUNT> _actionMap;
    std::array<bool, ACTION_COUNT> _actionValid;
    std::array<int, ACTION_COUNT> _actionStart;
    std::array<int, ACTION_COUNT> _actionEnd;
};

} // namespace Graphics
} // namespace Ego
