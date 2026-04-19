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

/// @file egolib/game/Entities/Object_appearance.cpp
/// @brief Appearance, visibility, and collision-facing Object implementation.

#include "egolib/Entities/Object_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

bool Object::setSkin(const size_t skinNumber)
{
    if (!getProfile()->isValidSkin(skinNumber)) {
        return false;
    }
    const SkinInfo& newSkin = getProfile()->getSkinInfo(skinNumber);

    for (size_t i = 0; i < DAMAGE_COUNT; ++i) {
        _baseAttribute[Ego::Attribute::resistFromDamageType(static_cast<DamageType>(i))] = newSkin.damageResistance[i];
        _baseAttribute[Ego::Attribute::modifierFromDamageType(static_cast<DamageType>(i))] = newSkin.damageModifier[i];
    }

    _baseAttribute[Ego::Attribute::ACCELERATION] = newSkin.maxAccel;
    _baseAttribute[Ego::Attribute::DEFENCE] = newSkin.defence;
    this->skin = skinNumber;

    return true;
}

uint16_t Object::getSkinCost(size_t skinNumber) const
{
    return getProfile()->getSkinInfo(skinNumber).cost;
}

bool Object::isCurrentSkinDressy() const
{
    return getProfile()->getSkinInfo(getSkin()).dressy;
}

bool Object::hasIntellectDamageParticle() const
{
    const auto& profile = getProfile();
    for (LocalParticleProfileRef iTmp(0); iTmp.get() < MAX_PIP_PER_PROFILE; ++iTmp)
    {
        const std::shared_ptr<ParticleProfile>& particleProfile =
            EngineContext::get().profileSystem().getParticleProfile(profile->getParticleProfile(iTmp));
        if (particleProfile && particleProfile->_intellectDamageBonus)
        {
            return true;
        }
    }

    return false;
}

bool Object::isOnWaterTile() const
{
    return 0 != activeModule().getMeshPointer()->test_fx(getTile(), MAPFX_WATER);
}

bool Object::isSubmerged() const
{
    return isOnWaterTile() && getPosZ() <= activeModule().getWater().get_level();
}

void Object::movePosition(const float x, const float y, const float z)
{
    _position += Ego::Vector3f(x, y, z);
}

void Object::setAlpha(const int alpha)
{
    uint8_t clampedAlpha = Ego::Math::constrain(alpha, 0, 0xFF);

    if (isPlayer()) {
        clampedAlpha = std::max<uint8_t>(SEEINVISIBLE, clampedAlpha);
    }

    inst.setAlpha(clampedAlpha);
}

void Object::setLight(const int light)
{
    uint8_t clampedLight = Ego::Math::constrain(light, 0, 0xFF);

    if (isPlayer()) {
        clampedLight = std::max<uint8_t>(SEEINVISIBLE, clampedLight);
    }

    inst.setLight(clampedLight);
}

void Object::setSheen(const int sheen)
{
    inst.setSheen(Ego::Math::constrain(sheen, 0, 0xFF));
}

bool Object::teleport(const Ego::Vector3f& position, Facing facing_z)
{
    if (!activeModule().isInside(position[kX], position[kY])) {
        return false;
    }

    Ego::Vector3f newPosition = position;
    Ego::Vector2f nrm;
    if (!hit_wall(newPosition, nrm, nullptr)) {
        ori_old.facing_z = idlib::canonicalize(facing_z);
        setPosition(newPosition);
        ori.facing_z = idlib::canonicalize(facing_z);

        if (!detatchFromHolder(true, false)) {
            chr_update_matrix(this, true);
        }

        return true;
    }

    return false;
}

std::string Object::getName(bool prefixArticle, bool prefixDefinite, bool capitalLetter) const
{
    std::string result;

    if (isNameKnown()) {
        result = _name;
        if (capitalLetter) {
            result[0] = idlib::to_upper(result[0]);
        }
    } else {
        if (getProfile()->getSpellEffectType() != ObjectProfile::NO_SKIN_OVERRIDE) {
            result = EngineContext::get().profileSystem().getProfile(SPELLBOOK)->getClassName();
        } else {
            result = getProfile()->getClassName();
        }

        if (prefixArticle) {
            if (capitalLetter) {
                result[0] = std::toupper(result[0]);
            }

            if (prefixDefinite) {
                result.insert(0, "the ");
            } else {
                char lTmp = idlib::to_upper(result[0]);

                if ('A' == lTmp || 'E' == lTmp || 'I' == lTmp || 'O' == lTmp || 'U' == lTmp) {
                    result.insert(0, "an ");
                } else {
                    result.insert(0, "a ");
                }
            }
        }
    }

    return result;
}

bool Object::isFacingLocation(const float x, const float y) const
{
    auto facing = idlib::canonicalize(vec_to_facing(x - getPosX(), y - getPosY()));
    facing -= idlib::canonicalize(ori.facing_z);
    return (facing.get_value() > 55535 || facing.get_value() < 10000);
}

