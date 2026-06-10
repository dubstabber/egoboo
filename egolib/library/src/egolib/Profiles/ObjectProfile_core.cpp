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

/// @file egolib/Profiles/ObjectProfile_core.cpp
/// @brief Core ObjectProfile helpers, accessors, and lifecycle.

#include "egolib/Profiles/ObjectProfile_internal.h"
#include "egolib/game/mesh.h"

ObjectProfile::ObjectProfile() :
    _spawnRequestCount(0),
    _spawnCount(0),
    _pathname("*NONE*"),
    _model(nullptr),
    _ieve(INVALID_EVE_REF),
    _slotNumber(ObjectProfileRef::Invalid),

    _randomName(),
    _aiScript(),
    _particleProfiles(),

    _texturesLoaded(),
    _iconsLoaded(),

    _messageList(),
    _soundMap(),

    //-------------------
    //Data.txt
    //-------------------
    _className("*NONE*"),

    // skins
    _skinInfo(),

    // overrides
    _skinOverride(NO_SKIN_OVERRIDE),
    _levelOverride(0),
    _stateOverride(0),
    _contentOverride(0),

    _idsz(),

    // inventory
    _maxAmmo(0),
    _ammo(0),
    _money(0),

    // characer stats
    _gender(GenderProfile::Neuter),

    //for imports
    _spawnLife(PERFECTBIG),
    _spawnMana(PERFECTBIG),

    //Base attributes
    _baseAttribute(),
    _attributeGain(),

    // physics
    _weight(1),
    _bounciness(0.0f),
    _bumpDampen(idlib::fraction<float, 1, 255>()),

    _size(1.0f),
    _sizeGainPerLevel(0.0f),
    _shadowSize(0),
    _bumpSize(0),
    _bumpOverrideSize(false),
    _bumpSizeBig(0),
    _bumpOverrideSizeBig(false),
    _bumpHeight(0),
    _bumpOverrideHeight(false),
    _stoppedBy(MAPFX_IMPASS),

    // movement
    _jumpPower(0.0f),
    _jumpNumber(0),
    _animationSpeedSneak(0.0f),
    _animationSpeedWalk(0.0f),
    _animationSpeedRun(0.0f),
    _flyHeight(0),
    _waterWalking(false),
    _jumpSound(-1),
    _footFallSound(-1),

    // status graphics
    _lifeColor(0),
    _manaColor(0),
    _drawIcon(true),

    // model graphics
    _flashAND(0),
    _alpha(0),
    _light(0),
    _transferBlending(false),
    _sheen(0),
    _phongMapping(false),
    _textureMovementRateX(0),
    _textureMovementRateY(0),
    _uniformLit(false),
    _hasReflection(true),
    _alwaysDraw(false),
    _forceShadow(false),
    _causesRipples(false),
    _dontCullBackfaces(false),

    // attack blocking info
    iframefacing(0),
    iframeangle(0),
    nframefacing(0),
    nframeangle(0),
    _blockRating(0),

    // defense
    _resistBumpSpawn(false),

    // xp
    _experienceForLevel(),
    _startingExperience(),
    _experienceWorth(0),
    _experienceExchange(0.0f),
    _experienceRate(),
    _levelUpRandomSeedOverride(0),

    // flags
    _isEquipment(false),
    _isItem(false),
    _isMount(false),
    _isStackable(false),
    _isInvincible(false),
    _isPlatform(false),
    _canUsePlatforms(false),
    _canGrabMoney(false),
    _canOpenStuff(false),
    _canBeDazed(false),
    _canBeGrogged(false),
    _isBigItem(false),
    _isRanged(false),
    _nameIsKnown(false),
    _usageIsKnown(false),
    _canCarryToNextModule(false),

    _damageTargetDamageType(DAMAGE_CRUSH),
    _slotsValid(),
    _riderCanAttack(false),
    _kurseChance(0),
    _hideState(NOHIDE),
    _isValuable(-1),
    _spellEffectType(NO_SKIN_OVERRIDE),

    // item usage
    _needSkillIDToUse(false),
    _weaponAction(0),
    _attachAttackParticleToWeapon(false),
    _attackParticle(-1),
    _attackFast(false),

    _strengthBonus(0.0f),
    _intelligenceBonus(0.0f),
    _dexterityBonus(0.0f),

    // special particle effects
    _attachedParticleAmount(0),
    _attachedParticleReaffirmDamageType(DAMAGE_FIRE),
    _attachedParticle(-1),

    _goPoofParticleAmount(0),
    _goPoofParticleFacingAdd(0),
    _goPoofParticle(-1),

    //Blood
    _bludValid(0),
    _bludParticle(-1),

    // skill system
    _seeInvisibleLevel(0),

    // random stuff
    _stickyButt(false),
    _useManaCost(0.0f),

    _startingPerks(),
    _perkPool()
{
    _experienceRate.fill(0.0f);
    _idsz.fill(IDSZ2::None);
    _experienceForLevel.fill(std::numeric_limits<uint32_t>::max());
}

ObjectProfile::~ObjectProfile()
{
    IProfileSystem* profileSystem = tryActiveProfileSystem();
    if (!profileSystem) return;

    for(const auto &element : _particleProfiles)
    {
        profileSystem->unloadParticleProfile(element.second);
    }
}

uint32_t ObjectProfile::getXPNeededForLevel(uint8_t level) const
{
    if(level >= _experienceForLevel.size()) {
        return std::numeric_limits<uint32_t>::max();
    }

    return _experienceForLevel[level];
}

