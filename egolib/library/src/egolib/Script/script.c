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
#include "egolib/Entities/_Include.hpp"


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

ai_state_t& runtimeState(Object& object)
{
    return object.ai;
}

const ai_state_t& runtimeState(const Object& object)
{
    return object.ai;
}

} // namespace Script
} // namespace Ego

ObjectProfileRef script_error_model = ObjectProfileRef::Invalid;
const char * script_error_classname = "UNKNOWN";

namespace
{
IMovementControl& movementControl(Object& object)
{
    return object;
}

const IMovementControl& movementControl(const Object& object)
{
    return object;
}

const IInventoryHolder& inventoryHolder(const Object& object)
{
    return object;
}

ObjectRef heldItemRef(const IInventoryHolder& holder, slot_t slot)
{
    return holder.getHeldObject(slot);
}

ObjectRef leftHandRiderRef(const IInventoryHolder& object)
{
    return heldItemRef(object, SLOT_LEFT);
}

bool isRuntimeObjectAlive(ObjectRef ref)
{
    const IInventoryHolder* holder = tryInventoryHolder(ref);
    return holder != nullptr && !holder->isTerminated();
}

void updateScriptErrorContext(const Object& object)
{
    script_error_classname = "UNKNOWN";
    script_error_model = object.getProfileID();
    if (script_error_model != ObjectProfileRef::Invalid)
    {
        script_error_classname = activeProfileSystem().getProfile(script_error_model)->getClassName().c_str();
    }
}

void dumpDebugScriptState(vfs_FILE* scr_file, const script_info_t& script, const ai_state_t& aiState)
{
    vfs_printf(scr_file, "\n\n--------\n%s\n", script._name.c_str());
    vfs_printf(scr_file, "%d - %s\n", REF_TO_INT(script_error_model.get()), script_error_classname);

    // who are we related to?
    vfs_printf(scr_file, "\tself   == %" PRIuZ "\n", aiState.getSelf().get());
    vfs_printf(scr_file, "\ttarget == %" PRIuZ "\n", aiState.getTarget().get());
    vfs_printf(scr_file, "\towner  == %" PRIuZ "\n", aiState.owner.get());
    vfs_printf(scr_file, "\tchild  == %" PRIuZ "\n", aiState.child.get());

    // some local storage
    vfs_printf(scr_file, "\talert     == %x\n", aiState.alert);
    vfs_printf(scr_file, "\tstate     == %d\n", aiState.state);
    vfs_printf(scr_file, "\tcontent   == %d\n", aiState.content);
    vfs_printf(scr_file, "\ttimer     == %d\n", aiState.timer);
    vfs_printf(scr_file, "\tupdate_wld == %d\n", worldUpdateCount());

    // ai memory from the last event
    vfs_printf(scr_file, "\tdirectionlast  == %" PRId32 "\n", aiState.directionlast.get_value());
    vfs_printf(scr_file, "\tbumped         == %" PRIuZ "\n", aiState.getBumped().get());
    vfs_printf(scr_file, "\tlast attacker  == %" PRIuZ "\n", aiState.getLastAttacker().get());
    vfs_printf(scr_file, "\thitlast        == %" PRIuZ "\n", aiState.hitlast.get());
    vfs_printf(scr_file, "\tdamagetypelast == %d\n", aiState.damagetypelast);
    vfs_printf(scr_file, "\tlastitemused   == %" PRIuZ "\n", aiState.lastitemused.get());
    vfs_printf(scr_file, "\told target     == %" PRIuZ "\n", aiState.getOldTarget().get());

    // message handling
    vfs_printf(scr_file, "\torder == %d\n", aiState.order_value);
    vfs_printf(scr_file, "\tcounter == %d\n", aiState.order_counter);

    // waypoints
    vfs_printf(scr_file, "\twp_tail == %d\n", aiState.wp_lst._tail);
    vfs_printf(scr_file, "\twp_head == %d\n\n", aiState.wp_lst._head);
}

struct RuntimeActorContext
{
    Object* object = nullptr;
    ai_state_t* aiState = nullptr;
    script_info_t* script = nullptr;

    Object& actor() const
    {
        return *object;
    }

    ai_state_t& state() const
    {
        return *aiState;
    }

    script_info_t& scriptInfo() const
    {
        return *script;
    }

