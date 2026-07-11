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

/// @file  egolib/Script/script.c
/// @brief Implements the game's scripting language.
/// @details

#include "egolib/Script/script.h"
#include "egolib/Script/script_internal.h"

#include "egolib/AI/AStar.hpp"
#include "egolib/Script/IRuntimeStatistics.hpp"

#include "egolib/game/script_compile.h"
#include "egolib/game/script_implementation.h"
#include "egolib/game/script_functions.h"
#include "egolib/game/script_functions_internal.h"
#include "egolib/game/script_variables.h"
#include "egolib/Entities/IObjectWorld.hpp"
#include "egolib/Entities/IScriptRuntimeState.hpp"


namespace Ego {
namespace Script {

/// @brief An implementation of runtime statistics.
struct RuntimeStatistics : IRuntimeStatistics<uint32_t>
{
public:
    void append(const std::string& pathname) override
    {
        auto target = std::shared_ptr<vfs_FILE>(vfs_openAppend(pathname),
                                                [](vfs_FILE *file) { if (nullptr != file) { vfs_close(file); } });
        if (nullptr != target)
        {
            for (const auto& functionStatistic : _functionStatistics)
            {
                vfs_printf(target.get(), "function = %" PRIu32 "\t function name = \"%s\"\tnumber of calls = %ld\ttotalTime = %lf\tmaxTime = %lf\n",
                           functionStatistic.first, _scriptFunctionNames[functionStatistic.first].c_str(), functionStatistic.second.numberOfCalls,
                           functionStatistic.second.totalTime, functionStatistic.second.maxTime);

            }
        }
    }
};

std::array<std::string, Ego::Script::ScriptVariables::SCRIPT_VARIABLES_COUNT> _scriptVariableNames = {
#define Define(cName, eName) #cName,
#define DefineAlias(cName, eName)
#include "egolib/Script/Variables.in"
#undef DefineAlias
#undef Define
};

std::array<std::string, ScriptFunctions::SCRIPT_FUNCTIONS_COUNT> _scriptFunctionNames = {
#define Define(name) #name,
#define DefineAlias(alias, name)
#include "egolib/Script/Functions.in"
#undef DefineAlias
#undef Define
};

Runtime::Runtime() :
    _functionValueCodeToFunctionPointer
    {
        #define Define(name) { name, &scr_##name },
        #define DefineAlias(alias, name) { alias, &scr_##name },     
        #include "egolib/Script/Functions.in"
        #undef DefineAlias
        #undef Define
    },
    m_opcodeInfos
    {
    #define Define(cname, name) { cname, { cname, #cname }},
    #define DefineAlias(calias, cname)
    #include "egolib/Script/Operators.in"
    #undef DefineAlias
    #undef Define
    },
    _statistics(std::make_unique<RuntimeStatistics>()),
    _clock(std::make_unique<Ego::Time::Clock<Ego::Time::ClockPolicy::NonRecursive>>("runtime clock", 1))
{
    /* Intentionally empty. */
}

Runtime::~Runtime()
{
    /* Intentionally empty. */
}

ai_state_t& runtimeState(IScriptRuntimeState& object)
{
    return object.scriptRuntimeState();
}

const ai_state_t& runtimeState(const IScriptRuntimeState& object)
{
    return object.scriptRuntimeState();
}

} // namespace Script
} // namespace Ego

ObjectProfileRef script_error_model = ObjectProfileRef::Invalid;
const char * script_error_classname = "UNKNOWN";

//--------------------------------------------------------------------------------------------
bool script_state_t::run_function_call(ai_state_t& aiState, script_info_t& script)
{
    uint8_t  functionreturn;

    // check for valid execution pointer
    if (script.get_pos() >= script._instructions.getNumberOfInstructions()) return false;

    // Run the function
    functionreturn = run_function(aiState, script);

    // move the execution pointer to the jump code
    script.increment_pos();
    if (functionreturn)
    {
        // move the execution pointer to the next opcode
        script.increment_pos();
    }
    else
    {
        // A well-formed function is always followed by its jump code; a malformed or
        // truncated script (e.g. one whose compilation failed) may not be, in which case
        // the position now sits past the end. Stop rather than read out of bounds.
        if (script.get_pos() >= script._instructions.getNumberOfInstructions())
        {
            return false;
        }

        // use the jump code to jump to the right location
        size_t new_index = script._instructions[script.get_pos()].getBits();

        // make sure the value is valid
        IDLIB_DEBUG_ASSERT(new_index <= script._instructions.getNumberOfInstructions());

        // actually do the jump
        script.set_pos(new_index);
    }

    return true;
}

