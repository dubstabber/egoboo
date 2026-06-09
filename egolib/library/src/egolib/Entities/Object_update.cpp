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

/// @file egolib/game/Entities/Object_update.cpp
/// @brief Per-frame update Object implementation.

#include "egolib/Entities/Object_internal.h"
#include "egolib/AI/LineOfSight.hpp"  // line_of_sight_info_t
#include "egolib/game/Core/EngineContext.hpp"        // EngineContext::particleHandler / tryActivePlayingState
#include "egolib/game/GameStates/PlayingState.hpp"   // PlayingState + getMiniMap (minimap reveal)
#include "egolib/game/GUI/MiniMap.hpp"               // MiniMap::setVisible / setShowPlayerPosition
#include "egolib/game/Physics/PhysicalConstants.hpp" // Ego::Physics::CHR_INFINITE_WEIGHT / CHR_MAX_WEIGHT

namespace
{
/// @brief Minimap reveal helper. Sole consumer of EngineContext::tryActivePlayingState();
///        kept TU-local so the 7-TU-propagating Object_internal.h no longer pulls the
///        minimap reveal chain (EngineContext/GameEngine/PlayingState/MiniMap) into every
///        Object_*.cpp.
std::shared_ptr<PlayingState> tryActivePlayingState()
{
    return EngineContext::get().tryActivePlayingState();
}
} // namespace

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

