#include "egolib/Graphics/ModelAnimationMetadata.hpp"

#include "egolib/Graphics/AnimatedModel.hpp"
#include "egolib/Log/_Include.hpp"
#include "egolib/Logic/ObjectSlot.hpp"
#include "egolib/Math/Random.hpp"
#include "egolib/strutil.h"
#include "egolib/Core/StringUtilities.hpp"
#include "egolib/fileutil.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>

namespace Ego
{
namespace Graphics
{

void ModelAnimationMetadata::initializeFromLegacyFrames(AnimatedModel& model,
                                                       const std::string& modelName,
                                                       const std::string& copyFilePath)
{
    // Recover the format-neutral metadata input from the legacy MD2 frame-name
    // encoding: action ranges + per-frame effects (written onto the frames) and
    // the copy.txt heal aliases. Then apply it through the shared neutral path.
    AnimationMetadataInput input;
    ripActions(model, modelName, input);
    collectHealAliases(copyFilePath, input);

    applyMetadata(model, input);
}

void ModelAnimationMetadata::ripActions(AnimatedModel& model, const std::string& modelName, AnimationMetadataInput& input)
{
    if (model.getFrames().empty())
    {
        return;
    }

    ModelAction last_action = ACTION_COUNT;
    int iframe = 0;
    for (const AnimatedModelFrame& frame : model.getFrames())
    {
        ModelAction action_now = stringToAction(frame.name);

        if (action_now == ACTION_COUNT)
        {
            Log::activeTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "got no action for frame name ", "`", frame.name, "`", " in model ", "`", modelName, "`", ": ignoring model", Log::EndOfEntry);
            iframe++;
            continue;
        }

        if (last_action != action_now)
        {
            input.actions[action_now].present = true;
            input.actions[action_now].start = iframe;
            input.actions[action_now].end = iframe;

            last_action = action_now;
        }
        else
        {
            input.actions[action_now].end = iframe;
        }

        parseFrameDescriptors(model, modelName, frame.name, iframe);
        iframe++;
    }
}

void ModelAnimationMetadata::parseFrameDescriptors(AnimatedModel& model,
                                                  const std::string& modelName,
                                                  const char* cFrameName,
                                                  int frame)
{
    char name_action[16], name_fx[16];
    int name_count;
    int cnt;

    static int token_count = -1;
    static const char* tokens[] = { "I", "S", "F", "P", "A", "G", "D", "C",
                                    "LA", "LG", "LD", "LC", "RA", "RG", "RD", "RC", nullptr };

    if (frame >= model.getFrames().size())
    {
        return;
    }

    AnimatedModelFrame& pframe = model.getFrames()[frame];

    if (token_count < 0)
    {
        token_count = 0;
        for (cnt = 0; nullptr != tokens[token_count] && cnt < 256; cnt++)
        {
            token_count++;
        }
    }

    BIT_FIELD fx = 0;
    pframe.framefx = fx;

    if (!VALID_CSTR(cFrameName))
    {
        return;
    }

    const char* ptmp = cFrameName;
    const char* ptmp_end = cFrameName + 16;
    for (; ptmp < ptmp_end && Ego::isspace(*ptmp); ptmp++) {};

    char* paction = name_action;
    char* paction_end = name_action + 16;
    for (; ptmp < ptmp_end && paction < paction_end && !Ego::isspace(*ptmp); ptmp++, paction++)
    {
        if (Ego::isdigit(*ptmp)) break;
        *paction = *ptmp;
    }
    if (paction < paction_end) *paction = CSTR_END;

    name_fx[0] = CSTR_END;
    sscanf(ptmp, "%d %15s", &name_count, name_fx);
    name_action[15] = CSTR_END;
    name_fx[15] = CSTR_END;

    if (name_fx[0] == '\0')
    {
        return;
    }

    ptmp = name_fx;
    ptmp_end = name_fx + 15;
    while (CSTR_END != *ptmp && ptmp < ptmp_end)
    {
        size_t len;
        int token_index = -1;
        for (cnt = 0; cnt < token_count; cnt++)
        {
            len = strlen(tokens[cnt]);
            if (0 == strncmp(tokens[cnt], ptmp, len))
            {
                ptmp += len;
                token_index = cnt;
                break;
            }
        }

        if (-1 == token_index)
        {
            if (*ptmp != '0')
            {
                Log::activeTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "model ", "`", modelName, "`", ", frame ", frame, ", frame name ", "`", cFrameName, "`", " has unknown frame effects command ", "`", ptmp, "`", Log::EndOfEntry);
            }
            ptmp++;
        }
        else
        {
            bool bad_form = false;
            switch (token_index)
            {
                case 0:
                    SET_BIT(fx, MADFX_INVICTUS);
                    break;
                case 1:
                    SET_BIT(fx, MADFX_STOP);
                    break;
                case 2:
                    SET_BIT(fx, MADFX_FOOTFALL);
                    break;
                case 3:
                    SET_BIT(fx, MADFX_POOF);
                    break;
                case 4:
                    while ((CSTR_END != *ptmp && ptmp < ptmp_end) && ('R' == *ptmp || 'L' == *ptmp))
                    {
                        SET_BIT(fx, ('L' == *ptmp) ? MADFX_ACTLEFT : MADFX_ACTRIGHT);
                        ptmp++;
                    }
                    break;
                case 5:
                    while ((CSTR_END != *ptmp && ptmp < ptmp_end) && ('R' == *ptmp || 'L' == *ptmp))
                    {
                        SET_BIT(fx, ('L' == *ptmp) ? MADFX_GRABLEFT : MADFX_GRABRIGHT);
                        ptmp++;
                    }
                    break;
                case 6:
                    while ((CSTR_END != *ptmp && ptmp < ptmp_end) && ('R' == *ptmp || 'L' == *ptmp))
                    {
                        fx |= ('L' == *ptmp) ? MADFX_DROPLEFT : MADFX_DROPRIGHT;
                        ptmp++;
                    }
                    break;
                case 7:
                    while ((CSTR_END != *ptmp && ptmp < ptmp_end) && ('R' == *ptmp || 'L' == *ptmp))
                    {
                        SET_BIT(fx, ('L' == *ptmp) ? MADFX_CHARLEFT : MADFX_CHARRIGHT);
                        ptmp++;
                    }
                    break;
                case 8:
                    bad_form = true;
                    SET_BIT(fx, MADFX_ACTLEFT);
                    break;
                case 9:
                    bad_form = true;
                    SET_BIT(fx, MADFX_GRABLEFT);
                    break;
                case 10:
                    bad_form = true;
                    SET_BIT(fx, MADFX_DROPLEFT);
                    break;
                case 11:
                    bad_form = true;
                    SET_BIT(fx, MADFX_CHARLEFT);
                    break;
                case 12:
                    bad_form = true;
                    SET_BIT(fx, MADFX_ACTRIGHT);
                    break;
                case 13:
                    bad_form = true;
                    SET_BIT(fx, MADFX_GRABRIGHT);
                    break;
                case 14:
                    bad_form = true;
                    SET_BIT(fx, MADFX_DROPRIGHT);
                    break;
                case 15:
                    bad_form = true;
                    SET_BIT(fx, MADFX_CHARRIGHT);
                    break;
            }

            if (bad_form && -1 != token_index)
            {
                Log::activeTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "model ", "`", modelName, "`", ", frame ", frame, ", frame name ", "`", cFrameName, "`", " has a frame effects command in an improper configuration ", "`", tokens[token_index], "`", Log::EndOfEntry);
            }
        }
    }

    pframe.framefx = fx;
}

void ModelAnimationMetadata::collectHealAliases(const std::string& filePath, AnimationMetadataInput& input)
{
    std::unique_ptr<ReadContext> ctxt = nullptr;
    try
    {
        ctxt = std::make_unique<ReadContext>(filePath);
    }
    catch (...)
    {
        return;
    }

    while (ctxt->skipToColon(true))
    {
        std::string szOne, szTwo;

        szOne = vfs_read_string_lit(*ctxt);
        ModelAction actiona = charToAction(szOne[0]);

        szTwo = vfs_read_string_lit(*ctxt);
        ModelAction actionb = charToAction(szTwo[0]);

        input.healAliases.emplace_back(actiona, actionb);
    }
}

} // namespace Graphics
} // namespace Ego
