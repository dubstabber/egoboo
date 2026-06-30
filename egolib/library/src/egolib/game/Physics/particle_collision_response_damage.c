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

/// @file egolib/game/Physics/particle_collision_response_damage.c
/// @brief Character-particle collision response: damage, deflection, and knockback.
/// @details The combat-response cluster dispatched by do_chr_prt_collision: missile
///          deflection and shield blocking (deflect), damage application with perk and
///          vulnerability modifiers (damage), and the knockback impulse (knockback). Shares
///          the chr_prt_collision_data_t block and cross-cluster helpers through
///          particle_collision_response_internal.h. Stays in egolib-library (game layer).

#include "egolib/game/Physics/particle_collision_response_internal.h"
#include "egolib/Logic/Action.hpp"  // getBlockActionColour (single-consumer header; kept TU-local to preserve its one-includer ODR invariant)

//Constants
static constexpr float MAX_KNOCKBACK_VELOCITY = 40.0f;

namespace
{
IScriptable& scriptable(Object& object)
{
    return object;
}

bool usedHeldItemForBlock(const IInventoryHolder& character, const IScriptable& scriptableCharacter, slot_t slot, ObjectRef& usedItem)
{
    usedItem = character.getHeldObject(slot);
    return objectWorld().getObjectHandler().exists(usedItem) && scriptableCharacter.getAILastItemUsed() == usedItem;
}

void publishScoredHit(IScriptable& attacker, ObjectRef targetRef)
{
    attacker.addAIAlertBits(ALERTIF_SCOREDAHIT);
    attacker.setAILastHit(targetRef);
}

bool publishWeaponScoredHit(Object* weapon, ObjectRef targetRef, ObjectRef lastUsedItem)
{
    if (!weapon)
    {
        return false;
    }

    IScriptable& scriptableWeapon = scriptable(*weapon);
    scriptableWeapon.setAILastHit(targetRef);
    if (lastUsedItem != weapon->getObjRef())
    {
        return false;
    }

    scriptableWeapon.addAIAlertBits(ALERTIF_SCOREDAHIT);
    return weapon->getProfile()->getIDSZ(IDSZ_SPECIAL).equals('X', 'W', 'E', 'P') && !weapon->getProfile()->isRangedWeapon();
}

ObjectAttribution objectAttributionFor(ObjectRef objectRef, TEAM_REF team)
{
    const Object* object = objectWorld().getObjectHandler().get(objectRef);
    return object ? object->attribution(team) : ObjectAttribution(team);
}
}

