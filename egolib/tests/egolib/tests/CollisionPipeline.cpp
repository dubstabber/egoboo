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

/// @file CollisionPipeline.cpp
/// @brief Characterization tests for the collision *pipeline* on live-spawned entities.
/// @details Pins the observable behavior of the two public collision entry points so a future
///          refactor (extending the lower-layer `Ego::Physics::ICollisionWorld` DIP seam to cover
///          the `GameModule` mesh queries the physics TUs use) can be verified behavior-preserving:
///
///          - chr-prt: `do_chr_prt_collision(objectRef, particleRef, tmin, tmax)` (game/Physics/particle_collision.h)
///          - chr-chr: `Ego::Physics::CollisionSystem::{detectCollision,handleCollision,handlePlatformCollision,
///            handleMountingCollision}` (game/Physics/CollisionSystem.hpp)
///
///          The fixture mirrors `CombatDamageIntegration.cpp` (fully headless: no GL context /
///          GameEngine::initialize; audio disabled; ParticleHandler + a recording StubBillboardSystem
///          installed; live `test.mod` begun so object/particle lookups + the installed ICollisionWorld
///          resolve). It additionally initializes the `CollisionSystem` singleton.
///
///          Determinism (mirrors CombatDamageIntegration): a damage particle is spawned and then its
///          PUBLIC instance fields are overridden (`damage.base`/`damage.rand=0`, `damagetype`,
///          `team`, `bump_size_stt`) so the rolled damage is `Random::next(x,x) == x` and the
///          collision volume is non-degenerate. With `owner_ref == Invalid` the damage path skips
///          every perk/crit/intellect branch, and with reduction primed to 0 the life delta is
///          exactly `FP8_TO_FLOAT(damage.base)` — hand-derived, not re-run. Every collision decision
///          asserted here is pure position/velocity/collision-volume/team math (no mesh/wall query).

#include "gtest/gtest.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#define private public
#include "egolib/Entities/_Include.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Graphics/Billboard.hpp"
#include "egolib/game/Physics/CollisionSystem.hpp"
#include "egolib/game/Module/Module.hpp"
#undef private
#include "egolib/game/Physics/particle_collision.h"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/Graphics/IBillboardSystem.hpp"
#include "egolib/Logic/Attribute.hpp"
#include "egolib/Logic/Damage.hpp"
#include "egolib/Logic/ObjectSlot.hpp"
#include "egolib/Logic/Team.hpp"
#include "egolib/Script/script.h"
#include "egolib/typedef.h"
#include "egolib/vfs.h"

namespace
{

/// Recording no-op billboard system; combat/collision side-edges reach the billboard service
/// (e.g. the dodge/grog/daze feedback in particle_collision.c), so install a stub rather than
/// leave the service uninstalled.
class StubBillboardSystem : public Ego::Graphics::IBillboardSystem
{
public:
    void update() override {}
    void reset() override {}
    std::shared_ptr<Ego::Graphics::Billboard> makeBillboard(ObjectRef, const std::string&,
                                                            const Ego::Colour4f&, const Ego::Colour4f&,
                                                            int, const BIT_FIELD, float) override
    {
        ++makeBillboardCalls;
        return nullptr;
    }

    int makeBillboardCalls = 0;
};

class CollisionPipelineFixture : public ::testing::Test
{
protected:
    static std::unique_ptr<ContentRuntimeBootstrap> s_runtime;

    StubBillboardSystem _billboard;

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
        opts.randomSeed = 23;
        opts.binaryPath = "";
        opts.logPath = "/debug/collision-pipeline-tests.log";
        opts.logLevel = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);

