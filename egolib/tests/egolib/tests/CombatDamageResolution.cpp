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

/// @file CombatDamageResolution.cpp
/// @brief Characterization tests for the core combat damage-scaling math on a live Object.
/// @details Pins the observable behavior of the resistance/reduction/invictus surface in
///          `Entities/Object_combat.cpp` against a spawned Object whose driving inputs (the
///          per-damage-type resist/modifier attributes, the DEFENCE attribute) are fully
///          controlled via `setBaseAttribute`. These functions read those values back through
///          `getAttribute`, which — for the resist/modifier/DEFENCE attribute types and an
///          object with no temporary (enchant) attributes — returns the base value unchanged
///          (none of them hit the special cases in `Object::getAttribute`). Expected values are
///          hand-derived from the verbatim implementations, not from the formulas re-run in the
///          test. Uses the proven ContentRuntimeBootstrap + spawn-a-live-follower fixture
///          (mirrors ObjectAccessors.cpp); no GL context / GameEngine::initialize is needed.

#include "gtest/gtest.h"

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#define private public
#include "egolib/Entities/_Include.hpp"
#include "egolib/Profiles/_Include.hpp"
#undef private
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Module/Module.hpp"  // GameModule::spawnObject (real spawn path)
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/Logic/Damage.hpp"
#include "egolib/Logic/Attribute.hpp"
#include "egolib/Physics/PhysicalConstants.hpp"  // Ego::Physics::CHR_INFINITE_WEIGHT
#include "egolib/typedef.h"
#include "egolib/vfs.h"

#include <cmath>
#include <memory>
#include <stdexcept>

namespace
{
class CombatDamageFixture : public ::testing::Test
{
protected:
    static std::unique_ptr<ContentRuntimeBootstrap> s_runtime;

    ObjectHandler _objectHandler;

    static void SetUpTestSuite()
    {
        Ego::Test::configureDataDirectory();

        ContentRuntimeBootstrap::Options opts;
        opts.initializeVirtualFileSystem = true;
        opts.initializeBaseVfsPaths = true;
        opts.initializeLogging = true;
        opts.configureLightweightProfileLoading = true;
        opts.initializeImageManager = true;
        opts.initializePerkHandler = true;
        opts.initializeProfileSystem = true;
        opts.clearModuleVfsPathsOnShutdown = true;
        opts.clearBaseVfsPathsOnShutdown = true;
        opts.seedRandom = true;
        opts.randomSeed = 17;
        opts.binaryPath = "";
        opts.logPath = "/debug/combat-damage-tests.log";
        opts.logLevel = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);

        setenv("EGOBOO_DISABLE_AUDIO", "1", 1);
        AudioSystem::initialize(EngineContext::get().config(), EngineContext::get().logTarget());
        EngineContext::get().installAudioSystem(AudioSystem::get());
        ParticleHandler::initialize();
        EngineContext::get().installParticleHandler(ParticleHandler::get());
    }

    static void TearDownTestSuite()
    {
        EngineContext::get().clearParticleHandler();
        ParticleHandler::uninitialize();
        EngineContext::get().clearAudioSystem();
        AudioSystem::uninitialize();
        s_runtime.reset();
    }

    void SetUp() override
    {
        if (GameSessionContext::get().hasActiveModule())
        {
            GameSessionContext::get().quitModule();
        }

        EngineContext::get().profileSystem().reset();
        EngineContext::get().profileSystem().loadModuleProfiles();
        setup_init_module_vfs_paths("mp_modules/test.mod");
        GameSessionContext::get().publishLocalPlayerPerception(LocalPlayerPerceptionState{});
    }

    void TearDown() override
    {
        if (GameSessionContext::get().hasActiveModule())
        {
            GameSessionContext::get().quitModule();
        }

        setup_clear_module_vfs_paths();
    }

    /// Spawn a plain live follower object (no active module required for these
    /// object-local damage-math queries).
    std::shared_ptr<Object> makeFollower(int slot)
    {
        const ObjectProfileRef profile =
            EngineContext::get().profileSystem().loadOneProfile("mp_objects/follower.obj", slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid)
        {
            return nullptr;
        }

        return _objectHandler.insert(profile);
    }

