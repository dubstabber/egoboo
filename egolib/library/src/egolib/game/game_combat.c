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

/// @file egolib/game/game_combat.c
/// @brief Combat mechanics — attack execution and damage particle spawning

#include "egolib/game/game_internal.h"

#define THROWFIX            30.0f                    ///< To correct thrown velocities
#define MINTHROWVELOCITY    15.0f
#define MAXTHROWVELOCITY    75.0f

//--------------------------------------------------------------------------------------------
bool chr_do_latch_attack( Object * pchr, slot_t which_slot )
{
    GameModule& module = activeModule();
    bool action_valid, allowedtoattack;

    bool retval = false;

    if (!pchr || pchr->isTerminated()) return false;
    auto iobj = GET_INDEX_PCHR( pchr );


    if (which_slot >= SLOT_COUNT) return false;

    // Which iweapon?
    auto iweapon = pchr->getHeldObject(which_slot);
    if ( !module.getObjectHandler().exists( iweapon ) )
    {
        // Unarmed means object itself is the weapon
        iweapon = iobj;
    }
    Object *pweapon = module.getObjectHandler().get(iweapon);
    const std::shared_ptr<ObjectProfile> &weaponProfile = pweapon->getProfile();

    // No need to continue if we have an attack cooldown
    if ( 0 != pweapon->getReloadTimer() ) return false;

    // grab the iweapon's action
    ModelAction base_action = static_cast<ModelAction>(weaponProfile->getWeaponAction());
    ModelAction hand_action = pchr->getProfile()->getModel()->randomizeAction( base_action, which_slot );

    // see if the character can play this action
    ModelAction action       = pchr->getProfile()->getModel()->getAction(hand_action);
    action_valid = ACTION_COUNT != action;

    // Can it do it?
    allowedtoattack = true;

    // First check if reload time and action is okay
    if ( !action_valid )
    {
        allowedtoattack = false;
    }
    else
    {
        // Then check if a skill is needed
        if ( weaponProfile->requiresSkillIDToUse() )
        {
            if (!pchr->hasSkillIDSZ(pweapon->getProfile()->getIDSZ(IDSZ_SKILL)))
            {
                allowedtoattack = false;
            }
        }
    }

    // Don't allow users with kursed weapon in the other hand to use longbows
    if ( allowedtoattack && ACTION_IS_TYPE( action, L ) )
    {
        const std::shared_ptr<Object> &offhandItem = which_slot == SLOT_LEFT ? pchr->getLeftHandItem() : pchr->getRightHandItem();
        if(offhandItem && offhandItem->isKursed()) allowedtoattack = false;
    }

    if ( !allowedtoattack )
    {
        // This character can't use this iweapon
        pweapon->setReloadTimer(ONESECOND);
        if (pchr->getShowStatus() || egoboo_config_t::get().debug_developerMode_enable.getValue())
        {
            // Tell the player that they can't use this iweapon
            DisplayMsg_printf("%s can't use this item...", pchr->getName(false, true, true).c_str());
        }
        return false;
    }

    if ( ACTION_DA == action )
    {
        allowedtoattack = false;
        if ( 0 == pweapon->getReloadTimer() )
        {
            SET_BIT( pweapon->ai.alert, ALERTIF_USED );
        }
    }

    // deal with your mount (which could steal your attack)
    if ( allowedtoattack )
    {
        // Rearing mount
        const std::shared_ptr<Object> &pmount = module.getObjectHandler()[pchr->getHolderRef()];

        if (pmount)
        {
            const std::shared_ptr<ObjectProfile> &mountProfile = pmount->getProfile();

            // let the mount steal the rider's attack
            if (!mountProfile->riderCanAttack()) allowedtoattack = false;

            // can the mount do anything?
            if ( pmount->isMount() && pmount->isAlive() )
            {
                // can the mount be told what to do?
                if ( !pmount->isPlayer() && pmount->inst.canBeInterrupted() )
                {
                    if ( !ACTION_IS_TYPE( action, P ) || !mountProfile->riderCanAttack() )
                    {
                        const ModelAction action = pmount->getProfile()->getModel()->randomizeAction(ACTION_UA);
                        pmount->inst.playAction(action, false );
                        SET_BIT( pmount->ai.alert, ALERTIF_USED );
                        pchr->ai.lastitemused = pmount->getObjRef();

                        retval = true;
                    }
                }
            }
        }
    }

    // Attack button
    if ( allowedtoattack )
    {
        //Attacking or using an item disables stealth
        pchr->deactivateStealth();

        if ( pchr->inst.canBeInterrupted() && action_valid )
        {
            //Check if we are attacking unarmed and cost mana to do so
            if(iweapon == pchr->getObjRef())
            {
                if(pchr->getProfile()->getUseManaCost() <= pchr->getMana())
                {
                    pchr->costMana(pchr->getProfile()->getUseManaCost(), pchr->getObjRef());
                }
                else
                {
                    allowedtoattack = false;
                }
            }

            if(allowedtoattack)
            {
                // randomize the action
                action = pchr->getProfile()->getModel()->randomizeAction( action, which_slot );

                // make sure it is valid
                action = pchr->getProfile()->getModel()->getAction(action);

                if ( ACTION_IS_TYPE( action, P ) )
                {
                    // we must set parry actions to be interrupted by anything
                    pchr->inst.playAction(action, true);
                }
                else
                {
                    float agility = pchr->getAttribute(Ego::Attribute::AGILITY);

                    pchr->inst.playAction(action, false);

                    // Make the weapon animate the attack as well as the character holding it
                    if (iweapon != iobj)
                    {
                        pweapon->inst.playAction(ACTION_MJ, false);
                    }

                    //Crossbow Mastery increases XBow attack speed by 30%
                    if(pchr->hasPerk(Ego::Perks::CROSSBOW_MASTERY) &&
                       pweapon->getProfile()->getIDSZ(IDSZ_PARENT).equals('X','B','O','W')) {
                        agility *= 1.30f;
                    }

                    //Determine the attack speed (how fast we play the animation)
                    pchr->inst.setAnimationSpeed(0.80f + agility * 0.02f);   //every Agility increases base attack speed by 2%

                    //If Quick Strike perk triggers then we have fastest possible attack (10% chance)
                    if(pchr->hasPerk(Ego::Perks::QUICK_STRIKE) && pweapon->getProfile()->isMeleeWeapon() && Random::getPercent() <= 10) {
                        pchr->inst.setAnimationSpeed(3.0f);
                        GFX::get().getBillboardSystem().makeBillboard(pchr->getObjRef(), "Quick Strike!", Ego::Colour4f::white(), Ego::Colour4f::blue(), 3, Ego::Graphics::Billboard::Flags::All);
                    }

                    //Add some reload time as a true limit to attacks per second
                    //Dexterity decreases the reload time for all weapons. We could allow other stats like intelligence
                    //reduce reload time for spells or gonnes here.
                    else if ( !weaponProfile->hasFastAttack() )
                    {
                        int base_reload_time = -agility;
                        if ( ACTION_IS_TYPE( action, U ) )      base_reload_time += 50;     //Unarmed  (Fists)
                        else if ( ACTION_IS_TYPE( action, T ) ) base_reload_time += 55;     //Thrust   (Spear)
                        else if ( ACTION_IS_TYPE( action, C ) ) base_reload_time += 85;     //Chop     (Axe)
                        else if ( ACTION_IS_TYPE( action, S ) ) base_reload_time += 65;     //Slice    (Sword)
                        else if ( ACTION_IS_TYPE( action, B ) ) base_reload_time += 70;     //Bash     (Mace)
                        else if ( ACTION_IS_TYPE( action, L ) ) base_reload_time += 60;     //Longbow  (Longbow)
                        else if ( ACTION_IS_TYPE( action, X ) ) base_reload_time += 130;    //Xbow     (Crossbow)
                        else if ( ACTION_IS_TYPE( action, F ) ) base_reload_time += 60;     //Flinged  (Unused)

                        //it is possible to have so high dex to eliminate all reload time
                        if ( base_reload_time > 0 ) pweapon->setReloadTimer(pweapon->getReloadTimer() + base_reload_time);
                    }
                }

                // let everyone know what we did
                pchr->ai.lastitemused = iweapon;

                /// @note ZF@> why should there any reason the weapon should NOT be alerted when it is used?
                // grab the MADFX_* flags for this action
//                BIT_FIELD action_madfx = getProfile()->getModel()->getActionFX(action);
//                if ( iweapon == ichr || HAS_NO_BITS( action, MADFX_ACTLEFT | MADFX_ACTRIGHT ) )
                {
                    SET_BIT( pweapon->ai.alert, ALERTIF_USED );
                }

                retval = true;
            }
        }
    }

    //Reset boredom timer if the attack succeeded
    if ( retval )
    {
        pchr->resetBoredTimer();
    }

    return retval;
}

