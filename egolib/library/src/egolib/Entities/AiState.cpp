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

/// @file  egolib/Entities/AiState.cpp
/// @brief Lifecycle and state management for ai_state_t, decoupled from the script VM.
/// @details ai_state_t is embedded by-value in every Object, but its method bodies are pure
///   AI-state data/manipulation with no dependency on the bytecode interpreter (no run_function,
///   no script_state_t, no Ego::Script::Runtime). They are kept here, beside Object in
///   egolib-library, so the script VM (script.c et al.) can later be carved into an
///   above-library archive without dragging Object's TUs up with it. The interpreter-coupled
///   driver entries (scr_run_chr_script / set_alerts / scripting_system_end) stay in script.c.

#include "egolib/Script/script.h"
#include "egolib/Entities/IObjectWorld.hpp"
#include "egolib/Entities/IPhysical.hpp"
#include "egolib/Entities/IProfiled.hpp"
#include "egolib/Entities/ObjectRoleAccess.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/Core/ISessionState.hpp"

namespace
{
// File-local helpers relocated from script.c together with the ai_state_t methods they serve.
// Each is referenced only by the methods below; all bottom out at game-core (library) seams.

bool isRuntimeObjectRefValid(ObjectRef ref)
{
    return Ego::Entities::activeObjectExists(ref);
}

uint32_t worldUpdateCount()
{
    return activeSessionState().worldUpdateCount();
}

void publishSpawnIdentity(ai_state_t& self, ObjectRef index)
{
    self.setSelf(index);
    self.setTarget(index);
    self.setOldTarget(index);
    self.setBumped(index);
    self.alert = ALERTIF_SPAWNED;
    self.passage = 0;
    self.owner = index;
    self.child = index;
    self.hitlast = index;
}

void publishSpawnOverrides(ai_state_t& self, const IProfiled& object)
{
    self.state = object.getProfile()->getStateOverride();
    self.content = object.getProfile()->getContentOverride();
}

void publishSpawnWaypoint(ai_state_t& self, const IPhysical& object)
{
    waypoint_list_t::push(self.wp_lst, object.getSpawnPosition().x(), object.getSpawnPosition().y());
}

void publishSpawnOrderDefaults(ai_state_t& self, uint16_t rank)
{
    self.order_counter = rank;
    self.order_value = 0;
}

bool shouldPublishBumpAlert(const ai_state_t& self, ObjectRef bumpedRef)
{
    return self.getBumped() != bumpedRef ||
           worldUpdateCount() > self.bumplast_time + GameEngine::GAME_TARGET_UPS / 5;
}

} // anonymous namespace

//--------------------------------------------------------------------------------------------

ai_state_t::ai_state_t()
    : AI::State<ObjectRef>()
{
    _clock = std::make_shared<Ego::Time::Clock<Ego::Time::ClockPolicy::NonRecursive>>("", 8);
    poof_time = -1;
    changed = false;
    terminate = false;

    // who are we related to?
    owner = ObjectRef::Invalid;
    child = ObjectRef::Invalid;

    // some local storage
    alert = 0;
    state = 0;
    content = 0;
    passage = 0;
    timer = 0;
    for (size_t i = 0; i < STOR_COUNT; ++i)
    {
        x[i] = 0;
        y[i] = 0;
    }

    // ai memory from the last event
    bumplast_time = 0;

    hitlast = ObjectRef::Invalid;
    directionlast = Facing(0);
    damagetypelast = DamageType::DAMAGE_DIRECT;
    lastitemused = ObjectRef::Invalid;

    // message handling
    order_value = 0;
    order_counter = 0;

    // waypoints
    wp_valid = false;
    wp_lst._head = wp_lst._tail = 0;
    astar_timer = 0;
}

ai_state_t::~ai_state_t()
{
    _clock = nullptr;
}

void ai_state_t::reset(ai_state_t& self)
{
    self._clock->reinit();

    self.poof_time = -1;
    self.changed = false;
    self.terminate = false;

    // who are we related to?
    self.setSelf(ObjectRef::Invalid);
    self.setTarget(ObjectRef::Invalid);
    self.setOldTarget(ObjectRef::Invalid);
    self.setBumped(ObjectRef::Invalid);
    self.setLastAttacker(ObjectRef::Invalid);

    self.owner = ObjectRef::Invalid;
    self.child = ObjectRef::Invalid;

    // some local storage
    self.alert = 0;         ///< Alerts for AI script
    self.state = 0;
    self.content = 0;
    self.passage = 0;
    self.timer = 0;
    for (size_t i = 0; i < STOR_COUNT; ++i)
    {
        self.x[i] = 0;
        self.y[i] = 0;
    }
    self.maxSpeed = 1.0f;

    // ai memory from the last event

    self.bumplast_time = 0;


    self.hitlast = ObjectRef::Invalid;
    self.directionlast = Facing(0);
    self.damagetypelast = DamageType::DAMAGE_DIRECT;
    self.lastitemused = ObjectRef::Invalid;

    // message handling
    self.order_value = 0;
    self.order_counter = 0;

    // waypoints
    self.wp_valid = false;
    self.wp_lst._head = self.wp_lst._tail = 0;
    self.astar_timer = 0;
}

bool ai_state_t::add_order(ai_state_t& self, uint32_t value, uint16_t counter)
{
    // this function is only truely valid if there is no other order
    bool retval = HAS_NO_BITS(self.alert, ALERTIF_ORDERED);

    SET_BIT(self.alert, ALERTIF_ORDERED);
    self.order_value = value;
    self.order_counter = counter;

    return retval;
}

bool ai_state_t::set_changed(ai_state_t& self)
{
    /// @author BB
    /// @details do something tricky here

    bool retval = false;

    if (HAS_NO_BITS(self.alert, ALERTIF_CHANGED))
    {
        SET_BIT(self.alert, ALERTIF_CHANGED);
        retval = true;
    }

    if (!self.changed)
    {
        self.changed = true;
        retval = true;
    }

    return retval;
}

bool ai_state_t::set_bumplast(ai_state_t& self, const ObjectRef ichr)
{
    /// @author BB
    /// @details bumping into a chest can initiate whole loads of update messages.
    ///     Try to throttle the rate that new "bump" messages can be passed to the ai

    if (!isRuntimeObjectRefValid(ichr))
    {
        return false;
    }

    // 5 bumps per second?
    if (shouldPublishBumpAlert(self, ichr))
    {
        self.bumplast_time = worldUpdateCount();
        SET_BIT(self.alert, ALERTIF_BUMPED);
    }
    self.setBumped(ichr);

    return true;
}

void ai_state_t::spawn(ai_state_t& self, const ObjectRef index, const PRO_REF iobj, uint16_t rank)
{
    const IProfiled* profiled = Ego::Entities::tryActiveProfiled(index);
    const IPhysical* physical = Ego::Entities::tryActivePhysical(index);
    ai_state_t::reset(self);

    if (profiled == nullptr || profiled->getProfile() == nullptr || physical == nullptr)
    {
        return;
    }

    publishSpawnIdentity(self, index);
    publishSpawnOverrides(self, *profiled);
    publishSpawnWaypoint(self, *physical);
    self.maxSpeed = 1.0f;
    publishSpawnOrderDefaults(self, rank);
}
