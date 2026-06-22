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

/// @file egolib/game/Entities/Object_combat.cpp
/// @brief Combat damage-application and death pipeline (Object implementation).

#include "egolib/Entities/Object_internal.h"
#include "egolib/game/CharacterParticleOps.h"                    // DisplayMsg_printf
#include "egolib/Script/IScriptSystem.hpp"                       // activeScriptSystem() driver seam
#include "egolib/Graphics/IBillboardSystem.hpp"  // Ego::Graphics::activeBillboardSystem
#include "egolib/egoboo_setup.h"                 // activeConfig
#include "egolib/game/Graphics/Billboard.hpp"  // Ego::Graphics::Billboard::Flags
#include "egolib/game/Logic/Player.hpp"        // Ego::Player (complete type)

namespace
{
egoboo_config_t& config()
{
    return Ego::activeConfig();
}

IAudioSystem& audioSystem()
{
    return activeAudioSystem();
}

ObjectHandler& objectHandler()
{
    return GameSessionContext::get().activeModule().getObjectHandler();
}

const std::shared_ptr<Object>& heldItem(const IInventoryHolder& object, slot_t slot)
{
    return objectHandler()[object.getHeldObject(slot)];
}

ObjectRef resolveHolderOrMountAttribution(const Object& actor)
{
    ObjectRef actualActor = actor.getObjRef();

    if (actor.isBeingHeld() && !objectHandler().get(actor.getHolderRef())->isMount())
    {
        actualActor = actor.getHolderRef();
    }
    else if (actor.isMount() && objectHandler().exists(actor.getHeldObject(SLOT_LEFT)))
    {
        actualActor = actor.getHeldObject(SLOT_LEFT);
    }

    return actualActor;
}

bool resolveLastAttackerAttribution(const Object& target,
                                    const std::shared_ptr<Object>& attacker,
                                    ObjectRef& actualAttacker)
{
    actualAttacker = ObjectRef::Invalid;
    if (!attacker)
    {
        return true;
    }

    if (attacker.get() == &target)
    {
        return false;
    }

    if (attacker->getTeam() == Team::TEAM_NULL)
    {
        return false;
    }

    if (attacker->getHolderRef() == target.getObjRef())
    {
        return false;
    }

    actualAttacker = resolveHolderOrMountAttribution(*attacker);
    return true;
}

std::shared_ptr<Object> resolveKillCreditRecipient(const std::shared_ptr<Object>& originalKiller)
{
    std::shared_ptr<Object> actualKiller = originalKiller;
    if (!actualKiller)
    {
        return actualKiller;
    }

    if (actualKiller->isBeingHeld() && !objectHandler().get(actualKiller->getHolderRef())->isMount())
    {
        actualKiller = objectHandler()[actualKiller->getHolderRef()];
    }
    else if (actualKiller->isMount() && heldItem(*actualKiller, SLOT_LEFT))
    {
        actualKiller = heldItem(*actualKiller, SLOT_LEFT);
    }

    return actualKiller;
}

void publishKillerTarget(IScriptable& killedScript, ObjectRef killedRef, const Object& killer)
{
    killedScript.setAITarget(killer.getObjRef());
    if (killer.getTeam() == Team::TEAM_DAMAGE || killer.getTeam() == Team::TEAM_NULL)
    {
        killedScript.setAITarget(killedRef);
    }
}

void awardDirectKillExperience(Object& killed,
                               Object& actualKiller,
                               uint16_t experience,
                               bool firstDeath)
{
    if (!actualKiller.getTeam().hatesTeam(killed.getTeam()))
    {
        return;
    }

    if (actualKiller.getProfile()->getIDSZ(IDSZ_HATE) == killed.getProfile()->getIDSZ(IDSZ_PARENT) ||
        actualKiller.getProfile()->getIDSZ(IDSZ_HATE) == killed.getProfile()->getIDSZ(IDSZ_TYPE))
    {
        actualKiller.giveExperience(experience, XP_KILLHATED, false);
    }
    else
    {
        actualKiller.giveExperience(experience, XP_KILLENEMY, false);
    }

    if (actualKiller.hasPerk(Ego::Perks::MERCENARY) && firstDeath)
    {
        actualKiller.giveMoney(1);
        audioSystem().playSound(killed.getPosition(), audioSystem().getGlobalSound(GSND_COINGET));
    }

    if (actualKiller.hasPerk(Ego::Perks::CRUSADER) && killed.getProfile()->getIDSZ(IDSZ_PARENT).equals('U','N','D','E'))
    {
        actualKiller.costMana(-1, actualKiller.getObjRef());
        Ego::Graphics::activeBillboardSystem().makeBillboard(actualKiller.getObjRef(), "Crusader", Ego::Colour4f::white(), Ego::Colour4f::yellow(), 3, Ego::Graphics::Billboard::Flags::All);
    }
}

void publishTargetKilledAlert(IScriptable& listener, ObjectRef targetRef)
{
    if (listener.getAITarget() == targetRef)
    {
        listener.addAIAlertBits(ALERTIF_TARGETKILLED);
    }
}

void publishDeathAlertsAndTeamExperience(Object& killed,
                                         const std::shared_ptr<Object>& actualKiller,
                                         uint16_t experience)
{
    for (const std::shared_ptr<Object>& listener : objectHandler().iterator())
    {
        if (!listener->isAlive()) continue;

        if (actualKiller && listener != actualKiller && !listener->getTeam().hatesTeam(actualKiller->getTeam()) && listener->getTeam().hatesTeam(killed.getTeam()))
        {
            listener->giveExperience(experience, XP_TEAMKILL, false);
        }

        if (killed.getTeam().getLeader().get() == &killed && listener->getTeam() == killed.getTeam())
        {
            listener->addAIAlertBits(ALERTIF_LEADERKILLED);
        }

        publishTargetKilledAlert(*listener, killed.getObjRef());
    }
}
}

