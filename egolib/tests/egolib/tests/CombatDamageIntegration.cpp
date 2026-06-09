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

/// @file CombatDamageIntegration.cpp
/// @brief Characterization tests for the *integrated* `Object::damage(...)` side-effect chain.
/// @details Where `CombatDamageResolution.cpp` pins only the pure resistance/reduction/invictus
///          math, this file pins the full integrated path through `Entities/Object_combat.cpp`:
///          reduction -> `actual_damage` -> `_currentLife` subtraction -> hurt-timer/attack-alert
///          on a survivor, and the lethal branch that routes into `Object::kill(...)`
///          (`_isAlive`, `_currentLife == -1`, `ALERTIF_KILLED`). It also pins the early guards
///          (invictus short-circuit, dead target, zero damage).
///
///          Determinism / hand-derivation discipline (mirrors `CombatDamageResolution.cpp`):
///          - Damage is rolled by `Random::next(damage.base, damage.base + damage.rand)`; with
///            `rand == 0` that is `Random::next(x, x) == x`, so `base_damage == damage.base`
///            exactly (no RNG dependence).
///          - The reduction inputs are fully controlled via `setBaseAttribute` (resist/DEFENCE/
///            modifier all zeroed) and the hit passes `ignoreArmour == true`, so
///            `getDamageReduction(...) == 0` and `actual_damage == base_damage`. Expected life
///            deltas are therefore `FP8_TO_FLOAT(damage.base)`, hand-derived, not re-run.
///          - The fixture spawns a live follower (Blud=False -> no blud particle; not a player ->
///            no difficulty scaling) *through the active test module*, which `Object::kill(...)`
///            requires (it iterates `activeModule().getObjectHandler()`).
///
///          Fully headless: no GL context / GameEngine::initialize. Audio is disabled, the
///          ParticleHandler is installed (the spawn path needs it), and a recording
///          StubBillboardSystem is installed so any defensive billboard edge is captured rather
///          than dereferencing an uninstalled service.

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
#include "egolib/game/Module/Module.hpp"
#undef private
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/Graphics/IBillboardSystem.hpp"
#include "egolib/Logic/Attribute.hpp"
#include "egolib/Logic/Damage.hpp"
#include "egolib/Logic/Team.hpp"
#include "egolib/Script/script.h"
#include "egolib/typedef.h"
#include "egolib/vfs.h"

namespace
{

/// Recording no-op billboard system. `Object::damage(...)`/`Object::kill(...)` reach the
/// billboard only on feedback / immune / death-perk edges that the attacker==nullptr,
/// no-special-perk follower fixtures below never hit; installing this stub keeps those edges
/// from dereferencing an uninstalled service if the path is ever broadened.
class StubBillboardSystem : public Ego::Graphics::IBillboardSystem
{
public:
    void reset() override {}
    void update() override {}

    std::shared_ptr<Ego::Graphics::Billboard> makeBillboard(ObjectRef /*objectRef*/,
                                                            const std::string& /*text*/,
                                                            const Ego::Colour4f& /*textColor*/,
                                                            const Ego::Colour4f& /*tint*/,
                                                            int /*lifetime_secs*/,
                                                            BIT_FIELD /*opt_bits*/,
                                                            float size) override
    {
        ++makeBillboardCalls;
        return std::make_shared<Ego::Graphics::Billboard>(Time::Ticks(), nullptr, size);
    }

