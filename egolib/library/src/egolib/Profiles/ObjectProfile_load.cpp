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

/// @file egolib/Profiles/ObjectProfile_load.cpp
/// @brief ObjectProfile loading and parsing helpers.

#include "egolib/Profiles/ObjectProfile_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

namespace
{
ObjectProfileRef vfs_get_next_object_profile_ref(ReadContext& ctxt)
{
    int number = vfs_get_next_int(ctxt);
    ObjectProfileRef reference = ObjectProfileRef::Invalid;
    if (number < ObjectProfileRef::Min.get() || number > ObjectProfileRef::Max.get())
    {
        return reference;
    }
    return ObjectProfileRef(number);
}
} // namespace

void ObjectProfile::loadTextures(const std::string &folderPath)
{
    //Clear texture references
    _texturesLoaded.clear();
    _iconsLoaded.clear();

    // Load the skins and icons
    for (size_t cnt = 0; cnt < 30; cnt++)
    {
        // do the texture
        const std::string skinPath = folderPath + "/tris" + std::to_string(cnt);
        if(ego_texture_exists_vfs(skinPath))
        {
            _texturesLoaded[cnt] = Ego::DeferredTexture(skinPath);

            // palshad's Golden Key uses a tiny legacy skin that collapses into a
            // flat-looking blob under the modern global mip/filter settings when
            // viewed at slight camera angles. Keep its original crisp sampling local
            // to this object instead of globally lowering texture quality.
            if (0 == cnt && std::string::npos != folderPath.find("palshad.mod/objects/keya.obj"))
            {
                _texturesLoaded[cnt].setFiltering(idlib::texture_filter_method::nearest,
                                                  idlib::texture_filter_method::nearest,
                                                  idlib::texture_filter_method::none);
            }
        }

        // do the icon
        const std::string iconPath = folderPath + "/icon" + std::to_string(cnt);
        if(ego_texture_exists_vfs(iconPath))
        {
            _iconsLoaded[cnt] = Ego::DeferredTexture(iconPath);
        }
    }

    // If we didn't get a skin, set it to the water texture
    if (_texturesLoaded.empty())
    {
        _texturesLoaded[0] = Ego::DeferredTexture("mp_data/waterlow");
        EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "object is missing a skin ", "`", getPathname(), "`", Log::EndOfEntry);
    }

    // If we didn't get a icon, set it to the NULL icon
    if (_iconsLoaded.empty())
    {
        _iconsLoaded[0] = Ego::DeferredTexture("mp_data/nullicon");
        EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Debug, __FILE__, __LINE__, "object is missing an icon ", "`", getPathname(), "`", Log::EndOfEntry);
    }
}

void ObjectProfile::loadAllMessages(const std::string &filePath)
{
    /// @author ZF
    /// @details This function loads all messages for an object

    std::unique_ptr<ReadContext> ctxt = nullptr;
    try {
        ctxt = std::make_unique<ReadContext>(filePath);
    } catch (...) {
        return;
    }

    while (ctxt->skipToColon(true))
    {
        //Load one line
        addMessage(vfs_read_string_lit(*ctxt));
    }
}

