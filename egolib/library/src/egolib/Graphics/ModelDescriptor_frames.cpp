#include "ModelDescriptor.hpp"
#include "egolib/Log/_Include.hpp"
#include "egolib/Graphics/AnimatedModel.hpp"
#include "egolib/strutil.h"
#include "egolib/Core/StringUtilities.hpp"
#include "egolib/fileutil.h"
#include "egolib/Logic/ObjectSlot.hpp"

namespace Ego
{

void ModelDescriptor::ripActions()
{
    // Clear out all actions and reset to invalid
    _actionMap.fill(ACTION_COUNT);
    _actionStart.fill(-1);
    _actionEnd.fill(-1);
    _actionValid.fill(false);

    // is there anything to do?
    if ( _model->getFrames().empty() ) return;

    // Make a default dance action (ACTION_DA) to be the 1st frame of the animation
    _actionMap[ACTION_DA]   = ACTION_DA;
    _actionValid[ACTION_DA] = true;
    _actionStart[ACTION_DA] = 0;
    _actionEnd[ACTION_DA]   = 0;

    //Make movement actions map default to each other
    _actionMap[ACTION_WC] = ACTION_WB;
    _actionMap[ACTION_WB] = ACTION_WA;
    _actionMap[ACTION_WA] = ACTION_DA;

    // Now go huntin' to see what each iframe is, look for runs of same action
    ModelAction last_action = ACTION_COUNT;
    int iframe = 0;
    for(const Graphics::AnimatedModelFrame &frame : _model->getFrames())
    {
        ModelAction action_now = stringToAction(frame.name);
        
        if (action_now == ACTION_COUNT) {
			Log::activeTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "got no action for frame name ", "`", frame.name, "`", " in model ", "`", _name, "`", ": ignoring model", Log::EndOfEntry);
            iframe++;
            continue;
        }

        if ( last_action != action_now )
        {
            // start a new action
            _actionMap[action_now]   = action_now;
            _actionStart[action_now] = iframe;
            _actionEnd[action_now]   = iframe;
            _actionValid[action_now] = true;

            last_action = action_now;
        }
        else
        {
            // keep expanding the action_end until the end of the action
            _actionEnd[action_now] = iframe;
        }

        parseFrameDescriptors(frame.name, iframe);
        iframe++;
    }
}

