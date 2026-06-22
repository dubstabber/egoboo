//********************************************************************************************
//*
//*    This file is part of Egoboo.
//*
//*    Egoboo is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Egoboo is distributed in the hope that it will be useful, but
//*    WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Egoboo.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************

/// @file  egolib/Script/script_operand.c
/// @brief Operand evaluator for the EgoScript VM.
/// @details Implements operand-context population (makeOperandContext and its populate*
///          helpers), variable load/store (loadVariable 80-case switch + storeVariable),
///          the run_operand arithmetic dispatcher, getVariableName,
///          onVariableNotDefinedError, and script_info_t position methods
///          (increment_pos / get_pos / set_pos).
///
///          The character-script/alert driver (scripting_system_begin/end,
///          scr_run_chr_script, runCharacterScript, set_alerts, ai_state_t::get_wp/
///          ensure_wp) lives in script_driver.c. The opcode dispatch methods and
///          order-publication helpers live in the sibling script.c.

#include "egolib/Script/script.h"
#include "egolib/Script/script_internal.h"

#include "egolib/game/script_compile.h"
#include "egolib/game/script_functions_internal.h"
#include "egolib/game/script_variables.h"

namespace
{

ObjectRef resolveLeaderRefForVariables(const ITargetInfo& selfInfo)
{
    return teamLeaderRef(selfInfo);
}

void populateSelfOperandContext(Ego::Script::ScriptOperandContext& context)
{
    context.selfObject = tryObject(context.selfRef);
    if (context.selfObject == nullptr)
    {
        return;
    }

    context.selfPhysical = tryPhysical(context.selfRef);
    context.selfCharacterState = tryCharacterState(context.selfRef);
    context.selfTargetInfo = tryTargetInfo(context.selfRef);
    context.selfWallet = tryWallet(context.selfRef);
    context.selfInventoryHolder = tryInventoryHolder(context.selfRef);
}

void populateTargetOperandContext(Ego::Script::ScriptOperandContext& context)
{
    context.targetObject = tryObject(context.targetRef);
    if (context.targetObject == nullptr)
    {
        return;
    }

    context.targetPhysical = tryPhysical(context.targetRef);
    context.targetCharacterState = tryCharacterState(context.targetRef);
    context.targetTargetInfo = tryTargetInfo(context.targetRef);
    context.targetWallet = tryWallet(context.targetRef);
}

void populateOwnerOperandContext(Ego::Script::ScriptOperandContext& context)
{
    context.ownerObject = tryObject(context.ownerRef);
    if (context.ownerObject == nullptr)
    {
        return;
    }

    context.ownerPhysical = tryPhysical(context.ownerRef);
}

void populateLeaderOperandContext(Ego::Script::ScriptOperandContext& context)
{
    if (context.selfTargetInfo == nullptr)
    {
        return;
    }

    context.leaderRef = resolveLeaderRefForVariables(*context.selfTargetInfo);
    context.leaderPhysical = tryPhysical(context.leaderRef);
}

Ego::Script::ScriptOperandContext makeOperandContext(const ai_state_t& aiState)
{
    Ego::Script::ScriptOperandContext context;
    context.selfRef = aiState.getSelf();
    context.targetRef = aiState.getTarget();
    context.ownerRef = aiState.owner;

    populateSelfOperandContext(context);
    if (!context.hasSelf())
    {
        return context;
    }

    populateTargetOperandContext(context);
    populateOwnerOperandContext(context);
    populateLeaderOperandContext(context);

    return context;
}

} // anonymous namespace

//--------------------------------------------------------------------------------------------
std::string getVariableName(int variableIndex)
{
    return Ego::Script::_scriptVariableNames[variableIndex];
}