//--------------------------------------------------------------------------------------------
bool do_chr_prt_collision_deflect(chr_prt_collision_data_t& pdata)
{
    bool prt_deflected = false;
    const IDamageable& constDamageableCharacter = damageable(*pdata.pchr);
    const IScriptable& scriptableCharacter = scriptable(*pdata.pchr);

    /// @note ZF@> Simply ignore characters with invictus for now, it causes some strange effects
    if (constDamageableCharacter.isInvincible()) return true;

    //Don't deflect money or particles spawned by the Object itself
    bool prt_wants_deflection = (pdata.pprt->owner_ref != pdata.pchr->getObjRef()) && !pdata.ppip->bump_money && pdata.max_damage > 0;
    if(!prt_wants_deflection) {
        return false;
    }

    // find the "attack direction" of the particle
    Facing direction = idlib::canonicalize(vec_to_facing(pdata.pchr->getPosX() - pdata.pprt->getPosX(), pdata.pchr->getPosY() - pdata.pprt->getPosY()));
    const IPhysical& physicalCharacter = physical(*pdata.pchr);
    direction = physicalCharacter.getFacingZ() - Facing(direction) + ATK_BEHIND;

    // shield block?
    // if the effect is shield piercing, ignore shielding
    bool chr_is_invictus = !pdata.ppip->hasBit(DAMFX_NBLOC) && pdata.pchr->isInvictusDirection(direction);

    // try to deflect the particle
    bool chr_can_deflect = (0 != constDamageableCharacter.getDamageTimer()) && (pdata.max_damage > 0);
    prt_deflected = false;
    pdata.mana_paid = false;
    if(chr_can_deflect)
    {
        MissileTreatment treatment = MissileTreatment_Normal;

        // make a ricochet if the character is invictus
        if(chr_is_invictus) {
            treatment = MissileTreatment_Deflect;
            prt_deflected = true;
        }

        //Check if the target has any enchantment that can deflect missiles
        else {
            for(const std::shared_ptr<Ego::Enchantment> &enchant : pdata.pchr->getActiveEnchants()) {
                if(enchant->isTerminated()) continue;

                //Does this enchant provide special missile protection?
                if(enchant->getMissileTreatment() != MissileTreatment_Normal) {
                    if(enchant->payOwnerMissileTreatmentCost(pdata.pprt->owner_ref)) {
                        pdata.mana_paid = true;
                        treatment = enchant->getMissileTreatment();
                        prt_deflected = true;
                        break;
                    }
                }
            }
        }

        //Was it deflected somehow?
        if (prt_deflected)
        {
            // Treat the missile
            if ( treatment == MissileTreatment_Deflect )
            {
                // Deflect the incoming ray off the normal
                pdata.pprt->phys.avel -= pdata.vdiff_para * 2.0f;

                // the ricochet is not guided
                pdata.pprt->setHoming(false);
            }
            else if ( treatment == MissileTreatment_Reflect )
            {
                // Reflect it back in the direction it came
                pdata.pprt->phys.avel -= pdata.vdiff * 2.0f;

                // Change the owner of the missile
                pdata.pprt->team       = pdata.pchr->getTeamRef();
                pdata.pprt->owner_ref  = pdata.pchr->getObjRef();
            }
        }
    }

    if (chr_is_invictus || prt_deflected)
    {
        bool using_shield = false;

        // If the attack was blocked by a shield, then check if the block caused a knockback
        if ( chr_is_invictus && ACTION_IS_TYPE(pdata.pchr->getCurrentAnimation(), P) )
        {
            // Figure out if we are really using a shield or if it is just a invictus frame
            ObjectRef item = ObjectRef::Invalid;

            // Check right hand for a shield
            if ( !using_shield )
            {
                using_shield = usedHeldItemForBlock(*pdata.pchr, scriptableCharacter, SLOT_RIGHT, item);
            }

            // Check left hand for a shield
            if ( !using_shield )
            {
                using_shield = usedHeldItemForBlock(*pdata.pchr, scriptableCharacter, SLOT_LEFT, item);
            }

            // Now we have the block rating and know the enemy
            if ( objectWorld().getObjectHandler().exists( pdata.pprt->owner_ref ) && using_shield )
            {
                int   total_block_rating;

                Object *pshield   = objectWorld().getObjectHandler().get( item );
                Object *pattacker = objectWorld().getObjectHandler().get( pdata.pprt->owner_ref );

                // use the character block skill plus the base block rating of the shield and adjust for strength
                total_block_rating = pshield->getProfile()->getBaseBlockRating();

                //Defender Perk gives +100% Block Rating
                if(pdata.pchr->hasPerk(Ego::Perks::DEFENDER)) {
                    total_block_rating += 100;
                }

                // -4% per attacker strength
                total_block_rating -= 4 * pattacker->getAttribute(Ego::Attribute::MIGHT);

                // +2% per defender strength
                total_block_rating += 2 * pdata.pchr->getAttribute(Ego::Attribute::MIGHT);

                // Now determine the result of the block
                if ( Random::getPercent() <= total_block_rating )
                {
                    // Defender won, the block holds
                    // Add a small stun to the attacker = 40/50 (0.8 seconds)
                    pattacker->setReloadTimer(pattacker->getReloadTimer() + 40);
                }
                else
                {
                    // Attacker broke the block and batters away the shield
                    // Time to raise shield again = 40/50 (0.8 seconds)
                    pdata.pchr->setReloadTimer(pdata.pchr->getReloadTimer() + 40);
                    audioSystem().playSound(pdata.pchr->getPosition(), audioSystem().getGlobalSound(GSND_SHIELDBLOCK));
                }
            }
        }

        // Tell the players that the attack was somehow deflected
        if (0 == constDamageableCharacter.getDamageTimer())
        {
            activeParticleHandler().spawnDefencePing(pdata.pchr->getObjRef(), pdata.pprt->owner_ref);
            if(using_shield) {
                Ego::Graphics::activeBillboardSystem().makeBillboard(pdata.pchr->getObjRef(), "Blocked!", Ego::Colour4f::white(), Ego::Colour4f(getBlockActionColour(), 1.0f), 3, Ego::Graphics::Billboard::Flags::All);
            }
            else {
                Ego::Graphics::activeBillboardSystem().makeBillboard(pdata.pchr->getObjRef(), "Deflected!", Ego::Colour4f::white(), Ego::Colour4f(getBlockActionColour(), 1.0f), 3, Ego::Graphics::Billboard::Flags::All);
            }
        }
    }

    return prt_deflected;
}