        setenv("EGOBOO_DISABLE_AUDIO", "1", 1);
        AudioSystem::initialize(EngineContext::get().config(), EngineContext::get().logTarget());
        EngineContext::get().installAudioSystem(AudioSystem::get());
        ParticleHandler::initialize();
        EngineContext::get().installParticleHandler(ParticleHandler::get());
        Ego::Physics::CollisionSystem::initialize();
    }

    static void TearDownTestSuite()
    {
        Ego::Physics::CollisionSystem::uninitialize();
        EngineContext::get().clearParticleHandler();
        ParticleHandler::uninitialize();
        EngineContext::get().clearAudioSystem();
        AudioSystem::uninitialize();
        s_runtime.reset();
    }

    void SetUp() override
    {
        auto& session = GameSessionContext::get();
        if (session.hasActiveModule())
        {
            session.quitModule();
        }

        EngineContext::get().profileSystem().reset();
        EngineContext::get().profileSystem().loadModuleProfiles();
        setup_init_module_vfs_paths("mp_modules/test.mod");
        session.publishLocalPlayerPerception(LocalPlayerPerceptionState{});

        EngineContext::get().clearBillboardSystem();
        EngineContext::get().installBillboardSystem(_billboard);
    }

    void TearDown() override
    {
        auto& session = GameSessionContext::get();
        if (session.hasActiveModule())
        {
            session.quitModule();
        }

        EngineContext::get().clearBillboardSystem();
        setup_clear_module_vfs_paths();
    }

    std::shared_ptr<ModuleProfile> findTestModule() const
    {
        for (const auto& module : EngineContext::get().profileSystem().getModuleProfiles())
        {
            if (module && module->getFolderName() == "test.mod")
            {
                return module;
            }
        }
        return nullptr;
    }

    GameModule& beginActiveTestModule()
    {
        auto module = findTestModule();
        EXPECT_NE(module, nullptr);
        if (module == nullptr)
        {
            throw std::runtime_error("test.mod profile not found");
        }

        auto& session = GameSessionContext::get();
        const bool began = session.beginModule(module, 23);
        EXPECT_TRUE(began);
        return session.activeModule();
    }

    /// Load the stock follower profile into a unique slot (Life=10, Blud=False, not a player,
    /// not a mount, not a platform).
    ObjectProfileRef loadFollowerProfile(int slot) const
    {
        const ObjectProfileRef profile =
            EngineContext::get().profileSystem().loadOneProfile("mp_objects/follower.obj", slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        return profile;
    }

    /// Spawn a live follower of the given profile on the given team at the given position.
    std::shared_ptr<Object> spawnFollower(GameModule& module, ObjectProfileRef profile,
                                          TEAM_REF team, const Ego::Vector3f& pos) const
    {
        if (profile == ObjectProfileRef::Invalid)
        {
            return nullptr;
        }
        const ObjectRef objectRef = module.spawnObjectRef(pos, profile, team, 0, Facing(0), "", ObjectRef::Invalid);
        auto object = module.getObjectHandler().getHandle(objectRef);
        if (object)
        {
            // The collision volume (chr_min_cv) is normally computed during the per-frame physics
            // update, which never runs in this headless fixture; populate it so the geometry gate
            // in the collision pipeline sees a real volume rather than a degenerate one.
            object->updateCollisionSize(true);
        }
        return object;
    }

    /// Spawn a live damage particle (follower-local pip 0) and force it deterministic: exact FP8
    /// damage, rand 0, chosen damage type/team, a non-degenerate collision volume, positioned to
    /// overlap @a pos. owner_ref is left Invalid so the damage path skips all perk/crit branches.
    std::shared_ptr<Ego::Particle> spawnDamageParticle(ObjectProfileRef profile,
                                                       const Ego::Vector3f& pos, TEAM_REF team,
                                                       uint32_t fp8Damage, DamageType type) const
    {
        auto p = ParticleHandler::get().spawnLocalParticle(
            pos, Facing(0), profile, LocalParticleProfileRef(0), ObjectRef::Invalid,
            static_cast<uint16_t>(GRIP_LAST), team, ObjectRef::Invalid, ParticleRef::Invalid, 0,
            ObjectRef::Invalid);
        EXPECT_NE(p, Ego::Particle::INVALID_PARTICLE);
        if (p == Ego::Particle::INVALID_PARTICLE)
        {
            return nullptr;
        }

        p->damage.base = static_cast<int>(fp8Damage);
        p->damage.rand = 0;
        p->damagetype = type;
        p->team = team;
        // The follower has no local pips, so LocalParticleProfileRef(0) resolves to a default pip;
        // normalize the two profile bits the damage path keys on so the hit is deterministic on a
        // freshly-spawned target (per-test-fresh profile, one particle per test):
        //   - clear DAMFX_ARRO ("only hurts the one it is attached to"), else a non-attached hit
        //     skips the damage block (particle_collision.c:757);
        //   - set DAMFX_NBLOC ("ignore shielding"), so chr_is_invictus is false regardless of the
        //     attack direction and the hit does not take the invictus/defence-ping deflect branch
        //     (particle_collision.c:533) — the deflect/invincible behavior is pinned separately.
        p->getProfile()->_particleEffectBits[static_cast<size_t>(DAMFX_ARRO)] = false;
        p->getProfile()->_particleEffectBits[static_cast<size_t>(DAMFX_NBLOC)] = true;
        // Give it a usable hit-box (the stock pip has bump size 0 -> degenerate cv -> the geometry
        // gate would early-return). 0x4000 == 64.0 in 8.8 fixed point.
        p->bump_size_stt = 0x4000;
        p->setSize(static_cast<int>(p->bump_size_stt));
        p->setPosition(pos);
        return p;
    }

    /// Pin `getDamageReduction(type, *) == 0`: zero the resist, the DEFENCE bonus source, and the
    /// per-type modifier bits, so `actual_damage == base_damage` for `type`.
    void primeNoReduction(const std::shared_ptr<Object>& object, DamageType type) const
    {
        object->setBaseAttribute(Ego::Attribute::resistFromDamageType(type), 0.0f);
        object->setBaseAttribute(Ego::Attribute::DEFENCE, 0.0f);
        object->setBaseAttribute(Ego::Attribute::modifierFromDamageType(type), 0.0f);
    }
};