size_t ObjectProfile::addMessage(const std::string &message, const bool filterDuplicates)
{
    std::string parsedMessage = message;

    //replace underscore with whitespace
    std::replace(parsedMessage.begin(), parsedMessage.end(), '_', ' ');

    //Don't add the same message twice
    if(filterDuplicates) {
        size_t messageListSize = _messageList.size();
        for(size_t pos = 0; pos < messageListSize; pos++) {
            if(_messageList[pos] == parsedMessage) {
                return pos;
            }
        }
    }

    //Add it to the list!
    _messageList.push_back(parsedMessage);
    return _messageList.size() - 1;
}

const std::string& ObjectProfile::getMessage(size_t index) const
{
    static const std::string EMPTY_STRING;

    if(index > _messageList.size()) {
        return EMPTY_STRING;
    }

    return _messageList[index];
}

const std::string ObjectProfile::generateRandomName()
{
    //If no random names loaded, return class name instead
    if (!_randomName.isLoaded())
    {
        return _className;
    }

    return _randomName.generateRandomName();
}

SoundID ObjectProfile::getSoundID(int index) const
{
    if(index < 0) return INVALID_SOUND_ID;

    //Try to find a SoundID that this number is mapped to
    const auto &result = _soundMap.find(index);

    //Not found?
    if(result == _soundMap.end()) {
        return INVALID_SOUND_ID;
    }

    return (*result).second;
}

const IDSZ2& ObjectProfile::getIDSZ(size_t type) const
{
    if(type >= IDSZ_COUNT) {
        return IDSZ2::None;
    }

    return _idsz[type];
}

const Ego::DeferredTexture& ObjectProfile::getSkin(size_t index)
{
    const auto& result = _texturesLoaded.find(index);
    if(result == _texturesLoaded.end()) {
        return _texturesLoaded[0];
    }

    return result->second;
}

const Ego::DeferredTexture& ObjectProfile::getIcon(size_t index)
{
    const auto& result = _iconsLoaded.find(index);
    if(result == _iconsLoaded.end()) {
        return _iconsLoaded[0];
    }

    return result->second;
}

PIP_REF ObjectProfile::getParticleProfile(const LocalParticleProfileRef& lppref) const
{
    if (lppref.get() <= -1) {
        return INVALID_PIP_REF;
    }

    const auto &result = _particleProfiles.find(lppref);

    //Not found in map?
    if(result == _particleProfiles.end()) {
        return INVALID_PIP_REF;
    }

    return result->second;
}

uint16_t ObjectProfile::getSkinOverride() const
{
    //Are we actually a spell book?
    if (_spellEffectType != NO_SKIN_OVERRIDE) {
        return _spellEffectType;
    }

    return _skinOverride;
}

void ObjectProfile::setupXPTable()
{
    for (size_t level = MAXBASELEVEL; level < MAXLEVEL; level++ )
    {
        uint32_t xpneeded = _experienceForLevel[MAXBASELEVEL - 1];
        xpneeded += ( level * level * level * 15 );
        xpneeded -= (( MAXBASELEVEL - 1 ) * ( MAXBASELEVEL - 1 ) * ( MAXBASELEVEL - 1 ) * 15 );
        _experienceForLevel[level] = xpneeded;
    }
}

const SkinInfo& ObjectProfile::getSkinInfo(size_t index) const
{
    const auto &result = _skinInfo.find(index);
    if(result == _skinInfo.end()) {
        return INVALID_SKIN; //empty skin (dont construct new element in map)
    }

    return (*result).second;
}

bool ObjectProfile::isValidSkin(size_t index) const
{
    return _skinInfo.find(index) != _skinInfo.end();
}

float ObjectProfile::getExperienceRate(XPType type) const
{
    if(type >= _experienceRate.size()) {
        return 0.0f;
    }

    return _experienceRate[type];
}

bool ObjectProfile::hasTypeIDSZ(const IDSZ2& idsz) const
{
    if ( IDSZ2::None == idsz ) return true;
    if ( idsz == _idsz[IDSZ_TYPE  ] ) return true;
    if ( idsz == _idsz[IDSZ_PARENT] ) return true;

    return false;
}

bool ObjectProfile::hasIDSZ(const IDSZ2& idsz) const
{
    for(const IDSZ2& compare : _idsz)
    {
        if(compare == idsz)
        {
            return true;
        }
    }

    return false;
}

bool ObjectProfile::isSlotValid(slot_t slot) const
{
    if(slot >= _slotsValid.size()) {
        return false;
    }

    return _slotsValid[slot];
}

size_t ObjectProfile::getRandomSkinID() const
{
    if(_skinInfo.empty()) {
        return 0;
    }

    auto element = _skinInfo.begin();
    std::advance(element, Random::next(_skinInfo.size()-1));
    return element->first;
}

const idlib::interval<float>& ObjectProfile::getAttributeGain(Ego::Attribute::AttributeType type) const
{
    IDLIB_DEBUG_ASSERT(type < _attributeGain.size());
    return _attributeGain[type];
}

const idlib::interval<float>& ObjectProfile::getAttributeBase(Ego::Attribute::AttributeType type) const
{
    IDLIB_DEBUG_ASSERT(type < _baseAttribute.size());
    return _baseAttribute[type];
}

bool ObjectProfile::canLearnPerk(const Ego::Perks::PerkID id) const
{
    if(id == Ego::Perks::NR_OF_PERKS) return false;
    return _perkPool[id];
}

bool ObjectProfile::beginsWithPerk(const Ego::Perks::PerkID id) const
{
    if(id == Ego::Perks::NR_OF_PERKS) return false;
    return _startingPerks[id];
}
