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
/// @brief Combat- and progression-oriented Object implementation.

#include "egolib/Entities/Object_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

namespace
{
egoboo_config_t& config()
{
    return EngineContext::get().config();
}

IAudioSystem& audioSystem()
{
    return EngineContext::get().audioSystem();
}

const std::shared_ptr<Object>& heldItem(const Object& object, slot_t slot)
{
    return GameSessionContext::get().activeModule().getObjectHandler()[object.getHeldObject(slot)];
}

void publishTargetKilledAlert(IScriptable& listener, ObjectRef targetRef)
{
    if (listener.getAITarget() == targetRef)
    {
        listener.addAIAlertBits(ALERTIF_TARGETKILLED);
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
            EngineContext::get().particleHandler().spawnDefencePing(selfHandle(*this), attacker);

            //Only draw "Immune!" if we are truly completely immune and it was not simply a weak attack
            if(HAS_SOME_BITS(damageModifier, DAMAGEINVICTUS) || damage.base + damage.rand <= damage_threshold) {
                GFX::get().getBillboardSystem().makeBillboard(_objRef, "Immune!", Ego::Colour4f::white(), Ego::Colour4f(0, 0.5, 0, 1), 3, Ego::Graphics::Billboard::Flags::All);
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
                        EngineContext::get().particleHandler().spawnParticle( getPosition(), ori.facing_z + direction,
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

                    GFX::get().getBillboardSystem().makeBillboard(_objRef, text_buffer, Ego::Colour4f::white(), friendly_fire ? tint_friend : tint_enemy, lifetime, Ego::Graphics::Billboard::Flags::All, size);
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
    // Don't let characters chase themselves...  That would be silly
    if ( this == attacker.get() ) return;

    // Don't alert the character too much if under constant fire
    if (0 != careful_timer) return;

    ObjectRef actual_attacker = ObjectRef::Invalid;

    // Figure out who is the real attacker, in case we are a held item or a controlled mount
    if(attacker)
    {
        actual_attacker = attacker->getObjRef();

        //Dont alert if the attacker/healer was on the null team
        if(attacker->getTeam() == Team::TEAM_NULL) {
            return;
        }

        //Do not alert items damaging (or healing) their holders, healing potions for example
        if ( attacker->attachedto == ai.getSelf() ) return;

        //If we are held, the holder is the real attacker... unless the holder is a mount
        if ( attacker->isBeingHeld() && !activeModule().getObjectHandler().get(attacker->attachedto)->isMount() )
        {
            actual_attacker = attacker->attachedto;
        }

        //If the attacker is a mount, try to blame the rider
        else if ( attacker->isMount() && activeModule().getObjectHandler().exists( attacker->holdingwhich[SLOT_LEFT] ) )
        {
            actual_attacker = attacker->holdingwhich[SLOT_LEFT];
        }
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

void Object::checkLevelUp()
{
    // Do level ups and stat changes
    uint8_t curlevel = experiencelevel + 1;
    if ( curlevel < MAXLEVEL )
    {
        uint32_t xpcurrent = experience;
        uint32_t xpneeded  = getProfile()->getXPNeededForLevel(curlevel);

        if ( xpcurrent >= xpneeded )
        {
            // The character is ready to advance...
            if(isPlayer()) {
                const std::shared_ptr<Ego::Player> &player = activeModule().getPlayer(is_which_player);
                if(!player->hasUnspentLevel()) {
                    player->setLevelUpIndicator(true);
                    DisplayMsg_printf("%s gained a level!!!", getName().c_str());
                    audioSystem().playSoundFull(audioSystem().getGlobalSound(GSND_LEVELUP));
                }
                return;
            }

            //Automatic level up for AI characters
            experiencelevel++;
            SET_BIT(ai.alert, ALERTIF_LEVELUP);

            // Size Increase
            fat_goto += getProfile()->getSizeGainPerMight() * 0.25f;  // Limit this?
            fat_goto_time += SIZETIME;

            //Primary Attribute increase
            for(size_t i = 0; i < Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES; ++i) {
                _baseAttribute[i] += Random::next(getProfile()->getAttributeGain(static_cast<Ego::Attribute::AttributeType>(i)));
            }

            //Grab random Perk? (ZF> just uncomment if we want to do this for AI characters as well)
            //std::vector<Ego::Perks::PerkID> perkPool = getValidPerks();
            //if(!perkPool.empty()) {
            //    addPerk(Random::getRandomElement(perkPool));
            //}
            //else {
            //    addPerk(Ego::Perks::TOUGHNESS); //Add TOUGHNESS as default perk if none other are available
            //}
        }
    }
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
            GFX::get().getBillboardSystem().makeBillboard(getObjRef(), "Too Silly to Die", Ego::Colour4f::white(), Ego::Colour4f::white(), 3, Ego::Graphics::Billboard::Flags::All);
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
            GFX::get().getBillboardSystem().makeBillboard(getObjRef(), "Guardian Angel", Ego::Colour4f::white(), Ego::Colour4f::white(), 3, Ego::Graphics::Billboard::Flags::All);
            DisplayMsg_printf("%s was saved by a Guardian Angel!", getName(false, true, true).c_str());
            audioSystem().playSound(getPosition(), audioSystem().getGlobalSound(GSND_ANGEL_CHOIR));
            return;
        }
    }

    //Fix who is actually the killer if needed
    std::shared_ptr<Object> actualKiller = originalKiller;
    if (actualKiller)
    {
        //If we are a held item, try to figure out who the actual killer is
        if ( actualKiller->isBeingHeld() && !activeModule().getObjectHandler().get(actualKiller->attachedto)->isMount() )
        {
            actualKiller = activeModule().getObjectHandler()[actualKiller->attachedto];
        }

        //If the killer is a mount, try to award the kill to the rider
        else if (actualKiller->isMount() && heldItem(*actualKiller, SLOT_LEFT))
        {
            actualKiller = heldItem(*actualKiller, SLOT_LEFT);
        }
    }

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
        // Set target
        ai.setTarget(actualKiller->getObjRef());
        if (actualKiller->getTeam() == Team::TEAM_DAMAGE || actualKiller->getTeam() == Team::TEAM_NULL) {
            ai.setTarget(getObjRef());
        }

        // Award experience for kill?
        if ( actualKiller->getTeam().hatesTeam(getTeam()) )
        {
            //Check for special hatred
            if ( actualKiller->getProfile()->getIDSZ(IDSZ_HATE) == getProfile()->getIDSZ(IDSZ_PARENT) ||
                 actualKiller->getProfile()->getIDSZ(IDSZ_HATE) == getProfile()->getIDSZ(IDSZ_TYPE) )
            {
                actualKiller->giveExperience(experience, XP_KILLHATED, false);
            }

            // Nope, award direct kill experience instead
            else actualKiller->giveExperience(experience, XP_KILLENEMY, false);

            //Mercenary Perk gives +1 Zenny per kill (if this is the first time we died)
            if(actualKiller->hasPerk(Ego::Perks::MERCENARY) && !_hasBeenKilled) {
                actualKiller->giveMoney(1);
                audioSystem().playSound(getPosition(), audioSystem().getGlobalSound(GSND_COINGET));
            }

            //Crusader Perk regains 1 mana per Undead kill
            if(actualKiller->hasPerk(Ego::Perks::CRUSADER) && getProfile()->getIDSZ(IDSZ_PARENT).equals('U','N','D','E')) {
                actualKiller->costMana(-1, actualKiller->getObjRef());
                GFX::get().getBillboardSystem().makeBillboard(actualKiller->getObjRef(), "Crusader", Ego::Colour4f::white(), Ego::Colour4f::yellow(), 3, Ego::Graphics::Billboard::Flags::All);
            }
        }
    }

    //Set various alerts to let others know it has died
    //and distribute experience to whoever needs it
    SET_BIT(ai.alert, ALERTIF_KILLED);

    for(const std::shared_ptr<Object> &listener : activeModule().getObjectHandler().iterator())
    {
        if (!listener->isAlive()) continue;

        // All allies get team experience, but only if they also hate the dead guy's team
        if (actualKiller && listener != actualKiller && !listener->getTeam().hatesTeam(actualKiller->getTeam()) && listener->getTeam().hatesTeam(getTeam()) )
        {
            listener->giveExperience(experience, XP_TEAMKILL, false);
        }

        // Check if we were a leader
        if ( getTeam().getLeader().get() == this && listener->getTeam() == getTeam() )
        {
            // All folks on the leaders team get the alert
            listener->addAIAlertBits(ALERTIF_LEADERKILLED);
        }

        // Let the other characters know it died
        publishTargetKilledAlert(*listener, getObjRef());
    }

    // Detach the character from the game
    removeFromGame(this);

    // If it's a player, let it die properly before enabling respawn
    if (isPlayer())  {
        gameSession().publishRespawnCooldown(ONESECOND); // 1 second
    }

    // Let it's AI script run one last time
    _hasBeenKilled = true;
    ai.timer = worldUpdateCount() + 1;            // Prevent IfTimeOut in scr_run_chr_script()
    scr_run_chr_script(this);
}

void Object::giveExperience(const int amount, const XPType xptype, const bool overrideInvincibility)
{
    //No xp to give
    if (0 == amount) return;

    if (!isInvincible() || overrideInvincibility)
    {
        // Figure out how much experience to give
        float newamount = amount;
        if ( xptype < XP_COUNT )
        {
            newamount = amount * getProfile()->getExperienceRate(xptype);
        }

        // Intellect affects xp gained (1% per intellect above 10, -1% per intellect below 10)
        float intadd = (getAttribute(Ego::Attribute::INTELLECT) - 10.0f) / 100.0f;
        newamount *= 1.00f + intadd;

        // Apply XP bonus/penality depending on game difficulty
        if (config().game_difficulty.getValue() >= Ego::GameDifficulty::Hard)
        {
            newamount *= 1.20f; // 20% extra on hard
        }
        else if (config().game_difficulty.getValue() >= Ego::GameDifficulty::Normal)
        {
            newamount *= 1.10f; // 10% extra on normal
        }


        //Fast Learner Perk gives +20% XP gain
        if(hasPerk(Ego::Perks::FAST_LEARNER)) {
            newamount *= 1.20f;
        }

        //Bookworm Perk gives +10% XP gain
        if(hasPerk(Ego::Perks::BOOKWORM)) {
            newamount *= 1.10f;
        }

        experience += newamount;
    }
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

float Object::getRawDamageResistance(const DamageType type, const bool includeArmor) const
{
    if(type >= DAMAGE_COUNT) {
        return 0.0f;
    }

    float resistance = getAttribute(Ego::Attribute::resistFromDamageType(type));

    //Stalwart perk increases CRUSH, SLASH and POKE by 1
    if(type == DAMAGE_CRUSH || type == DAMAGE_POKE || type == DAMAGE_SLASH) {
        if(hasPerk(Ego::Perks::STALWART)) {
            resistance += 1.0f;
        }
    }

    //Elemental Resistance perk increases FIRE, ICE and ZAP by 1
    if(type == DAMAGE_FIRE || type == DAMAGE_ICE || type == DAMAGE_ZAP) {
        if(hasPerk(Ego::Perks::ELEMENTAL_RESISTANCE)) {
            resistance += 1.0f;
        }

        //Ward perks increases it by further 3
        if(type == DAMAGE_FIRE && hasPerk(Ego::Perks::FIRE_WARD)) {
            resistance += 3.0f;
        }
        else if(type == DAMAGE_ZAP && hasPerk(Ego::Perks::ZAP_WARD)) {
            resistance += 3.0f;
        }
        else if(type == DAMAGE_ICE && hasPerk(Ego::Perks::ICE_WARD)) {
            resistance += 3.0f;
        }
    }

    //Rosemary perk gives +4
    if(type == DAMAGE_EVIL && hasPerk(Ego::Perks::ROSEMARY)) {
        resistance += 4.0f;
    }

    //Pyromaniac and Troll Blood perks *reduces* FIRE resistance by 10 each
    if(type == DAMAGE_FIRE) {
        if(hasPerk(Ego::Perks::PYROMANIAC)) {
            resistance -= 10.0f;
        }
        if(hasPerk(Ego::Perks::TROLL_BLOOD)) {
            resistance -= 10.0f;
        }
    }

    //Negative armor means it's a weakness
    if(resistance < 0.0f) {

        //Defence reduces weakness, but cannot eliminate it completely (50% weakness reduction at 255 defence)
        if(includeArmor) {
            resistance *= 1.0f - (getAttribute(Ego::Attribute::DEFENCE) / 512.0f);
        }
        return resistance;
    }

    //Defence bonus increases all damage type resistances (every 14 points gives +1.0 resistance)
    //This means at 255 defence and 0% resistance results in 52% damage reduction
    if(includeArmor && HAS_NO_BITS( static_cast<int>(getAttribute(Ego::Attribute::modifierFromDamageType(type))), DAMAGEINVERT)) {
        resistance += getAttribute(Ego::Attribute::DEFENCE) / 14.0f;
    }

    return resistance;
}

float Object::getDamageReduction(const DamageType type, const bool includeArmor) const
{
    //DAMAGE_COUNT simply means not affected by damage resistances
    if(type >= DAMAGE_COUNT) {
        return 0.0f;
    }

    //Immunity to damage type?
    if( HAS_SOME_BITS(static_cast<int>(getAttribute(Ego::Attribute::modifierFromDamageType(type))), DAMAGEINVICTUS) ) {
        return 1.0f;
    }

    const float resistance = getRawDamageResistance(type, includeArmor);

    //Negative resistance *increases* damage a lot
    if(resistance < 0.0f) {
        return 1.0f - std::pow(0.94f, resistance);
    }

    //Positive resistance reduces damage, but never 100%
    return ((resistance*0.06f) / (1.0f + resistance*0.06f));
}

bool Object::isInvictusDirection(Facing direction) const
{
    Facing left, right;

    static const Facing MAX = Facing(std::numeric_limits<uint16_t>::max());

    // if the invictus flag is set, we are invictus
    if (isInvincible()) return true;

    // if the character's frame is invictus, then check the angles
    if (HAS_SOME_BITS(inst.getFrameFX(), MADFX_INVICTUS))
    {
        //I Frame
        direction -= Facing(getProfile()->getInvictusFrameFacing());
        left       = MAX - Facing(getProfile()->getInvictusFrameAngle());
        right      = Facing(getProfile()->getInvictusFrameAngle());

        // If using shield, use the shield invictus instead
        if (ACTION_IS_TYPE(inst.getCurrentAnimation(), P))
        {
            bool parry_left = ( inst.getCurrentAnimation() < ACTION_PC );
            const std::shared_ptr<Object>& leftHandItem = heldItem(*this, SLOT_LEFT);
            const std::shared_ptr<Object>& rightHandItem = heldItem(*this, SLOT_RIGHT);

            // Using a shield?
            if (parry_left && leftHandItem)
            {
                // Check left hand
                // 0x00010000L ~ 65536 ~ 2^16
                left = MAX - Facing(leftHandItem->getProfile()->getInvictusFrameAngle());
                right = Facing(leftHandItem->getProfile()->getInvictusFrameAngle());
            }
            else if (rightHandItem)
            {
                // Check right hand
                left = MAX - Facing(rightHandItem->getProfile()->getInvictusFrameAngle());
                right = Facing(rightHandItem->getProfile()->getInvictusFrameAngle());
            }
        }
    }
    else
    {
        // Non invictus Frame
        direction -= Facing(getProfile()->getNormalFrameFacing());
        left = MAX - Facing(getProfile()->getNormalFrameAngle());
        right = Facing(getProfile()->getNormalFrameAngle());
    }

    // Check that direction
    if (direction <= left && direction <= right) {
        return true;
    }

    return false;
}