    int makeBillboardCalls = 0;
};

class CombatIntegrationFixture : public ::testing::Test
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
        opts.logPath = "/debug/combat-integration-tests.log";
        opts.logLevel = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);

        setenv("EGOBOO_DISABLE_AUDIO", "1", 1);
        AudioSystem::initialize();
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

    /// Spawn a live follower (Life points=10, Blud=False, not a player) into the active module
    /// at a valid in-bounds tile.
    std::shared_ptr<Object> spawnFollower(GameModule& module, int slot) const
    {
        const ObjectProfileRef profile =
            EngineContext::get().profileSystem().loadOneProfile("mp_objects/follower.obj", slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid)
        {
            return nullptr;
        }

        return module.spawnObject(Ego::Vector3f(64.0f, 64.0f, 0.0f), profile,
                                  static_cast<TEAM_REF>(Team::TEAM_NULL), 0, Facing(0), "",
                                  ObjectRef::Invalid);
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

std::unique_ptr<ContentRuntimeBootstrap> CombatIntegrationFixture::s_runtime;

// ---------------------------------------------------------------------------
// Survivor path: reduction -> actual_damage -> life, hurt timer, attack alert.
// ---------------------------------------------------------------------------

TEST_F(CombatIntegrationFixture, NonLethalPhysicalHit_SubtractsLifeSetsTimerAndAttackAlert)
{
    auto& module = beginActiveTestModule();
    auto victim = spawnFollower(module, 6401);
    ASSERT_NE(victim, nullptr);
    ASSERT_TRUE(victim->isAlive());

    primeNoReduction(victim, DAMAGE_SLASH);
    victim->setDamageTimer(0);
    // A freshly-spawned object starts with careful_timer == CAREFULTIME (50), which gates
    // `updateLastAttacker` and suppresses ALERTIF_ATTACKED on the very first hit. Clear it so
    // the attack-alert path is exercised; the hit then re-arms it to CAREFULTIME.
    victim->setCarefulTimer(0);

    const float lifeBefore = victim->getLife();

    // base_damage == 512 (FP8 == 2.0 life points), > HURTDAMAGE (256), reduction 0.
    // attacker == nullptr, attackerTeam == TEAM_NULL (not TEAM_DAMAGE), ignoreArmour == true.
    const int actual = victim->damage(Facing(0), IPair(512, 0), DAMAGE_SLASH,
                                      static_cast<TEAM_REF>(Team::TEAM_NULL),
                                      nullptr, /*ignoreArmour*/ true, /*setDamageTime*/ true,
                                      /*ignoreInvictus*/ false);

    EXPECT_EQ(actual, 512);
    EXPECT_TRUE(victim->isAlive());
    // _currentLife -= FP8_TO_FLOAT(512) == 2.0.
    EXPECT_FLOAT_EQ(victim->getLife(), lifeBefore - 2.0f);
    // base_damage > HURTDAMAGE && setDamageTime -> damage_timer = DAMAGETIME (32).
    EXPECT_EQ(victim->getDamageTimer(), static_cast<uint8_t>(32));
    // base_damage > HURTDAMAGE && careful_timer == 0 -> updateLastAttacker -> ALERTIF_ATTACKED
    // (set even when the attacker is null: the null-team early-return only fires for a real
    // attacker), and careful_timer is re-armed to CAREFULTIME (50).
    EXPECT_TRUE(HAS_SOME_BITS(Ego::Script::runtimeState(*victim).alert, ALERTIF_ATTACKED));
    EXPECT_EQ(victim->getCarefulTimer(), static_cast<uint8_t>(50));
}

// A freshly-spawned object starts with careful_timer == CAREFULTIME, so its first incoming hit
// does NOT raise ALERTIF_ATTACKED — the friendly-fire timer suppresses the alert even though the
// life loss / hurt timer still apply.
TEST_F(CombatIntegrationFixture, FreshCarefulTimerSuppressesFirstAttackAlert)
{
    auto& module = beginActiveTestModule();
    auto victim = spawnFollower(module, 6402);
    ASSERT_NE(victim, nullptr);
    ASSERT_EQ(victim->getCarefulTimer(), static_cast<uint8_t>(50));

    primeNoReduction(victim, DAMAGE_SLASH);
    victim->setDamageTimer(0);

    const float lifeBefore = victim->getLife();
    const int actual = victim->damage(Facing(0), IPair(512, 0), DAMAGE_SLASH,
                                      static_cast<TEAM_REF>(Team::TEAM_NULL),
                                      nullptr, /*ignoreArmour*/ true, /*setDamageTime*/ true,
                                      /*ignoreInvictus*/ false);

    EXPECT_EQ(actual, 512);
    EXPECT_FLOAT_EQ(victim->getLife(), lifeBefore - 2.0f);
    EXPECT_FALSE(HAS_SOME_BITS(Ego::Script::runtimeState(*victim).alert, ALERTIF_ATTACKED));
}

// ---------------------------------------------------------------------------
// Lethal path: damage routes into Object::kill -> death state + ALERTIF_KILLED.
// ---------------------------------------------------------------------------

TEST_F(CombatIntegrationFixture, LethalHit_KillsVictimAndSetsKilledAlert)
{
    auto& module = beginActiveTestModule();
    auto victim = spawnFollower(module, 6403);
    ASSERT_NE(victim, nullptr);
    ASSERT_TRUE(victim->isAlive());

    primeNoReduction(victim, DAMAGE_SLASH);
    victim->setDamageTimer(0);

    // 50.0 life points (FP8 == 12800) overwhelms the follower's 10 life -> _currentLife <= 0
    // -> Object::kill(...). kill() iterates activeModule().getObjectHandler(), which is why this
    // path requires the live module bring-up.
    const int actual = victim->damage(Facing(0), IPair(12800, 0), DAMAGE_SLASH,
                                      static_cast<TEAM_REF>(Team::TEAM_NULL),
                                      nullptr, /*ignoreArmour*/ true, /*setDamageTime*/ true,
                                      /*ignoreInvictus*/ false);

    EXPECT_EQ(actual, 12800);
    EXPECT_FALSE(victim->isAlive());
    // kill() pins _currentLife to exactly -1.0 (not the raw post-subtraction value).
    EXPECT_FLOAT_EQ(victim->getLife(), -1.0f);
    // kill() sets ALERTIF_KILLED (Object_combat.cpp:547) but then runs the victim's AI script one
    // last time (scr_run_chr_script at :581), which consumes the alert -- so ALERTIF_KILLED is NOT
    // observable on the victim's ai.alert after the call returns. The persistent witness that the
    // full kill path ran (set at :579, before the final script tick) is _hasBeenKilled.
    EXPECT_TRUE(victim->_hasBeenKilled);
}

// A second hit on an already-dead victim is rejected by the `!isAlive()` guard (returns 0,
// no further state change).
TEST_F(CombatIntegrationFixture, DeadVictimTakesNoFurtherDamage)
{
    auto& module = beginActiveTestModule();
    auto victim = spawnFollower(module, 6404);
    ASSERT_NE(victim, nullptr);

    primeNoReduction(victim, DAMAGE_SLASH);
    victim->damage(Facing(0), IPair(12800, 0), DAMAGE_SLASH, static_cast<TEAM_REF>(Team::TEAM_NULL),
                   nullptr, true, true, false);
    ASSERT_FALSE(victim->isAlive());
    const float lifeAfterDeath = victim->getLife();

    const int actual = victim->damage(Facing(0), IPair(512, 0), DAMAGE_SLASH,
                                      static_cast<TEAM_REF>(Team::TEAM_NULL),
                                      nullptr, true, true, false);

    EXPECT_EQ(actual, 0);
    EXPECT_FALSE(victim->isAlive());
    EXPECT_FLOAT_EQ(victim->getLife(), lifeAfterDeath);
}

// ---------------------------------------------------------------------------
// Early guards: invictus short-circuit and zero damage.
// ---------------------------------------------------------------------------

TEST_F(CombatIntegrationFixture, InvictusGuardSkipsDamageUnlessIgnored)
{
    auto& module = beginActiveTestModule();
    auto victim = spawnFollower(module, 6405);
    ASSERT_NE(victim, nullptr);

    primeNoReduction(victim, DAMAGE_SLASH);
    victim->setDamageTimer(0);
    victim->setInvincible(true);

    const float lifeBefore = victim->getLife();

    // ignoreInvictus == false: `if (invictus && !ignoreInvictus) return 0` fires first.
    const int blocked = victim->damage(Facing(0), IPair(512, 0), DAMAGE_SLASH,
                                       static_cast<TEAM_REF>(Team::TEAM_NULL),
                                       nullptr, true, true, /*ignoreInvictus*/ false);
    EXPECT_EQ(blocked, 0);
    EXPECT_FLOAT_EQ(victim->getLife(), lifeBefore);
    EXPECT_TRUE(victim->isAlive());

    // ignoreInvictus == true: the guard is bypassed and the damage lands normally.
    const int landed = victim->damage(Facing(0), IPair(512, 0), DAMAGE_SLASH,
                                      static_cast<TEAM_REF>(Team::TEAM_NULL),
                                      nullptr, true, true, /*ignoreInvictus*/ true);
    EXPECT_EQ(landed, 512);
    EXPECT_FLOAT_EQ(victim->getLife(), lifeBefore - 2.0f);
}

TEST_F(CombatIntegrationFixture, ZeroDamageIsANoOp)
{
    auto& module = beginActiveTestModule();
    auto victim = spawnFollower(module, 6406);
    ASSERT_NE(victim, nullptr);

    primeNoReduction(victim, DAMAGE_SLASH);
    const float lifeBefore = victim->getLife();

    // max_damage == |base| + |rand| == 0 -> `if (!isAlive() || 0 == max_damage) return 0`.
    const int actual = victim->damage(Facing(0), IPair(0, 0), DAMAGE_SLASH,
                                      static_cast<TEAM_REF>(Team::TEAM_NULL),
                                      nullptr, true, true, false);

    EXPECT_EQ(actual, 0);
    EXPECT_FLOAT_EQ(victim->getLife(), lifeBefore);
    EXPECT_TRUE(victim->isAlive());
}

// NOTE (deferred): the DAMAGEMANA / DAMAGECHARGE / DAMAGEINVERT modifier branches in
// Object::damage(...) (the mana-shield two-step at Object_combat.cpp:115-133) interact with
// setMana() clamping in a non-obvious, arguably buggy way and route into heal(); pinning them
// faithfully deserves its own focused pass with the mana accessors fully characterized first.

} // namespace
