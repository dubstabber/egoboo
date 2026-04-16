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

/// @file egolib/game/Entities/Object.hpp
/// @details An object representing instances of in-game egoboo objects (Object)
/// @author Johan Jansen

#include "egolib/Entities/Object_internal.h"

//Declare class static constants
const std::shared_ptr<Object> Object::INVALID_OBJECT = nullptr;

/// @brief Ouf-of-class definition for GCC & Clang.
/// @todo Remove this if GCC & Clang are fixed.
constexpr float Object::DROPZVEL;

/// @brief Out-of-class definition for GCC/Clang.
/// @todo Remove this if GCC & Clang are fixed.
constexpr float Object::DISMOUNTZVEL;

Team& Object::getTeam() const
{
    return activeModule().getTeamList()[team];
}

bool Object::canMount(const std::shared_ptr<Object> mount) const
{
    //Cannot mount ourselves!
    if(this == mount.get())
    {
        return false;
    }

    //Make sure they are a mount and alive
    if(!mount->isMount() || !mount->isAlive())
    {
        return false;
    }

    //We must be alive and not an item to become a rider
    if(!isAlive() || isitem || isBeingHeld())
    {
        return false;
    }

    //Cannot mount while flying
    if(isFlying())
    {
        return false;
    }

    //Make sure they aren't mounted already
    if(!mount->getProfile()->isSlotValid(SLOT_LEFT) || activeModule().getObjectHandler().exists(mount->holdingwhich[SLOT_LEFT]))
    {
        return false;
    }

    //We need a riding animation to be able to mount stuff
    int action_mi = getProfile()->getModel()->getAction(ACTION_MI);
    bool has_ride_anim = ( ACTION_COUNT != action_mi && !ACTION_IS_TYPE( action_mi, D ) );

    return has_ride_anim;
}