    /// Begin the live test module (needed for the real spawn path, which initialises an
    /// object's runtime physics — weight promotion, bump dampen — from its profile).
    GameModule& beginActiveTestModule()
    {
        std::shared_ptr<ModuleProfile> module;
        for (const auto& candidate : EngineContext::get().profileSystem().getModuleProfiles())
        {
            if (candidate && candidate->getFolderName() == "test.mod")
            {
                module = candidate;
                break;
            }
        }
        EXPECT_NE(module, nullptr);
        if (module == nullptr)
        {
            throw std::runtime_error("test.mod profile not found");
        }
        EXPECT_TRUE(GameSessionContext::get().beginModule(module, 17));
        return GameSessionContext::get().activeModule();
    }

    /// Spawn an object through the module (the real spawn path that initialises runtime physics).
    std::shared_ptr<Object> spawnInModule(GameModule& module, const std::string& profilePath, int slot)
    {
        const ObjectProfileRef profile =
            EngineContext::get().profileSystem().loadOneProfile(profilePath, slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid)
        {
            return nullptr;
        }
        const ObjectRef objectRef = module.spawnObjectRef(Ego::Vector3f(64.0f, 64.0f, 0.0f), profile,
                                                          static_cast<TEAM_REF>(Team::TEAM_NULL), 0, Facing(0), "", ObjectRef::Invalid);
        return module.getObjectHandler().getHandle(objectRef);
    }

    /// Fully control the three inputs the resistance/reduction math reads for `type`,
    /// using the same DamageType->Attribute maps the implementation uses, so no profile
    /// default can leak in. Returns the object for convenience.
    void primeDamageInputs(const std::shared_ptr<Object>& object, DamageType type,
                           float resist, float defence, float modifierBits) const
    {
        object->setBaseAttribute(Ego::Attribute::resistFromDamageType(type), resist);
        object->setBaseAttribute(Ego::Attribute::DEFENCE, defence);
        object->setBaseAttribute(Ego::Attribute::modifierFromDamageType(type), modifierBits);
    }
};

std::unique_ptr<ContentRuntimeBootstrap> CombatDamageFixture::s_runtime;

// ---------------------------------------------------------------------------
// Object::getRawDamageResistance
// ---------------------------------------------------------------------------

TEST_F(CombatDamageFixture, RawResistance_DamageTypeOutOfRangeReturnsZero)
{
    auto object = makeFollower(3100);
    ASSERT_NE(object, nullptr);

    // The guard `if (type >= DAMAGE_COUNT) return 0` fires before any attribute lookup,
    // so DAMAGE_DIRECT (0xFF) and DAMAGE_COUNT both short-circuit to exactly 0.
    EXPECT_FLOAT_EQ(object->getRawDamageResistance(DAMAGE_DIRECT, true), 0.0f);
    EXPECT_FLOAT_EQ(object->getRawDamageResistance(DAMAGE_COUNT, true), 0.0f);
    EXPECT_FLOAT_EQ(object->getRawDamageResistance(DAMAGE_DIRECT, false), 0.0f);
}

TEST_F(CombatDamageFixture, RawResistance_PositivePassesThroughUnchangedWithoutArmor)
{
    auto object = makeFollower(3101);
    ASSERT_NE(object, nullptr);
    ASSERT_FALSE(object->hasPerk(Ego::Perks::STALWART));

    primeDamageInputs(object, DAMAGE_SLASH, /*resist*/ 5.0f, /*defence*/ 200.0f, /*modifier*/ 0.0f);

    // includeArmor == false skips the DEFENCE bonus entirely: the raw resist is returned.
    EXPECT_FLOAT_EQ(object->getRawDamageResistance(DAMAGE_SLASH, false), 5.0f);
}

TEST_F(CombatDamageFixture, RawResistance_PositiveGainsDefenceBonusWithArmor)
{
    auto object = makeFollower(3102);
    ASSERT_NE(object, nullptr);
    ASSERT_FALSE(object->hasPerk(Ego::Perks::STALWART));

    // resist 5 + DEFENCE/14 == 5 + 14/14 == 6 (modifier has no DAMAGEINVERT bit).
    primeDamageInputs(object, DAMAGE_SLASH, /*resist*/ 5.0f, /*defence*/ 14.0f, /*modifier*/ 0.0f);

    EXPECT_FLOAT_EQ(object->getRawDamageResistance(DAMAGE_SLASH, true), 6.0f);
}