//--------------------------------------------------------------------------------------------
/// @todo Merge with caller.
bool script_state_t::run_operation(ai_state_t& aiState, script_info_t& script)
{
    // check for valid execution pointer
    if (script.get_pos() >= script._instructions.getNumberOfInstructions()) return false;

    auto constantIndex = script._instructions[script.get_pos()].getValueBits();
    const auto& constant = script._instructions.getConstantPool().getConstant(constantIndex);
    uint32_t variableIndex = constant.getAsInteger();

    // debug stuff
    std::string variable = "UNKNOWN";
    if (debug_scripts && debug_script_file)
    {

        for (auto i = 0; i < script.indent; i++) { vfs_printf(debug_script_file, "  "); }

        for (auto i = 0; i < Opcodes.size(); i++)
        {
            if (Ego::Script::PDLTokenKind::Variable == Opcodes[i]._kind && variableIndex == Opcodes[i].iValue)
            {
                variable = Opcodes[i].cName;
                break;
            }
        }

        vfs_printf(debug_script_file, "%s = ", variable.c_str());
    }

    // Get the number of operands. A truncated script may end immediately after the
    // operation opcode with no operand count following, leaving the position past the
    // end; stop rather than read out of bounds.
    script.increment_pos();
    if (script.get_pos() >= script._instructions.getNumberOfInstructions())
    {
        return false;
    }
    auto operand_count = script._instructions[script.get_pos()].getBits();

    // Now run the operation
    operationsum = 0;
    for (auto i = 0; i < operand_count && script.get_pos() < script._instructions.getNumberOfInstructions(); ++i)
    {
        script.increment_pos();
        run_operand(aiState, script);
    }
    if (debug_scripts && debug_script_file)
    {
        vfs_printf(debug_script_file, " == %d \n", (int)operationsum);
    }

    // Save the results in the register that called the arithmetic
    storeVariable(variableIndex);

    // go to the next opcode
    script.increment_pos();

    return true;
}

//--------------------------------------------------------------------------------------------
uint8_t script_state_t::run_function(ai_state_t& aiState, script_info_t& script)
{
    auto constantIndex = script._instructions[script.get_pos()].getValueBits();
    const auto& constant = script._instructions.getConstantPool().getConstant(constantIndex);
    uint32_t functionIndex = constant.getAsInteger();

    // Assume that the function will pass, as most do
    uint8_t returnCode = true;
    auto& runtime = Ego::Script::Runtime::get();
    {

        Ego::Time::ClockScope<Ego::Time::ClockPolicy::NonRecursive> scope(runtime.getClock());
        const auto& result = runtime._functionValueCodeToFunctionPointer.find(functionIndex);
        if (runtime._functionValueCodeToFunctionPointer.cend() == result)
        {
            throw idlib::runtime_error(__FILE__, __LINE__, "function not found");
        }
        returnCode = result->second(*this, aiState);
    }
    runtime.getStatistics().onFunctionInvoked(functionIndex, runtime.getClock().lst());
    return returnCode;
}

namespace
{
//--------------------------------------------------------------------------------------------
bool shouldReceiveOrder(const ITargetInfo& caller, const ITargetInfo& candidate)
{
    return candidate.getTeamRef() == caller.getTeamRef();
}

struct OrderRecipient
{
    const ITargetInfo* info = nullptr;
    IScriptable* scriptable = nullptr;
    const IInventoryHolder* inventory = nullptr;

    bool isLive() const
    {
        return info != nullptr &&
               scriptable != nullptr &&
               inventory != nullptr &&
               !inventory->isTerminated();
    }
};

template <typename Fn>
void forEachLiveRuntimeObjectRef(Fn&& fn)
{
    for (const ObjectRef& ref : Ego::Entities::activeObjectRefs())
    {
        fn(ref);
    }
}

OrderRecipient resolveOrderRecipient(ObjectRef candidateRef)
{
    OrderRecipient recipient;
    recipient.info = tryTargetInfo(candidateRef);
    recipient.scriptable = tryScriptable(candidateRef);
    recipient.inventory = tryInventoryHolder(candidateRef);
    return recipient;
}

bool tryPublishOrder(ObjectRef candidateRef,
                     const ITargetInfo& caller,
                     uint32_t value,
                     int& counter)
{
    const OrderRecipient recipient = resolveOrderRecipient(candidateRef);
    if (!recipient.isLive())
    {
        return false;
    }

    if (!shouldReceiveOrder(caller, *recipient.info))
    {
        return false;
    }

    recipient.scriptable->addAIOrder(value, counter);
    counter++;
    return true;
}

bool tryPublishSpecialOrder(ObjectRef candidateRef,
                            uint32_t value,
                            const IDSZ2& idsz,
                            int& counter)
{
    const OrderRecipient recipient = resolveOrderRecipient(candidateRef);
    if (!recipient.isLive() ||
        !recipient.info->matchesSpecialIDSZ(idsz))
    {
        return false;
    }

    recipient.scriptable->addAIOrder(value, counter);
    counter++;
    return true;
}
} // anonymous namespace

void issue_order(const ObjectRef character, uint32_t value)
{
    /// @author ZZ
    /// @details This function issues an value for help to all teammates
    int counter = 0;

    const ITargetInfo* caller = tryTargetInfo(character);
    if (caller == nullptr)
    {
        return;
    }

    forEachLiveRuntimeObjectRef([&](ObjectRef candidateRef)
    {
        tryPublishOrder(candidateRef, *caller, value, counter);
    });
}

//--------------------------------------------------------------------------------------------
void issue_special_order(uint32_t value, const IDSZ2& idsz)
{
    /// @author ZZ
    /// @details This function issues an order to all characters with the a matching special IDSZ
    int counter = 0;

    forEachLiveRuntimeObjectRef([&](ObjectRef candidateRef)
    {
        tryPublishSpecialOrder(candidateRef, value, idsz, counter);
    });
}

// NOTE: ai_state_t's lifecycle/state methods (ctor, dtor, reset, add_order, set_changed,
// set_bumplast, spawn) and their private spawn/bump helpers were relocated to
// egolib/Entities/AiState.cpp (egolib-library) so the script VM can be carved into an
// above-library archive without dragging Object's TUs up with it. Their declarations remain
// in script.h; get_wp/ensure_wp and the interpreter-coupled driver entries live in
// script_driver.c.

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
script_state_t::script_state_t()
    : x(0), y(0), turn(0), distance(0),
    argument(0), operationsum()
{}
