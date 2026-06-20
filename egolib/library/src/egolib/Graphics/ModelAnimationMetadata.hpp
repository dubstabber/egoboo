#pragma once

#include "egolib/Graphics/ModelAnimationTypes.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace Ego
{
namespace Graphics
{

class AnimatedModel;

/// Per-action presence and frame range as discovered in an asset.
/// The DA / WA / WB / WC fallback seeding and the action-heal table are engine
/// policy applied by ModelAnimationMetadata, not carried here; this records only
/// which actions the asset actually provides and the frame span of each.
struct AnimationActionRange
{
    bool present = false;   ///< whether the action appears in the asset
    int start = 0;          ///< first frame index of the action's last contiguous block
    int end = 0;            ///< last frame index of the action's last contiguous block
};

/// Format-neutral animation metadata input, decoupled from the legacy MD2
/// frame-name encoding. A non-MD2 loader (e.g. glTF via extras.egoboo) fills this
/// directly instead of forcing the engine to recover action ranges from 16-char
/// MD2 frame-name strings. Per-frame effects (framefx) remain model data set by
/// the loader on each AnimatedModelFrame, not part of this input.
struct AnimationMetadataInput
{
    /// Per-action presence + frame range.
    std::array<AnimationActionRange, ACTION_COUNT> actions;

    /// copy.txt / extras.egoboo action-family heal aliases; each pair (a, b) seeds
    /// the four actionCopyCorrect(a + i, b + i) calls used by the legacy mechanism.
    std::vector<std::pair<ModelAction, ModelAction>> healAliases;
};

class ModelAnimationMetadata
{
public:
    static const size_t FRAMELIP_COUNT = 16;

    ModelAnimationMetadata();

    void initializeFromLegacyFrames(AnimatedModel& model, const std::string& modelName, const std::string& copyFilePath);

    /// Populate the metadata from a format-neutral input (action ranges + heal
    /// aliases), bypassing the legacy MD2 frame-name parse. Per-frame effects are
    /// expected to already live on the model's frames (set by the loader).
    void initializeFromActionData(AnimatedModel& model, const AnimationMetadataInput& input);

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
    void applyMetadata(AnimatedModel& model, const AnimationMetadataInput& input);
    void ripActions(AnimatedModel& model, const std::string& modelName, AnimationMetadataInput& input);
    void parseFrameDescriptors(AnimatedModel& model, const std::string& modelName, const char* cFrameName, int frame);
    void collectHealAliases(const std::string& filePath, AnimationMetadataInput& input);
    void applyHealActions(const AnimationMetadataInput& input);
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