//--------------------------------------------------------------------------------------------
bool do_chr_prt_collision_damage( chr_prt_collision_data_t& pdata )
{
    Object* powner = objectWorld().getObjectHandler().get(pdata.pprt->owner_ref);
    IDamageable& damageableCharacter = damageable(*pdata.pchr);
    IScriptable& scriptableCharacter = scriptable(*pdata.pchr);

    //Get the Profile of the Object that spawned this particle (i.e the weapon itself, not the holder)
    const std::shared_ptr<ObjectProfile> &spawnerProfile = activeProfileSystem().getProfile(pdata.pprt->getSpawnerProfile());
    if(spawnerProfile != nullptr) { //global particles do not have a spawner profile, so this is possible
        // Check all enchants to see if they are removed
        for(const std::shared_ptr<Ego::Enchantment> &enchant : pdata.pchr->getActiveEnchants()) {
            if(enchant->isTerminated()) {
                continue;
            }

            // if nothing can remove it, just go on with your business
            if(enchant->getProfile()->removedByIDSZ == IDSZ2::None) {
                continue;
            }

            // check vs. every IDSZ that could have something to do with cancelling the enchant
            if ( enchant->getProfile()->removedByIDSZ == spawnerProfile->getIDSZ(IDSZ_TYPE) ||
                 enchant->getProfile()->removedByIDSZ == spawnerProfile->getIDSZ(IDSZ_PARENT) ) {
                enchant->requestTerminate();
            }
        }
    }

    // Steal some life.
    if ( pdata.pprt->lifedrain > 0 && pdata.pchr->getLife() > 0)
    {
        // Drain as much as allowed and possible.
        float drain = std::min(pdata.pchr->getLife(), FP8_TO_FLOAT(pdata.pprt->lifedrain));

        // Remove the drain from the character that was hit ...
        pdata.pchr->setLife(pdata.pchr->getLife() - drain);

        // ... and add it to the "caster".
        if (powner != nullptr)
        {
            powner->setLife(powner->getLife() + drain);
        }
    }

    // Steal some mana.
    if ( pdata.pprt->manadrain > 0 && pdata.pchr->getMana() > 0)
    {
        // Drain as much as allowed and possible.
        float drain = std::min(pdata.pchr->getMana(), FP8_TO_FLOAT(pdata.pprt->manadrain));

        // Remove the drain from the character that was hit ...
        pdata.pchr->setMana(pdata.pchr->getMana() - drain);

        // add it to the "caster"
        if (powner != nullptr)
        {
            powner->setMana(powner->getMana() + drain);
        }
    }

    // Do grog
    if (pdata.ppip->grogTime > 0 && pdata.pchr->getProfile()->canBeGrogged())
    {
        scriptableCharacter.addAIAlertBits(ALERTIF_CONFUSED);
        pdata.pchr->setGrogTimer(std::max(static_cast<unsigned>(pdata.pchr->getGrogTimer()), pdata.ppip->grogTime));

        Ego::Graphics::activeBillboardSystem().makeBillboard(pdata.pchr->getObjRef(), "Groggy!", Ego::Colour4f::white(), Ego::Colour4f::green(), 3, Ego::Graphics::Billboard::Flags::All);
    }

    // Do daze
    if (pdata.ppip->dazeTime > 0 && pdata.pchr->getProfile()->canBeDazed())
    {
        scriptableCharacter.addAIAlertBits(ALERTIF_CONFUSED);
        pdata.pchr->setDazeTimer(std::max(static_cast<unsigned>(pdata.pchr->getDazeTimer()), pdata.ppip->dazeTime));

        Ego::Graphics::activeBillboardSystem().makeBillboard(pdata.pchr->getObjRef(), "Dazed!", Ego::Colour4f::white(), Ego::Colour4f::yellow(), 3, Ego::Graphics::Billboard::Flags::All);
    }

    //---- Damage the character, if necessary
    if ( 0 != std::abs( pdata.pprt->damage.base ) + std::abs( pdata.pprt->damage.rand ) )
    {
        //bool prt_needs_impact = pdata->ppip->rotatetoface || pdata->pprt->isAttached();
        //if(spawnerProfile != nullptr) {
        //    if ( spawnerProfile->isRangedWeapon() ) prt_needs_impact = true;
        //}

        // DAMFX_ARRO means that it only does damage to the one it's attached to
        if (!pdata.ppip->hasBit(DAMFX_ARRO) /*&& (!prt_needs_impact || pdata->is_impact)*/ )
        {
            //Damage adjusted for attributes and weaknesses
            IPair modifiedDamage = pdata.pprt->damage;

            FACING_T direction = FACING_T(vec_to_facing( pdata.pprt->getVelocity().x() , pdata.pprt->getVelocity().y() ));
            const IPhysical& physicalCharacter = physical(*pdata.pchr);
            direction = FACING_T(physicalCharacter.getFacingZ() - Facing(direction) + ATK_BEHIND);

            // These things only apply if the particle has an owner
            if ( nullptr != powner )
            {
                IScriptable& scriptableOwner = scriptable(*powner);

                //Check special perk effects
                if(spawnerProfile != nullptr)
                {
                    // Check Crack Shot perk which applies 3 second Daze with fireweapons
                    if(pdata.pchr->getProfile()->canBeDazed() && powner->hasPerk(Ego::Perks::CRACKSHOT) && DamageType_isPhysical(pdata.pprt->damagetype))
                    {
                        //Is the particle spawned by a gun?
                        if(spawnerProfile->isRangedWeapon() && spawnerProfile->getIDSZ(IDSZ_SKILL).equals('T','E','C','H')) {
                            scriptableCharacter.addAIAlertBits(ALERTIF_CONFUSED);
                            pdata.pchr->setDazeTimer(pdata.pchr->getDazeTimer() + 3);

                            Ego::Graphics::activeBillboardSystem().makeBillboard(powner->getObjRef(), "Crackshot!", Ego::Colour4f::white(), Ego::Colour4f::blue(), 3, Ego::Graphics::Billboard::Flags::All);
                        }
                    }

                    //Brutal Strike has chance to inflict 2 second Grog with melee CRUSH attacks
                    if(pdata.pchr->getProfile()->canBeGrogged() && powner->hasPerk(Ego::Perks::BRUTAL_STRIKE) && spawnerProfile->isMeleeWeapon() && pdata.pprt->damagetype == DAMAGE_CRUSH) {
                        scriptableCharacter.addAIAlertBits(ALERTIF_CONFUSED);
                        pdata.pchr->setGrogTimer(pdata.pchr->getGrogTimer() + 2);

                        Ego::Graphics::activeBillboardSystem().makeBillboard(powner->getObjRef(), "Brutal Strike!", Ego::Colour4f::white(), Ego::Colour4f::red(), 3, Ego::Graphics::Billboard::Flags::All);
                        audioSystem().playSound(powner->getPosition(), audioSystem().getGlobalSound(GSND_CRITICAL_HIT));
                    }
                }

                // Apply intellect bonus damage for particles with the [IDAM] expansions (Low ability gives penality)
                // +2% bonus for every point of intellect. Below 14 gives -2% instead!
                if ( pdata.ppip->_intellectDamageBonus )
                {
                    float percent = ( powner->getAttribute(Ego::Attribute::INTELLECT) - 14.0f ) * 2.0f;

                    //Sorcery Perk increases spell damage by 10%
                    if(powner->hasPerk(Ego::Perks::SORCERY)) {
                        percent += 10.0f;
                    }

                    //Dark Arts Master perk gives evil damage +20%
                    if(pdata.pprt->damagetype == DAMAGE_EVIL && powner->hasPerk(Ego::Perks::DARK_ARTS_MASTERY)) {
                        percent += 20.0f;
                    }

                    percent /= 100.0f;
                    modifiedDamage.base *= 1.00f + percent;
                    modifiedDamage.rand *= 1.00f + percent;

                    //Disintegrate perk deals +100 ZAP damage at 0.025% chance per Intellect!
                    if(pdata.pprt->damagetype == DAMAGE_ZAP && powner->hasPerk(Ego::Perks::DISINTEGRATE)) {
                        if(Random::nextFloat()*100.0f <= powner->getAttribute(Ego::Attribute::INTELLECT) * 0.025f) {
                            modifiedDamage.base += FLOAT_TO_FP8(100.0f);
                            Ego::Graphics::activeBillboardSystem().makeBillboard(pdata.pchr->getObjRef(), "Disintegrated!", Ego::Colour4f::white(), Ego::Colour4f::purple(), 6, Ego::Graphics::Billboard::Flags::All);

                            //Disintegrate effect
                            activeParticleHandler().spawnGlobalParticle(pdata.pchr->getPosition(), ATK_FRONT, LocalParticleProfileRef(PIP_DISINTEGRATE_START), 0);
                        }
                    }
                }

                // Notify the attacker of a scored hit
                publishScoredHit(scriptableOwner, pdata.pchr->getObjRef());

                // Tell the weapons who the attacker hit last
                bool meleeAttack = false;
                Object* leftHanditem = heldItem(*powner, SLOT_RIGHT);
                meleeAttack = publishWeaponScoredHit(leftHanditem, pdata.pchr->getObjRef(), scriptableOwner.getAILastItemUsed()) || meleeAttack;

                Object* rightHandItem = heldItem(*powner, SLOT_RIGHT);
                meleeAttack = publishWeaponScoredHit(rightHandItem, pdata.pchr->getObjRef(), scriptableOwner.getAILastItemUsed()) || meleeAttack;

                //Unarmed attack?
                if (scriptableOwner.getAILastItemUsed() == powner->getObjRef()) {
                    meleeAttack = true;
                }

                //Grim Reaper (5% chance to trigger +50 EVIL damage)
                if(spawnerProfile != nullptr && powner->hasPerk(Ego::Perks::GRIM_REAPER)) {

                    //Is it a Scythe?
                    if(spawnerProfile->getIDSZ(IDSZ_TYPE).equals('S','C','Y','T') && Random::getPercent() <= 5) {

                        //Make sure they can be damaged by EVIL first
                        if(pdata.pchr->getAttribute(Ego::Attribute::EVIL_MODIFIER) == NONE) {
                            IPair grimReaperDamage;
                            grimReaperDamage.base = FLOAT_TO_FP8(50.0f);
                            grimReaperDamage.rand = 0.0f;
                            damageableCharacter.damage(Facing(direction), grimReaperDamage, DAMAGE_EVIL,
                                                       objectAttributionFor(pdata.pprt->owner_ref, pdata.pprt->team), false, true, false);
                            Ego::Graphics::activeBillboardSystem().makeBillboard(powner->getObjRef(), "Grim Reaper!", Ego::Colour4f::white(), Ego::Colour4f::red(), 3, Ego::Graphics::Billboard::Flags::All);
                            audioSystem().playSound(powner->getPosition(), audioSystem().getGlobalSound(GSND_CRITICAL_HIT));
                        }
                    }
                }

                //Deadly Strike perk (1% chance per character level to trigger vs non undead)
                if(meleeAttack && !pdata.pchr->getProfile()->getIDSZ(IDSZ_PARENT).equals('U','N','D','E'))
                {
                    if(powner->hasPerk(Ego::Perks::DEADLY_STRIKE) && powner->getExperienceLevel() >= Random::getPercent() && DamageType_isPhysical(pdata.pprt->damagetype)){
                        //Gain +0.25 damage per Agility
                        modifiedDamage.base += FLOAT_TO_FP8(powner->getAttribute(Ego::Attribute::AGILITY) * 0.25f);
                        Ego::Graphics::activeBillboardSystem().makeBillboard(powner->getObjRef(), "Deadly Strike", Ego::Colour4f::white(), Ego::Colour4f::blue(), 3, Ego::Graphics::Billboard::Flags::All);
                        audioSystem().playSound(powner->getPosition(), audioSystem().getGlobalSound(GSND_CRITICAL_HIT));
                    }
                }
            }

            // handle vulnerabilities, double the damage
            if(spawnerProfile != nullptr && pdata.pchr->getProfile()->getIDSZ(IDSZ_VULNERABILITY) != IDSZ2::None) {
                if (pdata.pchr->getProfile()->getIDSZ(IDSZ_VULNERABILITY) == spawnerProfile->getIDSZ(IDSZ_TYPE) ||
                    pdata.pchr->getProfile()->getIDSZ(IDSZ_VULNERABILITY) == spawnerProfile->getIDSZ(IDSZ_PARENT))
                {
                    // Double the damage
                    modifiedDamage.base = ( modifiedDamage.base << 1 );
                    modifiedDamage.rand = ( modifiedDamage.rand << 1 ) | 1;

                    scriptableCharacter.addAIAlertBits(ALERTIF_HITVULNERABLE);

                    // Initialize for the billboard
                    Ego::Graphics::activeBillboardSystem().makeBillboard(pdata.pchr->getObjRef(), "Super Effective!", Ego::Colour4f::white(), Ego::Colour4f::yellow(), 3, Ego::Graphics::Billboard::Flags::All);
                }
            }

            //Is it a critical hit?
            if(powner != nullptr && powner->hasPerk(Ego::Perks::CRITICAL_HIT) && DamageType_isPhysical(pdata.pprt->damagetype)) {
                //0.5% chance per agility to deal max damage
                float critChance = powner->getAttribute(Ego::Attribute::AGILITY)*0.5f;

                //Lucky Perk increases critical hit chance by 10%!
                if(powner->hasPerk(Ego::Perks::LUCKY)) {
                    critChance += 10.0f;
                }

                if(Random::getPercent() <= critChance) {
                    modifiedDamage.base += modifiedDamage.rand;
                    modifiedDamage.rand = 0;
                    Ego::Graphics::activeBillboardSystem().makeBillboard(powner->getObjRef(), "Critical Hit!", Ego::Colour4f::white(), Ego::Colour4f::red(), 3, Ego::Graphics::Billboard::Flags::All);
                    audioSystem().playSound(powner->getPosition(), audioSystem().getGlobalSound(GSND_CRITICAL_HIT));
                }
            }

            // Damage the character
            pdata.actual_damage = damageableCharacter.damage(Facing(direction), modifiedDamage, pdata.pprt->damagetype,
                objectAttributionFor(pdata.pprt->owner_ref, pdata.pprt->team), pdata.ppip->hasBit(DAMFX_ARMO), !pdata.ppip->hasBit(DAMFX_TIME), false);
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------
void do_chr_prt_collision_knockback(chr_prt_collision_data_t &pdata)
{
    /**
    * @brief
    *   ZF> New particle collision knockback algorithm (07.08.2015)
    **/

    //No knocback applicable?
    if(idlib::manhattan_norm(pdata.pprt->getVelocity()) == 0.0f) {
        return;
    }

    //Target immune to knockback?
    if(pdata.pchr->phys.bumpdampen == 0.0f || Ego::Physics::CHR_INFINITE_WEIGHT == pdata.pchr->phys.weight) {
        return;
    }

    //Is particle allowed to cause knockback?
    if(!pdata.ppip->allowpush) {
        return;
    }

    //If the particle was magically deflected, then there is no knockback
    if (pdata.mana_paid) {
        return;
    }

    float knockbackFactor = 1.0f;

    //If we are attached to a Object then the attacker's Might can increase knockback
    const Object* attachedObject = objectWorld().getObjectHandler().get(pdata.pprt->getAttachedObjectID());
    const Object* attacker = attachedObject;
    if (attacker)
    {
        //If we are actually a weapon, use the weapon holder's strength
        if(attacker->isBeingHeld()) {
            attacker = objectWorld().getObjectHandler().get(attacker->getHolderRef());
        }

        const float attackerMight = attacker->getAttribute(Ego::Attribute::MIGHT) - 10.0f;

        //Add 2% knockback per point of Might above 10
        if(attackerMight >= 0.0f) {
            knockbackFactor += attackerMight * 0.02f;
        }

        //Reduce knockback by 10% per point of Might below 10
        else {
            knockbackFactor += attackerMight * 0.1f;
        }

        //Telekinetic Staff perk can give +500% knockback
        const Object* powner = objectWorld().getObjectHandler().get(pdata.pprt->owner_ref);
        if(powner != nullptr && powner->hasPerk(Ego::Perks::TELEKINETIC_STAFF) &&
            attachedObject->getProfile()->getIDSZ(IDSZ_PARENT).equals('S','T','A','F')) {

            //+3% chance per owner Intellect and -1% per target Might
            float chance = attacker->getAttribute(Ego::Attribute::INTELLECT) * 0.03f - pdata.pchr->getAttribute(Ego::Attribute::MIGHT)*0.01f;
            if(Random::nextFloat() <= chance) {
                knockbackFactor += 5.0f;
                Ego::Graphics::activeBillboardSystem().makeBillboard(attacker->getObjRef(), "Telekinetic Staff!", Ego::Colour4f::white(), Ego::Colour4f::purple(), 2, Ego::Graphics::Billboard::Flags::All);
            }
        }
    }

    //Adjust knockback based on relative mass between particle and target
    if(pdata.pchr->phys.bumpdampen != 0.0f && Ego::Physics::CHR_INFINITE_WEIGHT != pdata.pchr->phys.weight) {
        float particleMass = 0.0f;
        float targetMass = pdata.pchr->getMass();
        get_prt_mass(pdata.pprt.get(), pdata.pchr, &particleMass);
        if(targetMass >= 0.0f) {
            knockbackFactor *= Ego::Math::constrain(particleMass / targetMass, 0.0f, 1.0f);
        }
    }

    //Amount of knockback is affected by damage type
    switch(pdata.pprt->damagetype)
    {
        // very blunt type of attack, the maximum effect
        case DAMAGE_CRUSH:
            knockbackFactor *= 1.0f;
        break;

        // very focussed type of attack, the minimum effect
        case DAMAGE_POKE:
            knockbackFactor *= 0.5f;
        break;

        // all other damage types are in the middle
        default:
            knockbackFactor *= idlib::inv_sqrt_two<float>();
        break;
    }

    //Apply knockback to the victim (limit between 0% and 300% knockback)
    Ego::Vector3f knockbackVelocity = pdata.pprt->getVelocity() * Ego::Math::constrain(knockbackFactor, 0.0f, 3.0f);

    //static constexpr float DEFAULT_KNOCKBACK_VELOCITY = 10.0f;
    //knockbackVelocity[kX] = std::cos(pdata.pprt->vel[kX]) * DEFAULT_KNOCKBACK_VELOCITY;
    //knockbackVelocity[kY] = std::sin(pdata.pprt->vel[kY]) * DEFAULT_KNOCKBACK_VELOCITY;
    //knockbackVelocity[kZ] = DEFAULT_KNOCKBACK_VELOCITY / 2;
    //knockbackVelocity *= Ego::Math::constrain(knockbackFactor, 0.0f, 3.0f);

    //Limit total knockback velocity to MAX_KNOCKBACK_VELOCITY
    const float magnitudeVelocity = idlib::euclidean_norm(knockbackVelocity);
    if(magnitudeVelocity > MAX_KNOCKBACK_VELOCITY) {
        knockbackVelocity *= MAX_KNOCKBACK_VELOCITY / magnitudeVelocity;
    }

    //Apply knockback
    pdata.pchr->phys.avel += knockbackVelocity;
}