int32_t script_state_t::loadVariable(uint8_t variableIndex, ai_state_t& aiState, const Ego::Script::ScriptOperandContext& context)
{
    switch (variableIndex)
    {

    #define DEFINE(name) \
        case Ego::Script::VAR##name: \
        { \
            return load_##VAR##name(*this, aiState, context); \
        }

        DEFINE(TMPX)
        DEFINE(TMPY)
        DEFINE(TMPDISTANCE)
        DEFINE(TMPTURN)
        DEFINE(TMPARGUMENT)
        DEFINE(RAND)
        DEFINE(SELFX)
        DEFINE(SELFY)
        DEFINE(SELFTURN)
        DEFINE(SELFCOUNTER)
        DEFINE(SELFORDER)
        DEFINE(SELFMORALE)
        DEFINE(SELFLIFE)
        DEFINE(TARGETX)
        DEFINE(TARGETY)
        DEFINE(TARGETDISTANCE)
        DEFINE(TARGETTURN)
        DEFINE(LEADERX)
        DEFINE(LEADERY)
        DEFINE(LEADERDISTANCE)
        DEFINE(LEADERTURN)
        DEFINE(GOTOX)
        DEFINE(GOTOY)
        DEFINE(GOTODISTANCE)
        DEFINE(TARGETTURNTO)
        DEFINE(PASSAGE)
        DEFINE(WEIGHT)
        DEFINE(SELFALTITUDE)
        DEFINE(SELFID)
        DEFINE(SELFHATEID)
        DEFINE(SELFMANA)
        DEFINE(TARGETSTR)
        DEFINE(TARGETINT)
        DEFINE(TARGETDEX)
        DEFINE(TARGETLIFE)
        DEFINE(TARGETMANA)
        DEFINE(TARGETLEVEL)
        DEFINE(TARGETSPEEDX)
        DEFINE(TARGETSPEEDY)
        DEFINE(TARGETSPEEDZ)
        DEFINE(SELFSPAWNX)
        DEFINE(SELFSPAWNY)
        DEFINE(SELFSTATE)
        DEFINE(SELFCONTENT)
        DEFINE(SELFSTR)
        DEFINE(SELFINT)
        DEFINE(SELFDEX)
        DEFINE(SELFMANAFLOW)
        DEFINE(TARGETMANAFLOW)
        DEFINE(SELFATTACHED)
        DEFINE(SWINGTURN)
        DEFINE(XYDISTANCE)
        DEFINE(SELFZ)
        DEFINE(TARGETALTITUDE)
        DEFINE(TARGETZ)
        DEFINE(SELFINDEX)
        DEFINE(OWNERX)
        DEFINE(OWNERY)
        DEFINE(OWNERTURN)
        DEFINE(OWNERDISTANCE)
        DEFINE(OWNERTURNTO)
        DEFINE(XYTURNTO)
        DEFINE(SELFMONEY)
        DEFINE(SELFACCEL)
        DEFINE(TARGETEXP)
        DEFINE(SELFAMMO)
        DEFINE(TARGETAMMO)
        DEFINE(TARGETMONEY)
        DEFINE(TARGETTURNAWAY)
        DEFINE(SELFLEVEL)
        DEFINE(TARGETRELOADTIME)
        DEFINE(SPAWNDISTANCE)
        DEFINE(TARGETMAXLIFE)
        DEFINE(TARGETTEAM)
        DEFINE(TARGETARMOR)
        DEFINE(DIFFICULTY)
        DEFINE(TIMEHOURS)
        DEFINE(TIMEMINUTES)
        DEFINE(TIMESECONDS)
        DEFINE(DATEMONTH)
        DEFINE(DATEDAY)

    #undef DEFINE

        default:
            onVariableNotDefinedError(variableIndex);
    }
}


void script_state_t::storeVariable(uint8_t variableIndex)
{
    auto variableName = getVariableName(variableIndex);
    switch (variableIndex)
    {
        case Ego::Script::VARTMPX:
            x = operationsum;
            break;

        case Ego::Script::VARTMPY:
            y = operationsum;
            break;

        case Ego::Script::VARTMPDISTANCE:
            distance = operationsum;
            break;

        case Ego::Script::VARTMPTURN:
            turn = operationsum;
            break;

        case Ego::Script::VARTMPARGUMENT:
            argument = operationsum;
            break;

        default:
            onVariableNotDefinedError(variableIndex);
    }
}

//--------------------------------------------------------------------------------------------