int Object::damage(Facing direction, const IPair  damage, const DamageType damagetype, const TEAM_REF attackerTeam,
                   const std::shared_ptr<Object> &attacker, const bool ignoreArmour, const bool setDamageTime, const bool ignoreInvictus)
{
    bool do_feedback = (Ego::FeedbackType::None != egoboo_config_t::get().hud_feedback.getValue());

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
            ParticleHandler::get().spawnDefencePing(toSharedPointer(), attacker);

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
            if (egoboo_config_t::get().game_difficulty.getValue() == Ego::GameDifficulty::Normal && isPlayer())
            {
                actual_damage *= 0.70f;
            }

            // Easy mode deals 25% extra actual damage by players and 50% less to players
            if (attacker && egoboo_config_t::get().game_difficulty.getValue() <= Ego::GameDifficulty::Easy)
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
                        ParticleHandler::get().spawnParticle( getPosition(), ori.facing_z + direction,
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
            ParticleHandler::get().spawnGlobalParticle({getPosX(), getPosY(), activeModule().getWater().get_level() + 10}, ATK_FRONT, LocalParticleProfileRef(PIP_SPLASH), 0);

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
                        ParticleHandler::get().spawnGlobalParticle({getPosX(), getPosY(), activeModule().getWater().get_level()}, ATK_FRONT, LocalParticleProfileRef(PIP_RIPPLE), 0);
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
        if (isBeingHeld() || getObjectPhysics().isTouchingGround() || jumpnumber > 0) { 
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
    inst.uoffset += getProfile()->getTextureMovementRateX();
    inst.voffset += getProfile()->getTextureMovementRateY();

    // Texture tint
    inst.colorshift = colorshift_t(Ego::Math::constrain<int>(1 + getAttribute(Ego::Attribute::RED_SHIFT), 0, 6),
                                   Ego::Math::constrain<int>(1 + getAttribute(Ego::Attribute::GREEN_SHIFT), 0, 6),
                                   Ego::Math::constrain<int>(1 + getAttribute(Ego::Attribute::BLUE_SHIFT), 0, 6));

    // do the mana and life regeneration for "living" characters
    if (isAlive()) {
        _currentMana += getAttribute(Ego::Attribute::MANA_REGEN) / GameEngine::GAME_TARGET_UPS;
        _currentMana = Ego::Math::constrain(_currentMana, 0.0f, getAttribute(Ego::Attribute::MAX_MANA));

        _currentLife += getAttribute(Ego::Attribute::LIFE_REGEN) / GameEngine::GAME_TARGET_UPS;
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
                local_stats.sense_enemies_team = this->team;
                local_stats.sense_enemies_idsz = IDSZ2::None;     //Reveal all
            }

            //Danger Sense reveals enemies on the minimap
            else if(hasPerk(Ego::Perks::SENSE_UNDEAD)) {
                local_stats.sense_enemies_team = this->team;
                local_stats.sense_enemies_idsz = IDSZ2('U','N','D','E');     //Reveal only undead
            }
        }        

        //Give Rally bonus to friends within 6 tiles
        if(hasPerk(Ego::Perks::RALLY)) {
            std::vector<std::shared_ptr<Object>> nearbyObjects = activeModule().getObjectHandler().findObjects(getPosX(), getPosY(), WIDE, false);
            for(const std::shared_ptr<Object> &object : nearbyObjects)
            {
                //Only valid objects that are on our team
                if(object->isTerminated() || object->getTeam() != getTeam()) continue;

                //Don't give bonus to ourselves!
                if(object.get() == this) continue;

                object->_reallyDuration = worldUpdateCount() + GameEngine::GAME_TARGET_UPS*3;    //Apply bonus for 3 seconds
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
            std::vector<std::shared_ptr<Object>> nearbyObjects = activeModule().getObjectHandler().findObjects(getPosX(), getPosY(), WIDE, false);
            for(const std::shared_ptr<Object> &target : nearbyObjects) {
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
                    AudioSystem::get().playSoundFull(AudioSystem::get().getGlobalSound(GSND_LEVELUP));
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
            AudioSystem::get().playSound(getPosition(), AudioSystem::get().getGlobalSound(GSND_DRUMS));
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
            AudioSystem::get().playSound(getPosition(), AudioSystem::get().getGlobalSound(GSND_ANGEL_CHOIR));
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
        else if (actualKiller->isMount() && actualKiller->getLeftHandItem())
        {
            actualKiller = actualKiller->getLeftHandItem();
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
                AudioSystem::get().playSound(getPosition(), AudioSystem::get().getGlobalSound(GSND_COINGET));
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
            SET_BIT( listener->ai.alert, ALERTIF_LEADERKILLED );
        }

        // Let the other characters know it died
        if ( listener->ai.getTarget() == getObjRef() )
        {
            SET_BIT( listener->ai.alert, ALERTIF_TARGETKILLED );
        }
    }

    // Detach the character from the game
	removeFromGame(this);

    // If it's a player, let it die properly before enabling respawn
    if (isPlayer())  {
        local_stats.revivetimer = ONESECOND; // 1 second
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
        if (egoboo_config_t::get().game_difficulty.getValue() >= Ego::GameDifficulty::Hard)
        {
            newamount *= 1.20f; // 20% extra on hard
        }
        else if (egoboo_config_t::get().game_difficulty.getValue() >= Ego::GameDifficulty::Normal)
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

            if (_currentLife <= 0 && egoboo_config_t::get().game_difficulty.getValue() >= Ego::GameDifficulty::Hard)
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

float Object::getBaseAttribute(const Ego::Attribute::AttributeType type) const
{
    IDLIB_DEBUG_ASSERT(type < _baseAttribute.size() && type != Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES);
    return _baseAttribute[type];
}

void Object::setBaseAttribute(const Ego::Attribute::AttributeType type, float value)
{
    IDLIB_DEBUG_ASSERT(type < _baseAttribute.size() && type != Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES);
    _baseAttribute[type] = value;
}

float Object::getAttribute(const Ego::Attribute::AttributeType type) const 
{ 
    IDLIB_DEBUG_ASSERT(type < _baseAttribute.size() && type != Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES);

    float attributeValue = _baseAttribute[type];

    //Try to find temp value in map, but don't create it if it doesn't already exist
    const auto& result = _tempAttribute.find(type);
    if(result != _tempAttribute.end()) {

        //Is this a SET type attribute or a cumulative ADD type attribute?
        if(isOverrideSetAttribute(type)) {
            return (*result).second;
        }
        else {
            //Total value is base plus temp bonuses from enchants
            attributeValue += (*result).second;
        }
    }

    switch(type) {

        //Wolverine perk gives +0.25 Life Regeneration while holding a Claw weapon
        case Ego::Attribute::LIFE_REGEN:
            if(hasPerk(Ego::Perks::WOLVERINE)) {
                if( (getLeftHandItem() && getLeftHandItem()->getProfile()->getIDSZ(IDSZ_PARENT).equals('C','L','A','W'))
                 || (getRightHandItem() && getRightHandItem()->getProfile()->getIDSZ(IDSZ_PARENT).equals('C','L','A','W')))
                 {
                    attributeValue += 0.25f;
                 }                
            }
        break;

        case Ego::Attribute::JUMP_POWER:
            //Special value for flying Objects
            if(getAttribute(Ego::Attribute::FLY_TO_HEIGHT) > 0.0f) {
                return Object::JUMPINFINITE;
            }

            //Athletics Perks gives +25% jump power
            if(hasPerk(Ego::Perks::ATHLETICS)) {
                attributeValue *= 1.25f;
            }

            //Every point of Might increases jump power by 1%
            attributeValue *= 1.0f + (getAttribute(Ego::Attribute::MIGHT) / 100.0f);
        break;

        //Limit lowest acceleration to zero
        case Ego::Attribute::ACCELERATION:
        {
            if(attributeValue < 0.0f) return 0.0f;
        }
        break;

        //Limit lowest base attribute to 1
        case Ego::Attribute::MIGHT:
        case Ego::Attribute::AGILITY:
        case Ego::Attribute::INTELLECT:
        {
            if(attributeValue < 1.0f) return 1.0f;
        }
        break;

        default:
            //nothing, keep default case to quench GCC warnings
        break;
    }

    return attributeValue; 
}

void Object::increaseBaseAttribute(const Ego::Attribute::AttributeType type, float value)
{
    IDLIB_DEBUG_ASSERT(type < _baseAttribute.size() && type != Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES);
    _baseAttribute[type] = Ego::Math::constrain(_baseAttribute[type] + value, 0.0f, 255.0f);

    //Handle current life and mana increase as well
    if(type == Ego::Attribute::MAX_LIFE) {
        _currentLife += value;
    }
    else if(type == Ego::Attribute::MAX_MANA) {
        _currentMana += value;
    }
}

bool Object::hasPerk(Ego::Perks::PerkID perk) const
{
    if(perk == Ego::Perks::NR_OF_PERKS) return true;

    //@note ZF> We also have to check our profile in case we are polymorphed and gain new
    //          skills from our new form (e.g Lumpkin form allows gunplay)
    return _perks[perk] || getProfile()->beginsWithPerk(perk);
}

std::vector<Ego::Perks::PerkID> Object::getValidPerks() const
{
    //Build list of perks we can learn
    std::vector<Ego::Perks::PerkID> result;
    for(size_t i = 0; i < Ego::Perks::NR_OF_PERKS; ++i)
    {
        const Ego::Perks::PerkID id = static_cast<Ego::Perks::PerkID>(i);

        //Can we learn this perk?
        if(!getProfile()->canLearnPerk(id)) {
            continue;
        }

        //Cannot learn the same perk twice
        if(hasPerk(id)) {
            continue;
        }

        //Do we fulfill the requirements for this perk?
        const Ego::Perks::Perk& perk = Ego::Perks::PerkHandler::get().getPerk(id);
        if(perk.getRequirement() == Ego::Perks::NR_OF_PERKS || hasPerk(perk.getRequirement())) {
            result.push_back(id);
        }
    }

    return result;
}

void Object::addPerk(Ego::Perks::PerkID perk)
{
    if(perk == Ego::Perks::NR_OF_PERKS) return;
    _perks[perk] = true;
}

float Object::getLife() const
{
    return _currentLife;
}

float Object::getMana() const
{
    return _currentMana;
}

std::shared_ptr<Ego::Enchantment> Object::addEnchant(ENC_REF enchantProfile, PRO_REF spawnerProfile, const std::shared_ptr<Object>& owner, const std::shared_ptr<Object> &spawner)
{
    if (enchantProfile >= ENCHANTPROFILES_MAX) {
        Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to add enchant with invalid enchant profile ", enchantProfile, Log::EndOfEntry);
        return nullptr;
    }    
    const std::shared_ptr<EnchantProfile> &enchantmentProfile = ProfileSystem::get().EnchantProfileSystem.get_ptr(enchantProfile);
    
    if(!ProfileSystem::get().isLoaded(spawnerProfile)) {
        Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to add enchant with invalid spawner object profile ", spawnerProfile, Log::EndOfEntry);
        return nullptr;
    }

    std::shared_ptr<Ego::Enchantment> enchant = std::make_shared<Ego::Enchantment>(enchantmentProfile, ObjectProfileRef(spawnerProfile), owner);
    enchant->applyEnchantment(this->toSharedPointer());

    //Succeeded to apply the enchantment to the target?
    if(!enchant->isTerminated() && spawner) {
        spawner->_lastEnchantSpawned = enchant;
        return enchant;
    }

    return nullptr;
}

void Object::removeEnchantsWithIDSZ(const IDSZ2& idsz)
{
    //Nothing to do?
    if(idsz == IDSZ2::None) return;

    //Remove all active enchants that have the corresponding IDSZ
    for(const std::shared_ptr<Ego::Enchantment> &enchant : _activeEnchants)
    {
        if(enchant->isTerminated()) continue;
        if(idsz == enchant->getProfile()->removedByIDSZ) {
            enchant->requestTerminate();
        }
    }
}

std::forward_list<std::shared_ptr<Ego::Enchantment>>& Object::getActiveEnchants()
{
    return _activeEnchants;
}

bool Object::disenchant()
{
    bool oneRemoved = false;

    for(const std::shared_ptr<Ego::Enchantment> &enchant : _activeEnchants) {
        if(enchant->isTerminated()) continue;
        enchant->requestTerminate();
        oneRemoved = true;
    }

    return oneRemoved;
}

std::unordered_map<Ego::Attribute::AttributeType, float, std::hash<uint8_t>>& Object::getTempAttributes()
{
    return _tempAttribute;
}

bool Object::isFlying() const
{
    return getAttribute(Ego::Attribute::FLY_TO_HEIGHT) > 0.0f;
}

std::shared_ptr<Ego::Enchantment> Object::getLastEnchantmentSpawned() const
{
    return _lastEnchantSpawned.lock();
}


void Object::setMana(const float value)
{
    _currentMana = Ego::Math::constrain(_currentMana+value, 0.00f, getAttribute(Ego::Attribute::MAX_MANA));
}

void Object::setLife(const float value)
{
    _currentLife = Ego::Math::constrain(_currentLife+value, 0.01f, getAttribute(Ego::Attribute::MAX_LIFE));
}

void Object::polymorphObject(ObjectProfileRef profileID, const SKIN_T newSkin)
{
    if(!ProfileSystem::get().isLoaded(profileID)) {
		Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to polymorph object: target profile ", profileID, " does not exist", Log::EndOfEntry);
        return;
    }

    _profileID = profileID;
    _profile = ProfileSystem::get().getProfile(_profileID);

    //Exit stealth if we change form
    deactivateStealth();

    //Get any items we are holding
    const std::shared_ptr<Object> &leftItem = getLeftHandItem();
    const std::shared_ptr<Object> &rightItem = getRightHandItem();

    // Drop left weapon if we have no left grip
    if ( leftItem && ( !_profile->isSlotValid(SLOT_LEFT) || _profile->isMount() ) )
    {
        leftItem->detatchFromHolder(true, true);
        leftItem->getObjectPhysics().detachFromPlatform();

        if ( isMount() )
        {
            leftItem->setVelocity({leftItem->getVelocity().x(),
                                   leftItem->getVelocity().y(),
                                   DISMOUNTZVEL});
            leftItem->jump_timer = JUMPDELAY;
            leftItem->movePosition(0.0f, 0.0f, DISMOUNTZVEL);
        }
    }

    // Drop right weapon if we have no right grip
    if ( rightItem && !_profile->isSlotValid(SLOT_RIGHT) )
    {
        rightItem->detatchFromHolder(true, true);
        rightItem->getObjectPhysics().detachFromPlatform();

        if ( isMount() )
        {
            rightItem->setVelocity({rightItem->getVelocity().x(),
                                    rightItem->getVelocity().y(),
                                    DISMOUNTZVEL});
            rightItem->jump_timer = JUMPDELAY;
            rightItem->movePosition(0.0f, 0.0f, DISMOUNTZVEL);
        }
    }

    // Stuff that must be set
    stoppedby = _profile->getStoppedByMask();

    // Ammo
    ammomax = _profile->getMaxAmmo();
    ammo    = _profile->getAmmo();

    // Gender
    switch (_profile->getGender()) {
        case GenderProfile::Female:
            gender = Gender::Female;
            break;
        case GenderProfile::Male:
            gender = Gender::Male;
            break;
        case GenderProfile::Neuter:
            gender = Gender::Neuter;
            break;
        case GenderProfile::Random:
            /// @todo Random means, retain current gender, which is not intuitive.
            //gender = Random::getRandomElement(std::vector<Gender>{Gender::Female, Gender::Male, Gender::Neuter});
            break;
    };

    // AI stuff
    ai.state = 0;
    ai.timer          = 0;
    turnmode          = TURNMODE_VELOCITY;
    resetInputCommands();

    // Flags
    platform        = _profile->isPlatform();
    canuseplatforms = _profile->canUsePlatforms();
    isitem          = _profile->isItem();
    invictus        = _profile->isInvincible();
    jump_timer      = JUMPDELAY;
    reaffirm_damagetype = _profile->getReaffirmDamageType();

    //Physics
    phys.bumpdampen = _profile->getBumpDampen();

    if (CAP_INFINITE_WEIGHT == _profile->getWeight())
    {
        phys.weight = Ego::Physics::CHR_INFINITE_WEIGHT;
    }
    else
    {
        phys.weight = std::min<uint32_t>(_profile->getWeight() * fat * fat * fat, Ego::Physics::CHR_MAX_WEIGHT);
    }

    // Character size and bumping
    // set the character size so that the new model is the same size as the old model
    // the model will then morph its size to the correct size over time
    {
        float oldFat = fat;
        float newFat;

        if ( 0.0f == bump.size ) {
            newFat = _profile->getSize();
        }
        else {
            newFat = ( _profile->getBumpSize() * _profile->getSize() ) / bump.size;
        }

        // Spellbooks should stay the same size, even if their spell effect cause changes in size
        if (getProfileID() == ObjectProfileRef(SPELLBOOK)) newFat = oldFat = 1.00f;

        // copy all the cap size info over, as normal
        fat_stt           = _profile->getSize();
        shadow_size_stt   = _profile->getShadowSize();
        bump_stt.size     = _profile->getBumpSize();
        bump_stt.size_big = _profile->getBumpSizeBig();
        bump_stt.height   = _profile->getBumpHeight();

        //Initialize model size and collision box
        fat                = fat_stt;
        shadow_size_save   = shadow_size_stt;
        bump_save.size     = bump_stt.size;
        bump_save.size_big = bump_stt.size_big;
        bump_save.height   = bump_stt.height;
        recalculateCollisionSize();

        // make the model's size congruent
        if (0.0f != newFat && newFat != oldFat)
        {
            setFat(newFat);
            fat_goto      = oldFat;
            fat_goto_time = SIZETIME;
        }
        else
        {
            setFat(oldFat);
            fat_goto      = oldFat;
            fat_goto_time = 0;
        }
    }

    //Remove attached particles before changing our model
    disaffirm_attached_particles(getObjRef());

    //Actually change the model
    inst.setObjectProfile(getProfile());
    chr_update_matrix(this, true);

    // Set the skin after changing the model in ObjectGraphics::setProfile()
    setSkin(newSkin);

    // Must set the wepon grip AFTER the model is changed in ObjectGraphics::setProfile()
    if (isBeingHeld())
    {
        set_weapongrip(getObjRef(), attachedto, slot_to_grip_offset(inwhich_slot) );
    }

    if (leftItem)
    {
        IDLIB_DEBUG_ASSERT(leftItem->attachedto == getObjRef());
        set_weapongrip(leftItem->getObjRef(), getObjRef(), GRIP_LEFT);
    }

    if (rightItem)
    {
        IDLIB_DEBUG_ASSERT(rightItem->attachedto == getObjRef());
        set_weapongrip(rightItem->getObjRef(), getObjRef(), GRIP_RIGHT);
    }

    /// @note ZF@> disabled so that books dont burn when dropped
    //reaffirm_attached_particles( ichr );

    ai_state_t::set_changed(ai);
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

            // Using a shield?
            if (parry_left && getLeftHandItem())
            {
                // Check left hand
                // 0x00010000L ~ 65536 ~ 2^16
                left = MAX - Facing(getLeftHandItem()->getProfile()->getInvictusFrameAngle());
                right = Facing(getLeftHandItem()->getProfile()->getInvictusFrameAngle());
            }
            else if(getRightHandItem())
            {
                // Check right hand
                left = MAX - Facing(getRightHandItem()->getProfile()->getInvictusFrameAngle());
                right = Facing(getRightHandItem()->getProfile()->getInvictusFrameAngle());
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

bool Object::isStealthed() const
{
    return _stealth;
}

void Object::deactivateStealth()
{
    //Not in stealth?
    if(!isStealthed()) return;

    //Reset stealth timer
    _stealthTimer = std::max<uint16_t>(_stealthTimer, ONESECOND);
    _stealth = false;

    GFX::get().getBillboardSystem().makeBillboard(getObjRef(), "Revealed!", Ego::Colour4f::white(), Ego::Colour4f::white(), 2, Ego::Graphics::Billboard::Flags::All);
    AudioSystem::get().playSound(getPosition(), AudioSystem::get().getGlobalSound(GSND_STEALTH_END));
    setAlpha(0xFF);
}

bool Object::activateStealth()
{
    //Already in stealth?
    if(isStealthed()) return true;

    //Not allowed to stealth yet?
    if(_stealthTimer > 0) {
        return false;
    }

    //Limit stealth atttempts to once per second
    _stealthTimer = ONESECOND;

    //Do they have the required stealth Perk?
    if(!hasPerk(Ego::Perks::STEALTH)) {
        if(isPlayer()) {
            DisplayMsg_printf("%s does not know how to stealth...", getName().c_str());
        }
        return false;
    }

    //Setup line of sight data
    line_of_sight_info_t lineOfSightInfo;
    lineOfSightInfo.x1 = getPosX();
    lineOfSightInfo.y1 = getPosY();
    lineOfSightInfo.z1 = getPosZ() + std::max(1.0f, bump.height);

    //Check if there are any nearby Objects disrupting our stealth attempt
    std::vector<std::shared_ptr<Object>> nearbyObjects = activeModule().getObjectHandler().findObjects(getPosX(), getPosY(), WIDE, false);
    for(const std::shared_ptr<Object> &object : nearbyObjects) {
        //Valid objects only
        if(object->isTerminated() || !object->isAlive() || object->isBeingHeld()) continue;

        //Skip scenery objects
        if (object->isScenery()) {
            continue;
        }

        //Ignore objects that are doing the sleep animation
        if(object->inst.getCurrentAnimation() == ACTION_MK) {
            continue;
        }

        //Do they consider us an enemy?
        if(!object->getTeam().hatesTeam(getTeam())) {
            continue;
        }

        //Can they see us?
        lineOfSightInfo.x0         = object->getPosX();
        lineOfSightInfo.y0         = object->getPosY();
        lineOfSightInfo.z0         = object->getPosZ() + std::max(1.0f, object->bump.height);
        lineOfSightInfo.stopped_by = object->stoppedby;
        if (line_of_sight_info_t::blocked(lineOfSightInfo, activeModule().getMeshPointer())) {
            continue;
        }
        
        //Camouflage Perk allows us to hide as long as enemies aren't directly looking at us
        if(hasPerk(Ego::Perks::CAMOUFLAGE) && !object->isFacingLocation(getPosX(), getPosY())) {
            continue;
        }

        //We can't stealth while an enemy is nearby
        if(isPlayer()) {
            GFX::get().getBillboardSystem().makeBillboard(getObjRef(), "Hide Failed!", Ego::Colour4f::white(), Ego::Colour4f::white(), 2, Ego::Graphics::Billboard::Flags::All);
            AudioSystem::get().playSound(getPosition(), AudioSystem::get().getGlobalSound(GSND_STEALTH_END));
        }
        return false;
    }

    //All good, we are now stealthed!
    _stealth = true;
    setAlpha(0);
    GFX::get().getBillboardSystem().makeBillboard(getObjRef(), "Hidden!", Ego::Colour4f::white(), Ego::Colour4f::white(), 2, Ego::Graphics::Billboard::Flags::All);
    AudioSystem::get().playSound(getPosition(), AudioSystem::get().getGlobalSound(GSND_STEALTH));
   
    return true;
}

void Object::setTeam(TEAM_REF team_new, bool permanent)
{
    //No change?
    if(getTeam() == team_new) {
        return;
    }

    // do we count this character as being on a team?
    const bool canHaveTeam = !isItem() && isAlive() && !isInvincible();

    // take the character off of its old team
    if ( VALID_TEAM_RANGE(this->team) )
    {
        // remove the character from the old team
        if ( canHaveTeam )
        {
            getTeam().decreaseMorale();
        }

        //Were we the leader?
        if (this == getTeam().getLeader().get())
        {
            getTeam().setLeader(Object::INVALID_OBJECT);
        }
    }

    // make sure we have a valid value
    if(!VALID_TEAM_RANGE(team_new)) {
        team_new = static_cast<TEAM_REF>(Team::TEAM_NULL);
    }

    // place the character onto its new team
    this->team = team_new;

    // switch the base team only if required
    if (permanent) {
        team_base = this->team;
    }

    // add the character to the new team
    if (canHaveTeam) {
        getTeam().increaseMorale();
    }

    // we are the new leader if there isn't one already
    if (canHaveTeam && !getTeam().getLeader()) {
        getTeam().setLeader(activeModule().getObjectHandler()[getObjRef()]);
    }

    if(permanent) {
        //Set the team of our mount as well
        if(isBeingHeld() && getHolder()->isMount()) {
            getHolder()->setTeam(team_new, false);
        }

        //Switch team of whatever we are holding as well
        if(getLeftHandItem()) {
            getLeftHandItem()->setTeam(team_new, false);
        }
        if(getRightHandItem()) {
            getRightHandItem()->setTeam(team_new, false);
        }
    }
}

bool Object::hasSkillIDSZ(const IDSZ2& whichskill) const
{
    if (isTerminated()) return false;

    //Any [NONE] IDSZ returns always "true"
    if ( IDSZ2::None == whichskill ) return true;

    // First check the character Skill ID matches
    if ( getProfile()->getIDSZ(IDSZ_SKILL) == whichskill ) {
        return true;
    }

    // Then check if any Perk matches
    switch(whichskill.toUint32())
    {
        case IDSZ2::caseLabel('P', 'O', 'I', 'S'):
            return hasPerk(Ego::Perks::POISONRY);

        case IDSZ2::caseLabel('C', 'K', 'U', 'R'):
            return hasPerk(Ego::Perks::SENSE_KURSES);

        case IDSZ2::caseLabel('D', 'A', 'R', 'K'):
            return hasPerk(Ego::Perks::NIGHT_VISION) || hasPerk(Ego::Perks::PERCEPTIVE);

        case IDSZ2::caseLabel('A', 'W', 'E', 'P'):
            return hasPerk(Ego::Perks::WEAPON_PROFICIENCY);

        case IDSZ2::caseLabel('W', 'M', 'A', 'G'):
            return hasPerk(Ego::Perks::ARCANE_MAGIC);

        case IDSZ2::caseLabel('D', 'M', 'A', 'G'):
        case IDSZ2::caseLabel('H', 'M', 'A', 'G'):
            return hasPerk(Ego::Perks::DIVINE_MAGIC);

        case IDSZ2::caseLabel('D', 'I', 'S', 'A'):
            return hasPerk(Ego::Perks::TRAP_LORE);

        case IDSZ2::caseLabel('F', 'I', 'N', 'D'):
            return hasPerk(Ego::Perks::PERCEPTIVE);

        case IDSZ2::caseLabel('T', 'E', 'C', 'H'):
            return hasPerk(Ego::Perks::USE_TECHNOLOGICAL_ITEMS);

        case IDSZ2::caseLabel('S', 'T', 'A', 'B'):
            return hasPerk(Ego::Perks::BACKSTAB);

        case IDSZ2::caseLabel('R', 'E', 'A', 'D'):
            return hasPerk(Ego::Perks::LITERACY);

        case IDSZ2::caseLabel('W', 'A', 'N', 'D'):
            return hasPerk(Ego::Perks::THAUMATURGY);

        case IDSZ2::caseLabel('J', 'O', 'U', 'S'):
            return hasPerk(Ego::Perks::JOUSTING);            

        case IDSZ2::caseLabel('T', 'E', 'L', 'E'):
            return hasPerk(Ego::Perks::TELEPORT_MASTERY); 

        case IDSZ2::caseLabel('G', 'N', 'O', 'M'):
            return hasPerk(Ego::Perks::READ_GNOMISH);
    }

    //Skill not found
    return false;    
}

void Object::giveMoney(int amount)
{
    _money = Ego::Math::constrain<int>(static_cast<int>(_money) + amount, 0, MAXMONEY);
}