bool ObjectProfile::loadDataFile(const std::string &filePath)
{
    // Open the file
    ReadContext ctxt(filePath);

    //read slot number (ignored for now)
    vfs_get_next_object_profile_ref(ctxt);
    //_slotNumber = vfs_get_next_object_profile_ref(ctxt);

    // Read in the class name
    std::string buffer = vfs_get_next_string_lit(ctxt);

    // fix class name capitalization
    buffer[0] = idlib::to_upper(buffer[0]);

    _className = buffer;

    // Light cheat
    _uniformLit = vfs_get_next_bool(ctxt);

    // Ammo
    _maxAmmo = vfs_get_next_int(ctxt);
    _ammo = vfs_get_next_int(ctxt);

    // Gender
    switch (idlib::to_upper(vfs_get_next_printable(ctxt)))
    {
        case 'F': _gender = GenderProfile::Female; break;
        case 'M': _gender = GenderProfile::Male; break;
        case 'R': _gender = GenderProfile::Random; break;
        default:  _gender = GenderProfile::Neuter; break;
    }

    // Read in the starting stats
    _lifeColor = vfs_get_next_int(ctxt);
    _manaColor = vfs_get_next_int(ctxt);

    _baseAttribute[Ego::Attribute::MAX_LIFE] = vfs_get_next_range(ctxt);
    _attributeGain[Ego::Attribute::MAX_LIFE] = vfs_get_next_range(ctxt);

    //@note ZF> All life is doubled because Aaron decided it was a good idea to shift all damage by 1 by default (>> 1)
    //          With the newer damage system, we do it this way instead to keep the same game balance as before
    //          This allows for more granularity and show the player that he is slowly losing health from posion for example
    _baseAttribute[Ego::Attribute::MAX_LIFE] *= 2.0f;

    _baseAttribute[Ego::Attribute::MAX_MANA] = vfs_get_next_range(ctxt);
    _attributeGain[Ego::Attribute::MAX_MANA] = vfs_get_next_range(ctxt);

    _baseAttribute[Ego::Attribute::MANA_REGEN] = vfs_get_next_range(ctxt);
    _attributeGain[Ego::Attribute::MANA_REGEN] = vfs_get_next_range(ctxt);

    _baseAttribute[Ego::Attribute::SPELL_POWER] = vfs_get_next_range(ctxt);
    _attributeGain[Ego::Attribute::SPELL_POWER] = vfs_get_next_range(ctxt);

    _baseAttribute[Ego::Attribute::MIGHT] = vfs_get_next_range(ctxt);
    _attributeGain[Ego::Attribute::MIGHT] = vfs_get_next_range(ctxt);

    auto wisdom = vfs_get_next_range(ctxt);
    auto wisdomGain = vfs_get_next_range(ctxt);

    _baseAttribute[Ego::Attribute::INTELLECT] = vfs_get_next_range(ctxt);
    _attributeGain[Ego::Attribute::INTELLECT] = vfs_get_next_range(ctxt);

    //Wisdom used to be an attribute in Egoboo, but now its deprecated. To figure out intellect use average of WIS and INT
    if(!wisdom.is_zero()) {
        _baseAttribute[Ego::Attribute::INTELLECT] = idlib::interval<float>(_baseAttribute[Ego::Attribute::INTELLECT].lower() + wisdom.lower(),
                                                                           _baseAttribute[Ego::Attribute::INTELLECT].upper() + wisdom.upper()) * 0.5f;
    }
    if(!wisdomGain.is_zero()) {
        _attributeGain[Ego::Attribute::INTELLECT] = idlib::interval<float>(_attributeGain[Ego::Attribute::INTELLECT].lower() + wisdomGain.lower(),
                                                                           _attributeGain[Ego::Attribute::INTELLECT].upper() + wisdomGain.upper()) * 0.5f;
    }

    _baseAttribute[Ego::Attribute::AGILITY] = vfs_get_next_range(ctxt);
    _attributeGain[Ego::Attribute::AGILITY] = vfs_get_next_range(ctxt);

    // More physical attributes
    _size = vfs_get_next_float(ctxt);
    _sizeGainPerLevel = vfs_get_next_float(ctxt);
    _shadowSize = vfs_get_next_int(ctxt);
    _bumpSize = vfs_get_next_int(ctxt);
    _bumpHeight = vfs_get_next_int(ctxt);
    {
        const float rawBumpDampen = vfs_get_next_float(ctxt);
        // Preserve legacy "0.0 means infinite mass" content semantics used by immovable scenery.
        _bumpDampen = rawBumpDampen < 0.0f ? idlib::fraction<float, 1, 255>() : rawBumpDampen;
    }
    _weight = vfs_get_next_int(ctxt);
    _jumpPower = vfs_get_next_float(ctxt);
    _jumpNumber = vfs_get_next_int(ctxt);
    _animationSpeedSneak = 2.0f * vfs_get_next_float(ctxt);
    _animationSpeedWalk = 2.0f * vfs_get_next_float(ctxt);
    _animationSpeedRun = 2.0f * vfs_get_next_float(ctxt);
    _flyHeight = vfs_get_next_int(ctxt);
    _flashAND = vfs_get_next_int(ctxt);
    _alpha = vfs_get_next_int(ctxt);
    _light = vfs_get_next_int(ctxt);
    _transferBlending = vfs_get_next_bool(ctxt);
    _sheen = vfs_get_next_int(ctxt);
    _phongMapping = vfs_get_next_bool(ctxt);
    _textureMovementRateX = FLOAT_TO_FFFF(vfs_get_next_float(ctxt));
    _textureMovementRateY = FLOAT_TO_FFFF(vfs_get_next_float(ctxt));
    _stickyButt = vfs_get_next_bool(ctxt);

    // Invulnerability data
    _isInvincible = vfs_get_next_bool(ctxt);
    nframefacing = vfs_get_next_int(ctxt);
    nframeangle = vfs_get_next_int(ctxt);
    iframefacing = vfs_get_next_int(ctxt);
    iframeangle = vfs_get_next_int(ctxt);

    // Resist burning and stuck arrows with nframe angle of 1 or more
    if ( 1 == nframeangle )
    {
        nframeangle = 0;
    }

    // Skin defenses ( 4 skins )
    ctxt.skipToColon(false);
    for (size_t cnt = 0; cnt < SKINS_PEROBJECT_MAX; cnt++)
    {
        _skinInfo[cnt].defence = Ego::Math::constrain(ctxt.readIntegerLiteral(), 0, 0xFF);
    }

    for (size_t damagetype = 0; damagetype < DAMAGE_COUNT; damagetype++ )
    {
        ctxt.skipToColon(false);
        for (size_t cnt = 0; cnt < SKINS_PEROBJECT_MAX; cnt++)
        {
            _skinInfo[cnt].damageResistance[damagetype] = vfs_get_damage_resist(ctxt);
        }
    }

    for (size_t damagetype = 0; damagetype < DAMAGE_COUNT; damagetype++ )
    {
        ctxt.skipToColon(false);

        for (size_t cnt = 0; cnt < SKINS_PEROBJECT_MAX; cnt++)
        {
            _skinInfo[cnt].damageModifier[damagetype] = 0;
            switch (idlib::to_upper(ctxt.readPrintable()))
            {
                case 'T': _skinInfo[cnt].damageModifier[damagetype] |= DAMAGEINVERT;   break;
                case 'C': _skinInfo[cnt].damageModifier[damagetype] |= DAMAGECHARGE;   break;
                case 'M': _skinInfo[cnt].damageModifier[damagetype] |= DAMAGEMANA;     break;
                case 'I': _skinInfo[cnt].damageModifier[damagetype] |= DAMAGEINVICTUS; break;

                    //F is nothing
                default: break;
            }
        }
    }

    ctxt.skipToColon(false);
    for (size_t cnt = 0; cnt < SKINS_PEROBJECT_MAX; cnt++)
    {
        _skinInfo[cnt].maxAccel = ctxt.readRealLiteral() / 80.0f;
    }

    // Experience and level data
    _experienceForLevel[0] = 0;
    for ( size_t level = 1; level < MAXBASELEVEL; level++ )
    {
        _experienceForLevel[level] = vfs_get_next_int(ctxt);
    }
    setupXPTable();

    _startingExperience = vfs_get_next_range(ctxt);

    _experienceWorth = vfs_get_next_int(ctxt);
    _experienceExchange = vfs_get_next_float(ctxt);

    for(size_t i = 0; i < _experienceRate.size(); ++i)
    {
        _experienceRate[i] = vfs_get_next_float(ctxt) + 0.001f;
    }

    // IDSZ tags
    for (size_t i = 0; i < _idsz.size(); ++i)
    {
        _idsz[i] = vfs_get_next_idsz(ctxt);
    }

    // Item and damage flags
    _isItem = vfs_get_next_bool(ctxt);
    _isMount = vfs_get_next_bool(ctxt);
    _isStackable = vfs_get_next_bool(ctxt);
    _nameIsKnown = vfs_get_next_bool(ctxt);
    _usageIsKnown = vfs_get_next_bool(ctxt);
    _canCarryToNextModule = vfs_get_next_bool(ctxt);
    _needSkillIDToUse = vfs_get_next_bool(ctxt);
    _isPlatform = vfs_get_next_bool(ctxt);
    _canGrabMoney = vfs_get_next_bool(ctxt);
    _canOpenStuff = vfs_get_next_bool(ctxt);

    // More item and damage stuff
    _damageTargetDamageType = vfs_get_next_damage_type(ctxt);
    _weaponAction = Ego::ModelDescriptor::charToAction(vfs_get_next_printable(ctxt));

    // Particle attachments
    _attachedParticleAmount = vfs_get_next_int(ctxt);
    _attachedParticleReaffirmDamageType = vfs_get_next_damage_type(ctxt);
    _attachedParticle = vfs_get_next_local_particle_profile_ref(ctxt);

    // Character hands
    _slotsValid[SLOT_LEFT] = vfs_get_next_bool(ctxt);
    _slotsValid[SLOT_RIGHT] = vfs_get_next_bool(ctxt);

    // Attack order ( weapon )
    _attachAttackParticleToWeapon = vfs_get_next_bool(ctxt);
    _attackParticle = vfs_get_next_local_particle_profile_ref(ctxt);

    // GoPoof
    _goPoofParticleAmount = vfs_get_next_int(ctxt);
    _goPoofParticleFacingAdd = vfs_get_next_int(ctxt);
    _goPoofParticle = vfs_get_next_local_particle_profile_ref(ctxt);

    // Blud
    switch (idlib::to_upper(vfs_get_next_printable(ctxt)))
    {
        case 'T': _bludValid = true;        break;
        case 'U': _bludValid = ULTRABLUDY;  break;
        default:  _bludValid = false;       break;
    }
    _bludParticle = vfs_get_next_local_particle_profile_ref(ctxt);

    // Stuff I forgot
    _waterWalking = vfs_get_next_bool(ctxt);
    _bounciness = vfs_get_next_float(ctxt);

    // More stuff I forgot
    vfs_get_next_float(ctxt);  //ZF> deprecated value LifeReturn (no longer used)
    _useManaCost = vfs_get_next_float(ctxt);
    _baseAttribute[Ego::Attribute::LIFE_REGEN] = vfs_get_next_range(ctxt);
    _attributeGain[Ego::Attribute::LIFE_REGEN] = idlib::interval<float>();    //ZF> TODO: regen gain per level not implemented
    _baseAttribute[Ego::Attribute::LIFE_REGEN] /= 256.0f;
    _stoppedBy |= vfs_get_next_int(ctxt);

    for (size_t cnt = 0; cnt < SKINS_PEROBJECT_MAX; cnt++)
    {
        _skinInfo[cnt].name = vfs_get_next_string_lit(ctxt);
    }

    for (size_t cnt = 0; cnt < SKINS_PEROBJECT_MAX; cnt++)
    {
        _skinInfo[cnt].cost = vfs_get_next_int(ctxt);
    }

    _strengthBonus = vfs_get_next_float(ctxt);          //ZF> Deprecated, but keep here for backwards compatability

    // Another memory lapse
    _riderCanAttack = !vfs_get_next_bool(ctxt);     //ZF> note value is inverted intentionally
    _canBeDazed = vfs_get_next_bool(ctxt);
    _canBeGrogged = vfs_get_next_bool(ctxt);

    ctxt.skipToColon(false);  // Depracated, no longer used (permanent life add)
    ctxt.skipToColon(false);  // Depracated, no longer used (permanent mana add)
    if (vfs_get_next_bool(ctxt)) {
        _seeInvisibleLevel = 1;
    }

    _kurseChance = vfs_get_next_int(ctxt);
    _footFallSound = vfs_get_next_int(ctxt);  // Footfall sound
    _jumpSound = vfs_get_next_int(ctxt);  // Jump sound

    // assume the normal dependence of _causesRipples on _isItem
    _causesRipples = !_isItem;

    // assume a round object
    _bumpSizeBig = _bumpSize * idlib::sqrt_two<float>();

    // assume the normal icon usage
    _drawIcon = _usageIsKnown;

    // assume normal platform usage
    _canUsePlatforms = !_isPlatform;

    // Read expansions
    while (ctxt.skipToColon(true))
    {
        const IDSZ2 idsz = ctxt.readIDSZ();

        switch(idsz.toUint32())
        {
            case IDSZ2::caseLabel( 'D', 'R', 'E', 'S' ):
                _skinInfo[ctxt.readIntegerLiteral()].dressy = true;
            break;

            case IDSZ2::caseLabel( 'G', 'O', 'L', 'D' ):
                _money = ctxt.readIntegerLiteral();
            break;

            case IDSZ2::caseLabel( 'S', 'T', 'U', 'K' ):
                _resistBumpSpawn = (0 != (1 - ctxt.readIntegerLiteral()));
            break;

            case IDSZ2::caseLabel( 'P', 'A', 'C', 'K' ):
                _isBigItem = !(0 != ctxt.readIntegerLiteral());
            break;

            case IDSZ2::caseLabel( 'V', 'A', 'M', 'P' ):
                _hasReflection = (0 == ctxt.readIntegerLiteral());
            break;

            case IDSZ2::caseLabel( 'D', 'R', 'A', 'W' ):
                _alwaysDraw = (0 != ctxt.readIntegerLiteral());
            break;

            case IDSZ2::caseLabel( 'R', 'A', 'N', 'G' ):
                _isRanged = (0 != ctxt.readIntegerLiteral());
            break;

            case IDSZ2::caseLabel( 'H', 'I', 'D', 'E' ):
                _hideState = ctxt.readIntegerLiteral();
            break;

            case IDSZ2::caseLabel( 'E', 'Q', 'U', 'I' ):
                _isEquipment = (0 != ctxt.readIntegerLiteral());
            break;

            case IDSZ2::caseLabel( 'S', 'Q', 'U', 'A' ):
                _bumpSizeBig = _bumpSize * 2;
            break;

            case IDSZ2::caseLabel( 'I', 'C', 'O', 'N' ):
                _drawIcon = (0 != ctxt.readIntegerLiteral());
            break;

            case IDSZ2::caseLabel( 'S', 'H', 'A', 'D' ):
                _forceShadow = (0 != ctxt.readIntegerLiteral());
            break;

            case IDSZ2::caseLabel( 'S', 'K', 'I', 'N' ):
            {
                /// @note BB@> This is the skin value of a saved character.
                ///            It should(!) correspond to a valid skin for this object,
                ///            but possibly it could have one of two special values (NO_SKIN_OVERRIDE or SKINS_PEROBJECT_MAX)

                int iTmp = ctxt.readIntegerLiteral();

                iTmp = ( iTmp < 0 ) ? NO_SKIN_OVERRIDE : iTmp;
                _skinOverride = iTmp;
            }
            break;

            case IDSZ2::caseLabel( 'C', 'O', 'N', 'T' ):
                _contentOverride = ctxt.readIntegerLiteral();
            break;

            case IDSZ2::caseLabel( 'S', 'T', 'A', 'T' ):
                _stateOverride = ctxt.readIntegerLiteral();
            break;

            case IDSZ2::caseLabel( 'L', 'E', 'V', 'L' ):
                _levelOverride = ctxt.readIntegerLiteral();
            break;

            case IDSZ2::caseLabel( 'P', 'L', 'A', 'T' ):
                _canUsePlatforms = (0 != ctxt.readIntegerLiteral());
            break;

            case IDSZ2::caseLabel( 'R', 'I', 'P', 'P' ):
                _causesRipples = (0 != ctxt.readIntegerLiteral());
            break;

            case IDSZ2::caseLabel( 'V', 'A', 'L', 'U' ):
                _isValuable = ctxt.readIntegerLiteral();
            break;

            case IDSZ2::caseLabel( 'L', 'I', 'F', 'E' ):
                _spawnLife = 0xff * ctxt.readRealLiteral();
            break;

            case IDSZ2::caseLabel( 'M', 'A', 'N', 'A' ):
                _spawnMana = 0xff * ctxt.readRealLiteral();
            break;

            case IDSZ2::caseLabel( 'B', 'O', 'O', 'K' ):
            {
                _spellEffectType = ctxt.readIntegerLiteral();
            }
            break;

            //Damage bonuses from stats
            case IDSZ2::caseLabel( 'F', 'A', 'S', 'T' ):
                _attackFast = (0 != ctxt.readIntegerLiteral());
            break;

            case IDSZ2::caseLabel( 'S', 'T', 'R', 'D' ):
                _strengthBonus = ctxt.readRealLiteral();
            break;

            case IDSZ2::caseLabel( 'I', 'N', 'T', 'D' ):
                _intelligenceBonus = ctxt.readRealLiteral();
            break;

            case IDSZ2::caseLabel( 'D', 'E', 'X', 'D' ):
                _dexterityBonus = ctxt.readRealLiteral();
            break;

            case IDSZ2::caseLabel( 'M', 'O', 'D', 'L' ):
            {
                std::string tmp_buffer = vfs_read_string_lit(ctxt);
                if (tmp_buffer.length() > 0)
                {
                    size_t position = tmp_buffer.find_first_of("SBHCT");
                    while (position != std::string::npos)
                    {
                        if ( 'S' == tmp_buffer[position] )
                        {
                            _bumpOverrideSize = true;
                        }
                        else if ( 'B' == tmp_buffer[position] )
                        {
                            _bumpOverrideSizeBig = true;
                        }
                        else if ( 'H' == tmp_buffer[position] )
                        {
                            _bumpOverrideHeight = true;
                        }
                        else if ( 'C' == tmp_buffer[position])
                        {
                            _dontCullBackfaces = true;
                        }

                        // start on the next character
                        position++;
                        position = tmp_buffer.find_first_of("SBHCT", position);
                    }
                }
            }
            break;

            case IDSZ2::caseLabel('B', 'L', 'O', 'C'):
            {
                _blockRating = ctxt.readIntegerLiteral();
            }
            break;

            //Random Seed for level ups
            case  IDSZ2::caseLabel( 'S', 'E', 'E', 'D' ):
                _levelUpRandomSeedOverride = ctxt.readIntegerLiteral();
            break;

            //Perks known
            case IDSZ2::caseLabel( 'P', 'E', 'R', 'K' ):
            {
                std::string perkName = ctxt.readName();
                std::replace(perkName.begin(), perkName.end(), '_', ' '); //replace underscore with spaces
                Ego::Perks::PerkID id = EngineContext::get().perkHandler().fromString(perkName);
                if(id != Ego::Perks::NR_OF_PERKS)
                {
                    _startingPerks[id] = true;
                }
                else
                {
                    EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "in file ", "`", filePath ,"`", ": ", "unknown [PERK] perk ", "`", perkName, "`", " parsed", Log::EndOfEntry);
                }
            }
            break;

            //Perk Pool (perks that we can learn in the future)
            case IDSZ2::caseLabel( 'P', 'O', 'O', 'L' ):
            {
                std::string perkName = ctxt.readOldString();
                std::replace(perkName.begin(), perkName.end(), '_', ' '); //replace underscore with spaces
                Ego::Perks::PerkID id = EngineContext::get().perkHandler().fromString(perkName);
                if(id != Ego::Perks::NR_OF_PERKS)
                {
                    _perkPool[id] = true;
                }
                else
                {
                    EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "in file ", "`", filePath, "`", ": unknown [POOL] perk ", "`", perkName, "`", " parsed", Log::EndOfEntry);
                }
            }
            break;

            //Backwards compatability with old skill system (for older data files)
            case IDSZ2::caseLabel('A', 'W', 'E', 'P'): _startingPerks[Ego::Perks::WEAPON_PROFICIENCY] = true; break;
            case IDSZ2::caseLabel('P', 'O', 'I', 'S'): _startingPerks[Ego::Perks::POISONRY] = true; break;
            case IDSZ2::caseLabel('C', 'K', 'U', 'R'): _startingPerks[Ego::Perks::SENSE_KURSES] = true; break;
            case IDSZ2::caseLabel('R', 'E', 'A', 'D'): _startingPerks[Ego::Perks::LITERACY] = true; break;
            case IDSZ2::caseLabel('W', 'M', 'A', 'G'): _startingPerks[Ego::Perks::ARCANE_MAGIC] = true; break;
            case IDSZ2::caseLabel('H', 'M', 'A', 'G'): _startingPerks[Ego::Perks::DIVINE_MAGIC] = true; break;
            case IDSZ2::caseLabel('T', 'E', 'C', 'H'): _startingPerks[Ego::Perks::USE_TECHNOLOGICAL_ITEMS] = true; break;
            case IDSZ2::caseLabel('D', 'I', 'S', 'A'): _startingPerks[Ego::Perks::TRAP_LORE] = true; break;
            case IDSZ2::caseLabel('S', 'T', 'A', 'B'): _startingPerks[Ego::Perks::BACKSTAB] = true; break;
            case IDSZ2::caseLabel('D', 'A', 'R', 'K'): _startingPerks[Ego::Perks::NIGHT_VISION] = true; break;
            case IDSZ2::caseLabel('J', 'O', 'U', 'S'): _startingPerks[Ego::Perks::JOUSTING] = true; break;

            default:
                EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "`", filePath, "`: ", "unknown IDSZ ", "`", idsz.toString(), "`", Log::EndOfEntry);
            break;
        }
    }
    return true;
}