void script_state_t::onVariableNotDefinedError(uint8_t variableIndex)
{
    auto variableName = getVariableName(variableIndex);
    Log::Entry e(Log::Level::Warning, __FILE__, __LINE__);
    e << "variable " << variableName << "/" << (uint16_t)variableIndex << " not defined" << Log::EndOfEntry;
    Log::activeTarget() << e;
    throw idlib::runtime_error(__FILE__, __LINE__, e.getText());
}

void script_state_t::run_operand(ai_state_t& aiState, script_info_t& script)
{
    /// @author ZZ
    /// @details This function does the scripted arithmetic in OPERATOR, OPERAND pscriptrs

    const Ego::Script::ScriptOperandContext context = makeOperandContext(aiState);
    if (!context.hasSelf()) return;

    std::string varname;

    // get the operator
    int32_t iTmp = 0;

    auto constantIndex = script._instructions[script.get_pos()].getValueBits();
    const auto& constant = script._instructions.getConstantPool().getConstant(constantIndex);
    uint8_t operation = script._instructions[script.get_pos()].getDataBits();
    if (script._instructions[script.get_pos()].isLdc())
    {
        // Load the constant.
        iTmp = constant.getAsInteger();
        if (debug_scripts)
        {
            std::stringstream stringStream;
            stringStream << iTmp;
            varname = stringStream.str();
        }
    }
    else
    {
        // Load the variable.
        auto variableIndex = constant.getAsInteger();
        varname = getVariableName(variableIndex);
        iTmp = loadVariable(variableIndex, aiState, context);
    }

    // Now do the math
    std::string op = "UNKNOWN";
    switch (operation)
    {
        case Ego::Script::OPADD:
            op = "ADD";
            operationsum = int(operationsum) + iTmp;
            break;

        case Ego::Script::OPSUB:
            op = "SUB";
            operationsum = int(operationsum) - iTmp;
            break;

        case Ego::Script::OPAND:
            op = "AND";
            operationsum = int(operationsum) & iTmp;
            break;

        case Ego::Script::OPSHR:
            op = "SHR";
            operationsum = int(operationsum) >> iTmp;
            break;

        case Ego::Script::OPSHL:
            op = "SHL";
            operationsum = int(operationsum) << iTmp;
            break;

        case Ego::Script::OPMUL:
            op = "MUL";
            operationsum = int(operationsum) * iTmp;
            break;

        case Ego::Script::OPDIV:
            op = "DIV";
            if (iTmp != 0)
            {
                operationsum = static_cast<float>(operationsum) / iTmp;
            }
            else
            {
                Log::activeTarget() << Log::Entry::create(Log::Level::Message, __FILE__, __LINE__, "script error - model = ",
                                                 script_error_model, " class name == `", script_error_classname,
                                                 "`: divide by zero", Log::EndOfEntry);
            }
            break;

        case Ego::Script::OPMOD:
            op = "MOD";
            if (iTmp != 0)
            {
                operationsum = int(operationsum) % iTmp;
            }
            else
            {
                Log::activeTarget() << Log::Entry::create(Log::Level::Message, __FILE__, __LINE__, "script error - model = ",
                                                 script_error_model, " class name == `", script_error_classname,
                                                 "`: modulo by zero", Log::EndOfEntry);
            }
            break;

        default:
            Log::activeTarget() << Log::Entry::create(Log::Level::Message, __FILE__, __LINE__, "script error - model = ",
                                             script_error_model, " class name == `", script_error_classname,
                                             "`: unknown opcode", Log::EndOfEntry);
            break;
    }

    if (debug_scripts && debug_script_file)
    {
        vfs_printf(debug_script_file, "%s %s(%d) ", op.c_str(), varname.c_str(), iTmp);
    }
}

//--------------------------------------------------------------------------------------------

bool script_info_t::increment_pos()
{
    if (_position >= _instructions.getNumberOfInstructions())
    {
        return false;
    }
    _position++;
    return true;
}

size_t script_info_t::get_pos() const
{
    return _position;
}

bool script_info_t::set_pos(size_t position)
{
    if (position >= _instructions.getNumberOfInstructions())
    {
        return false;
    }
    _position = position;
    return true;
}