TEST_F(CombatDamageFixture, RawResistance_DamageInvertModifierSuppressesDefenceBonus)
{
    auto object = makeFollower(3103);
    ASSERT_NE(object, nullptr);
    ASSERT_FALSE(object->hasPerk(Ego::Perks::STALWART));

    // With the DAMAGEINVERT bit set in the modifier the DEFENCE bonus is gated off,
    // so the positive resist is returned unchanged even with armor included.
    primeDamageInputs(object, DAMAGE_SLASH, /*resist*/ 5.0f, /*defence*/ 14.0f,
                      /*modifier*/ static_cast<float>(DAMAGEINVERT));

    EXPECT_FLOAT_EQ(object->getRawDamageResistance(DAMAGE_SLASH, true), 5.0f);
}

TEST_F(CombatDamageFixture, RawResistance_NegativeIsWeaknessUnreducedWithoutArmor)
{
    auto object = makeFollower(3104);
    ASSERT_NE(object, nullptr);
    ASSERT_FALSE(object->hasPerk(Ego::Perks::STALWART));

    // Negative resistance is a weakness; without armor it passes through untouched.
    primeDamageInputs(object, DAMAGE_SLASH, /*resist*/ -3.0f, /*defence*/ 256.0f, /*modifier*/ 0.0f);

    EXPECT_FLOAT_EQ(object->getRawDamageResistance(DAMAGE_SLASH, false), -3.0f);
}

TEST_F(CombatDamageFixture, RawResistance_NegativeWeaknessIsSoftenedByDefenceWithArmor)
{
    auto object = makeFollower(3105);
    ASSERT_NE(object, nullptr);
    ASSERT_FALSE(object->hasPerk(Ego::Perks::STALWART));

    // Weakness with armor: resist *= 1 - DEFENCE/512.  -4 * (1 - 256/512) == -4 * 0.5 == -2.
    primeDamageInputs(object, DAMAGE_SLASH, /*resist*/ -4.0f, /*defence*/ 256.0f, /*modifier*/ 0.0f);

    EXPECT_FLOAT_EQ(object->getRawDamageResistance(DAMAGE_SLASH, true), -2.0f);
}

// ---------------------------------------------------------------------------
// Object::getDamageReduction
// ---------------------------------------------------------------------------

TEST_F(CombatDamageFixture, DamageReduction_DamageTypeOutOfRangeReturnsZero)
{
    auto object = makeFollower(3110);
    ASSERT_NE(object, nullptr);

    EXPECT_FLOAT_EQ(object->getDamageReduction(DAMAGE_DIRECT, true), 0.0f);
    EXPECT_FLOAT_EQ(object->getDamageReduction(DAMAGE_COUNT, true), 0.0f);
}

TEST_F(CombatDamageFixture, DamageReduction_InvictusModifierGivesFullImmunity)
{
    auto object = makeFollower(3111);
    ASSERT_NE(object, nullptr);

    // The DAMAGEINVICTUS modifier bit short-circuits to full (1.0) reduction *before*
    // resistance is even computed, so the resist value is irrelevant.
    primeDamageInputs(object, DAMAGE_POKE, /*resist*/ 5.0f, /*defence*/ 0.0f,
                      /*modifier*/ static_cast<float>(DAMAGEINVICTUS));

    EXPECT_FLOAT_EQ(object->getDamageReduction(DAMAGE_POKE, true), 1.0f);
    EXPECT_FLOAT_EQ(object->getDamageReduction(DAMAGE_POKE, false), 1.0f);
}

TEST_F(CombatDamageFixture, DamageReduction_PositiveResistanceFollowsDiminishingCurve)
{
    auto object = makeFollower(3112);
    ASSERT_NE(object, nullptr);
    ASSERT_FALSE(object->hasPerk(Ego::Perks::STALWART));

    // reduction = (r*0.06)/(1 + r*0.06).  r == 10 (no-armor) => 0.6/1.6 == 0.375.
    primeDamageInputs(object, DAMAGE_POKE, /*resist*/ 10.0f, /*defence*/ 0.0f, /*modifier*/ 0.0f);

    EXPECT_FLOAT_EQ(object->getDamageReduction(DAMAGE_POKE, false), 0.375f);
}