void ModelDescriptor::parseFrameDescriptors(const char * cFrameName, int frame)
{
    char name_action[16], name_fx[16];
    int name_count;
    int cnt;

    static int token_count = -1;
    static const char * tokens[] = { "I", "S", "F", "P", "A", "G", "D", "C",          /* the normal command tokens */
                                     "LA", "LG", "LD", "LC", "RA", "RG", "RD", "RC", NULL
                                   }; /* the "bad" token aliases */

    // check for a valid frame number
    if(frame >= _model->getFrames().size())
    {
        return; 
    }

    Graphics::AnimatedModelFrame &pframe = _model->getFrames()[frame];

    // this should only be initialized the first time through
    if (token_count < 0)
    {
        token_count = 0;
        for (cnt = 0; nullptr != tokens[token_count] && cnt < 256; cnt++)
        {
            token_count++;
        }
    }

    // set the default values
    BIT_FIELD fx = 0;
    pframe.framefx = fx;

    // check for a non-trivial frame name
    if ( !VALID_CSTR(cFrameName) ) return;

    // skip over whitespace
    const char* ptmp     = cFrameName;
    const char* ptmp_end = cFrameName + 16;
    for ( /* nothing */; ptmp < ptmp_end && Ego::isspace(*ptmp); ptmp++) {};

    // copy non-numerical text
    char* paction     = name_action;
    char* paction_end = name_action + 16;
    for ( /* nothing */; ptmp < ptmp_end && paction < paction_end && !Ego::isspace(*ptmp); ptmp++, paction++ )
    {
        if (Ego::isdigit(*ptmp)) break;
        *paction = *ptmp;
    }
    if ( paction < paction_end ) *paction = CSTR_END;

    name_fx[0] = CSTR_END;
    sscanf( ptmp, "%d %15s", &name_count, name_fx ); //ZF> NOTE: return value not used
    name_action[15] = CSTR_END;
    name_fx[15] = CSTR_END;

    // check for a non-trivial fx command
    if ( name_fx[0] == '\0' ) return;

    // scan the fx string for valid commands
    ptmp     = name_fx;
    ptmp_end = name_fx + 15;
    while ( CSTR_END != *ptmp && ptmp < ptmp_end )
    {
        size_t len;
        int token_index = -1;
        for ( cnt = 0; cnt < token_count; cnt++ )
        {
            len = strlen( tokens[cnt] );
            if ( 0 == strncmp( tokens[cnt], ptmp, len ) )
            {
                ptmp += len;
                token_index = cnt;
                break;
            }
        }

        if ( -1 == token_index )
        {
            //Ignore trailing zeros. Some older models appended zeros to all frames
            if(*ptmp != '0') {
				Log::activeTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__,  "model ", "`", _name, "`", ", frame ", frame, ", frame name ", "`", cFrameName,"`", " has unknown frame effects command ", "`", ptmp, "`", Log::EndOfEntry);
            }
            ptmp++;
        }
        else
        {
            bool bad_form = false;
            switch ( token_index )
            {
                case  0: // "I" == invulnerable
                    SET_BIT( fx, MADFX_INVICTUS );
                    break;

                case  1: // "S" == stop
                    SET_BIT( fx, MADFX_STOP );
                    break;

                case  2: // "F" == footfall
                    SET_BIT( fx, MADFX_FOOTFALL );
                    break;

                case  3: // "P" == poof
                    SET_BIT( fx, MADFX_POOF );
                    break;

                case  4: // "A" == action

                    // get any modifiers
                    while (( CSTR_END != *ptmp && ptmp < ptmp_end ) && ( 'R' == *ptmp || 'L' == *ptmp ) )
                    {
                        SET_BIT( fx, ( 'L' == *ptmp ) ? MADFX_ACTLEFT : MADFX_ACTRIGHT );
                        ptmp++;
                    }
                    break;

                case  5: // "G" == grab

                    // get any modifiers
                    while (( CSTR_END != *ptmp && ptmp < ptmp_end ) && ( 'R' == *ptmp || 'L' == *ptmp ) )
                    {
                        SET_BIT( fx, ( 'L' == *ptmp ) ? MADFX_GRABLEFT : MADFX_GRABRIGHT );
                        ptmp++;
                    }
                    break;

                case  6: // "D" == drop

                    // get any modifiers
                    while (( CSTR_END != *ptmp && ptmp < ptmp_end ) && ( 'R' == *ptmp || 'L' == *ptmp ) )
                    {
                        fx |= ( 'L' == *ptmp ) ? MADFX_DROPLEFT : MADFX_DROPRIGHT;
                        ptmp++;
                    }
                    break;

                case  7: // "C" == grab a character

                    // get any modifiers
                    while (( CSTR_END != *ptmp && ptmp < ptmp_end ) && ( 'R' == *ptmp || 'L' == *ptmp ) )
                    {
                        SET_BIT( fx, ( 'L' == *ptmp ) ? MADFX_CHARLEFT : MADFX_CHARRIGHT );
                        ptmp++;
                    }
                    break;

                case  8: // "LA"
                    bad_form = true;
                    SET_BIT( fx, MADFX_ACTLEFT );
                    break;

                case  9: // "LG"
                    bad_form = true;
                    SET_BIT( fx, MADFX_GRABLEFT );
                    break;

                case 10: // "LD"
                    bad_form = true;
                    SET_BIT( fx, MADFX_DROPLEFT );
                    break;

                case 11: // "LC"
                    bad_form = true;
                    SET_BIT( fx, MADFX_CHARLEFT );
                    break;

                case 12: // "RA"
                    bad_form = true;
                    SET_BIT( fx, MADFX_ACTRIGHT );
                    break;

                case 13: // "RG"
                    bad_form = true;
                    SET_BIT( fx, MADFX_GRABRIGHT );
                    break;

                case 14: // "RD"
                    bad_form = true;
                    SET_BIT( fx, MADFX_DROPRIGHT );
                    break;

                case 15: // "RC"
                    bad_form = true;
                    SET_BIT( fx, MADFX_CHARRIGHT );
                    break;
            }

            if ( bad_form && -1 != token_index )
            {
				Log::activeTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "model ", "`", _name, "`", ", frame ", frame, ", frame name ", "`", cFrameName, "`", " has a frame effects command in an improper configuration ", "`", tokens[token_index], "`", Log::EndOfEntry);
            }
        }
    }

    pframe.framefx = fx;
}

void ModelDescriptor::initializeWalkFrame(int lip, ModelAction action)
{
    int action_stt, action_end;

    action = getAction(action);
    if ( action >= ACTION_COUNT || !_actionValid[action] )
    {
        // make a fake action
        action_stt = _actionStart[ACTION_DA];
        action_end = _actionStart[ACTION_DA];
    }
    else
    {
        action_stt = _actionStart[action];
        action_end = _actionEnd[action];
    }

    // count the number of frames
    int action_count = 1 + ( action_end - action_stt );

    // scan through all the frames of the framelip
    for ( size_t frame = 0; frame < FRAMELIP_COUNT; frame++ )
    {
        int framealong = 0;

        if ( action_count > 0 )
        {
            // this SHOULD produce a number between 0 and (action_count - 1),
            // but there could be rounding error
            framealong = ( frame * action_count ) / FRAMELIP_COUNT;

            framealong = std::min( framealong, action_count - 1 );
        }

        _framelipToWalkframe[lip][frame] = action_stt + framealong;
    }
}