std::unique_ptr<ContentRuntimeBootstrap> CollisionPipelineFixture::s_runtime;

// ---------------------------------------------------------------------------
// chr-prt: a damaging particle hit subtracts exactly the hand-derived life.
// ---------------------------------------------------------------------------

TEST_F(CollisionPipelineFixture, ChrPrt_DamagingHit_SubtractsExactLife)
{
    auto& module = beginActiveTestModule();
    // The victim must be on a NON-NULL team: a TEAM_NULL particle can only attack a non-neutral
    // character (the friend-foe gate in do_chr_prt_collision_bump requires chr_team != TEAM_NULL
    // when prt_team == TEAM_NULL).
    const ObjectProfileRef profile = loadFollowerProfile(6501);
    auto victim = spawnFollower(module, profile, static_cast<TEAM_REF>(Team::TEAM_GOOD),
                                Ego::Vector3f(64.0f, 64.0f, 0.0f));
    ASSERT_NE(victim, nullptr);
    ASSERT_TRUE(victim->isAlive());

    primeNoReduction(victim, DAMAGE_SLASH);
    victim->setDamageTimer(0);

    // base damage 512 (FP8 == 2.0 life points), rand 0, owner Invalid (no perk/crit branches).
    // Offset the particle from the victim so the attack direction is well-defined and outside the
    // victim's invictus arc (a particle at the victim's exact origin degenerates vec_to_facing(0,0)
    // to ATK_BEHIND, which triggers isInvictusDirection -> a defence-ping deflect, not damage). The
    // collision volumes still overlap (victim cv ~X[37,96] Z[-1,91]; particle cv +/-25 around here).
    auto particle = spawnDamageParticle(profile,
                                        victim->getPosition() + Ego::Vector3f(-20.0f, 0.0f, 40.0f),
                                        static_cast<TEAM_REF>(Team::TEAM_NULL), 512, DAMAGE_SLASH);
    ASSERT_NE(particle, nullptr);

    const float lifeBefore = victim->getLife();
    const bool result = do_chr_prt_collision(victim->getObjRef(), particle->getParticleID(), -1.0f, 1.0f);

    EXPECT_TRUE(result);
    // _currentLife -= FP8_TO_FLOAT(512) == 2.0 (reduction 0, ignoreArmour via DAMFX_ARMO path).
    EXPECT_FLOAT_EQ(victim->getLife(), lifeBefore - 2.0f);
    // A successful damaging hit records the collision so the same particle cannot re-hit.
    EXPECT_TRUE(particle->hasCollided(victim->getObjRef()));
}

