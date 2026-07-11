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

/// @file  egolib/Script/script_driver.c
/// @brief VM execution driver for EgoScript character scripts and alert polling.
/// @details Keeps the per-character script loop and alert waypoint polling separate from
///          Runtime construction and opcode/operand evaluation in the sibling script.c and
///          script_operand.c TUs.

#include "egolib/Script/script.h"
#include "egolib/Script/script_internal.h"

#include "egolib/Profiles/IProfileSystem.hpp"
#include "egolib/Profiles/_Include.hpp" // ObjectProfile (complete type)
#include "egolib/Script/IRuntimeStatistics.hpp"
#include "egolib/game/script_compile.h"
#include "egolib/game/script_functions_internal.h"

namespace
{
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

void updateScriptErrorContext(const IProfiled& profiled)
{
    script_error_classname = "UNKNOWN";
    script_error_model = ObjectProfileRef(profiled.getProfileRef());
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
    ObjectRef ref = ObjectRef::Invalid;
    ai_state_t* aiState = nullptr;
    script_info_t* script = nullptr;
    const IProfiled* profiled = nullptr;
    IInventoryHolder* inventory = nullptr;
    IMovementControl* movement = nullptr;
    const IPhysical* physical = nullptr;
    const ITargetInfo* targetInfo = nullptr;
    const IVisibilityObserver* visibility = nullptr;

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
        return targetInfo != nullptr && targetInfo->isPlayer();
    }

    bool isResolved() const
    {
        return ref != ObjectRef::Invalid && aiState != nullptr && script != nullptr &&
               profiled != nullptr && inventory != nullptr && movement != nullptr &&
               physical != nullptr && targetInfo != nullptr && visibility != nullptr;
    }
};

bool tryResolveRuntimeActorContext(ObjectRef actorRef, RuntimeActorContext& context)
{
    IScriptRuntimeState* runtimeStateRole = tryScriptRuntimeState(actorRef);
    context.ref = actorRef;
    context.profiled = tryProfiled(actorRef);
    context.inventory = tryInventoryHolder(actorRef);
    context.movement = tryMovementControl(actorRef);
    context.physical = tryPhysical(actorRef);
    context.targetInfo = tryTargetInfo(actorRef);
    context.visibility = tryVisibilityObserver(actorRef);
    if (runtimeStateRole == nullptr || context.profiled == nullptr ||
        context.profiled->getProfile() == nullptr)
    {
        return false;
    }

    context.aiState = &Ego::Script::runtimeState(*runtimeStateRole);
    context.script = &context.profiled->getProfile()->getAIScript();
    return context.isResolved();
}

bool shouldSkipScriptRun(const RuntimeActorContext& context)
{
    const ai_state_t& aiState = context.state();
    return context.inventory->isTerminated() ||
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
        context.movement->resetInputCommands();
    }
}

void resetInvisibleTargetToSelf(RuntimeActorContext& context)
{
    ai_state_t& aiState = context.state();
    if (aiState.getTarget() == aiState.getSelf())
    {
        return;
    }

    if (!context.visibility->canSeeObject(aiState.getTarget()))
    {
        aiState.setTarget(aiState.getSelf());
    }
}

void publishWaypointVelocity(RuntimeActorContext& context)
{
    const ai_state_t& aiState = context.state();
    context.movement->setDesiredVelocity(Ego::Vector2f(
        (aiState.wp[kX] - context.physical->getPosX()) / Info<float>::Grid::Size(),
        (aiState.wp[kY] - context.physical->getPosY()) / Info<float>::Grid::Size()));
}

void applyNonPlayerMovementLatchUpdate(RuntimeActorContext& context)
{
    ai_state_t& aiState = context.state();
    ai_state_t::ensure_wp(aiState);

    IMovementControl* riderMovement = tryMovementControl(leftHandRiderRef(*context.inventory));
    if (context.targetInfo->isMount() && riderMovement != nullptr)
    {
        // Mount (rider is held in left grip)
        context.movement->setDesiredVelocity(riderMovement->getDesiredVelocity());
    }
    else if (aiState.wp_valid)
    {
        // Normal AI
        publishWaypointVelocity(context);
    }
}

bool isAtCurrentWaypoint(const RuntimeActorContext& context)
{
    const ai_state_t& aiState = context.state();
    return aiState.wp_valid &&
           (std::abs(context.physical->getPosX() - aiState.wp[kX]) < WAYTHRESH) &&
           (std::abs(context.physical->getPosY() - aiState.wp[kY]) < WAYTHRESH);
}

void publishWaypointArrivalAlert(ai_state_t& aiState)
{
    SET_BIT(aiState.alert, ALERTIF_ATWAYPOINT);
}

void advanceWaypointPathAfterArrival(RuntimeActorContext& context)
{
    ai_state_t& aiState = context.state();
    if (waypoint_list_t::finished(aiState.wp_lst))
    {
        // we are now at the last waypoint
        // if the object can be alerted to last waypoint, do it
        // this test needs to be done because the ALERTIF_ATLASTWAYPOINT
        // doubles for "at last waypoint" and "not put away"
        if (!context.profiled->getProfile()->isEquipment())
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
    // Attached actors are handled by their holder and return earlier.

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

    ai_state_t& aiState = context.state();
    script_info_t& script = context.scriptInfo();

    publishChangedAlertIfNeeded(aiState);

    Ego::Time::ClockScope<Ego::Time::ClockPolicy::NonRecursive> scope(*aiState._clock);

    // debug a certain script
    // debug_scripts = ( 385 == pself->index && 76 == pchr->profile_ref );

    // target_old is set to the target every time the script is run
    aiState.setOldTarget(aiState.getTarget());

    // Make life easier
    updateScriptErrorContext(*context.profiled);

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
} // anonymous namespace

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