//--------------------------------------------------------------------------------------------
void character_swipe( ObjectRef ichr, slot_t slot )
{
    /// @author ZZ
    /// @details This function spawns an attack particle
    GameModule& module = activeModule();
    const std::shared_ptr<Object> &pchr = module.getObjectHandler()[ichr];
    if(!pchr) {
        return;
    }

    ObjectRef iweapon = pchr->getHeldObject(slot);

    // See if it's an unarmed attack...
    bool unarmed_attack;
    int spawn_vrt_offset;
    if ( !module.getObjectHandler().exists(iweapon) )
    {
        unarmed_attack   = true;
        iweapon          = ichr;
        spawn_vrt_offset = slot_to_grip_offset( slot );  // SLOT_LEFT -> GRIP_LEFT, SLOT_RIGHT -> GRIP_RIGHT
    }
    else
    {
        unarmed_attack   = false;
        spawn_vrt_offset = GRIP_LAST;
    }

    const std::shared_ptr<Object> &pweapon = module.getObjectHandler()[iweapon];
    const std::shared_ptr<ObjectProfile> &weaponProfile = pweapon->getProfile();

    // find the 1st non-item that is holding the weapon
    ObjectRef iholder = chr_get_lowest_attachment( iweapon, true );
    const std::shared_ptr<Object> &pholder = module.getObjectHandler()[iholder];

    /*
        if ( iweapon != iholder && iweapon != ichr )
        {
            // This seems to be the "proper" place to activate the held object.
            // If the attack action  of the character holding the weapon does not have
            // MADFX_ACTLEFT or MADFX_ACTRIGHT bits (and so character_swipe function is never called)
            // then the action is played and the ALERTIF_USED bit is set in the chr_do_latch_attack()
            // function.
            //
            // It would be better to move all of this to the character_swipe() function, but we cannot be assured
            // that all models have the proper bits set.

            // Make the iweapon attack too
            chr_play_action( pweapon, ACTION_MJ, false );

            SET_BIT( pweapon->ai.alert, ALERTIF_USED );
        }
    */

    // What kind of attack are we going to do?
    if ( !unarmed_attack && (( weaponProfile->isStackable() && pweapon->getAmmo() > 1 ) || ACTION_IS_TYPE( pweapon->inst.getCurrentAnimation(), F ) ) )
    {
        // Throw the weapon if it's stacked or a hurl animation
        std::shared_ptr<Object> pthrown = module.spawnObject(pchr->getPosition(), ObjectProfileRef(pweapon->getProfileID()), pholder->getTeam().toRef(), pweapon->getSkin(), pchr->getFacingZ(), pweapon->getName(), ObjectRef::Invalid);
        if (pthrown)
        {
            pthrown->setKursed(false);
            pthrown->setAmmo(1);
            SET_BIT( pthrown->ai.alert, ALERTIF_THROWN );

            // deterimine the throw velocity
            float velocity = MINTHROWVELOCITY;
            if ( 0 == pthrown->phys.weight )
            {
                velocity += MAXTHROWVELOCITY;
            }
            else
            {
                velocity += FLOAT_TO_FP8(pchr->getAttribute(Ego::Attribute::MIGHT)) / ( pthrown->phys.weight * THROWFIX );
            }
            velocity = Ego::Math::constrain( velocity, MINTHROWVELOCITY, MAXTHROWVELOCITY );

            Facing turn = pchr->getFacingZ() + ATK_BEHIND;
            pthrown->setVelocity({pthrown->getVelocity().x() + std::cos(turn) * velocity,
                                  pthrown->getVelocity().y() + std::sin(turn) * velocity,
                                  Object::DROPZVEL});

            //Was that the last one?
            if ( pweapon->getAmmo() <= 1 ) {
                // Poof the item
                pweapon->requestTerminate();
                return;
            }
            else {
                pweapon->setAmmo(pweapon->getAmmo() - 1);
            }
        }
    }
    else
    {
        // A generic attack. Spawn the damage particle.
        if ( 0 == pweapon->getAmmoMax() || 0 != pweapon->getAmmo() )
        {
            if ( pweapon->getAmmo() > 0 && !weaponProfile->isStackable() )
            {
                //Is it a wand? (Wand Mastery perk has chance to not use charge)
                if(pweapon->getProfile()->getIDSZ(IDSZ_SKILL).equals('W','A','N','D')
                    && pchr->hasPerk(Ego::Perks::WAND_MASTERY)) {

                    //1% chance per Intellect
                    if(Random::getPercent() <= pchr->getAttribute(Ego::Attribute::INTELLECT)) {
                        GFX::get().getBillboardSystem().makeBillboard(pchr->getObjRef(), "Wand Mastery!", Ego::Colour4f::white(), Ego::Colour4f::purple(), 3, Ego::Graphics::Billboard::Flags::All);
                    }
                    else {
                        pweapon->setAmmo(pweapon->getAmmo() - 1);  // Ammo usage
                    }
                }
                else {
                    pweapon->setAmmo(pweapon->getAmmo() - 1);  // Ammo usage
                }
            }

            PIP_REF attackParticle = weaponProfile->getAttackParticleProfile();
            int NR_OF_ATTACK_PARTICLES = 1;

            //Handle Double Shot perk
            if(pchr->hasPerk(Ego::Perks::DOUBLE_SHOT) && weaponProfile->getIDSZ(IDSZ_PARENT).equals('L','B','O','W'))
            {
                //1% chance per Agility
                if(Random::getPercent() <= pchr->getAttribute(Ego::Attribute::AGILITY) && pweapon->getAmmo() > 0) {
                    NR_OF_ATTACK_PARTICLES = 2;
                    GFX::get().getBillboardSystem().makeBillboard(pchr->getObjRef(), "Double Shot!", Ego::Colour4f::white(), Ego::Colour4f::green(), 3, Ego::Graphics::Billboard::Flags::All);

                    //Spend one extra ammo
                    pweapon->setAmmo(pweapon->getAmmo() - 1);
                }
            }

            // Spawn an attack particle
            if (INVALID_PIP_REF != attackParticle)
            {
                for(int i = 0; i < NR_OF_ATTACK_PARTICLES; ++i)
                {
                    // make the weapon's holder the owner of the attack particle?
                    // will this mess up wands?
                    std::shared_ptr<Ego::Particle> particle = ParticleHandler::get().spawnParticle(pweapon->getPosition(),
                        idlib::canonicalize(pchr->getFacingZ()), weaponProfile->getSlotNumber(),
                        attackParticle, weaponProfile->hasAttachParticleToWeapon() ? iweapon : ObjectRef::Invalid,
                        spawn_vrt_offset, pholder->getTeam().toRef(), iholder);

                    if (particle)
                    {
                        Ego::Vector3f tmp_pos = particle->getPosition();

                        if ( weaponProfile->hasAttachParticleToWeapon() )
                        {
                            particle->phys.weight     = pchr->phys.weight;
                            particle->phys.bumpdampen = pweapon->phys.bumpdampen;

                            particle->placeAtVertex(pweapon, spawn_vrt_offset);
                        }
                        else if ( particle->getProfile()->startontarget && particle->hasValidTarget() )
                        {
                            particle->placeAtVertex(particle->getTarget(), spawn_vrt_offset);

                            // Correct Z spacing base, but nothing else...
                            tmp_pos[kZ] += particle->getProfile()->getSpawnPositionOffsetZ().base;
                        }
                        else
                        {
                            // NOT ATTACHED

                            // Don't spawn in walls
                            if (EMPTY_BIT_FIELD != particle->test_wall( tmp_pos ))
                            {
                                tmp_pos[kX] = pweapon->getPosX();
                                tmp_pos[kY] = pweapon->getPosY();
                                if (EMPTY_BIT_FIELD != particle->test_wall( tmp_pos ))
                                {
                                    tmp_pos[kX] = pchr->getPosX();
                                    tmp_pos[kY] = pchr->getPosY();
                                }
                            }
                        }

                        // Initial particles get a bonus, which may be zero. Increases damage with +(factor)% per attribute point (e.g Might=10 and MightFactor=0.06 then damageBonus=0.6=60%)
                        particle->damage.base += (pchr->getAttribute(Ego::Attribute::MIGHT)     * weaponProfile->getStrengthDamageFactor());
                        particle->damage.base += (pchr->getAttribute(Ego::Attribute::INTELLECT) * weaponProfile->getIntelligenceDamageFactor());
                        particle->damage.base += (pchr->getAttribute(Ego::Attribute::AGILITY)   * weaponProfile->getDexterityDamageFactor());

                        // Initial particles get an enchantment bonus
                        particle->damage.base += pweapon->getAttribute(Ego::Attribute::DAMAGE_BONUS);

                        //Handle traits that increase weapon damage
                        float damageBonus = 1.0f;
                        switch(weaponProfile->getIDSZ(IDSZ_PARENT).toUint32())
                        {
                            //Wolverine perk gives +100% Claw damage
                            case IDSZ2::caseLabel('C','L','A','W'):
                                if(pchr->hasPerk(Ego::Perks::WOLVERINE)) {
                                    damageBonus += 1.0f;
                                }
                            break;

                            //+20% damage with polearms
                            case IDSZ2::caseLabel('P','O','L','E'):
                                if(pchr->hasPerk(Ego::Perks::POLEARM_MASTERY)) {
                                    damageBonus += 0.2f;
                                }
                            break;

                            //+20% damage with swords
                            case IDSZ2::caseLabel('S','W','O','R'):
                                if(pchr->hasPerk(Ego::Perks::SWORD_MASTERY)) {
                                    damageBonus += 0.2f;
                                }
                            break;

                            //+20% damage with Axes
                            case IDSZ2::caseLabel('A','X','E','E'):
                                if(pchr->hasPerk(Ego::Perks::AXE_MASTERY)) {
                                    damageBonus += 0.2f;
                                }
                            break;

                            //+20% damage with Longbows
                            case IDSZ2::caseLabel('L','B','O','W'):
                                if(pchr->hasPerk(Ego::Perks::BOW_MASTERY)) {
                                    damageBonus += 0.2f;
                                }
                            break;

                            //+100% damage with Whips
                            case IDSZ2::caseLabel('W','H','I','P'):
                                if(pchr->hasPerk(Ego::Perks::WHIP_MASTERY)) {
                                    damageBonus += 1.0f;
                                }
                            break;
                        }

                        //Improvised Weapons perk gives +100% to some unusual weapons
                        if(pchr->hasPerk(Ego::Perks::IMPROVISED_WEAPONS)) {
                            if (weaponProfile->getIDSZ(IDSZ_PARENT).equals('T','O','R','C')    //Torch
                             || weaponProfile->getIDSZ(IDSZ_TYPE).equals('S','H','O','V')      //Shovel
                             || weaponProfile->getIDSZ(IDSZ_TYPE).equals('P','L','U','N')      //Toilet Plunger
                             || weaponProfile->getIDSZ(IDSZ_TYPE).equals('C','R','O','W')      //Crowbar
                             || weaponProfile->getIDSZ(IDSZ_TYPE).equals('P','I','C','K')) {   //Pick
                                damageBonus += 1.0f;
                            }
                        }

                        //Berserker perk deals +25% damage if you are below 25% life
                        if(pchr->hasPerk(Ego::Perks::BERSERKER) && pchr->getLife() <= pchr->getAttribute(Ego::Attribute::MAX_LIFE)/4) {
                            damageBonus += 0.25f;
                        }

                        //If it is a ranged attack then Sharpshooter increases damage by 10%
                        if(pchr->hasPerk(Ego::Perks::SHARPSHOOTER) && weaponProfile->isRangedWeapon() && DamageType_isPhysical(particle->damagetype)) {
                            damageBonus += 0.1f;
                        }

                        //+25% damage with Blunt Weapons Mastery
                        if(particle->damagetype == DAMAGE_CRUSH && pchr->hasPerk(Ego::Perks::BLUNT_WEAPONS_MASTERY) && weaponProfile->isMeleeWeapon()) {
                            damageBonus += 0.25f;
                        }

                        //If it is a melee attack then Brute perk increases damage by 10%
                        if(pchr->hasPerk(Ego::Perks::BRUTE) && weaponProfile->isMeleeWeapon()) {
                            damageBonus += 0.1f;
                        }

                        //Rally Bonus? (+10%)
                        if(pchr->hasPerk(Ego::Perks::RALLY) && worldUpdateCount() < pchr->getRallyDuration()) {
                            damageBonus += 0.1f;
                        }

                        //Apply damage bonus modifiers
                        particle->damage.base *= damageBonus;
                        particle->damage.rand *= damageBonus;

                        //If this is a double shot particle, then add a little space between the arrows
                        if(i > 0) {
                            float x, y;
                            facing_to_vec(particle->facing, &x, &y);
                            tmp_pos[kX] -= x*32.0f;
                            tmp_pos[kY] -= x*32.0f;
                        }

                        particle->setPosition(tmp_pos);
                    }
                    else
                    {
                        Log::get() << Log::Entry::create(Log::Level::Debug, __FILE__, __LINE__, "unable to spawn attack particle for ", "`", weaponProfile->getClassName(), "`", Log::EndOfEntry);
                    }
                }
            }
            else
            {
                Log::get() << Log::Entry::create(Log::Level::Debug, __FILE__, __LINE__, "invalid attack particle ", "`", weaponProfile->getClassName(), "`", Log::EndOfEntry);
            }
        }
        else
        {
            pweapon->setAmmoKnown(true);
        }
    }
}