TEST_F(CombatDamageFixture, DamageReduction_NegativeResistanceAmplifiesDamage)
{
    auto object = makeFollower(3113);
    ASSERT_NE(object, nullptr);
    ASSERT_FALSE(object->hasPerk(Ego::Perks::STALWART));

    // Negative resistance amplifies: reduction = 1 - 0.94^r.  r == -2 => 1 - 0.94^-2
    // == 1 - 1.131734 == -0.131734 (a negative reduction => the object takes *more* damage).
    primeDamageInputs(object, DAMAGE_POKE, /*resist*/ -2.0f, /*defence*/ 0.0f, /*modifier*/ 0.0f);

    const float reduction = object->getDamageReduction(DAMAGE_POKE, false);
    EXPECT_LT(reduction, 0.0f);
    EXPECT_NEAR(reduction, -0.131734f, 1.0e-4f);
}

// ---------------------------------------------------------------------------
// Object::isInvictusDirection
// ---------------------------------------------------------------------------

TEST_F(CombatDamageFixture, InvictusDirection_InvincibleObjectIsInvictusFromEveryDirection)
{
    auto object = makeFollower(3120);
    ASSERT_NE(object, nullptr);

    // The `isInvincible()` flag is the first short-circuit: an invincible object is
    // invictus from every facing, independent of frame / profile arc geometry.
    object->setInvincible(true);
    EXPECT_TRUE(object->isInvictusDirection(Facing(0x0000)));
    EXPECT_TRUE(object->isInvictusDirection(Facing(0x4000)));
    EXPECT_TRUE(object->isInvictusDirection(Facing(0x8000)));
    EXPECT_TRUE(object->isInvictusDirection(Facing(0xC000)));
    EXPECT_TRUE(object->isInvictusDirection(Facing(0xFFFF)));
}

// ---------------------------------------------------------------------------
// ObjectPhysics::getMass — immovable scenery collision mass
// ---------------------------------------------------------------------------

// Regression guard for commit c0fd22e53, which dropped the "weight 255 -> infinite collision mass"
// rule from getMass() in favour of "immovability comes from bumpdampen == 0". That broke the tent:
// it is the one immovable scenery object that marks itself immovable purely with Weight 255
// (CAP_INFINITE_WEIGHT) and a NON-ZERO bump dampen (0.1) — every other immovable object also sets
// bumpdampen 0.0. Without the weight rule the tent got a finite mass of 255/0.1 = 2550 and a
// colliding character could shove it. getMass() must return an infinite (negative sentinel) mass.
TEST_F(CombatDamageFixture, Weight255SceneryHasInfiniteCollisionMass)
{
    auto& module = beginActiveTestModule();
    auto tent = spawnInModule(module, "mp_data/globalobjects/misc/tent.obj", 3500);
    ASSERT_NE(tent, nullptr);

    // The content preconditions the regression hinges on: weight 255 with a non-zero bump dampen.
    EXPECT_EQ(tent->getProfile()->getWeight(), CAP_INFINITE_WEIGHT);
    EXPECT_GT(tent->getProfile()->getBumpDampen(), 0.0f);
    // Weight 255 is promoted to the infinite-weight sentinel at spawn (Module_spawn.cpp).
    EXPECT_EQ(tent->getPhysicsWeight(), Ego::Physics::CHR_INFINITE_WEIGHT);

    // The actual guard: an infinite (negative) collision mass => immovable on character collision.
    EXPECT_LT(tent->getMass(), 0.0f);

    // Control: an ordinary finite-weight follower has a finite, positive collision mass (movable).
    auto follower = spawnInModule(module, "mp_objects/follower.obj", 3501);
    ASSERT_NE(follower, nullptr);
    EXPECT_NE(follower->getPhysicsWeight(), Ego::Physics::CHR_INFINITE_WEIGHT);  // finite weight
    EXPECT_GT(follower->getMass(), 0.0f);
}

} // namespace
