#include "egolib/Graphics/ModelAnimationMetadata.hpp"

#include "egolib/Graphics/AnimatedModel.hpp"
#include "egolib/Logic/ObjectSlot.hpp"
#include "egolib/Math/Random.hpp"
#include "egolib/Core/StringUtilities.hpp"

#include <algorithm>
#include <cassert>

namespace Ego
{
namespace Graphics
{

namespace
{

constexpr int STRING_SWITCH(int a, int b)
{
    return a | (b << 16);
}

} // namespace

ModelAnimationMetadata::ModelAnimationMetadata()
{
    reset();
}

void ModelAnimationMetadata::reset()
{
    _actionMap.fill(ACTION_COUNT);
    _actionValid.fill(false);
    _actionStart.fill(0);
    _actionEnd.fill(0);

    for (size_t i = 0; i < LIP_COUNT; ++i)
    {
        for (size_t j = 0; j < FRAMELIP_COUNT; ++j)
        {
            _framelipToWalkframe[i][j] = 0;
        }
    }
}

void ModelAnimationMetadata::initializeFromActionData(AnimatedModel& model,
                                                      const AnimationMetadataInput& input)
{
    applyMetadata(model, input);
}

void ModelAnimationMetadata::applyMetadata(AnimatedModel& model, const AnimationMetadataInput& input)
{
    reset();

    _actionMap.fill(ACTION_COUNT);
    _actionStart.fill(-1);
    _actionEnd.fill(-1);
    _actionValid.fill(false);

    if (!model.getFrames().empty())
    {
        _actionMap[ACTION_DA] = ACTION_DA;
        _actionValid[ACTION_DA] = true;
        _actionStart[ACTION_DA] = 0;
        _actionEnd[ACTION_DA] = 0;

        _actionMap[ACTION_WC] = ACTION_WB;
        _actionMap[ACTION_WB] = ACTION_WA;
        _actionMap[ACTION_WA] = ACTION_DA;

        for (size_t action = 0; action < ACTION_COUNT; ++action)
        {
            const AnimationActionRange& range = input.actions[action];
            if (!range.present)
            {
                continue;
            }

            _actionMap[action] = static_cast<ModelAction>(action);
            _actionStart[action] = range.start;
            _actionEnd[action] = range.end;
            _actionValid[action] = true;
        }
    }

    applyHealActions(input);

    for (AnimatedModelFrame& frame : model.getFrames())
    {
        frame.framelip = 0;
    }

    initializeFrameLip(model, ACTION_WA);
    initializeFrameLip(model, ACTION_WB);
    initializeFrameLip(model, ACTION_WC);

    initializeWalkFrame(LIPDA, ACTION_DA);
    initializeWalkFrame(LIPWA, ACTION_WA);
    initializeWalkFrame(LIPWB, ACTION_WB);
    initializeWalkFrame(LIPWC, ACTION_WC);
}

ModelAction ModelAnimationMetadata::stringToAction(const std::string& action)
{
    if (action.size() >= 2)
    {
        switch (STRING_SWITCH(action[0], action[1]))
        {
            case STRING_SWITCH('D','A'): return ACTION_DA;
            case STRING_SWITCH('D','B'): return ACTION_DB;
            case STRING_SWITCH('D','C'): return ACTION_DC;
            case STRING_SWITCH('D','D'): return ACTION_DD;
            case STRING_SWITCH('U','A'): return ACTION_UA;
            case STRING_SWITCH('U','B'): return ACTION_UB;
            case STRING_SWITCH('U','C'): return ACTION_UC;
            case STRING_SWITCH('U','D'): return ACTION_UD;
            case STRING_SWITCH('T','A'): return ACTION_TA;
            case STRING_SWITCH('T','B'): return ACTION_TB;
            case STRING_SWITCH('T','C'): return ACTION_TC;
            case STRING_SWITCH('T','D'): return ACTION_TD;
            case STRING_SWITCH('C','A'): return ACTION_CA;
            case STRING_SWITCH('C','B'): return ACTION_CB;
            case STRING_SWITCH('C','C'): return ACTION_CC;
            case STRING_SWITCH('C','D'): return ACTION_CD;
            case STRING_SWITCH('S','A'): return ACTION_SA;
            case STRING_SWITCH('S','B'): return ACTION_SB;
            case STRING_SWITCH('S','C'): return ACTION_SC;
            case STRING_SWITCH('S','D'): return ACTION_SD;
            case STRING_SWITCH('B','A'): return ACTION_BA;
            case STRING_SWITCH('B','B'): return ACTION_BB;
            case STRING_SWITCH('B','C'): return ACTION_BC;
            case STRING_SWITCH('B','D'): return ACTION_BD;
            case STRING_SWITCH('L','A'): return ACTION_LA;
            case STRING_SWITCH('L','B'): return ACTION_LB;
            case STRING_SWITCH('L','C'): return ACTION_LC;
            case STRING_SWITCH('L','D'): return ACTION_LD;
            case STRING_SWITCH('X','A'): return ACTION_XA;
            case STRING_SWITCH('X','B'): return ACTION_XB;
            case STRING_SWITCH('X','C'): return ACTION_XC;
            case STRING_SWITCH('X','D'): return ACTION_XD;
            case STRING_SWITCH('F','A'): return ACTION_FA;
            case STRING_SWITCH('F','B'): return ACTION_FB;
            case STRING_SWITCH('F','C'): return ACTION_FC;
            case STRING_SWITCH('F','D'): return ACTION_FD;
            case STRING_SWITCH('P','A'): return ACTION_PA;
            case STRING_SWITCH('P','B'): return ACTION_PB;
            case STRING_SWITCH('P','C'): return ACTION_PC;
            case STRING_SWITCH('P','D'): return ACTION_PD;
            case STRING_SWITCH('E','A'): return ACTION_EA;
            case STRING_SWITCH('E','B'): return ACTION_EB;
            case STRING_SWITCH('R','A'): return ACTION_RA;
            case STRING_SWITCH('Z','A'): return ACTION_ZA;
            case STRING_SWITCH('Z','B'): return ACTION_ZB;
            case STRING_SWITCH('Z','C'): return ACTION_ZC;
            case STRING_SWITCH('Z','D'): return ACTION_ZD;
            case STRING_SWITCH('W','A'): return ACTION_WA;
            case STRING_SWITCH('W','B'): return ACTION_WB;
            case STRING_SWITCH('W','C'): return ACTION_WC;
            case STRING_SWITCH('W','D'): return ACTION_WD;
            case STRING_SWITCH('J','A'): return ACTION_JA;
            case STRING_SWITCH('J','B'): return ACTION_JB;
            case STRING_SWITCH('J','C'): return ACTION_JC;
            case STRING_SWITCH('H','A'): return ACTION_HA;
            case STRING_SWITCH('H','B'): return ACTION_HB;
            case STRING_SWITCH('H','C'): return ACTION_HC;
            case STRING_SWITCH('H','D'): return ACTION_HD;
            case STRING_SWITCH('K','A'): return ACTION_KA;
            case STRING_SWITCH('K','B'): return ACTION_KB;
            case STRING_SWITCH('K','C'): return ACTION_KC;
            case STRING_SWITCH('K','D'): return ACTION_KD;
            case STRING_SWITCH('M','A'): return ACTION_MA;
            case STRING_SWITCH('M','B'): return ACTION_MB;
            case STRING_SWITCH('M','C'): return ACTION_MC;
            case STRING_SWITCH('M','D'): return ACTION_MD;
            case STRING_SWITCH('M','E'): return ACTION_ME;
            case STRING_SWITCH('M','F'): return ACTION_MF;
            case STRING_SWITCH('M','G'): return ACTION_MG;
            case STRING_SWITCH('M','H'): return ACTION_MH;
            case STRING_SWITCH('M','I'): return ACTION_MI;
            case STRING_SWITCH('M','J'): return ACTION_MJ;
            case STRING_SWITCH('M','K'): return ACTION_MK;
            case STRING_SWITCH('M','L'): return ACTION_ML;
            case STRING_SWITCH('M','M'): return ACTION_MM;
            case STRING_SWITCH('M','N'): return ACTION_MN;
            default:
                break;
        }
    }

    return ACTION_COUNT;
}

void ModelAnimationMetadata::applyHealActions(const AnimationMetadataInput& input)
{
    actionCopyCorrect(ACTION_DA, ACTION_DB);
    actionCopyCorrect(ACTION_DB, ACTION_DC);
    actionCopyCorrect(ACTION_DC, ACTION_DD);
    actionCopyCorrect(ACTION_DB, ACTION_DC);
    actionCopyCorrect(ACTION_DA, ACTION_DB);
    actionCopyCorrect(ACTION_UA, ACTION_UB);
    actionCopyCorrect(ACTION_UB, ACTION_UC);
    actionCopyCorrect(ACTION_UC, ACTION_UD);
    actionCopyCorrect(ACTION_TA, ACTION_TB);
    actionCopyCorrect(ACTION_TC, ACTION_TD);
    actionCopyCorrect(ACTION_CA, ACTION_CB);
    actionCopyCorrect(ACTION_CC, ACTION_CD);
    actionCopyCorrect(ACTION_SA, ACTION_SB);
    actionCopyCorrect(ACTION_SC, ACTION_SD);
    actionCopyCorrect(ACTION_BA, ACTION_BB);
    actionCopyCorrect(ACTION_BC, ACTION_BD);
    actionCopyCorrect(ACTION_LA, ACTION_LB);
    actionCopyCorrect(ACTION_LC, ACTION_LD);
    actionCopyCorrect(ACTION_XA, ACTION_XB);
    actionCopyCorrect(ACTION_XC, ACTION_XD);
    actionCopyCorrect(ACTION_FA, ACTION_FB);
    actionCopyCorrect(ACTION_FC, ACTION_FD);
    actionCopyCorrect(ACTION_PA, ACTION_PB);
    actionCopyCorrect(ACTION_PC, ACTION_PD);
    actionCopyCorrect(ACTION_ZA, ACTION_ZB);
    actionCopyCorrect(ACTION_ZC, ACTION_ZD);
    actionCopyCorrect(ACTION_WA, ACTION_WB);
    actionCopyCorrect(ACTION_WB, ACTION_WC);
    actionCopyCorrect(ACTION_WC, ACTION_WD);
    actionCopyCorrect(ACTION_DA, ACTION_WD);
    actionCopyCorrect(ACTION_WC, ACTION_WD);
    actionCopyCorrect(ACTION_WB, ACTION_WC);
    actionCopyCorrect(ACTION_WA, ACTION_WB);
    actionCopyCorrect(ACTION_JA, ACTION_JB);
    actionCopyCorrect(ACTION_JB, ACTION_JC);
    actionCopyCorrect(ACTION_DA, ACTION_JC);
    actionCopyCorrect(ACTION_JB, ACTION_JC);
    actionCopyCorrect(ACTION_JA, ACTION_JB);
    actionCopyCorrect(ACTION_HA, ACTION_HB);
    actionCopyCorrect(ACTION_HB, ACTION_HC);
    actionCopyCorrect(ACTION_HC, ACTION_HD);
    actionCopyCorrect(ACTION_HB, ACTION_HC);
    actionCopyCorrect(ACTION_HA, ACTION_HB);
    actionCopyCorrect(ACTION_KA, ACTION_KB);
    actionCopyCorrect(ACTION_KB, ACTION_KC);
    actionCopyCorrect(ACTION_KC, ACTION_KD);
    actionCopyCorrect(ACTION_KB, ACTION_KC);
    actionCopyCorrect(ACTION_KA, ACTION_KB);
    actionCopyCorrect(ACTION_MH, ACTION_MI);
    actionCopyCorrect(ACTION_DA, ACTION_MM);
    actionCopyCorrect(ACTION_MM, ACTION_MN);

    for (const std::pair<ModelAction, ModelAction>& alias : input.healAliases)
    {
        const ModelAction actiona = alias.first;
        const ModelAction actionb = alias.second;

        actionCopyCorrect(static_cast<ModelAction>(actiona + 0), static_cast<ModelAction>(actionb + 0));
        actionCopyCorrect(static_cast<ModelAction>(actiona + 1), static_cast<ModelAction>(actionb + 1));
        actionCopyCorrect(static_cast<ModelAction>(actiona + 2), static_cast<ModelAction>(actionb + 2));
        actionCopyCorrect(static_cast<ModelAction>(actiona + 3), static_cast<ModelAction>(actionb + 3));
    }
}

ModelAction ModelAnimationMetadata::charToAction(char cTmp)
{
    switch (idlib::to_upper(cTmp))
    {
        case 'D': return ACTION_DA;
        case 'U': return ACTION_UA;
        case 'T': return ACTION_TA;
        case 'C': return ACTION_CA;
        case 'S': return ACTION_SA;
        case 'B': return ACTION_BA;
        case 'L': return ACTION_LA;
        case 'X': return ACTION_XA;
        case 'F': return ACTION_FA;
        case 'P': return ACTION_PA;
        case 'Z': return ACTION_ZA;
        case 'H': return ACTION_HA;
        case 'K': return ACTION_KA;
        default:  return ACTION_DA;
    }
}

void ModelAnimationMetadata::actionCopyCorrect(ModelAction actiona, ModelAction actionb)
{
    if (ACTION_COUNT == _actionMap[actiona])
    {
        if (_actionValid[actionb])
        {
            _actionMap[actiona] = actionb;
        }
        else if (ACTION_COUNT != _actionMap[actionb])
        {
            _actionMap[actiona] = _actionMap[actionb];
        }
    }
    else if (ACTION_COUNT == _actionMap[actionb])
    {
        if (_actionValid[actiona])
        {
            _actionMap[actionb] = actiona;
        }
        else if (ACTION_COUNT != _actionMap[actiona])
        {
            _actionMap[actionb] = _actionMap[actiona];
        }
    }
}

void ModelAnimationMetadata::initializeWalkFrame(int lip, ModelAction action)
{
    int action_stt, action_end;

    action = getAction(action);
    if (action >= ACTION_COUNT || !_actionValid[action])
    {
        action_stt = _actionStart[ACTION_DA];
        action_end = _actionStart[ACTION_DA];
    }
    else
    {
        action_stt = _actionStart[action];
        action_end = _actionEnd[action];
    }

    int action_count = 1 + (action_end - action_stt);
    for (size_t frame = 0; frame < FRAMELIP_COUNT; frame++)
    {
        int framealong = 0;

        if (action_count > 0)
        {
            framealong = (frame * action_count) / FRAMELIP_COUNT;
            framealong = std::min(framealong, action_count - 1);
        }

        _framelipToWalkframe[lip][frame] = action_stt + framealong;
    }
}

void ModelAnimationMetadata::initializeFrameLip(AnimatedModel& model, ModelAction action)
{
    action = getAction(action);

    if (!isActionValid(action)) return;

    const int action_stt = _actionStart[action];
    const int action_end = _actionEnd[action];
    const int action_count = 1 + (action_end - action_stt);

    for (int frame = action_stt; frame <= action_end; frame++)
    {
        if (frame >= model.getFrames().size()) break;

        int framelip = ((frame - action_stt) * FRAMELIP_COUNT) / action_count;
        model.getFrames()[frame].framelip = std::min<size_t>(framelip, FRAMELIP_COUNT - 1);
    }
}

bool ModelAnimationMetadata::isActionValid(int action) const
{
    if (action < 0 || action >= _actionValid.size())
    {
        return false;
    }
    return _actionValid[action];
}

ModelAction ModelAnimationMetadata::getAction(int action) const
{
    ModelAction retval = ACTION_DA;
    if (!isActionValid(ACTION_DA))
    {
        retval = ACTION_COUNT;
    }

    if (isActionValid(action))
    {
        return static_cast<ModelAction>(action);
    }
    else if (ACTION_COUNT != _actionMap[action])
    {
        ModelAction tnc = _actionMap[action];
        for (size_t cnt = 0; cnt < ACTION_COUNT; cnt++)
        {
            if (tnc >= ACTION_COUNT || tnc == action) break;

            if (isActionValid(tnc))
            {
                return tnc;
            }
            tnc = _actionMap[tnc];
        }
    }

    return retval;
}

BIT_FIELD ModelAnimationMetadata::getMadFX(const AnimatedModel& model, int action) const
{
    if (!isActionValid(action)) return EMPTY_BIT_FIELD;

    BIT_FIELD retval = EMPTY_BIT_FIELD;
    const std::vector<AnimatedModelFrame>& frames = model.getFrames();
    for (size_t cnt = _actionStart[action]; cnt <= _actionEnd[action]; cnt++)
    {
        SET_BIT(retval, frames[cnt].framefx);
    }

    return retval;
}

ModelAction ModelAnimationMetadata::randomizeAction(ModelAction action, int slot) const
{
    if (slot < 0 || slot >= SLOT_COUNT) return action;
    if (action >= ACTION_COUNT) return action;

    const int diff = slot * 2;

    switch(action)
    {
        case ACTION_MG:
        case ACTION_MH:
        case ACTION_MI:
        case ACTION_MJ:
        case ACTION_MK:
        case ACTION_ML:
        case ACTION_JA:
        case ACTION_RA:
            return action;
        case ACTION_EA:
        case ACTION_EB:
            action = static_cast<ModelAction>(ACTION_JB + slot);
            break;
        case ACTION_JB:
        case ACTION_JC:
            action = static_cast<ModelAction>(ACTION_JB + slot);
            break;
        case ACTION_MA:
        case ACTION_MB:
            action = static_cast<ModelAction>(ACTION_MA + slot);
            break;
        case ACTION_MC:
        case ACTION_MD:
            action = static_cast<ModelAction>(ACTION_MC + slot);
            break;
        case ACTION_ME:
        case ACTION_MF:
            action = static_cast<ModelAction>(ACTION_ME + slot);
            break;
        case ACTION_MM:
        case ACTION_MN:
            action = static_cast<ModelAction>(ACTION_MM + slot);
            break;
        default:
            if (ACTION_IS_TYPE(action, W)) return action;
    }

    if (action == ACTION_DA || action == ACTION_DB || action == ACTION_DC || action == ACTION_DD)      action = static_cast<ModelAction>(ACTION_DA + Random::next(3));
    else if (ACTION_IS_TYPE(action, U)) action = static_cast<ModelAction>(ACTION_UA + diff + Random::next(1));
    else if (ACTION_IS_TYPE(action, T)) action = static_cast<ModelAction>(ACTION_TA + diff + Random::next(1));
    else if (ACTION_IS_TYPE(action, C)) action = static_cast<ModelAction>(ACTION_CA + diff + Random::next(1));
    else if (ACTION_IS_TYPE(action, S)) action = static_cast<ModelAction>(ACTION_SA + diff + Random::next(1));
    else if (ACTION_IS_TYPE(action, B)) action = static_cast<ModelAction>(ACTION_BA + diff + Random::next(1));
    else if (ACTION_IS_TYPE(action, L)) action = static_cast<ModelAction>(ACTION_LA + diff + Random::next(1));
    else if (ACTION_IS_TYPE(action, X)) action = static_cast<ModelAction>(ACTION_XA + diff + Random::next(1));
    else if (ACTION_IS_TYPE(action, F)) action = static_cast<ModelAction>(ACTION_FA + diff + Random::next(1));
    else if (ACTION_IS_TYPE(action, P)) action = static_cast<ModelAction>(ACTION_PA + diff + Random::next(1));
    else if (ACTION_IS_TYPE(action, Z)) action = static_cast<ModelAction>(ACTION_ZA + diff + Random::next(1));
    else if (ACTION_IS_TYPE(action, H)) action = static_cast<ModelAction>(ACTION_HA + Random::next(3));
    else if (ACTION_IS_TYPE(action, K)) action = static_cast<ModelAction>(ACTION_KA + Random::next(3));

    return action;
}

bool ModelAnimationMetadata::isFrameValid(int action, int frame) const
{
    if (!isActionValid(action)) return false;
    if (frame < _actionStart[action]) return false;
    if (frame > _actionEnd[action]) return false;

    return true;
}

int ModelAnimationMetadata::getFrameLipToWalkFrame(int lip, int framelip) const
{
    assert(lip >= 0 && lip < LIP_COUNT && framelip >= 0 && framelip < FRAMELIP_COUNT);
    return _framelipToWalkframe[lip][framelip];
}

int ModelAnimationMetadata::getFirstFrame(int action) const
{
    return _actionStart[action];
}

int ModelAnimationMetadata::getLastFrame(int action) const
{
    return _actionEnd[action];
}

} // namespace Graphics
} // namespace Ego