// ---------------------------------------------------------------------------
// chr-prt: the same particle cannot damage the same target twice (hasCollided gate).
// ---------------------------------------------------------------------------

TEST_F(CollisionPipelineFixture, ChrPrt_ReHitSuppressed_HasCollidedGate)
{
    auto& module = beginActiveTestModule();
    const ObjectProfileRef profile = loadFollowerProfile(6502);
    auto victim = spawnFollower(module, profile, static_cast<TEAM_REF>(Team::TEAM_GOOD),
                               Ego::Vector3f(64.0f, 64.0f, 0.0f));
    ASSERT_NE(victim, nullptr);

    primeNoReduction(victim, DAMAGE_SLASH);
    victim->setDamageTimer(0);
    auto particle = spawnDamageParticle(profile,
                                        victim->getPosition() + Ego::Vector3f(-20.0f, 0.0f, 40.0f),
                                        static_cast<TEAM_REF>(Team::TEAM_NULL), 512, DAMAGE_SLASH);
    ASSERT_NE(particle, nullptr);

    // First hit lands.
    ASSERT_TRUE(do_chr_prt_collision(victim->getObjRef(), particle->getParticleID(), -1.0f, 1.0f));
    const float lifeAfterFirst = victim->getLife();
    ASSERT_TRUE(particle->hasCollided(victim->getObjRef()));

    // The hit re-armed the damage timer; clear it so only the hasCollided gate (not the timer)
    // can suppress the second hit. The same non-eternal particle has already collided with this
    // target, so do_chr_prt_collision_bump rejects it (particle_collision.c:934).
    victim->setDamageTimer(0);
    const bool second = do_chr_prt_collision(victim->getObjRef(), particle->getParticleID(), -1.0f, 1.0f);

    EXPECT_FALSE(second);
    EXPECT_FLOAT_EQ(victim->getLife(), lifeAfterFirst);
}

// ---------------------------------------------------------------------------
// chr-prt: an invincible target deflects the hit (no life loss, no collision recorded).
// ---------------------------------------------------------------------------

TEST_F(CollisionPipelineFixture, ChrPrt_InvincibleTarget_NoDamage)
{
    auto& module = beginActiveTestModule();
    const ObjectProfileRef profile = loadFollowerProfile(6503);
    auto victim = spawnFollower(module, profile, static_cast<TEAM_REF>(Team::TEAM_GOOD),
                               Ego::Vector3f(64.0f, 64.0f, 0.0f));
    ASSERT_NE(victim, nullptr);

    primeNoReduction(victim, DAMAGE_SLASH);
    victim->setDamageTimer(0);
    victim->setInvincible(true);

    auto particle = spawnDamageParticle(profile,
                                        victim->getPosition() + Ego::Vector3f(-20.0f, 0.0f, 40.0f),
                                        static_cast<TEAM_REF>(Team::TEAM_NULL), 512, DAMAGE_SLASH);
    ASSERT_NE(particle, nullptr);

    const float lifeBefore = victim->getLife();
    const bool result = do_chr_prt_collision(victim->getObjRef(), particle->getParticleID(), -1.0f, 1.0f);

    // do_chr_prt_collision_deflect returns true for an invincible target (particle_collision.c:518),
    // so the collision is "handled" (result true) but the damage branch is skipped.
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(victim->getLife(), lifeBefore);
    EXPECT_FALSE(particle->hasCollided(victim->getObjRef()));
}

