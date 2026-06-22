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

/// @file egolib/game/Entities/Object_ai.cpp
/// @brief Object-owned AI state bridge and script-facing accessors.

#include "egolib/Entities/Object_internal.h"

ai_state_t& Object::scriptRuntimeState() noexcept
{
    return ai;
}

const ai_state_t& Object::scriptRuntimeState() const noexcept
{
    return ai;
}

BIT_FIELD Object::getAIAlertBits() const
{
    return ai.alert;
}

void Object::setAIAlertBits(BIT_FIELD bits)
{
    ai.alert = bits;
}

void Object::addAIAlertBits(BIT_FIELD bits)
{
    ai.alert |= bits;
}

void Object::clearAIAlertBits(BIT_FIELD bits)
{
    ai.alert &= ~bits;
}

bool Object::hasAnyAIAlertBits(BIT_FIELD bits) const
{
    return HAS_SOME_BITS(ai.alert, bits);
}

int Object::getAIStateValue() const
{
    return ai.state;
}

void Object::setAIStateValue(int value)
{
    ai.state = value;
}

int Object::getAIContent() const
{
    return ai.content;
}

void Object::setAIContent(int value)
{
    ai.content = value;
}

int Object::getAIPassage() const
{
    return ai.passage;
}

void Object::setAIPassage(int value)
{
    ai.passage = value;
}

uint32_t Object::getAITimer() const
{
    return ai.timer;
}

void Object::setAITimer(uint32_t timer)
{
    ai.timer = timer;
}

int32_t Object::getAIPoofTime() const
{
    return ai.poof_time;
}

void Object::setAIPoofTime(int32_t time)
{
    ai.poof_time = time;
}

ObjectRef Object::getAIOwner() const
{
    return ai.owner;
}

void Object::setAIOwner(ObjectRef objectRef)
{
    ai.owner = objectRef;
}

ObjectRef Object::getAIChild() const
{
    return ai.child;
}

void Object::setAIChild(ObjectRef objectRef)
{
    ai.child = objectRef;
}

ObjectRef Object::getAITarget() const
{
    return ai.getTarget();
}

void Object::setAITarget(ObjectRef objectRef)
{
    ai.setTarget(objectRef);
}

ObjectRef Object::getAILastAttacker() const
{
    return ai.getLastAttacker();
}

void Object::setAILastAttacker(ObjectRef objectRef)
{
    ai.setLastAttacker(objectRef);
}

ObjectRef Object::getAIBumped() const
{
    return ai.getBumped();
}

ObjectRef Object::getAILastItemUsed() const
{
    return ai.lastitemused;
}

void Object::setAILastItemUsed(ObjectRef objectRef)
{
    ai.lastitemused = objectRef;
}

ObjectRef Object::getAILastHit() const
{
    return ai.hitlast;
}

void Object::setAILastHit(ObjectRef objectRef)
{
    ai.hitlast = objectRef;
}

DamageType Object::getAILastDamageType() const
{
    return ai.damagetypelast;
}

void Object::setAILastDamageType(DamageType damageType)
{
    ai.damagetypelast = damageType;
}

Facing Object::getAILastDirection() const
{
    return ai.directionlast;
}

void Object::setAILastDirection(Facing direction)
{
    ai.directionlast = direction;
}

float Object::getAIMaxSpeed() const
{
    return ai.maxSpeed;
}

void Object::setAIMaxSpeed(float speed)
{
    ai.maxSpeed = speed;
}

bool Object::addAIOrder(uint32_t value, uint16_t counter)
{
    return ai_state_t::add_order(ai, value, counter);
}

bool Object::markAIChanged()
{
    return ai_state_t::set_changed(ai);
}

bool Object::recordAIBump(ObjectRef objectRef)
{
    return ai_state_t::set_bumplast(ai, objectRef);
}

void Object::resetAIState()
{
    ai_state_t::reset(ai);
}

void Object::spawnAIState(uint16_t rank)
{
    ai_state_t::spawn(ai, getObjRef(), getProfileID().get(), rank);
}