int Object::damage(Facing direction, const IPair  damage, const DamageType damagetype, const TEAM_REF attackerTeam,
                   const std::shared_ptr<Object> &attacker, const bool ignoreArmour, const bool setDamageTime, const bool ignoreInvictus)
{
    bool do_feedback = (Ego::FeedbackType::None != config().hud_feedback.getValue());

    // Simply ignore damaging invincible targets.
    if(invictus && !ignoreInvictus)
    {
        return 0;
    }

    // Don't continue if there is no damage or the character isn't alive.
    int max_damage = std::abs( damage.base ) + std::abs( damage.rand );
    if ( !isAlive() || 0 == max_damage ) return 0;

    // make a special exception for DAMAGE_DIRECT
    uint8_t damageModifier = ( damagetype >= DAMAGE_COUNT ) ? 0 : getAttribute(Ego::Attribute::modifierFromDamageType(damagetype));

    // determine some optional behavior
    bool friendly_fire = false;
    if ( !attacker )
    {
        do_feedback = false;
    }
    else
    {
        // do not show feedback for damaging yourself
        if ( attacker.get() == this )
        {
            do_feedback = false;
        }

        // identify friendly fire for color selection :)
        if ( getTeam() == attacker->getTeam() )
        {
            friendly_fire = true;
        }

        // don't show feedback from random objects hitting each other
        //if ( !attacker->show_stats )
        //{
        //    do_feedback = false;
        //}

        // don't show damage to players since they get feedback from the status bars
        //if ( show_stats || VALID_PLA( is_which_player ) )
        //{
        //    do_feedback = false;
        //}
    }

    // Lessen actual damage taken by resistance
    // This can also be used to lessen effectiveness of healing
    int base_damage = Random::next(damage.base, damage.base+damage.rand);
    int actual_damage = base_damage - base_damage*getDamageReduction(damagetype, !ignoreArmour);

    // Increase electric damage when in water
    if (damagetype == DAMAGE_ZAP && isSubmerged() && activeModule().getWater()._is_water)
    {
        actual_damage *= 2.0f;     /// @note ZF> Is double damage too much?
    }

    // Allow actual_damage to be dealt to mana (mana shield spell)
    if (HAS_SOME_BITS(damageModifier, DAMAGEMANA))
    {
        setMana(getMana() - FP8_TO_FLOAT(actual_damage));
        actual_damage -= std::max<int>(FLOAT_TO_FP8(getMana()) - actual_damage, 0);
        updateLastAttacker(attacker, false);
    }

    // Allow charging (Invert actual_damage to mana)
    if (HAS_SOME_BITS(damageModifier, DAMAGECHARGE))
    {
        setMana(getMana() + FP8_TO_FLOAT(actual_damage));
        return 0;
    }

    // Invert actual_damage to heal
    if (HAS_SOME_BITS(damageModifier, DAMAGEINVERT))
    {
        actual_damage = -actual_damage;
    }

    // Remember the actual_damage type
    ai.damagetypelast = damagetype;
    ai.directionlast  = direction;

    // Check for characters who are immune to this damage, no need to continue if they have
    bool immune_to_damage = HAS_SOME_BITS(damageModifier, DAMAGEINVICTUS) || (actual_damage > 0 && actual_damage <= damage_threshold);
    if ( immune_to_damage && !ignoreInvictus )
    {
        actual_damage = 0;

        //Tell that the character is simply immune to the damage
        //but don't do message and ping for mounts, it's just irritating
        if ( !isMount() && 0 == damage_timer )
        {
            //Ping!
            activeParticleHandler().spawnDefencePing(selfHandle(*this), attacker);

            //Only draw "Immune!" if we are truly completely immune and it was not simply a weak attack
            if(HAS_SOME_BITS(damageModifier, DAMAGEINVICTUS) || damage.base + damage.rand <= damage_threshold) {
                Ego::Graphics::activeBillboardSystem().makeBillboard(_objRef, "Immune!", Ego::Colour4f::white(), Ego::Colour4f(0, 0.5, 0, 1), 3, Ego::Graphics::Billboard::Flags::All);
            }
        }
    }

    // Do it already
    if ( actual_damage > 0 )
    {
        // Only actual_damage if not invincible
        if ( 0 == damage_timer || ignoreInvictus )
        {
            // Normal mode reduces damage dealt by monsters with 30%!
            if (config().game_difficulty.getValue() == Ego::GameDifficulty::Normal && isPlayer())
            {
                actual_damage *= 0.70f;
            }

            // Easy mode deals 25% extra actual damage by players and 50% less to players
            if (attacker && config().game_difficulty.getValue() <= Ego::GameDifficulty::Easy)
            {
                if ( attacker->isPlayer()  && !isPlayer() ) actual_damage *= 1.25f;
                if ( !attacker->isPlayer() &&  isPlayer() ) actual_damage *= 0.5f;
            }

            if ( 0 != actual_damage )
            {
                _currentLife -= FP8_TO_FLOAT(actual_damage);

                // Spawn blud particles
                if ( _profile->getBludType() )
                {
                    if ( _profile->getBludType() == ULTRABLUDY || ( base_damage > HURTDAMAGE && DamageType_isPhysical( damagetype ) ) )
                    {
                        activeParticleHandler().spawnParticle( getPosition(), ori.facing_z + direction,
                                                                              _profile->getSlotNumber(), _profile->getBludParticleProfile(),
                                                                              ObjectRef::Invalid, GRIP_LAST, attackerTeam, _objRef);
                    }
                }

                // Set attack alert if it wasn't an accident
                if ( base_damage > HURTDAMAGE )
                {
                    if ( attackerTeam == Team::TEAM_DAMAGE )
                    {
                        ai.setLastAttacker(ObjectRef::Invalid);
                    }
                    else
                    {
                        updateLastAttacker(attacker, false );
                    }
                }

                //Did we survive?
                if (_currentLife <= 0)
                {
                    this->kill(attacker, ignoreInvictus);
                }
                else
                {
                    //Yes, but play the hurt animation
                    if ( base_damage > HURTDAMAGE )
                    {
                        //If we have Endurance perk, we have 1% chance per Might to resist hurt animation (which cause a minor delay)
                        if(!hasPerk(Ego::Perks::ENDURANCE) || Random::getPercent() > getAttribute(Ego::Attribute::MIGHT))
                        {
                            if(inst.getModelDescriptor()->isActionValid(ACTION_HA)) {
                                inst.playAction(getProfile()->getModel()->randomizeAction(ACTION_HA), false);
                            }
                        }

                        // Make the character invincible for a limited time only
                        if (setDamageTime)
                        {
                            damage_timer = DAMAGETIME;
                        }
                    }
                }
            }

            /// @test spawn a fly-away damage indicator?
            if ( do_feedback )
            {
#if 0
                const char * tmpstr;
                int rank;


                tmpstr = describe_wounds( life_max, life );

                tmpstr = describe_value( actual_damage, UINT_TO_UFP8( 10 ), &rank );
                if ( rank < 4 )
                {
                    tmpstr = describe_value( actual_damage, max_damage, &rank );
                    if ( rank < 0 )
                    {
                        tmpstr = "Fumble!";
                    }
                    else
                    {
                        tmpstr = describe_damage( actual_damage, life_max, &rank );
                        if ( rank >= -1 && rank <= 1 )
                        {
                            tmpstr = describe_wounds( life_max, life );
                        }
                    }
                }

                if ( NULL != tmpstr )
#endif
                {
                    const int lifetime = 2;

                    // friendly damage = "purple"
                    // @todo MH: The colour here is approximately "mauve" and it is already associated with "holy" damage.
                    const auto tint_friend = Ego::Colour4f(0.88, 0.75, 1, 1);

                    // enemy damage color depends on damage type
                    const auto tint_enemy = Ego::Colour4f(DamageType_getColour(damagetype), 1);

                    // write the string into the buffer
                    std::stringstream stringStream;
                    stringStream.precision(1);
                    stringStream << (static_cast<float>(actual_damage) / 256.0f);
                    auto text_buffer = stringStream.str();

                    //Size depends on the amount of damage (more = bigger)
                    float size = Ego::Math::constrain(0.35f + std::abs(FP8_TO_FLOAT(actual_damage)) * 0.075f, 0.35f, 1.5f);

                    Ego::Graphics::activeBillboardSystem().makeBillboard(_objRef, text_buffer, Ego::Colour4f::white(), friendly_fire ? tint_friend : tint_enemy, lifetime, Ego::Graphics::Billboard::Flags::All, size);
                }
            }
        }
    }

    // Heal 'em instead
    else if ( actual_damage < 0 )
    {
        heal(attacker, -actual_damage, ignoreInvictus);

        // Isssue an alert
        if ( attackerTeam == Team::TEAM_DAMAGE )
        {
            ai.setLastAttacker(ObjectRef::Invalid);
        }

        /// @test spawn a fly-away heal indicator?
#if 0
        if ( do_feedback )
        {
            const float lifetime = 3;
            STRING text_buffer = EMPTY_CSTR;

            // "white" text
            const auto text_color = Ego::Colour4f::white();
            // heal == yellow, right ;)
            const auto tint = Ego::Colour4f(1, 1, 0.75, 1);

            // write the string into the buffer
            snprintf( text_buffer, SDL_arraysize( text_buffer ), "%s", describe_value( -actual_damage, damage.base + damage.rand, NULL ) );

            chr_make_text_billboard(_objRef, text_buffer, text_color, tint, lifetime, Billboard::Flags::All );
        }
#endif
    }

    return actual_damage;
}