// ---------------------------------------------------------------------------
// chr-prt: an active missile-treatment enchant can deflect an incoming particle.
// ---------------------------------------------------------------------------

TEST_F(CollisionPipelineFixture, ChrPrt_EnchantMissileTreatmentDeflectsParticle)
{
    auto& module = beginActiveTestModule();
    const ObjectProfileRef victimProfile = loadFollowerProfile(6515);
    auto victim = spawnFollower(module, victimProfile, static_cast<TEAM_REF>(Team::TEAM_GOOD),
                               Ego::Vector3f(64.0f, 64.0f, 0.0f));
    auto owner = spawnFollower(module, loadFollowerProfile(6516), static_cast<TEAM_REF>(Team::TEAM_GOOD),
                              Ego::Vector3f(96.0f, 64.0f, 0.0f));
    ASSERT_NE(victim, nullptr);
    ASSERT_NE(owner, nullptr);

    const ENC_REF enchantRef = EngineContext::get().profileSystem().loadEnchantProfile(
        "mp_data/globalobjects/magic/metamorph.obj/enchant.txt",
        INVALID_EVE_REF);
    ASSERT_LT(enchantRef, ENCHANTPROFILES_MAX);
    auto enchant = victim->addEnchant(enchantRef,
                                      owner->getProfileID().get(),
                                      owner->getObjRef(),
                                      owner->getObjRef());
    ASSERT_NE(enchant, nullptr);
    ASSERT_EQ(enchant->getMissileTreatment(), MissileTreatment_Deflect);

    primeNoReduction(victim, DAMAGE_SLASH);
    victim->setDamageTimer(1);
    auto particle = spawnDamageParticle(victimProfile,
                                        victim->getPosition() + Ego::Vector3f(-20.0f, 0.0f, 40.0f),
                                        static_cast<TEAM_REF>(Team::TEAM_NULL), 512, DAMAGE_SLASH);
    ASSERT_NE(particle, nullptr);
    particle->setHoming(true);

    const float lifeBefore = victim->getLife();
    const bool result = do_chr_prt_collision(victim->getObjRef(), particle->getParticleID(), -1.0f, 1.0f);

    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(victim->getLife(), lifeBefore);
    EXPECT_FALSE(particle->isHoming());
    EXPECT_FALSE(particle->hasCollided(victim->getObjRef()));
}

// ---------------------------------------------------------------------------
// chr-prt: a TEAM_NULL particle cannot damage a TEAM_NULL (neutral) target (friend-foe gate).
// ---------------------------------------------------------------------------

TEST_F(CollisionPipelineFixture, ChrPrt_NeutralTargetRejectsNeutralParticle)
{
    auto& module = beginActiveTestModule();
    const ObjectProfileRef profile = loadFollowerProfile(6504);
    // Target on the NULL (neutral) team: the neutral-attacks-anything clause requires the target
    // team != TEAM_NULL (particle_collision.c:964), so a TEAM_NULL particle does not bump it.
    auto victim = spawnFollower(module, profile, static_cast<TEAM_REF>(Team::TEAM_NULL),
                               Ego::Vector3f(64.0f, 64.0f, 0.0f));
    ASSERT_NE(victim, nullptr);

    primeNoReduction(victim, DAMAGE_SLASH);
    victim->setDamageTimer(0);
    auto particle = spawnDamageParticle(profile,
                                        victim->getPosition() + Ego::Vector3f(-20.0f, 0.0f, 40.0f),
                                        static_cast<TEAM_REF>(Team::TEAM_NULL), 512, DAMAGE_SLASH);
    ASSERT_NE(particle, nullptr);

    const float lifeBefore = victim->getLife();
    const bool result = do_chr_prt_collision(victim->getObjRef(), particle->getParticleID(), -1.0f, 1.0f);

    EXPECT_FALSE(result);
    EXPECT_FLOAT_EQ(victim->getLife(), lifeBefore);
    EXPECT_FALSE(particle->hasCollided(victim->getObjRef()));
}

