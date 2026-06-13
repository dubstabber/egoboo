#include "ModelDescriptor.hpp"
#include "egolib/Log/_Include.hpp"
#include "egolib/Graphics/MD2Model.hpp"
#include "egolib/strutil.h"
#include "egolib/Core/StringUtilities.hpp"
#include "egolib/fileutil.h"
#include "egolib/Logic/ObjectSlot.hpp"

namespace Ego
{

constexpr int STRING_SWITCH(int a, int b)
{
    return a | (b << 16);
}

ModelAction ModelDescriptor::stringToAction(const std::string &action) const
{
    if(action.size() >= 2) {

        switch(STRING_SWITCH(action[0], action[1]))
        {
            case STRING_SWITCH('D','A'): return ACTION_DA;         //"Dance ( Standing still )"
            case STRING_SWITCH('D','B'): return ACTION_DB;         //"Dance ( Bored )"
            case STRING_SWITCH('D','C'): return ACTION_DC;         //"Dance ( Bored )"
            case STRING_SWITCH('D','D'): return ACTION_DD;         //"Dance ( Bored )"
            case STRING_SWITCH('U','A'): return ACTION_UA;         //"Unarmed"
            case STRING_SWITCH('U','B'): return ACTION_UB;         //"Unarmed"
            case STRING_SWITCH('U','C'): return ACTION_UC;         //"Unarmed"
            case STRING_SWITCH('U','D'): return ACTION_UD;         //"Unarmed"
            case STRING_SWITCH('T','A'): return ACTION_TA;         //"Thrust"
            case STRING_SWITCH('T','B'): return ACTION_TB;         //"Thrust"
            case STRING_SWITCH('T','C'): return ACTION_TC;         //"Thrust"
            case STRING_SWITCH('T','D'): return ACTION_TD;         //"Thrust"
            case STRING_SWITCH('C','A'): return ACTION_CA;         //"Crush"
            case STRING_SWITCH('C','B'): return ACTION_CB;         //"Crush"
            case STRING_SWITCH('C','C'): return ACTION_CC;         //"Crush"
            case STRING_SWITCH('C','D'): return ACTION_CD;         //"Crush"
            case STRING_SWITCH('S','A'): return ACTION_SA;         //"Slash"
            case STRING_SWITCH('S','B'): return ACTION_SB;         //"Slash"
            case STRING_SWITCH('S','C'): return ACTION_SC;         //"Slash"
            case STRING_SWITCH('S','D'): return ACTION_SD;         //"Slash"
            case STRING_SWITCH('B','A'): return ACTION_BA;         //"Bash"
            case STRING_SWITCH('B','B'): return ACTION_BB;         //"Bash"
            case STRING_SWITCH('B','C'): return ACTION_BC;         //"Bash"
            case STRING_SWITCH('B','D'): return ACTION_BD;         //"Bash"
            case STRING_SWITCH('L','A'): return ACTION_LA;         //"Longbow"
            case STRING_SWITCH('L','B'): return ACTION_LB;         //"Longbow"
            case STRING_SWITCH('L','C'): return ACTION_LC;         //"Longbow"
            case STRING_SWITCH('L','D'): return ACTION_LD;         //"Longbow"
            case STRING_SWITCH('X','A'): return ACTION_XA;         //"Crossbow"
            case STRING_SWITCH('X','B'): return ACTION_XB;         //"Crossbow"
            case STRING_SWITCH('X','C'): return ACTION_XC;         //"Crossbow"
            case STRING_SWITCH('X','D'): return ACTION_XD;         //"Crossbow"
            case STRING_SWITCH('F','A'): return ACTION_FA;         //"Flinged"
            case STRING_SWITCH('F','B'): return ACTION_FB;         //"Flinged"
            case STRING_SWITCH('F','C'): return ACTION_FC;         //"Flinged"
            case STRING_SWITCH('F','D'): return ACTION_FD;         //"Flinged"
            case STRING_SWITCH('P','A'): return ACTION_PA;         //"Parry"
            case STRING_SWITCH('P','B'): return ACTION_PB;         //"Parry"
            case STRING_SWITCH('P','C'): return ACTION_PC;         //"Parry"
            case STRING_SWITCH('P','D'): return ACTION_PD;         //"Parry"
            case STRING_SWITCH('E','A'): return ACTION_EA;         //"Evade"
            case STRING_SWITCH('E','B'): return ACTION_EB;         //"Evade"
            case STRING_SWITCH('R','A'): return ACTION_RA;         //"Roll"
            case STRING_SWITCH('Z','A'): return ACTION_ZA;         //"Zap Magic"
            case STRING_SWITCH('Z','B'): return ACTION_ZB;         //"Zap Magic"
            case STRING_SWITCH('Z','C'): return ACTION_ZC;         //"Zap Magic"
            case STRING_SWITCH('Z','D'): return ACTION_ZD;         //"Zap Magic"
            case STRING_SWITCH('W','A'): return ACTION_WA;         //"Sneak"
            case STRING_SWITCH('W','B'): return ACTION_WB;         //"Walk"
            case STRING_SWITCH('W','C'): return ACTION_WC;         //"Run"
            case STRING_SWITCH('W','D'): return ACTION_WD;         //"Push"
            case STRING_SWITCH('J','A'): return ACTION_JA;         //"Jump"
            case STRING_SWITCH('J','B'): return ACTION_JB;         //"Jump ( Falling ) ( Drop left )"
            case STRING_SWITCH('J','C'): return ACTION_JC;         //"Jump ( Falling ) ( Drop right )"
            case STRING_SWITCH('H','A'): return ACTION_HA;         //"Hit ( Taking damage )"
            case STRING_SWITCH('H','B'): return ACTION_HB;         //"Hit ( Taking damage )"
            case STRING_SWITCH('H','C'): return ACTION_HC;         //"Hit ( Taking damage )"
            case STRING_SWITCH('H','D'): return ACTION_HD;         //"Hit ( Taking damage )"
            case STRING_SWITCH('K','A'): return ACTION_KA;         //"Killed"
            case STRING_SWITCH('K','B'): return ACTION_KB;         //"Killed"
            case STRING_SWITCH('K','C'): return ACTION_KC;         //"Killed"
            case STRING_SWITCH('K','D'): return ACTION_KD;         //"Killed"
            case STRING_SWITCH('M','A'): return ACTION_MA;         //"Drop Item Left"
            case STRING_SWITCH('M','B'): return ACTION_MB;         //"Drop Item Right"
            case STRING_SWITCH('M','C'): return ACTION_MC;         //"Cheer"
            case STRING_SWITCH('M','D'): return ACTION_MD;         //"Show Off"
            case STRING_SWITCH('M','E'): return ACTION_ME;         //"Grab Item Left"
            case STRING_SWITCH('M','F'): return ACTION_MF;         //"Grab Item Right"
            case STRING_SWITCH('M','G'): return ACTION_MG;         //"Open Chest"
            case STRING_SWITCH('M','H'): return ACTION_MH;         //"Sit ( Riding a mount )"
            case STRING_SWITCH('M','I'): return ACTION_MI;         //"Ride"
            case STRING_SWITCH('M','J'): return ACTION_MJ;         //"Activated ( For items )"
            case STRING_SWITCH('M','K'): return ACTION_MK;         //"Snoozing"
            case STRING_SWITCH('M','L'): return ACTION_ML;         //"Unlock"
            case STRING_SWITCH('M','M'): return ACTION_MM;         //"Held Left"
            case STRING_SWITCH('M','N'): return ACTION_MN;         //"Held Right
            default:
                //fall through
            break;
        }
    }

    return ACTION_COUNT;
}

ModelDescriptor::ModelDescriptor(const std::string &folderPath) :
    _name(folderPath),      //Make up a name for the model...  IMPORT\TEMP0000.OBJ
    _actionMap(),
    _actionValid(),
    _actionStart(),
    _actionEnd(),
    _md2Model(nullptr)
{
    // Clear out all actions and reset to invalid
    _actionMap.fill(ACTION_COUNT);
    _actionValid.fill(false);
    _actionStart.fill(0);
    _actionEnd.fill(0);

    for(size_t i = 0; i < LIP_COUNT; ++i) {
        for(size_t j = 0; j < FRAMELIP_COUNT; ++j) {
            _framelipToWalkframe[i][j] = 0;
        }
    }

    // load the model from the file
    _md2Model = MD2Model::loadFromFile(folderPath + "/tris.md2");
    if(!_md2Model) {
        throw std::runtime_error("File not found: " + folderPath + "/tris.md2");
    }

    /// @details Egoboo md2 models were designed with 1 tile = 32x32 units, but internally Egoboo uses
    ///      1 tile = 128x128 units. Previously, this was handled by sprinkling a bunch of
    ///      commands that multiplied various quantities by 4 or by 4.125 throughout the code.
    ///      It was very counterintuitive, and caused me no end of headaches...  Of course the
    ///      solution is to scale the model!
    _md2Model->scaleModel(-3.5f, 3.5f, 3.5f);

    // Create the actions table for this imad
    ripActions();
    healActions(folderPath + "/copy.txt");

    // Create table for doing transition from one type of walk to another...
    // Clear 'em all to start
    for(MD2_Frame &frame : _md2Model->getFrames())
    {
        frame.framelip = 0;
    }

    // Need to figure out how far into action each frame is
    initializeFrameLip(ACTION_WA);
    initializeFrameLip(ACTION_WB);
    initializeFrameLip(ACTION_WC);

    // Now do the same, in reverse, for walking animations
    initializeWalkFrame(LIPDA, ACTION_DA);
    initializeWalkFrame(LIPWA, ACTION_WA);
    initializeWalkFrame(LIPWB, ACTION_WB);
    initializeWalkFrame(LIPWC, ACTION_WC);    
}

const std::string& ModelDescriptor::getName() const
{
    return _name;
}

bool ModelDescriptor::isActionValid(int action) const
{
    if(action < 0 || action >= _actionValid.size()) {
        return false;
    }
    return _actionValid[action];
}


ModelAction ModelDescriptor::getAction(int action) const
{
    // you are pretty much guaranteed that ACTION_DA will be valid for a model,
    // I guess it could be invalid if the model had no frames or something
    ModelAction retval = ACTION_DA;
    if (!isActionValid(ACTION_DA))
    {
        retval = ACTION_COUNT;
    }

    // track down a valid value
    if ( isActionValid(action) )
    {
        return static_cast<ModelAction>(action);
    }
    else if ( ACTION_COUNT != _actionMap[action] )
    {
        // do a "recursive" search for a valid action
        // we should never really have to check more than once if the map is prepared
        // properly BUT you never can tell. Make sure we do not get a runaway loop by
        // you never go farther than ACTION_COUNT steps and that you never see the
        // original action again

        ModelAction tnc = _actionMap[action];
        for (size_t cnt = 0; cnt < ACTION_COUNT; cnt++)
        {
            if ( tnc >= ACTION_COUNT || tnc == action ) break;

            if (isActionValid(tnc))
            {
                return tnc;
            }
            tnc = _actionMap[tnc];
        }
    }

    return retval;    
}

BIT_FIELD ModelDescriptor::getMadFX(int action) const
{
    if ( !isActionValid(action) ) return EMPTY_BIT_FIELD;

    //Loop through all frames in animation and collect all FX bits that are set
    BIT_FIELD retval = EMPTY_BIT_FIELD;
    const std::vector<MD2_Frame> &frames = _md2Model->getFrames();
    for (size_t cnt = _actionStart[action]; cnt <= _actionEnd[action]; cnt++)
    {
        SET_BIT(retval, frames[cnt].framefx);
    }

    return retval;
}

ModelAction ModelDescriptor::randomizeAction(ModelAction action, int slot) const
{
    // a valid slot?
    if ( slot < 0 || slot >= SLOT_COUNT ) return action;

    // a valid action?
    if (action >= ACTION_COUNT) return action;

    const int diff = slot * 2;

    //---- non-randomizable actions
    switch(action)
    {
        case ACTION_MG:         // MG      = Open Chest
        case ACTION_MH:         // MH      = Sit
        case ACTION_MI:         // MI      = Ride
        case ACTION_MJ:         // MJ      = Object Activated
        case ACTION_MK:         // MK      = Snoozing
        case ACTION_ML:         // ML      = Unlock
        case ACTION_JA:         // JA      = Jump
        case ACTION_RA:         // RA      = Roll
            return action;

        //---- do a couple of special actions that have left/right
        case ACTION_EA:
        case ACTION_EB:
          action = static_cast<ModelAction>(ACTION_JB + slot);   // EA/EB = Evade left/right
        break;
        case ACTION_JB:
        case ACTION_JC:
          action = static_cast<ModelAction>(ACTION_JB + slot);   // JB/JC = Dropped item left/right
        break;
        case ACTION_MA:
        case ACTION_MB:
          action = static_cast<ModelAction>(ACTION_MA + slot);   // MA/MB = Drop left/right item
        break;
        case ACTION_MC:
        case ACTION_MD:
          action = static_cast<ModelAction>(ACTION_MC + slot);   // MC/MD = Slam left/right
        break;
        case ACTION_ME:
        case ACTION_MF:
          action = static_cast<ModelAction>(ACTION_ME + slot);   // ME/MF = Grab item left/right
        break;
        case ACTION_MM:
        case ACTION_MN:
          action = static_cast<ModelAction>(ACTION_MM + slot);   // MM/MN = Held left/right
        break;

        default:
            if ( ACTION_IS_TYPE(action, W) ) return action;  // WA - WD = Walk
    }

    //---- actions that can be randomized, but are not left/right sensitive
    // D = dance (idle animation)
    if (action == ACTION_DA || action == ACTION_DB || action == ACTION_DC || action == ACTION_DD)      action = static_cast<ModelAction>(ACTION_DA + Random::next(3));

    //---- handle all the normal attack/defense animations
    // U = unarmed
    else if ( ACTION_IS_TYPE( action, U ) ) action = static_cast<ModelAction>(ACTION_UA + diff + Random::next(1));
    // T = thrust
    else if ( ACTION_IS_TYPE( action, T ) ) action = static_cast<ModelAction>(ACTION_TA + diff + Random::next(1));
    // C = chop
    else if ( ACTION_IS_TYPE( action, C ) ) action = static_cast<ModelAction>(ACTION_CA + diff + Random::next(1));
    // S = slice
    else if ( ACTION_IS_TYPE( action, S ) ) action = static_cast<ModelAction>(ACTION_SA + diff + Random::next(1));
    // B = bash
    else if ( ACTION_IS_TYPE( action, B ) ) action = static_cast<ModelAction>(ACTION_BA + diff + Random::next(1));
    // L = longbow
    else if ( ACTION_IS_TYPE( action, L ) ) action = static_cast<ModelAction>(ACTION_LA + diff + Random::next(1));
    // X = crossbow
    else if ( ACTION_IS_TYPE( action, X ) ) action = static_cast<ModelAction>(ACTION_XA + diff + Random::next(1));
    // F = fling
    else if ( ACTION_IS_TYPE( action, F ) ) action = static_cast<ModelAction>(ACTION_FA + diff + Random::next(1));
    // P = parry/block
    else if ( ACTION_IS_TYPE( action, P ) ) action = static_cast<ModelAction>(ACTION_PA + diff + Random::next(1));
    // Z = zap
    else if ( ACTION_IS_TYPE( action, Z ) ) action = static_cast<ModelAction>(ACTION_ZA + diff + Random::next(1));

    //---- these are passive actions
    // H = hurt
    else if ( ACTION_IS_TYPE( action, H ) ) action = static_cast<ModelAction>(ACTION_HA + Random::next(3));
    // K = killed
    else if ( ACTION_IS_TYPE( action, K ) ) action = static_cast<ModelAction>(ACTION_KA + Random::next(3));

    return action;    
}

} //Ego