void Object::updateLastAttacker(const std::shared_ptr<Object> &attacker, bool healing)
{
    // Don't alert the character too much if under constant fire
    if (0 != careful_timer) return;

    ObjectRef actual_attacker;
    if (!resolveLastAttackerAttribution(*this, attacker, actual_attacker))
    {
        return;
    }

    //Update alerts and timers
    ai.setLastAttacker(actual_attacker);
    SET_BIT(ai.alert, healing ? ALERTIF_HEALED : ALERTIF_ATTACKED);
    careful_timer = CAREFULTIME;
}

bool Object::heal(const std::shared_ptr<Object> &healer, const UFP8_T amount, const bool ignoreInvincibility)
{
    //Don't heal dead and invincible stuff
    if (!isAlive() || (invictus && !ignoreInvincibility)) return false;

    //This actually heals the character
    setLife(_currentLife + FP8_TO_FLOAT(amount));

    //With Magical Attunement perk 25% of healing effects also refills mana
    if(hasPerk(Ego::Perks::MAGIC_ATTUNEMENT)) {
        setMana(_currentMana + FP8_TO_FLOAT(amount)*0.25f);
    }

    // Set alerts, but don't alert that we healed ourselves
    if (healer && this != healer.get() && healer->attachedto != _objRef && amount > HURTDAMAGE)
    {
        updateLastAttacker(healer, true);
    }

    return true;
}