    bool isPlayerActor() const
    {
        return object != nullptr && object->isPlayer();
    }
};

bool tryResolveRuntimeActorContext(Object& object, RuntimeActorContext& context)
{
    context.object = &object;
    context.aiState = &Ego::Script::runtimeState(object);
    context.script = &object.getProfile()->getAIScript();
    return true;
}

bool tryResolveRuntimeActorContext(ObjectRef actorRef, RuntimeActorContext& context)
{
    Object* object = tryObject(actorRef);
    return object != nullptr && tryResolveRuntimeActorContext(*object, context);
}

bool shouldSkipScriptRun(const RuntimeActorContext& context)
{
    const ai_state_t& aiState = context.state();
    return context.actor().isTerminated() ||
           (aiState.poof_time >= 0 && aiState.poof_time <= static_cast<int32_t>(worldUpdateCount()));
}

void publishChangedAlertIfNeeded(ai_state_t& aiState)
{
    if (!aiState.changed)
    {
        return;
    }

    SET_BIT(aiState.alert, ALERTIF_CHANGED);
    aiState.changed = false;
}

void resetNonPlayerInputCommands(RuntimeActorContext& context)
{
    if (!context.isPlayerActor())
    {
        context.actor().resetInputCommands();
    }
}

void resetInvisibleTargetToSelf(RuntimeActorContext& context)
{
    ai_state_t& aiState = context.state();
    if (aiState.getTarget() == aiState.getSelf())
    {
        return;
    }

    if (!context.actor().canSeeObject(aiState.getTarget()))
    {
        aiState.setTarget(aiState.getSelf());
    }
}

void publishWaypointVelocity(RuntimeActorContext& context)
{
    const ai_state_t& aiState = context.state();
    Object& object = context.actor();
    movementControl(object).setDesiredVelocity(Ego::Vector2f(
        (aiState.wp[kX] - object.getPosX()) / Info<float>::Grid::Size(),
        (aiState.wp[kY] - object.getPosY()) / Info<float>::Grid::Size()));
}

void applyNonPlayerMovementLatchUpdate(RuntimeActorContext& context)
{
    Object& object = context.actor();
    ai_state_t& aiState = context.state();
    ai_state_t::ensure_wp(aiState);

    if (const Object* rider = tryObject(leftHandRiderRef(object));
        object.isMount() && rider != nullptr)
    {
        // Mount (rider is held in left grip)
        movementControl(object).setDesiredVelocity(movementControl(*rider).getDesiredVelocity());
    }
    else if (aiState.wp_valid)
    {
        // Normal AI
        publishWaypointVelocity(context);
    }
}

bool isAtCurrentWaypoint(const RuntimeActorContext& context)
{
    const Object& object = context.actor();
    const ai_state_t& aiState = context.state();
    return aiState.wp_valid &&
           (std::abs(object.getPosX() - aiState.wp[kX]) < WAYTHRESH) &&
           (std::abs(object.getPosY() - aiState.wp[kY]) < WAYTHRESH);
}

void publishWaypointArrivalAlert(ai_state_t& aiState)
{
    SET_BIT(aiState.alert, ALERTIF_ATWAYPOINT);
}

void advanceWaypointPathAfterArrival(RuntimeActorContext& context)
{
    const Object& object = context.actor();
    ai_state_t& aiState = context.state();
    if (waypoint_list_t::finished(aiState.wp_lst))
    {
        // we are now at the last waypoint
        // if the object can be alerted to last waypoint, do it
        // this test needs to be done because the ALERTIF_ATLASTWAYPOINT
        // doubles for "at last waypoint" and "not put away"
        if (!object.getProfile()->isEquipment())
        {
            SET_BIT(aiState.alert, ALERTIF_ATLASTWAYPOINT);
        }

        // !!!!restart the waypoint list, do not clear them!!!!
        waypoint_list_t::reset(aiState.wp_lst);

        // load the top waypoint
        ai_state_t::get_wp(aiState);
        return;
    }

    if (waypoint_list_t::advance(aiState.wp_lst))
    {
        // load the top waypoint
        ai_state_t::get_wp(aiState);
    }
}

void pollWaypointAlerts(RuntimeActorContext& context)
{
    ai_state_t& aiState = context.state();
    if (waypoint_list_t::empty(aiState.wp_lst))
    {
        return;
    }

    // let's let mounts get alert updates...
    // imagine a mount, like a racecar, that needs to make sure that it follows X
    // waypoints around a track or something

    // mounts do not get alerts
    // if ( objectHandler().exists(pchr->attachedto) ) return;

    // is the current waypoint is not valid, try to load up the top waypoint
    ai_state_t::ensure_wp(aiState);

    if (!isAtCurrentWaypoint(context))
    {
        return;
    }

    publishWaypointArrivalAlert(aiState);
    advanceWaypointPathAfterArrival(context);
}

void runCharacterScript(RuntimeActorContext& context)
{
    if (shouldSkipScriptRun(context))
    {
        return;
    }

    Object& actor = context.actor();
    ai_state_t& aiState = context.state();
    script_info_t& script = context.scriptInfo();

    publishChangedAlertIfNeeded(aiState);

    Ego::Time::ClockScope<Ego::Time::ClockPolicy::NonRecursive> scope(*aiState._clock);

    // debug a certain script
    // debug_scripts = ( 385 == pself->index && 76 == pchr->profile_ref );

    // target_old is set to the target every time the script is run
    aiState.setOldTarget(aiState.getTarget());

    // Make life easier
    updateScriptErrorContext(actor);

    if (debug_scripts && debug_script_file)
    {
        dumpDebugScriptState(debug_script_file, script, aiState);
    }

    resetNonPlayerInputCommands(context);
    resetInvisibleTargetToSelf(context);

    script_state_t my_state;

    aiState.terminate = false;
    script.indent = 0;

    script.set_pos(0);
    while (!aiState.terminate && script.get_pos() < script._instructions.getNumberOfInstructions())
    {
        script.indent_last = script.indent;
        script.indent = script._instructions[script.get_pos()].getDataBits();

        if (script._instructions[script.get_pos()].isInv())
        {
            if (!my_state.run_function_call(aiState, script))
            {
                break;
            }
        }
        else
        {
            if (!my_state.run_operation(aiState, script))
            {
                break;
            }
        }
    }

    if (!context.isPlayerActor())
    {
        applyNonPlayerMovementLatchUpdate(context);
    }

    RESET_BIT_FIELD(aiState.alert);
}
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
void scripting_system_begin()
{
    if (!Ego::Script::Runtime::is_initialized())
    {
        Ego::Script::Runtime::initialize();
    }
}

void scripting_system_end()
{
    if (Ego::Script::Runtime::is_initialized())
    {
        Ego::Script::Runtime::get().getStatistics().append("/debug/script_function_timing.txt");
        Ego::Script::Runtime::uninitialize();
    }
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
void scr_run_chr_script(Object *pchr)
{
    // Make sure that this module is initialized.
    scripting_system_begin();

    if (pchr == nullptr)
    {
        return;
    }
    RuntimeActorContext context;
    if (!tryResolveRuntimeActorContext(*pchr, context))
    {
        return;
    }

    runCharacterScript(context);
}
void scr_run_chr_script(const ObjectRef character)
{
    /// @author ZZ
    /// @details This function lets one character do AI stuff

    // Make sure that this module is initialized.
    scripting_system_begin();

    RuntimeActorContext context;
    if (!tryResolveRuntimeActorContext(character, context))
    {
        return;
    }
    runCharacterScript(context);
}

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

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
bool ai_state_t::get_wp(ai_state_t& self)
{
    // try to load up the top waypoint

    if (!isRuntimeObjectAlive(self.getSelf())) return false;

    self.wp_valid = waypoint_list_t::peek(self.wp_lst, self.wp);

    return true;
}

//--------------------------------------------------------------------------------------------
bool ai_state_t::ensure_wp(ai_state_t& self)
{
    // is the current waypoint is not valid, try to load up the top waypoint

    if (!isRuntimeObjectAlive(self.getSelf()))
    {
        return false;
    }
    if (self.wp_valid)
    {
        return true;
    }
    return ai_state_t::get_wp(self);
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
void set_alerts(const ObjectRef character)
{
    /// @author ZZ
    /// @details This function polls some alert conditions

    RuntimeActorContext context;
    if (!tryResolveRuntimeActorContext(character, context))
    {
        return;
    }
    pollWaypointAlerts(context);
}

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
    ObjectHandler* handler = GameSessionContext::get().tryObjectHandler();
    if (handler == nullptr)
    {
        return;
    }

    for (const ObjectRef ref : handler->objectRefIterator())
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
// in script.h; get_wp/ensure_wp and the interpreter-coupled set_alerts stay here.

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
script_state_t::script_state_t()
    : x(0), y(0), turn(0), distance(0),
    argument(0), operationsum()
{}