// ---------------------------------------------------------------------------
// chr-prt: a spatially-separated particle does not interact (geometry gate).
// ---------------------------------------------------------------------------

TEST_F(CollisionPipelineFixture, ChrPrt_NonOverlappingParticle_NoInteraction)
{
    auto& module = beginActiveTestModule();
    const ObjectProfileRef profile = loadFollowerProfile(6505);
    auto victim = spawnFollower(module, profile, static_cast<TEAM_REF>(Team::TEAM_GOOD),
                               Ego::Vector3f(64.0f, 64.0f, 0.0f));
    ASSERT_NE(victim, nullptr);

    primeNoReduction(victim, DAMAGE_SLASH);
    victim->setDamageTimer(0);
    // Far away (real collision volume, purely positional miss): the geometry gate
    // do_chr_prt_collision_get_details produces depth_min <= 0 and the call early-returns.
    auto particle = spawnDamageParticle(profile,
                                        victim->getPosition() + Ego::Vector3f(1000.0f, 0.0f, 0.0f),
                                        static_cast<TEAM_REF>(Team::TEAM_NULL), 512, DAMAGE_SLASH);
    ASSERT_NE(particle, nullptr);

    const float lifeBefore = victim->getLife();
    const bool result = do_chr_prt_collision(victim->getObjRef(), particle->getParticleID(), -1.0f, 1.0f);

    EXPECT_FALSE(result);
    EXPECT_FLOAT_EQ(victim->getLife(), lifeBefore);
}

// ---------------------------------------------------------------------------
// chr-prt: ref-based entry points fail cleanly for stale object/particle ids.
// ---------------------------------------------------------------------------

TEST_F(CollisionPipelineFixture, ChrPrt_InvalidObjectRef_ReturnsFalse)
{
    beginActiveTestModule();
    const ObjectProfileRef profile = loadFollowerProfile(6512);
    auto particle = spawnDamageParticle(profile,
                                        Ego::Vector3f(64.0f, 64.0f, 40.0f),
                                        static_cast<TEAM_REF>(Team::TEAM_NULL), 512, DAMAGE_SLASH);
    ASSERT_NE(particle, nullptr);

    EXPECT_FALSE(do_chr_prt_collision(ObjectRef::Invalid, particle->getParticleID(), -1.0f, 1.0f));
}

TEST_F(CollisionPipelineFixture, ChrPrt_InvalidParticleRef_ReturnsFalse)
{
    auto& module = beginActiveTestModule();
    const ObjectProfileRef profile = loadFollowerProfile(6513);
    auto victim = spawnFollower(module, profile, static_cast<TEAM_REF>(Team::TEAM_GOOD),
                               Ego::Vector3f(64.0f, 64.0f, 0.0f));
    ASSERT_NE(victim, nullptr);

    EXPECT_FALSE(do_chr_prt_collision(victim->getObjRef(), ParticleRef::Invalid, -1.0f, 1.0f));
}