void Object::update()
{
    //Update active enchantments on this Object
    if(!_activeEnchants.empty()) {
        _activeEnchants.remove_if([this](const std::shared_ptr<Ego::Enchantment> &enchant)
            {
                //Update enchantment
                enchant->update();

                //Remove all terminated enchants
                if(enchant->isTerminated()) {
                    enchant->playEndSound();

                    if(enchant->getProfile()->killtargetonend) {
                        this->kill(enchant->getOwner(), true);
                    }

                    return true;
                }

                return false;
            });
    }

    // the following functions should not be done the first time through the update loop
    if (0 == worldUpdateCount()) return;

    //Don't do items that are inside an inventory
    if (isInsideInventory()) {
        return;
    }

    // do the character interaction with water
    if (!isHidden() && isSubmerged() && !isScenery())
    {
        // do splash when entering water the first time
        if (!inwater)
        {
            // Splash
            EngineContext::get().particleHandler().spawnGlobalParticle({getPosX(), getPosY(), activeModule().getWater().get_level() + 10}, ATK_FRONT, LocalParticleProfileRef(PIP_SPLASH), 0);

            if ( activeModule().getWater()._is_water )
            {
                SET_BIT(ai.alert, ALERTIF_INWATER);
            }
        }

        //Submerged in water (fully or partially)
        else
        {
            // Ripples
            if(isAlive())
            {
                if ( !isBeingHeld() && getProfile()->causesRipples()
                    && getPosZ() + chr_min_cv._maxs[OCT_Z] + RIPPLETOLERANCE > activeModule().getWater().get_level()
                    && getPosZ() + chr_min_cv._mins[OCT_Z] < activeModule().getWater().get_level())
                {
                    // suppress ripples if we are far below the surface
                    int ripple_suppression = 4 * (activeModule().getWater().get_level() - (getPosZ() + chr_min_cv._maxs[OCT_Z]));
                    ripple_suppression = ripple_suppression / RIPPLETOLERANCE;
                    ripple_suppression = Ego::Math::constrain(ripple_suppression, 0, 4);

                    // make more ripples if we are moving
                    ripple_suppression -= (( int )getVelocity()[kX] != 0 ) | (( int )getVelocity()[kY] != 0 );

                    int ripand;
                    if ( ripple_suppression > 0 )
                    {
                        ripand = ~(RIPPLEAND << ripple_suppression);
                    }
                    else
                    {
                        ripand = RIPPLEAND >> ( -ripple_suppression );
                    }

                    if ( 0 == ( (worldUpdateCount() + getObjRef().get()) & ripand ))
                    {
                        EngineContext::get().particleHandler().spawnGlobalParticle({getPosX(), getPosY(), activeModule().getWater().get_level()}, ATK_FRONT, LocalParticleProfileRef(PIP_RIPPLE), 0);
                    }
                }
            }

            if (activeModule().getWater()._is_water && HAS_NO_BITS(worldUpdateCount(), 7))
            {
                jumpready = true;
                jumpnumber = 1; //Limit to 1 jump while in water
            }
        }

        inwater = true;
    }
    else
    {
        inwater = false;
    }

    //---- Do timers and such

    // reduce attack cooldowns
    if ( reload_timer > 0 ) reload_timer--;

    // decrement the dismount timer
    if ( dismount_timer > 0 ) dismount_timer--;

    if ( 0 == dismount_timer ) {
        dismount_object = ObjectRef::Invalid;
    }

    // Down jump timer
    if(jump_timer > 0) {
        if (isBeingHeld() || isTouchingGround() || jumpnumber > 0) {
            jump_timer--;
        }
    }
    // Down that ol' damage timer
    if ( damage_timer > 0 ) damage_timer--;

    // Do "Be careful!" delay
    if ( careful_timer > 0 ) careful_timer--;

    //Reduce stealth timeout
    if(_stealthTimer > 0) _stealthTimer--;

    // Texture movement
    inst.setUOffset(inst.getUOffset() + getProfile()->getTextureMovementRateX());
    inst.setVOffset(inst.getVOffset() + getProfile()->getTextureMovementRateY());

    // Texture tint
    inst.setColorShift(colorshift_t(Ego::Math::constrain<int>(1 + getAttribute(Ego::Attribute::RED_SHIFT), 0, 6),
                                    Ego::Math::constrain<int>(1 + getAttribute(Ego::Attribute::GREEN_SHIFT), 0, 6),
                                    Ego::Math::constrain<int>(1 + getAttribute(Ego::Attribute::BLUE_SHIFT), 0, 6)));

    // do the mana and life regeneration for "living" characters
    if (isAlive()) {
        _currentMana += getAttribute(Ego::Attribute::MANA_REGEN) / ONESECOND;
        _currentMana = Ego::Math::constrain(_currentMana, 0.0f, getAttribute(Ego::Attribute::MAX_MANA));

        _currentLife += getAttribute(Ego::Attribute::LIFE_REGEN) / ONESECOND;
        _currentLife = Ego::Math::constrain(_currentLife, 0.01f, getAttribute(Ego::Attribute::MAX_LIFE));
    }

    // Do stats once every second
    if ( characterStatClock() >= ONESECOND )
    {
        // check for a level up
        checkLevelUp();

        // countdown confuse effects
        if (grog_timer > 0) {
           grog_timer--;
        }

        if (daze_timer > 0) {
           daze_timer--;
        }

        // update some special skills (players and NPC's)
        if(getShowStatus())
        {
            //Cartography perk reveals the minimap
            if(hasPerk(Ego::Perks::CARTOGRAPHY)) {
                if (std::shared_ptr<PlayingState> playingState = tryActivePlayingState())
                {
                    playingState->getMiniMap()->setVisible(true);
                }
            }

            //Navigation reveals the players position on the minimap
            if(hasPerk(Ego::Perks::NAVIGATION)) {
                if (std::shared_ptr<PlayingState> playingState = tryActivePlayingState())
                {
                    playingState->getMiniMap()->setShowPlayerPosition(true);
                }
            }

            //Danger Sense reveals enemies on the minimap
            if(hasPerk(Ego::Perks::DANGER_SENSE)) {
                gameSession().publishEnemySense(EnemySenseState(this->team, IDSZ2::None));     //Reveal all
            }

            //Danger Sense reveals enemies on the minimap
            else if(hasPerk(Ego::Perks::SENSE_UNDEAD)) {
                gameSession().publishEnemySense(EnemySenseState(this->team, IDSZ2('U','N','D','E')));     //Reveal only undead
            }
        }

        //Give Rally bonus to friends within 6 tiles
        if(hasPerk(Ego::Perks::RALLY)) {
            std::vector<ObjectRef> nearbyObjectRefs;
            activeModule().getObjectHandler().findObjectRefs(getPosX(), getPosY(), WIDE, nearbyObjectRefs, false);
            for (const ObjectRef& objectRef : nearbyObjectRefs)
            {
                Object* object = activeModule().getObjectHandler().get(objectRef);
                if (object == nullptr)
                {
                    continue;
                }

                //Only valid objects that are on our team
                if(object->isTerminated() || object->getTeam() != getTeam()) continue;

                //Don't give bonus to ourselves!
                if(object == this) continue;

                object->_reallyDuration = worldUpdateCount() + ONESECOND*3;    //Apply bonus for 3 seconds
            }
        }
    }

    //Try to detect any hidden objects every so often (unless we are scenery object)
    if(!isScenery() && isAlive() && !isBeingHeld() && inst.getCurrentAnimation() != ACTION_MK) {  //ACTION_MK = sleeping
        if(worldUpdateCount() > _observationTimer)
        {
            _observationTimer = worldUpdateCount() + ONESECOND;

            //Setup line of sight data
            line_of_sight_info_t lineOfSightInfo;
            lineOfSightInfo.x0         = getPosX();
            lineOfSightInfo.y0         = getPosY();
            lineOfSightInfo.z0         = getPosZ() + std::max(1.0f, bump.height);
            lineOfSightInfo.stopped_by = stoppedby;

            //Check for nearby enemies
            std::vector<ObjectRef> nearbyObjectRefs;
            activeModule().getObjectHandler().findObjectRefs(getPosX(), getPosY(), WIDE, nearbyObjectRefs, false);
            for (const ObjectRef& objectRef : nearbyObjectRefs) {
                Object* target = activeModule().getObjectHandler().get(objectRef);
                if (target == nullptr) {
                    continue;
                }

                //Valid objects only
                if(target->isTerminated() || target->isHidden()) continue;

                //Only look for stealthed objects
                if(!target->isStealthed()) continue;

                //Are they a enemy of us?
                if(!target->getTeam().hatesTeam(getTeam())) {
                    continue;
                }

                //Can we see them?
                lineOfSightInfo.x1 = target->getPosX();
                lineOfSightInfo.y1 = target->getPosY();
                lineOfSightInfo.z1 = target->getPosZ() + std::max(1.0f, target->bump.height);
                if (line_of_sight_info_t::blocked(lineOfSightInfo, activeModule().getMeshPointer())) {
                    continue;
                }

                //Sense Invisible = automatic detection
                if(target->canSeeInvisible()) {
                    target->deactivateStealth();
                    target->_stealthTimer = ONESECOND * 6; //6 second timeout
                    break;
                }

                //Check for detection chance, Base chance 20%
                int chance = 20;

                //+0.5% per Intellect
                chance += getAttribute(Ego::Attribute::INTELLECT)*0.5f;

                //-0.5% per target Agility
                chance -= target->getAttribute(Ego::Attribute::AGILITY)*0.5f;

                //-5% per tile distance
                chance -= 5 * (idlib::euclidean_norm(getPosition()-target->getPosition()) / Info<float>::Grid::Size());

                //Perceptive Perk doubles chance
                if(target->hasPerk(Ego::Perks::PERCEPTIVE)) {
                    chance *= 2;
                }

                //If they are not looking towards us, then halve detection chance
                if(!target->isFacingLocation(getPosX(), getPosY())) {
                    chance /= 2;
                }

                //Were they detected by us?
                if(Random::getPercent() <= chance) {
                    target->deactivateStealth();
                    target->_stealthTimer = ONESECOND * 6; //6 second timeout
                    break;
                }
            }
        }
    }

    //Generate movement and attacks from input latches
    updateLatchButtons();

    //Finally update model resizing effects
    updateResize();
}

void Object::updateResize()
{
    if (fat_goto_time < 0) {
        return;
    }

    if (fat_goto != fat)
    {
        int bump_increase = ( fat_goto - fat ) * 0.10f * bump.size;

        // Make sure it won't get caught in a wall
        bool willgetcaught = false;
        if ( fat_goto > fat )
        {
            bump.size += bump_increase;

            if ( EMPTY_BIT_FIELD != Collidable::test_wall() )
            {
                willgetcaught = true;
            }

            bump.size -= bump_increase;
        }

        // If it is getting caught, simply halt growth until later
        if ( !willgetcaught )
        {
            // Figure out how big it is
            fat_goto_time--;

            float newsize = fat_goto;
            if ( fat_goto_time > 0 )
            {
                newsize = ( fat * 0.90f ) + ( newsize * 0.10f );
            }

            // Make it that big...
            setFat(newsize);

            if ( CAP_INFINITE_WEIGHT == getProfile()->getWeight() )
            {
                phys.weight = Ego::Physics::CHR_INFINITE_WEIGHT;
            }
            else
            {
                phys.weight = std::min<uint32_t>(getProfile()->getWeight() * fat * fat * fat, Ego::Physics::CHR_MAX_WEIGHT);
            }
        }
    }
}