void ModelDescriptor::makeEquallyLit()
{
    _model->makeEquallyLit();
}

void ModelDescriptor::initializeFrameLip(ModelAction action)
{
    action = getAction(action);

    if ( !isActionValid(action) ) return;

    // grab the animation info
    const int action_stt = _actionStart[action];
    const int action_end = _actionEnd[action];
    const int action_count = 1 + ( action_end - action_stt );

    // scan through all the frames of the action
    for (int frame = action_stt; frame <= action_end; frame++)
    {
        // grab a valid frame
        if (frame >= _model->getFrames().size()) break;

        // calculate the framelip.
        // this should produce a number between 0 and FRAMELIP_COUNT-1, but
        // watch out for possible rounding errors
        int framelip = (( frame - action_stt ) * FRAMELIP_COUNT ) / action_count;

        // limit the framelip to the valid range
        _model->getFrames()[frame].framelip = std::min<size_t>(framelip, FRAMELIP_COUNT - 1);
    }
}

void ModelDescriptor::healActions(const std::string &filePath)
{
    actionCopyCorrect(ACTION_DA, ACTION_DB);  // All dances should be safe
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
    actionCopyCorrect(ACTION_DA, ACTION_WD);  // All walks should be safe
    actionCopyCorrect(ACTION_WC, ACTION_WD);
    actionCopyCorrect(ACTION_WB, ACTION_WC);
    actionCopyCorrect(ACTION_WA, ACTION_WB);
    actionCopyCorrect(ACTION_JA, ACTION_JB);
    actionCopyCorrect(ACTION_JB, ACTION_JC);
    actionCopyCorrect(ACTION_DA, ACTION_JC);  // All jumps should be safe
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

    // Copy entire actions to save frame space COPY.TXT
    std::unique_ptr<ReadContext> ctxt = nullptr;
    try {
        ctxt = std::make_unique<ReadContext>(filePath);
    } catch (...) {
        return;
    }
        while (ctxt->skipToColon(true))
        {
            std::string szOne, szTwo;

            szOne = vfs_read_string_lit( *ctxt );
            ModelAction actiona = ModelDescriptor::charToAction(szOne[0]);

            szTwo = vfs_read_string_lit( *ctxt );
            ModelAction actionb = ModelDescriptor::charToAction(szTwo[0]);

            actionCopyCorrect(static_cast<ModelAction>(actiona + 0), static_cast<ModelAction>(actionb + 0));
            actionCopyCorrect(static_cast<ModelAction>(actiona + 1), static_cast<ModelAction>(actionb + 1));
            actionCopyCorrect(static_cast<ModelAction>(actiona + 2), static_cast<ModelAction>(actionb + 2));
            actionCopyCorrect(static_cast<ModelAction>(actiona + 3), static_cast<ModelAction>(actionb + 3));
        }
}

ModelAction ModelDescriptor::charToAction(char cTmp)
{
    switch ( idlib::to_upper( cTmp ) )
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
        // case 'W': action = ACTION_WA; break;   /// @note ZF@> Can't do this, attack animation WALK is used for doing nothing (for example charging spells)
        case 'H': return ACTION_HA;
        case 'K': return ACTION_KA;
        default:  return ACTION_DA;
    }
}

void ModelDescriptor::actionCopyCorrect(ModelAction actiona, ModelAction actionb)
{
    // With the new system using the action_map, this is all that is really necessary
    if ( ACTION_COUNT == _actionMap[actiona] )
    {
        if ( _actionValid[actionb] )
        {
            _actionMap[actiona] = actionb;
        }
        else if ( ACTION_COUNT != _actionMap[actionb] )
        {
            _actionMap[actiona] = _actionMap[actionb];
        }
    }
    else if ( ACTION_COUNT == _actionMap[actionb] )
    {
        if ( _actionValid[actiona] )
        {
            _actionMap[actionb] = actiona;
        }
        else if ( ACTION_COUNT != _actionMap[actiona] )
        {
            _actionMap[actionb] = _actionMap[actiona];
        }
    }
}

const std::shared_ptr<Ego::Graphics::AnimatedModel>& ModelDescriptor::getModel() const
{
    return _model;
}

bool ModelDescriptor::isFrameValid(int action, int frame) const
{
    if(!isActionValid(action)) return false;
    if (frame < _actionStart[action]) return false;
    if (frame > _actionEnd[action]) return false;

    return true;
}

int ModelDescriptor::getFrameLipToWalkFrame(int lip, int framelip) const
{
    assert(lip >= 0 && lip < LIP_COUNT && framelip >= 0 && framelip < FRAMELIP_COUNT);
    return _framelipToWalkframe[lip][framelip]; 
}

} //Ego