TEST_F(CollisionPipelineFixture, ChrPrt_SharedPointerCompatibilityWrapperRejectsNullInputs)
{
    auto& module = beginActiveTestModule();
    const ObjectProfileRef profile = loadFollowerProfile(6514);
    auto victim = spawnFollower(module, profile, static_cast<TEAM_REF>(Team::TEAM_GOOD),
                               Ego::Vector3f(64.0f, 64.0f, 0.0f));
    ASSERT_NE(victim, nullptr);
    auto particle = spawnDamageParticle(profile,
                                        victim->getPosition() + Ego::Vector3f(-20.0f, 0.0f, 40.0f),
                                        static_cast<TEAM_REF>(Team::TEAM_NULL), 512, DAMAGE_SLASH);
    ASSERT_NE(particle, nullptr);

    const std::shared_ptr<Object> nullObject;
    const std::shared_ptr<Ego::Particle> nullParticle;

    EXPECT_FALSE(do_chr_prt_collision(nullObject, particle, -1.0f, 1.0f));
    EXPECT_FALSE(do_chr_prt_collision(victim, nullParticle, -1.0f, 1.0f));
}

// ---------------------------------------------------------------------------
// chr-chr: CollisionSystem detects overlapping objects and not far-apart ones.
// ---------------------------------------------------------------------------

TEST_F(CollisionPipelineFixture, ChrChr_OverlappingObjects_Detected)
{
    auto& module = beginActiveTestModule();
    auto a = spawnFollower(module, loadFollowerProfile(6506),
                          static_cast<TEAM_REF>(Team::TEAM_GOOD), Ego::Vector3f(64.0f, 64.0f, 0.0f));
    auto b = spawnFollower(module, loadFollowerProfile(6507),
                          static_cast<TEAM_REF>(Team::TEAM_GOOD), Ego::Vector3f(76.0f, 64.0f, 0.0f));
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    auto& cs = Ego::Physics::CollisionSystem::get();
    float tmin = 0.0f, tmax = 0.0f;
    const bool detected = cs.detectCollision(*a, *b, &tmin, &tmax);

    // Both followers have a real bump volume and are 12 units apart in X (well within the summed
    // collision volumes), so phys_intersect_oct_bb reports an overlap.
    EXPECT_TRUE(detected);
}

TEST_F(CollisionPipelineFixture, ChrChr_FarApartObjects_NotDetected)
{
    auto& module = beginActiveTestModule();
    auto a = spawnFollower(module, loadFollowerProfile(6508),
                          static_cast<TEAM_REF>(Team::TEAM_GOOD), Ego::Vector3f(64.0f, 64.0f, 0.0f));
    auto b = spawnFollower(module, loadFollowerProfile(6509),
                          static_cast<TEAM_REF>(Team::TEAM_GOOD), Ego::Vector3f(400.0f, 64.0f, 0.0f));
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    auto& cs = Ego::Physics::CollisionSystem::get();
    float tmin = -999.0f, tmax = -999.0f;
    const bool detected = cs.detectCollision(*a, *b, &tmin, &tmax);

    EXPECT_FALSE(detected);
}

// ---------------------------------------------------------------------------
// chr-chr: two plain followers neither mount nor platform each other.
// ---------------------------------------------------------------------------

TEST_F(CollisionPipelineFixture, ChrChr_PlainFollowers_NoMountOrPlatform)
{
    auto& module = beginActiveTestModule();
    auto a = spawnFollower(module, loadFollowerProfile(6510),
                          static_cast<TEAM_REF>(Team::TEAM_GOOD), Ego::Vector3f(64.0f, 64.0f, 0.0f));
    auto b = spawnFollower(module, loadFollowerProfile(6511),
                          static_cast<TEAM_REF>(Team::TEAM_GOOD), Ego::Vector3f(76.0f, 64.0f, 0.0f));
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    auto& cs = Ego::Physics::CollisionSystem::get();
    // A follower is neither a mount nor a platform, so neither resolution attaches the objects.
    EXPECT_FALSE(cs.handleMountingCollision(*a, *b));
    EXPECT_FALSE(cs.handlePlatformCollision(*a, *b));
    EXPECT_EQ(a->getAttachedPlatformRef(), ObjectRef::Invalid);
    EXPECT_EQ(a->getHolderRef(), ObjectRef::Invalid);
}

} // namespace