bool Object::isAttacking() const
{
    return inst.getCurrentAnimation() >= ACTION_UA && inst.getCurrentAnimation() <= ACTION_FD;
}

void Object::kill(const std::shared_ptr<Object> &originalKiller, bool ignoreInvincibility)
{
    //No need to continue is there?
    if (!isAlive() || (isInvincible() && !ignoreInvincibility)) return;

    //Too silly to Die perk?
    if(hasPerk(Ego::Perks::TOO_SILLY_TO_DIE) && !ignoreInvincibility)
    {
        //1% per character level to simply not die
        if(Random::getPercent() <= getExperienceLevel())
        {
            //Refill to full Life instead!
            _currentLife = getAttribute(Ego::Attribute::MAX_LIFE);
            Ego::Graphics::activeBillboardSystem().makeBillboard(getObjRef(), "Too Silly to Die", Ego::Colour4f::white(), Ego::Colour4f::white(), 3, Ego::Graphics::Billboard::Flags::All);
            DisplayMsg_printf("%s decided not to die after all!", getName(false, true, true).c_str());
            audioSystem().playSound(getPosition(), audioSystem().getGlobalSound(GSND_DRUMS));
            return;
        }
    }

    //Guardian Angel perk?
    if(hasPerk(Ego::Perks::GUARDIAN_ANGEL) && !ignoreInvincibility)
    {
        //1% per character level to be rescued by your guardian angel
        if(Random::getPercent() <= getExperienceLevel())
        {
            //Refill to full Life instead!
            _currentLife = getAttribute(Ego::Attribute::MAX_LIFE);
            Ego::Graphics::activeBillboardSystem().makeBillboard(getObjRef(), "Guardian Angel", Ego::Colour4f::white(), Ego::Colour4f::white(), 3, Ego::Graphics::Billboard::Flags::All);
            DisplayMsg_printf("%s was saved by a Guardian Angel!", getName(false, true, true).c_str());
            audioSystem().playSound(getPosition(), audioSystem().getGlobalSound(GSND_ANGEL_CHOIR));
            return;
        }
    }

    std::shared_ptr<Object> actualKiller = resolveKillCreditRecipient(originalKiller);

    _isAlive = false;

    _currentLife    = -1.0f;
    platform        = true;
    canuseplatforms = true;
    phys.bumpdampen = phys.bumpdampen * 0.5f;
    setBumpWidth(bump_stt.size * 0.5f);

    //End stealth if we were hidden
    deactivateStealth();

    // Play the death animation
    ModelAction action = getProfile()->getModel()->randomizeAction(ACTION_KA);
    inst.playAction(action, false);
    inst.setActionKeep(true);

    // Give kill experience
    uint16_t experience = getProfile()->getExperienceValue() + (this->experience * getProfile()->getExperienceExchangeRate());

    // distribute experience to the attacker
    if (actualKiller)
    {
        publishKillerTarget(*this, getObjRef(), *actualKiller);
        awardDirectKillExperience(*this, *actualKiller, experience, !_hasBeenKilled);
    }

    //Set various alerts to let others know it has died
    //and distribute experience to whoever needs it
    SET_BIT(ai.alert, ALERTIF_KILLED);

    publishDeathAlertsAndTeamExperience(*this, actualKiller, experience);

    // Detach the character from the game
    removeFromGame(this);

    // If it's a player, let it die properly before enabling respawn
    if (isPlayer())  {
        gameSession().publishRespawnCooldown(ONESECOND); // 1 second
    }

    // Let it's AI script run one last time
    _hasBeenKilled = true;
    ai.timer = worldUpdateCount() + 1;            // Prevent IfTimeOut in scr_run_chr_script()
    Ego::Script::activeScriptSystem().runCharacterScript(this);
}