bool Object::canSeeObject(const std::shared_ptr<Object>& target) const
{
    if (getProfile()->isInvincible()) {
        return true;
    }

    int enviro_light = (target->getAlpha() * target->getMaxLight()) * idlib::fraction<float, 1, 255>();
    int self_light = (target->getLight() == 255) ? 0 : target->getLight();
    int light = std::max(enviro_light, self_light);
    light *= expf(0.32f * getAttribute(Ego::Attribute::DARKVISION));
    if (light < INVISIBLE) {
        return false;
    }

    if (!canSeeInvisible() && target->isStealthed()) {
        return false;
    }

    if (!canSeeInvisible() && target->getAlpha() < INVISIBLE) {
        return false;
    }

    return true;
}

void Object::setFat(const float fat)
{
    this->fat = fat;
    recalculateCollisionSize();
}

void Object::setBumpHeight(const float height)
{
    bump_save.height = std::max(height, 0.0f);
    recalculateCollisionSize();
}

void Object::setBumpWidth(const float width)
{
    float ratio = 1.0f;
    if (bump_stt.size > 0) {
        std::abs(width / bump_stt.size);
    }

    shadow_size_save = shadow_size_stt * ratio;
    bump_save.size = bump_stt.size * ratio;
    bump_save.size_big = bump_stt.size_big * ratio;

    recalculateCollisionSize();
}

void Object::recalculateCollisionSize()
{
    shadow_size = shadow_size_save * fat;
    bump.size = bump_save.size * fat;
    bump.size_big = bump_save.size_big * fat;
    bump.height = bump_save.height * fat;

    updateCollisionSize(true);
}

BIT_FIELD Object::hit_wall(const Ego::Vector3f& pos, Ego::Vector2f& nrm, float* pressure)
{
    if (Ego::Physics::CHR_INFINITE_WEIGHT == phys.weight) {
        return EMPTY_BIT_FIELD;
    }

    float radius = 0.0f;
    if (CameraSystem::is_initialized() &&
        CameraSystem::get().getMainCamera()->getTileList()->inRenderList(getTile())) {
        radius = bump_1.size;
    }

    g_meshStats.mpdfxTests = 0;
    g_meshStats.boundTests = 0;
    g_meshStats.pressureTests = 0;
    BIT_FIELD result = activeModule().getMeshPointer()->hit_wall(pos, radius, stoppedby, nrm, pressure);
    chr_stoppedby_tests += g_meshStats.mpdfxTests;
    chr_pressure_tests += g_meshStats.pressureTests;

    return result;
}

BIT_FIELD Object::hit_wall(const Ego::Vector3f& pos, Ego::Vector2f& nrm, float* pressure, mesh_wall_data_t& data)
{
    if (Ego::Physics::CHR_INFINITE_WEIGHT == phys.weight) {
        return EMPTY_BIT_FIELD;
    }

    float radius = 0.0f;
    if (CameraSystem::is_initialized() &&
        CameraSystem::get().getMainCamera()->getTileList()->inRenderList(getTile())) {
        radius = bump_1.size;
    }

    g_meshStats.mpdfxTests = 0;
    g_meshStats.boundTests = 0;
    g_meshStats.pressureTests = 0;
    BIT_FIELD result = activeModule().getMeshPointer()->hit_wall(pos, radius, stoppedby, nrm, pressure, data);
    chr_stoppedby_tests += g_meshStats.mpdfxTests;
    chr_pressure_tests += g_meshStats.pressureTests;

    return result;
}

BIT_FIELD Object::test_wall(const Ego::Vector3f& pos)
{
    if (isTerminated()) {
        return EMPTY_BIT_FIELD;
    }
    if (Ego::Physics::CHR_INFINITE_WEIGHT == phys.weight) {
        return EMPTY_BIT_FIELD;
    }

    float radius = 0.0f;

    g_meshStats.mpdfxTests = 0;
    g_meshStats.boundTests = 0;
    g_meshStats.pressureTests = 0;

    BIT_FIELD result = activeModule().getMeshPointer()->test_wall(pos, radius, stoppedby);
    chr_stoppedby_tests += g_meshStats.mpdfxTests;
    chr_pressure_tests += g_meshStats.pressureTests;

    return result;
}

bool Object::isScenery() const
{
    if (isItem()) {
        return false;
    }

    if (getBaseAttribute(Ego::Attribute::ACCELERATION) > 0) {
        return false;
    }

    if (getTeam() == Team::TEAM_NULL) {
        return true;
    }

    return getProfile()->isInvincible() || getProfile()->getWeight() == CAP_INFINITE_WEIGHT;
}

bool Object::isHidden() const
{
    if (getProfile()->getHideState() == NOHIDE) {
        return false;
    }
    return getProfile()->getHideState() == ai.state;
}

std::shared_ptr<const Ego::Texture> Object::getIcon() const
{
    if (getProfile()->getSpellEffectType() == ObjectProfile::NO_SKIN_OVERRIDE) {
        return getProfile()->getIcon(skin).get_ptr();
    } else {
        return EngineContext::get().profileSystem().getSpellBookIcon(getProfile()->getSpellEffectType()).get_ptr();
    }
}

std::shared_ptr<const Ego::Texture> Object::getSkinTexture() const
{
    return getProfile()->getSkin(this->skin).get_ptr();
}