//--------------------------------------------------------------------------------------------
ObjectRef chr_get_lowest_attachment( ObjectRef ichr, bool non_item )
{
    GameModule& module = activeModule();
    /// @author BB
    /// @details Find the lowest attachment for a given object.
    ///               This was basically taken from the script function scr_set_TargetToLowestTarget()
    ///
    ///               You should be able to find the holder of a weapon by specifying non_item == true
    ///
    ///               To prevent possible loops in the data structures, use a counter to limit
    ///               the depth of the search, and make sure that ichr != module.getObjectHandler().get(object)->attachedto

    if (!module.getObjectHandler().exists(ichr)) return ObjectRef::Invalid;

    ObjectRef original_object, object;
    original_object = object = ichr;
    for (size_t cnt = 0; cnt < OBJECTS_MAX; cnt++)
    {
        // check for one of the ending condiitons
        if (non_item && !module.getObjectHandler().get(object)->isItem())
        {
            break;
        }

        // grab the next object in the list
        ObjectRef object_next = module.getObjectHandler().get(object)->getHolderRef();

        // check for an end of the list
        if (!module.getObjectHandler().exists(object_next))
        {
            break;
        }

        // check for a list with a loop. shouldn't happen, but...
        if (object_next == original_object)
        {
            break;
        }

        // go to the next object
        object = object_next;
    }

    return object;
}