std::shared_ptr<ObjectProfile> ObjectProfile::loadFromFile(const std::string& folderPath, ObjectProfileRef ref, bool lightWeight)
{
    // Assert the reference is valid.
    if (!ref)
    {
        EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "invalid profile reference ", ref, Log::EndOfEntry);
        return nullptr;
    }

    // Allocate the object profile object.
    std::shared_ptr<ObjectProfile> profile = std::make_shared<ObjectProfile>();

    // Set some data
    profile->_pathname = folderPath;
    profile->_slotNumber = ref.get();

    //Don't load 3d model, enchant, messages, sounds or particle effects for lightweight profiles
    if (!lightWeight)
    {
        // Load the model for this profile
        try
        {
            profile->_model = std::make_shared<Ego::ModelDescriptor>(folderPath.c_str());
        }
        catch (const std::runtime_error &ex)
        {
            EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to load model ", "`", folderPath, "`", Log::EndOfEntry);
            return nullptr;
        }

        // Load the enchantment for this profile (optional)
        profile->_ieve = EngineContext::get().profileSystem().loadEnchantProfile(folderPath + "/enchant.txt",
                                                                                static_cast<EVE_REF>(ref.get()));

        // Load the messages for this profile, do this before loading the AI script
        // to ensure any dynamic loaded messages get loaded last (optional)
        profile->loadAllMessages(folderPath + "/message.txt");

        // Load the particles for this profile (optional)
        for (LocalParticleProfileRef cnt(0); cnt.get() < 30; ++cnt) //TODO: find better way of listing files
        {
            const std::string particleName = folderPath + "/part" + std::to_string(cnt.get()) + ".txt";
            PIP_REF particleProfile = EngineContext::get().profileSystem().loadParticleProfile(particleName.c_str(),
                                                                                               INVALID_PIP_REF);

            // Make sure it's referenced properly
            if (particleProfile != INVALID_PIP_REF)
            {
                profile->_particleProfiles[cnt] = particleProfile;
            }
        }

        // Load the waves for this iobj
        for (size_t cnt = 0; cnt < 30; cnt++) //TODO: make better search than just 30 (list files?)
        {
            const std::string soundName = folderPath + "/sound" + std::to_string(cnt);
            SoundID soundID = AudioSystem::get().loadSound(soundName);

            if (soundID != INVALID_SOUND_ID)
            {
                profile->_soundMap[cnt] = soundID;
            }
        }
    }

    //Load profile graphics (optional)
    profile->loadTextures(folderPath);

    // Load the random naming table for this icap (optional)
    profile->_randomName.loadFromFile(folderPath + "/naming.txt");

    // Finally load the character profile
    // Do after loading particle and sound profiles
    try
    {
        if (!profile->loadDataFile(folderPath + "/data.txt"))
        {
            EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to load data.txt for profile ", "`", folderPath, "`", Log::EndOfEntry);
            return nullptr;
        }
    }
    catch (const std::runtime_error &ex)
    {
        EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "failed to parse ", "`", folderPath, "/data.txt", "`", ": ", ex.what(), Log::EndOfEntry);
        return nullptr;
    }

    // Fix lighting if need be
    if (profile->_uniformLit && EngineContext::get().config().graphic_gouraudShading_enable.getValue())
    {
        profile->getModel()->makeEquallyLit();
    }

    return profile;
}

std::shared_ptr<ObjectProfile> ObjectProfile::loadFromFile(const std::string &folderPath, PRO_REF ref, const bool lightWeight)
{
    return loadFromFile(folderPath, ObjectProfileRef(ref), lightWeight);
}
