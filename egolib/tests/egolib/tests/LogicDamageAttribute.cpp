#include "gtest/gtest.h"

#include "egolib/Logic/Damage.hpp"
#include "egolib/Logic/Attribute.hpp"

#include <stdexcept>

// Characterization tests for the header-only enum-mapping logic in
// Logic/Damage.hpp and Logic/Attribute.hpp (previously 0-coverage):
//   - DamageType_isPhysical / DamageType_getColour (Damage.hpp)
//   - Ego::Attribute::{toString, resistFromDamageType, modifierFromDamageType,
//     isOverrideSetAttribute} (Attribute.hpp)
// DamageType is a plain GLOBAL enum (DAMAGE_SLASH..DAMAGE_DIRECT); AttributeType
// is a plain enum inside namespace Ego::Attribute. Pure, no fixture.

namespace
{

namespace A = Ego::Attribute;

// ---------------------------------------------------------------------------
// DamageType_isPhysical
// ---------------------------------------------------------------------------

TEST(LogicDamage, IsPhysicalTrueOnlyForSlashCrushPoke)
{
    EXPECT_TRUE(DamageType_isPhysical(DAMAGE_SLASH));
    EXPECT_TRUE(DamageType_isPhysical(DAMAGE_CRUSH));
    EXPECT_TRUE(DamageType_isPhysical(DAMAGE_POKE));

    EXPECT_FALSE(DamageType_isPhysical(DAMAGE_HOLY));
    EXPECT_FALSE(DamageType_isPhysical(DAMAGE_EVIL));
    EXPECT_FALSE(DamageType_isPhysical(DAMAGE_FIRE));
    EXPECT_FALSE(DamageType_isPhysical(DAMAGE_ICE));
    EXPECT_FALSE(DamageType_isPhysical(DAMAGE_ZAP));
    EXPECT_FALSE(DamageType_isPhysical(DAMAGE_COUNT));
    EXPECT_FALSE(DamageType_isPhysical(DAMAGE_DIRECT));
}

// ---------------------------------------------------------------------------
// DamageType_getColour
// ---------------------------------------------------------------------------

TEST(LogicDamage, GetColourMapsEachTypeToItsSingleton)
{
    EXPECT_TRUE(DamageType_getColour(DAMAGE_ZAP)  == Ego::Colour3f::yellow());
    EXPECT_TRUE(DamageType_getColour(DAMAGE_FIRE) == Ego::Colour3f::red());
    EXPECT_TRUE(DamageType_getColour(DAMAGE_EVIL) == Ego::Colour3f::green());
    EXPECT_TRUE(DamageType_getColour(DAMAGE_HOLY) == Ego::Colour3f::mauve());
    EXPECT_TRUE(DamageType_getColour(DAMAGE_ICE)  == Ego::Colour3f::blue());
    EXPECT_TRUE(DamageType_getColour(DAMAGE_POKE) == Ego::Colour3f::white());
}

TEST(LogicDamage, PhysicalTypesShareTheSameWhiteSingleton)
{
    // All three physical types alias the one Colour3f::white() singleton.
    EXPECT_EQ(&DamageType_getColour(DAMAGE_SLASH), &Ego::Colour3f::white());
    EXPECT_EQ(&DamageType_getColour(DAMAGE_SLASH), &DamageType_getColour(DAMAGE_CRUSH));
    EXPECT_EQ(&DamageType_getColour(DAMAGE_CRUSH), &DamageType_getColour(DAMAGE_POKE));
}

TEST(LogicDamage, GetColourThrowsOnDirectAndCount)
{
    EXPECT_THROW(DamageType_getColour(DAMAGE_DIRECT), std::runtime_error);
    EXPECT_THROW(DamageType_getColour(DAMAGE_COUNT), std::runtime_error);
}

// ---------------------------------------------------------------------------
// Ego::Attribute::toString
// ---------------------------------------------------------------------------

TEST(LogicAttribute, ToStringMapsNamedAttributes)
{
    EXPECT_EQ(A::toString(A::MIGHT), "Might");
    EXPECT_EQ(A::toString(A::SPELL_POWER), "Spell Power");
    EXPECT_EQ(A::toString(A::MAX_MANA), "Mana"); // not "Max Mana"
    EXPECT_EQ(A::toString(A::MAX_LIFE), "Life"); // not "Max Life"
    EXPECT_EQ(A::toString(A::ACCELERATION), "Speed"); // not "Acceleration"
    EXPECT_EQ(A::toString(A::SLASH_RESIST), "Slash Resist");
    EXPECT_EQ(A::toString(A::SENSE_KURSES), "Sense Kurses");
}

TEST(LogicAttribute, ToStringThrowsOnUnmappedAttribute)
{
    EXPECT_THROW(A::toString(A::MORPH), idlib::unhandled_switch_case_error);
    EXPECT_THROW(A::toString(A::NR_OF_ATTRIBUTES), idlib::unhandled_switch_case_error);
}

// ---------------------------------------------------------------------------
// resistFromDamageType / modifierFromDamageType
// ---------------------------------------------------------------------------

TEST(LogicAttribute, ResistFromDamageTypeMapsEachTypeElseSentinel)
{
    EXPECT_EQ(A::resistFromDamageType(DAMAGE_SLASH), A::SLASH_RESIST);
    EXPECT_EQ(A::resistFromDamageType(DAMAGE_POKE),  A::POKE_RESIST);
    EXPECT_EQ(A::resistFromDamageType(DAMAGE_CRUSH), A::CRUSH_RESIST);
    EXPECT_EQ(A::resistFromDamageType(DAMAGE_FIRE),  A::FIRE_RESIST);
    EXPECT_EQ(A::resistFromDamageType(DAMAGE_ICE),   A::ICE_RESIST);
    EXPECT_EQ(A::resistFromDamageType(DAMAGE_ZAP),   A::ZAP_RESIST);
    EXPECT_EQ(A::resistFromDamageType(DAMAGE_HOLY),  A::HOLY_RESIST);
    EXPECT_EQ(A::resistFromDamageType(DAMAGE_EVIL),  A::EVIL_RESIST);
    EXPECT_EQ(A::resistFromDamageType(DAMAGE_DIRECT), A::NR_OF_ATTRIBUTES);
    EXPECT_EQ(A::resistFromDamageType(DAMAGE_COUNT),  A::NR_OF_ATTRIBUTES);
}

TEST(LogicAttribute, ModifierFromDamageTypeMapsEachTypeElseSentinel)
{
    EXPECT_EQ(A::modifierFromDamageType(DAMAGE_SLASH), A::SLASH_MODIFIER);
    EXPECT_EQ(A::modifierFromDamageType(DAMAGE_POKE),  A::POKE_MODIFIER);
    EXPECT_EQ(A::modifierFromDamageType(DAMAGE_CRUSH), A::CRUSH_MODIFIER);
    EXPECT_EQ(A::modifierFromDamageType(DAMAGE_FIRE),  A::FIRE_MODIFIER);
    EXPECT_EQ(A::modifierFromDamageType(DAMAGE_ICE),   A::ICE_MODIFIER);
    EXPECT_EQ(A::modifierFromDamageType(DAMAGE_ZAP),   A::ZAP_MODIFIER);
    EXPECT_EQ(A::modifierFromDamageType(DAMAGE_HOLY),  A::HOLY_MODIFIER);
    EXPECT_EQ(A::modifierFromDamageType(DAMAGE_EVIL),  A::EVIL_MODIFIER);
    EXPECT_EQ(A::modifierFromDamageType(DAMAGE_DIRECT), A::NR_OF_ATTRIBUTES);
}

// ---------------------------------------------------------------------------
// isOverrideSetAttribute
// ---------------------------------------------------------------------------

TEST(LogicAttribute, IsOverrideSetTrueForOverrideGroup)
{
    EXPECT_TRUE(A::isOverrideSetAttribute(A::FLASHING_AND));
    EXPECT_TRUE(A::isOverrideSetAttribute(A::MORPH));
    EXPECT_TRUE(A::isOverrideSetAttribute(A::DAMAGE_TYPE));
    EXPECT_TRUE(A::isOverrideSetAttribute(A::SLASH_MODIFIER));
    EXPECT_TRUE(A::isOverrideSetAttribute(A::ZAP_MODIFIER));
    EXPECT_TRUE(A::isOverrideSetAttribute(A::NUMBER_OF_JUMPS));
    EXPECT_TRUE(A::isOverrideSetAttribute(A::LIFE_BARCOLOR));
    EXPECT_TRUE(A::isOverrideSetAttribute(A::SEE_INVISIBLE));
}

TEST(LogicAttribute, IsOverrideSetFalseForAdditiveGroup)
{
    // KEY contrast: *_MODIFIER attrs are override-set, but *_RESIST attrs are not.
    EXPECT_FALSE(A::isOverrideSetAttribute(A::MIGHT));
    EXPECT_FALSE(A::isOverrideSetAttribute(A::SLASH_RESIST));
    EXPECT_FALSE(A::isOverrideSetAttribute(A::ZAP_RESIST));
    EXPECT_FALSE(A::isOverrideSetAttribute(A::DEFENCE));
    EXPECT_FALSE(A::isOverrideSetAttribute(A::DAMAGE_BONUS));
    EXPECT_FALSE(A::isOverrideSetAttribute(A::ACCELERATION));
    EXPECT_FALSE(A::isOverrideSetAttribute(A::DARKVISION));
    EXPECT_FALSE(A::isOverrideSetAttribute(A::SENSE_KURSES));
}

} // namespace
