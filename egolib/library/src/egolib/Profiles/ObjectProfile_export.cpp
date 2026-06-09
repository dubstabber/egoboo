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

/// @file egolib/Profiles/ObjectProfile_export.cpp
/// @brief ObjectProfile export and serialization helpers.

#include "egolib/Profiles/ObjectProfile_internal.h"
#include "egolib/Logic/IPerkHandler.hpp"              // Ego::Perks::activePerkHandler
#include "egolib/game/Physics/PhysicalConstants.hpp"  // CHR_INFINITE_WEIGHT
#include "egolib/fileutil.h"

namespace
{
struct ObjectProfileExportServices
{
    Ego::Perks::IPerkHandler& perkHandler;
};

ObjectProfileExportServices objectProfileExportServices()
{
    return {Ego::Perks::activePerkHandler()};
}

std::string exportPerkName(const Ego::Perks::Perk& perk)
{
    std::string name = perk.getName();
    std::replace(name.begin(), name.end(), ' ', '_');
    return name;
}
} // namespace

bool ObjectProfile::exportCharacterToFile(const std::string &filePath, const Object *character)
{
    if (nullptr == character) {
        return false;
    }

    // Open the file
    vfs_FILE *fileWrite = vfs_openWrite(filePath);
    if (!fileWrite) {
        return false;
    }

    // open the template file
    vfs_FILE *fileTemp = template_open_vfs( "mp_data/templates/data.txt" );

    //did we find a template file?
    if (!fileTemp)
    {
        vfs_close( fileWrite );
        return false;
    }

    const std::shared_ptr<ObjectProfile> &profile = character->getProfile();
    const auto services = objectProfileExportServices();

    // Real general data
    template_put_int( fileTemp, fileWrite, -1 );     // -1 signals a flexible load thing
    template_put_string_under( fileTemp, fileWrite, profile->_className.c_str() );
    template_put_bool( fileTemp, fileWrite, profile->_uniformLit );
    template_put_int( fileTemp, fileWrite, character->getAmmoMax() );     //Note: overridden by chr
    template_put_int( fileTemp, fileWrite, character->getAmmo() );        //Note: overridden by chr
    template_put_gender( fileTemp, fileWrite, character->getGender() );   //Note: overridden by chr

     //Attributes (TODO: can be easily converted into a for loop if order does not matter)
    template_put_int( fileTemp, fileWrite, character->getBaseAttribute(Ego::Attribute::LIFE_BARCOLOR) );              //Note: overriden by chr
    template_put_int( fileTemp, fileWrite, character->getBaseAttribute(Ego::Attribute::MANA_BARCOLOR) );              //Note: overriden by chr
    template_put_float( fileTemp, fileWrite, character->getBaseAttribute(Ego::Attribute::MAX_LIFE)*0.5f ); //Note: overriden by chr (ZF> Halved hp because it is doubled on parse)
    template_put_range( fileTemp, fileWrite, profile->getAttributeGain(Ego::Attribute::MAX_LIFE));
    template_put_float( fileTemp, fileWrite, character->getBaseAttribute(Ego::Attribute::MAX_MANA) ); //Note: overriden by chr
    template_put_range( fileTemp, fileWrite, profile->getAttributeGain(Ego::Attribute::MAX_MANA));
    template_put_float( fileTemp, fileWrite, character->getBaseAttribute(Ego::Attribute::MANA_REGEN)); //Note: overriden by chr
    template_put_range( fileTemp, fileWrite, profile->getAttributeGain(Ego::Attribute::MANA_REGEN));
    template_put_float( fileTemp, fileWrite, character->getBaseAttribute(Ego::Attribute::SPELL_POWER) ); //Note: overriden by chr
    template_put_range( fileTemp, fileWrite, profile->getAttributeGain(Ego::Attribute::SPELL_POWER));
    template_put_float( fileTemp, fileWrite, character->getBaseAttribute(Ego::Attribute::MIGHT) ); //Note: overriden by chr
    template_put_range( fileTemp, fileWrite, profile->getAttributeGain(Ego::Attribute::MIGHT));
    template_put_float( fileTemp, fileWrite, 0.0f); //Note: deprecated
    template_put_float( fileTemp, fileWrite, 0.0f); //Note: deprecated
    template_put_float( fileTemp, fileWrite, character->getBaseAttribute(Ego::Attribute::INTELLECT) ); //Note: overriden by chr
    template_put_range( fileTemp, fileWrite, profile->getAttributeGain(Ego::Attribute::INTELLECT));
    template_put_float( fileTemp, fileWrite, character->getBaseAttribute(Ego::Attribute::AGILITY) ); //Note: overriden by chr
    template_put_range( fileTemp, fileWrite, profile->getAttributeGain(Ego::Attribute::AGILITY));

    // More physical attributes
    template_put_float( fileTemp, fileWrite, character->getTargetFat() );                   //Note: overriden by chr
    template_put_float( fileTemp, fileWrite, profile->_sizeGainPerLevel );
    template_put_int( fileTemp, fileWrite, profile->_shadowSize );
    template_put_int( fileTemp, fileWrite, profile->_bumpSize );
    template_put_int( fileTemp, fileWrite, profile->_bumpHeight );
    template_put_float( fileTemp, fileWrite, character->phys.bumpdampen );           //Note: overriden by chr

    //Weight
    if (Ego::Physics::CHR_INFINITE_WEIGHT == character->phys.weight || 0.0f == character->getFat())
    {
        template_put_int( fileTemp, fileWrite, CAP_INFINITE_WEIGHT );           //Note: overriden by chr
    }
    else
    {
        uint32_t weight = character->phys.weight / character->getFat() / character->getFat() / character->getFat();
        template_put_int( fileTemp, fileWrite, std::min(weight, static_cast<uint32_t>(CAP_MAX_WEIGHT)) );   //Note: overriden by chr
    }

    template_put_float( fileTemp, fileWrite, character->getBaseAttribute(Ego::Attribute::JUMP_POWER) );    //Note: overriden by chr
    template_put_int( fileTemp, fileWrite, character->getBaseAttribute(Ego::Attribute::NUMBER_OF_JUMPS) ); //Note: overriden by chr
    template_put_float( fileTemp, fileWrite, profile->_animationSpeedSneak);
    template_put_float( fileTemp, fileWrite, profile->_animationSpeedWalk);
    template_put_float( fileTemp, fileWrite, profile->_animationSpeedRun);
    template_put_int( fileTemp, fileWrite, character->getBaseAttribute(Ego::Attribute::FLY_TO_HEIGHT) ); //Note: overriden by chr
    template_put_int(fileTemp, fileWrite, profile->_flashAND);
    template_put_int(fileTemp, fileWrite, profile->_alpha);
    template_put_int(fileTemp, fileWrite, profile->_light);
    template_put_bool(fileTemp, fileWrite, profile->_transferBlending);
    template_put_int( fileTemp, fileWrite, profile->_sheen );
    template_put_bool( fileTemp, fileWrite, profile->_phongMapping );
    template_put_float( fileTemp, fileWrite, FFFF_TO_FLOAT( profile->_textureMovementRateX ) );
    template_put_float( fileTemp, fileWrite, FFFF_TO_FLOAT( profile->_textureMovementRateY ) );
    template_put_bool(fileTemp, fileWrite, profile->_stickyButt);

    // Invulnerability data
    template_put_bool( fileTemp, fileWrite, character->isInvincible() );
    template_put_int( fileTemp, fileWrite, profile->nframefacing );
    template_put_int( fileTemp, fileWrite, profile->nframeangle );
    template_put_int( fileTemp, fileWrite, profile->iframefacing );
    template_put_int( fileTemp, fileWrite, profile->iframeangle );

    // Skin defenses (TODO: add support for more than 4)
    template_put_int( fileTemp, fileWrite, profile->getSkinInfo(0).defence );
    template_put_int( fileTemp, fileWrite, profile->getSkinInfo(1).defence );
    template_put_int( fileTemp, fileWrite, profile->getSkinInfo(2).defence );
    template_put_int( fileTemp, fileWrite, profile->getSkinInfo(3).defence );

    for (size_t damagetype = 0; damagetype < DAMAGE_COUNT; damagetype++ )
    {
        //TODO: add support for more than 4
        for(int i = 0; i < 4; ++i) {
            //ZF> Another small hack to prevent 0 damage resist to be parsed as 0 damage shift
            float damageResist = profile->getSkinInfo(i).damageResistance[damagetype];
            template_put_float( fileTemp, fileWrite, damageResist == 0.0f ? 1 : damageResist);
        }
    }

    for (size_t damagetype = 0; damagetype < DAMAGE_COUNT; damagetype++ )
    {
        char code;

        for (size_t skin = 0; skin < SKINS_PEROBJECT_MAX; skin++)
        {
            if ( HAS_SOME_BITS( profile->getSkinInfo(skin).damageModifier[damagetype], DAMAGEMANA ) )
            {
                code = 'M';
            }
            else if ( HAS_SOME_BITS( profile->getSkinInfo(skin).damageModifier[damagetype], DAMAGECHARGE ) )
            {
                code = 'C';
            }
            else if ( HAS_SOME_BITS( profile->getSkinInfo(skin).damageModifier[damagetype], DAMAGEINVERT ) )
            {
                code = 'T';
            }
            else if ( HAS_SOME_BITS( profile->getSkinInfo(skin).damageModifier[damagetype], DAMAGEINVICTUS ) )
            {
                code = 'I';
            }
            else
            {
                code = 'F';
            }

            template_put_char( fileTemp, fileWrite, code );
        }
    }

    template_put_float( fileTemp, fileWrite, profile->getSkinInfo(0).maxAccel*80 );
    template_put_float( fileTemp, fileWrite, profile->getSkinInfo(1).maxAccel*80 );
    template_put_float( fileTemp, fileWrite, profile->getSkinInfo(2).maxAccel*80 );
    template_put_float( fileTemp, fileWrite, profile->getSkinInfo(3).maxAccel*80 );

    // Experience and level data
    template_put_int( fileTemp, fileWrite, profile->_experienceForLevel[1] );
    template_put_int( fileTemp, fileWrite, profile->_experienceForLevel[2] );
    template_put_int( fileTemp, fileWrite, profile->_experienceForLevel[3] );
    template_put_int( fileTemp, fileWrite, profile->_experienceForLevel[4] );
    template_put_int( fileTemp, fileWrite, profile->_experienceForLevel[5] );
    template_put_float( fileTemp, fileWrite, character->getExperience() );    //Note overriden by chr
    template_put_int( fileTemp, fileWrite, profile->_experienceWorth );
    template_put_float( fileTemp, fileWrite, profile->_experienceExchange );
    for(size_t i = 0; i < profile->_experienceRate.size(); ++i) {
        template_put_float( fileTemp, fileWrite, profile->_experienceRate[i] );
    }

    // IDSZ identification tags
    template_put_idsz( fileTemp, fileWrite, profile->_idsz[IDSZ_PARENT] );
    template_put_idsz( fileTemp, fileWrite, profile->_idsz[IDSZ_TYPE] );
    template_put_idsz( fileTemp, fileWrite, profile->_idsz[IDSZ_SKILL] );
    template_put_idsz( fileTemp, fileWrite, profile->_idsz[IDSZ_SPECIAL] );
    template_put_idsz( fileTemp, fileWrite, profile->_idsz[IDSZ_HATE] );
    template_put_idsz( fileTemp, fileWrite, profile->_idsz[IDSZ_VULNERABILITY] );

    // Item and damage flags
    template_put_bool( fileTemp, fileWrite, character->isItem());  //Note overriden by chr
    template_put_bool( fileTemp, fileWrite, profile->_isMount );
    template_put_bool( fileTemp, fileWrite, profile->_isStackable );
    template_put_bool( fileTemp, fileWrite, character->isNameKnown() || character->isAmmoKnown()); // make sure that identified items are saved as identified );
    template_put_bool( fileTemp, fileWrite, profile->_usageIsKnown );
    template_put_bool( fileTemp, fileWrite, profile->_canCarryToNextModule );
    template_put_bool( fileTemp, fileWrite, profile->_needSkillIDToUse );
    template_put_bool( fileTemp, fileWrite, character->isPlatform() );       //Note overriden by chr
    template_put_bool(fileTemp, fileWrite, profile->_canGrabMoney);
    template_put_bool(fileTemp, fileWrite, profile->_canOpenStuff);

    // Other item and damage stuff
    template_put_damage_type( fileTemp, fileWrite, character->getDamageTargetType() ); //Note overriden by chr
    template_put_action( fileTemp, fileWrite, profile->_weaponAction );

    // Particle attachments
    template_put_int( fileTemp, fileWrite, profile->_attachedParticleAmount );
    template_put_damage_type(fileTemp, fileWrite, character->getReaffirmDamageType());
    template_put_local_particle_profile_ref( fileTemp, fileWrite, profile->_attachedParticle );

    // Character hands
    template_put_bool( fileTemp, fileWrite, profile->_slotsValid[SLOT_LEFT] );
    template_put_bool( fileTemp, fileWrite, profile->_slotsValid[SLOT_RIGHT] );

    // Particle spawning on attack
    template_put_bool( fileTemp, fileWrite, 0 != profile->_attachAttackParticleToWeapon );
    template_put_local_particle_profile_ref( fileTemp, fileWrite, profile->_attackParticle );

    // Particle spawning for GoPoof
    template_put_int( fileTemp, fileWrite, profile->_goPoofParticleAmount );
    template_put_int( fileTemp, fileWrite, profile->_goPoofParticleFacingAdd );
    template_put_local_particle_profile_ref(fileTemp, fileWrite, profile->_goPoofParticle);

    // Particle spawning for blud
    template_put_bool( fileTemp, fileWrite, 0 != profile->_bludValid );
    template_put_local_particle_profile_ref( fileTemp, fileWrite, profile->_bludParticle );

    // Extra stuff
    template_put_bool(fileTemp, fileWrite, character->getBaseAttribute(Ego::Attribute::WALK_ON_WATER) > 0); //Note: overriden by chr
    template_put_float( fileTemp, fileWrite, character->phys.dampen );   //Note: overriden by chr

    // More stuff
    template_put_float(fileTemp, fileWrite, 0); //unused
    template_put_float(fileTemp, fileWrite, profile->_useManaCost);
    template_put_float(fileTemp, fileWrite, character->getBaseAttribute(Ego::Attribute::LIFE_REGEN) * 256.0f);   //Note: overridden by chr
    template_put_int( fileTemp, fileWrite, character->getStoppedByMask() );   //Note: overridden by chr
    template_put_string_under( fileTemp, fileWrite, profile->getSkinInfo(0).name.c_str() );
    template_put_string_under( fileTemp, fileWrite, profile->getSkinInfo(1).name.c_str() );
    template_put_string_under( fileTemp, fileWrite, profile->getSkinInfo(2).name.c_str() );
    template_put_string_under( fileTemp, fileWrite, profile->getSkinInfo(3).name.c_str() );
    template_put_int( fileTemp, fileWrite, profile->getSkinInfo(0).cost );
    template_put_int( fileTemp, fileWrite, profile->getSkinInfo(1).cost );
    template_put_int( fileTemp, fileWrite, profile->getSkinInfo(2).cost );
    template_put_int( fileTemp, fileWrite, profile->getSkinInfo(3).cost );
    template_put_float( fileTemp, fileWrite, 0); //unused

    // Another memory lapse
    template_put_bool( fileTemp, fileWrite, !profile->_riderCanAttack );
    template_put_bool( fileTemp, fileWrite, profile->_canBeDazed );
    template_put_bool( fileTemp, fileWrite, profile->_canBeGrogged );
    template_put_int( fileTemp, fileWrite, 0 );
    template_put_int( fileTemp, fileWrite, 0 );
    template_put_bool( fileTemp, fileWrite, character->getBaseAttribute(Ego::Attribute::SEE_INVISIBLE) > 0 ); //Note: Overridden by chr
    template_put_int( fileTemp, fileWrite, character->isKursed() ? 100 : 0 );  //Note: overridden by chr
    template_put_int( fileTemp, fileWrite, profile->_footFallSound);
    template_put_int( fileTemp, fileWrite, profile->_jumpSound);

    vfs_flush( fileWrite );

    // copy the template file to the next free output section
    template_seek_free( fileTemp, fileWrite );

    // Expansions
    for(int i = 0; i < 4; ++i) {
        if (profile->getSkinInfo(i).dressy) {
            vfs_put_expansion(fileWrite, "", IDSZ2( 'D', 'R', 'E', 'S' ), i);
        }
    }

    if ( profile->_resistBumpSpawn )
        vfs_put_expansion( fileWrite, "", IDSZ2( 'S', 'T', 'U', 'K' ), 0 );

    if ( profile->_isBigItem )
        vfs_put_expansion( fileWrite, "", IDSZ2( 'P', 'A', 'C', 'K' ), 0 );

    if ( !profile->_hasReflection )
        vfs_put_expansion( fileWrite, "", IDSZ2( 'V', 'A', 'M', 'P' ), 1 );

    if ( profile->_alwaysDraw )
        vfs_put_expansion( fileWrite, "", IDSZ2( 'D', 'R', 'A', 'W' ), 1 );

    if ( profile->_isRanged )
        vfs_put_expansion( fileWrite, "", IDSZ2( 'R', 'A', 'N', 'G' ), 1 );

    if ( profile->_hideState != NOHIDE )
        vfs_put_expansion( fileWrite, "", IDSZ2( 'H', 'I', 'D', 'E' ), profile->_hideState );

    if ( profile->_isEquipment )
        vfs_put_expansion( fileWrite, "", IDSZ2( 'E', 'Q', 'U', 'I' ), 1 );

    if ( profile->_bumpSizeBig >= profile->_bumpSize * 2 )
        vfs_put_expansion( fileWrite, "", IDSZ2( 'S', 'Q', 'U', 'A' ), 1 );

    if ( profile->_drawIcon != profile->_usageIsKnown )
        vfs_put_expansion( fileWrite, "", IDSZ2( 'I', 'C', 'O', 'N' ), character->shouldDrawIcon() ); //note: overridden by chr

    if ( profile->_forceShadow )
        vfs_put_expansion( fileWrite, "", IDSZ2( 'S', 'H', 'A', 'D' ), 1 );

    if ( profile->_causesRipples == profile->_isItem )
        vfs_put_expansion( fileWrite, "", IDSZ2( 'R', 'I', 'P', 'P' ), profile->_causesRipples );

    if ( -1 != profile->_isValuable )
        vfs_put_expansion( fileWrite, "", IDSZ2( 'V', 'A', 'L', 'U' ), profile->_isValuable );

    if ( profile->_spellEffectType != NO_SKIN_OVERRIDE )
        vfs_put_expansion( fileWrite, "", IDSZ2( 'B', 'O', 'O', 'K' ), profile->_spellEffectType );

    if ( profile->_attackFast )
        vfs_put_expansion( fileWrite, "", IDSZ2( 'F', 'A', 'S', 'T' ), profile->_attackFast );

    if ( profile->_strengthBonus > 0 )
        vfs_put_expansion_float( fileWrite, "", IDSZ2( 'S', 'T', 'R', 'D' ), profile->_strengthBonus );

    if ( profile->_intelligenceBonus > 0 )
        vfs_put_expansion_float( fileWrite, "", IDSZ2( 'I', 'N', 'T', 'D' ), profile->_intelligenceBonus );

    if ( profile->_dexterityBonus > 0 )
        vfs_put_expansion_float( fileWrite, "", IDSZ2( 'D', 'E', 'X', 'D' ), profile->_dexterityBonus );

    if ( profile->_bumpOverrideSize || profile->_bumpOverrideSizeBig ||  profile->_bumpOverrideHeight )
    {
        std::string sz_tmp;

        if ( profile->_bumpOverrideSize ) sz_tmp += "S";
        if ( profile->_bumpOverrideSizeBig ) sz_tmp += "B";
        if ( profile->_bumpOverrideHeight ) sz_tmp += "H";
        if ( profile->_dontCullBackfaces ) sz_tmp += "C";

        if ( sz_tmp != "" )
        {
            vfs_put_expansion_string( fileWrite, "", IDSZ2( 'M', 'O', 'D', 'L' ), sz_tmp.c_str() );
        }
    }

    // Basic stuff that is always written
    vfs_put_expansion(fileWrite, "", IDSZ2('G', 'O', 'L', 'D'), character->getMoney());
    vfs_put_expansion(fileWrite, "", IDSZ2('P', 'L', 'A', 'T'), character->canUsePlatforms());
    vfs_put_expansion(fileWrite, "", IDSZ2('S', 'K', 'I', 'N'), character->getSkin());
    vfs_put_expansion(fileWrite, "", IDSZ2('C', 'O', 'N', 'T'), character->getAIContent());
    vfs_put_expansion(fileWrite, "", IDSZ2('S', 'T', 'A', 'T'), character->getAIStateValue());
    vfs_put_expansion(fileWrite, "", IDSZ2('L', 'E', 'V', 'L'), character->getExperienceLevelIndex());
    vfs_put_expansion(fileWrite, "", IDSZ2('S', 'E', 'E', 'D'), character->getLevelUpSeed());
    vfs_put_expansion_float(fileWrite, "", IDSZ2('L', 'I', 'F', 'E'), character->getLife());
    vfs_put_expansion_float(fileWrite, "", IDSZ2('M', 'A', 'N', 'A'), character->getMana());

    // write down any perks that have been mastered
    for(size_t i = 0; i < Ego::Perks::NR_OF_PERKS; ++i) {
        const Ego::Perks::Perk& perk = services.perkHandler.getPerk(static_cast<Ego::Perks::PerkID>(i));
        if(!character->hasPerk(perk.getID())) continue;
        const std::string name = exportPerkName(perk);
        vfs_put_expansion_string(fileWrite, "", IDSZ2( 'P', 'E', 'R', 'K' ), name.c_str() );
    }

    // write down all perks that we can still learn
    for(size_t i = 0; i < Ego::Perks::NR_OF_PERKS; ++i) {
        const Ego::Perks::Perk& perk = services.perkHandler.getPerk(static_cast<Ego::Perks::PerkID>(i));
        if(!profile->_perkPool[i] || character->hasPerk(perk.getID())) continue;
        const std::string name = exportPerkName(perk);
        vfs_put_expansion_string(fileWrite, "", IDSZ2( 'P', 'O', 'O', 'L' ), name.c_str() );
    }

    // dump the rest of the template file
    template_flush( fileTemp, fileWrite );

    // The end
    vfs_close( fileWrite );
    template_close_vfs( fileTemp );

    return true;
}