bool Object::costMana(int amount, const ObjectRef killer)
{
    const std::shared_ptr<Object> &pkiller = activeModule().getObjectHandler()[killer];

    bool manaPaid  = false;
    int manaFinal = static_cast<int>(FLOAT_TO_FP8(getMana())) - amount;

    if (manaFinal < 0)
    {
        int manaDebt = -manaFinal;
        _currentMana = 0.0f;

        if ( getAttribute(Ego::Attribute::CHANNEL_LIFE) > 0 )
        {
            _currentLife -= FP8_TO_FLOAT(manaDebt);

            if (_currentLife <= 0 && config().game_difficulty.getValue() >= Ego::GameDifficulty::Hard)
            {
                kill(pkiller != nullptr ? pkiller : activeModule().getObjectHandler()[this->getObjRef()], false);
            }

            manaPaid = true;
        }
    }
    else
    {
        int mana_surplus = 0;

        _currentMana = FP8_TO_FLOAT(manaFinal);

        if ( manaFinal > FLOAT_TO_FP8(getMaxMana()) )
        {
            mana_surplus = manaFinal - FLOAT_TO_FP8(getMaxMana());
            _currentMana = getMaxMana();
        }

        // allow surplus mana to go to health if you can channel?
        if ( getAttribute(Ego::Attribute::CHANNEL_LIFE) > 0 && mana_surplus > 0 )
        {
            // use some factor, divide by 2
            heal(pkiller, mana_surplus / 2, true);
        }

        manaPaid = true;
    }

    return manaPaid;
}
