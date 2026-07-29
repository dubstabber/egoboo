/// @file egolib/tests/egolib/tests/LatchAttackChain.cpp
/// @brief Characterization of the input-latch -> attack chain and of the latch *reset*
///        ownership split between script_driver.c and Ego::Player.
///
/// The chain has TWO halves that meet only through the animation system:
///
///   half 1  Object::setLatchButton -> Object::updateLatchButtons
///           (Object_interaction.cpp:467-473) -> chr_do_latch_attack (game_combat.c:142-366).
///           chr_do_latch_attack NEVER calls character_swipe. It publishes alerts, plays
///           actions, parks reload timers, charges mana, deactivates stealth and resets
///           the boredom timer — but it never spawns anything.
///
///   half 2  ObjectGraphics::updateAnimation -> applyPublishedInterpolationStep
///           (ObjectGraphics_animation.cpp:289-298) -> handleAnimationFX
///           (ObjectGraphics_animation.cpp:111-126) -> character_swipe. The ONLY edge from
///           the attack chain to character_swipe is a per-frame MADFX_ACTLEFT/MADFX_ACTRIGHT
///           effect on the action chr_do_latch_attack started, stepped by the animation
///           clock. `AnimationActLeftFrameEffectIsTheOnlyEdgeIntoCharacterSwipe` pins it.
///
/// CHARACTERIZATION ONLY. Every test in this translation unit pins the behavior the
/// engine has TODAY. Tests whose name ends in `_Quirk` (and the blocks flagged
/// "QUIRK PIN") pin behavior that looks like a defect; they cite the exact source
/// lines and must not be "fixed" by editing the assertion.
///
/// This TU doubles as the reproduction harness for the OPEN wizard.mod play-test bug
/// ("player fires homing missiles continuously without input"). The engine-side
/// mechanism is pinned by
/// `DanceResolvedActionRepublishesUsedEveryCallAndTickWithoutCooldown_WizardBugHarness`
/// (the ACTION_DA arm: unconditional IfUsed, no timer) and
/// `UnarmedZapSetsNoReloadAndOnlyAnimationGateThrottles_Quirk` (a successful attack whose
/// action family is absent from the reload table, so only the animation gate throttles);
/// the DISABLED `StuckLatchRepro_PlayerFlaggedObjectIsResetByNobody` drives the two
/// together for 300 ticks and counts charge/fire cycles.
///
/// WIZARD CONTENT NOTE (checked against the shipped data, do not "simplify" this):
/// data/modules/wizard.mod/gamedat/spawn.txt:9 puts Missile in the player's LEFT grip, and
/// data/basicdat/globalobjects/magic/missile.obj/data.txt:111 declares `Attack type : WALK`.
/// ModelAnimationMetadata::charToAction (ModelAnimationMetadata.cpp:261-280) has no 'W'
/// case, so the weapon action falls to `default: return ACTION_DA`. The wizard therefore
/// lands on the **D** family (game_combat.c:220-227 plus the D sub-picks produced by
/// randomizeAction at ModelAnimationMetadata.cpp:457), NOT the Z family. D and Z are both
/// absent from the reload table (game_combat.c:326-340), which is why the Z-family fixture
/// below is a faithful analogue of the "no cooldown" half. The ZAP animation the player
/// visibly plays comes from missile.obj/script.txt's `TargetDoAction ACTIONZA`, which
/// bypasses chr_do_latch_attack entirely.
///
/// KNOWN UNPINNED (deliberate scope cut; the map for the next pass):
///   * the damage pipeline and the perk multiplier table (game_combat.c:555-647);
///   * the throw arm of character_swipe (game_combat.c:425-466);
///   * particle attribution through chr_get_lowest_attachment (game_combat.c:401);
///   * Module_update step ORDERING (only the pairing is assumed here, never asserted);
///   * the remaining cells of the latch reset-ownership matrix (respawn/inventory-mode
///     paths in Player::updateLatches, ALERTIF_CLEANEDUP/CRUSHED handling in
///     MainLoop::let_all_characters_think).

#include "gtest/gtest.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "TestEnvironment.hpp"
#include "ObjectGraphicsTestAccess.hpp"                // frame-state pokes for the animation-FX pin
#include "egolib/Audio/AudioSystem.hpp"
// Private-access idiom (see ObjectAccessors.cpp:6-9): the latch bitset
// (ObjectState.hpp:179) and `bore_timer` (ObjectState.hpp:118) live in the privately
// inherited ObjectState base, and both are load-bearing for these characterizations.
#define private public
#include "egolib/Entities/_Include.hpp"
#include "egolib/Profiles/_Include.hpp"
#undef private
#include "egolib/Logic/Team.hpp"
#include "egolib/Script/script.h"                      // scr_run_chr_script, script_state_t, ai_state_t
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Graphics/Camera.hpp"             // Camera, CameraOptions (StubCameraSystem member)
#include "egolib/game/Graphics/ICameraSystem.hpp"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/InputControl/InputDevice.hpp"         // Ego::Input::InputDevice::DeviceList
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/game.h"                          // -> CharacterParticleOps.h: chr_do_latch_attack, character_swipe
#include "egolib/game/script_functions.h"              // scr_PressLatchButton, scr_PressTargetLatchButton
#include "egolib/vfs.h"

namespace
{

/// @brief Camera-system stub with an EMPTY camera list, so `getCamera()` returns nullptr.
///        Shape copied from ScriptActionFunctions.cpp:299-355. Required because
///        `EngineContext::cameraSystem()` THROWS when no camera system is installed and
///        `Player::updateLatches` fetches it (Player.cpp:162) right after the reset at :159.
class StubCameraSystem : public ICameraSystem
{
public:
    void updateAll(const ego_mesh_t*) override {}

    void setNumberOfCameras(size_t) override {}

    const std::vector<std::shared_ptr<Camera>>& getCameraList() const override { return _cameraList; }

    std::shared_ptr<Camera> getMainCamera() const override
    {
        return _cameraList.empty() ? nullptr : _cameraList.front();
    }

    std::shared_ptr<Camera> getCamera(ObjectRef) const override
    {
        return _cameraList.empty() ? nullptr : _cameraList.front();
    }

    CameraOptions& getCameraOptions() override { return _cameraOptions; }

    void renderAll(std::function<void(std::shared_ptr<Camera>, std::shared_ptr<Ego::Graphics::TileList>, std::shared_ptr<Ego::Graphics::EntityList>)>) override {}

    bool isTileInMainCameraRenderList(const Index1D&) const override { return false; }

private:
    std::vector<std::shared_ptr<Camera>> _cameraList;
    CameraOptions _cameraOptions{};
};

/// @brief RAII guard installing/clearing the stub camera system on EngineContext.
class ScopedStubCameraSystem
{
public:
    ScopedStubCameraSystem()
    {
        EngineContext::get().installCameraSystem(_stub);
    }

    ~ScopedStubCameraSystem()
    {
        EngineContext::get().clearCameraSystem();
    }

private:
    StubCameraSystem _stub;
};

/// @brief `Object::setMana` is ADDITIVE and clamps to [0, MAX_MANA] (Object_attributes.cpp:378-381).
void drainMana(const std::shared_ptr<Object>& object)
{
    object->setMana(-object->getMana());
}

/// @brief See drainMana: a large positive delta saturates at MAX_MANA.
void topUpMana(const std::shared_ptr<Object>& object)
{
    object->setMana(10000.0f);
}

class LatchAttackChainFixture : public ::testing::Test
{
protected:
    static std::unique_ptr<ContentRuntimeBootstrap> s_runtime;

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
        opts.randomSeed = 43;
        opts.binaryPath = "";
        opts.logPath = "/debug/latch-attack-chain-tests.log";
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
        auto& session = GameSessionContext::get();
        if (session.hasActiveModule())
        {
            session.quitModule();
        }

        EngineContext::get().profileSystem().reset();
        EngineContext::get().profileSystem().loadModuleProfiles();
        setup_init_module_vfs_paths("mp_modules/test.mod");
        session.publishLocalPlayerPerception(LocalPlayerPerceptionState{});
    }

    void TearDown() override
    {
        auto& session = GameSessionContext::get();
        if (session.hasActiveModule())
        {
            session.quitModule();
        }

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

    ObjectProfileRef loadProfile(const std::string& profilePath, int slot) const
    {
        return EngineContext::get().profileSystem().loadOneProfile(profilePath, slot);
    }

    std::shared_ptr<Object> makeObject(GameModule& module, const std::string& profilePath, int slot,
                                       const Ego::Vector3f& position = Ego::Vector3f(64.0f, 64.0f, 0.0f)) const
    {
        const ObjectProfileRef profile = loadProfile(profilePath, slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid)
        {
            return nullptr;
        }

        const ObjectRef objectRef = module.spawnObjectRef(position, profile, static_cast<TEAM_REF>(Team::TEAM_NULL), 0, Facing(0), "", ObjectRef::Invalid);
        return module.getObjectHandler().getHandle(objectRef);
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
        const bool began = session.beginModule(module, 43);
        EXPECT_TRUE(began);
        return session.activeModule();
    }

    void flushObjectHandler(GameModule& module) const
    {
        auto refs = module.getObjectHandler().objectRefIterator();
        (void)refs;
    }

    ai_state_t makeScriptSelf(const std::shared_ptr<Object>& selfObject) const
    {
        ai_state_t self;
        self.setSelf(selfObject ? selfObject->getObjRef() : ObjectRef::Invalid);
        self.setTarget(ObjectRef::Invalid);
        return self;
    }
};

std::unique_ptr<ContentRuntimeBootstrap> LatchAttackChainFixture::s_runtime;

//--------------------------------------------------------------------------------------------
// T1 — argument guards and the weapon-reload gate.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, GuardsAndWeaponReloadGateReturnFalseWithoutSideEffects)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5701);
    auto mace = makeObject(module, "mp_data/globalobjects/weapons/mace.obj", 5702);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(mace, nullptr);
    ASSERT_TRUE(actor->isAlive());
    ASSERT_TRUE(mace->attachToObject(actor->getObjRef(), GRIP_LEFT));

    // Phase A — null object / out-of-range slot guards (game_combat.c:148 and :152).
    EXPECT_FALSE(chr_do_latch_attack(nullptr, SLOT_LEFT));
    EXPECT_FALSE(chr_do_latch_attack(actor.get(), SLOT_COUNT));

    // Phase B — the reload gate is read off the WEAPON, not the character
    // (game_combat.c:160-172). A cooling weapon returns false with no side effects at all.
    mace->setReloadTimer(10);
    const ObjectRef lastItemUsedBefore = actor->getAILastItemUsed();
    const bool actorInterruptibleBefore = actor->getGraphics().canBeInterrupted();
    mace->clearAIAlertBits(ALERTIF_USED);

    EXPECT_FALSE(chr_do_latch_attack(actor.get(), SLOT_LEFT));
    EXPECT_EQ(mace->getReloadTimer(), 10);
    EXPECT_FALSE(mace->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_EQ(actor->getAILastItemUsed(), lastItemUsedBefore);
    EXPECT_EQ(actor->getGraphics().canBeInterrupted(), actorInterruptibleBefore);

    // Phase C — unarmed: the "weapon" IS the character (game_combat.c:161-165), so the
    // character's own reload timer closes the same gate.
    auto unarmedActor = makeObject(module, "mp_objects/follower.obj", 5703);
    ASSERT_NE(unarmedActor, nullptr);
    ASSERT_EQ(unarmedActor->getHeldObject(SLOT_LEFT), ObjectRef::Invalid);

    unarmedActor->setReloadTimer(10);
    unarmedActor->clearAIAlertBits(ALERTIF_USED);

    EXPECT_FALSE(chr_do_latch_attack(unarmedActor.get(), SLOT_LEFT));
    EXPECT_FALSE(unarmedActor->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_EQ(unarmedActor->getReloadTimer(), 10);
}

//--------------------------------------------------------------------------------------------
// T2 — the missing-skill penalty lands on the WEAPON.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, MissingSkillIdszSetsOneSecondWeaponReloadPenalty)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5704);
    auto longbow = makeObject(module, "mp_data/globalobjects/weapons/lbow.obj", 5705);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(longbow, nullptr);
    ASSERT_TRUE(longbow->attachToObject(actor->getObjRef(), GRIP_LEFT));

    // Content premises (fail loudly on content drift).
    ASSERT_TRUE(longbow->getProfile()->requiresSkillIDToUse());
    ASSERT_FALSE(actor->hasSkillIDSZ(longbow->getProfile()->getIDSZ(IDSZ_SKILL)));
    // NOTE: on the follower model the LONGBOW action resolves to ACTION_DA (the model has no
    // L-family frames), but the skill gate at game_combat.c:193-199 fires first and the
    // function returns at :208-218 long before the ACTION_DA block at :220.
    const auto& actorModel = actor->getProfile()->getModel();
    ASSERT_EQ(actorModel->getAction(ACTION_LA), ACTION_DA);
    ASSERT_EQ(actorModel->getAction(ACTION_LB), ACTION_DA);

    const ObjectRef lastItemUsedBefore = actor->getAILastItemUsed();
    longbow->clearAIAlertBits(ALERTIF_USED);

    EXPECT_FALSE(chr_do_latch_attack(actor.get(), SLOT_LEFT));

    // game_combat.c:211 — ONESECOND == 50 (egolib_config.h:52) parked on the WEAPON.
    EXPECT_EQ(longbow->getReloadTimer(), 50);
    EXPECT_FALSE(longbow->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_EQ(actor->getAILastItemUsed(), lastItemUsedBefore);
    EXPECT_EQ(actor->getReloadTimer(), 0);
}

//--------------------------------------------------------------------------------------------
// T3 — QUIRK: the "kursed weapon in the other hand" gate reads the SAME hand.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, KursedOffHandDoesNotBlockLongbow_Quirk)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_data/globalobjects/players/archadventurer.obj", 5706);
    auto longbow = makeObject(module, "mp_data/globalobjects/weapons/lbow.obj", 5707);
    auto torch = makeObject(module, "mp_data/globalobjects/items/torch.obj", 5708);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(longbow, nullptr);
    ASSERT_NE(torch, nullptr);
    ASSERT_TRUE(longbow->attachToObject(actor->getObjRef(), GRIP_LEFT));
    ASSERT_TRUE(torch->attachToObject(actor->getObjRef(), GRIP_RIGHT));

    // Content premises: the actor HAS the [AWEP] skill, and the longbow action really does
    // resolve to the L family on this model (otherwise the :203 gate would never be reached).
    ASSERT_TRUE(actor->hasSkillIDSZ(longbow->getProfile()->getIDSZ(IDSZ_SKILL)));
    const auto& actorModel = actor->getProfile()->getModel();
    ASSERT_EQ(actorModel->getAction(ACTION_LA), ACTION_LA);
    ASSERT_EQ(actorModel->getAction(ACTION_LB), ACTION_LA);
    // Premises for the reload/weapon-animation assertions below.
    ASSERT_FALSE(longbow->getProfile()->hasFastAttack());
    ASSERT_FALSE(actor->hasPerk(Ego::Perks::QUICK_STRIKE));
    ASSERT_FALSE(actor->hasPerk(Ego::Perks::CROSSBOW_MASTERY));
    // Unlike mace.obj, lbow.obj's model really does define the MJ ("weapon strikes") action,
    // so the ACTION_MJ play at game_combat.c:305 is observable here rather than aliasing onto
    // the ACTION_DA fallback (ModelAnimationMetadata.cpp:367-395).
    const auto& longbowModel = longbow->getProfile()->getModel();
    ASSERT_EQ(longbowModel->getAction(ACTION_MJ), ACTION_MJ);
    const ModelAction longbowAnimationBefore = longbow->getGraphics().getCurrentAnimation();
    ASSERT_NE(longbowAnimationBefore, ACTION_MJ);

    torch->setKursed(true);
    longbow->clearAIAlertBits(ALERTIF_USED);
    ASSERT_FALSE(longbow->isKursed());

    const float agility = actor->getAttribute(Ego::Attribute::AGILITY);

    // QUIRK PIN — characterizes current behavior — do not fix silently.
    // The source comment at game_combat.c:202 reads "Don't allow users with kursed weapon in
    // the OTHER hand to use longbows", but heldItemIsKursed(characterInventory, which_slot)
    // (game_combat.c:205, helper at :101-105) inspects the item in the slot BEING USED, not
    // the off hand. A kursed off-hand item therefore blocks nothing.
    EXPECT_TRUE(chr_do_latch_attack(actor.get(), SLOT_LEFT));
    EXPECT_TRUE(longbow->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_EQ(actor->getAILastItemUsed(), longbow->getObjRef());

    // Free rider on the successful attack: the L (Longbow) reload family
    // (game_combat.c:334) parks `(int)(-agility) + 60` on the WEAPON. Mirror the source
    // expression; never hard-code a value that depends on the spawned actor's attributes.
    const int expectedReload = static_cast<int>(-agility) + 60;
    ASSERT_GT(expectedReload, 0) << "content drift: archadventurer agility now zeroes the reload";
    EXPECT_EQ(longbow->getReloadTimer(), static_cast<uint16_t>(expectedReload));
    EXPECT_EQ(actor->getReloadTimer(), 0);

    // ... and the weapon plays ACTION_MJ (game_combat.c:302-306); ObjectGraphics::playAction
    // maps the request through the weapon model's own getAction() (ObjectGraphics.cpp:148-151)
    // with override_action == true, so a non-interruptible weapon would still commit it.
    EXPECT_EQ(longbow->getGraphics().getCurrentAnimation(), ACTION_MJ);

    // The blocked half of the gate (kursed item in the USED slot) is already pinned by
    // GameplayAlertPublication.cpp:323-342 (LongbowLatchAttackPreservesKursedHeldItemGate).
}

//--------------------------------------------------------------------------------------------
// T4 — THE WIZARD-BUG HARNESS: a weapon whose action resolves to ACTION_DA republishes
//      ALERTIF_USED on every call and every tick, with zero engine cooldown.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, DanceResolvedActionRepublishesUsedEveryCallAndTickWithoutCooldown_WizardBugHarness)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5709);
    auto knife = makeObject(module, "mp_data/globalobjects/weapons/knife.obj", 5710);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(knife, nullptr);
    ASSERT_TRUE(knife->attachToObject(actor->getObjRef(), GRIP_LEFT));

    // Content premises: THRUST weapon, no skill gate, and the follower model has NO T-family
    // frames, so getAction(TA)/getAction(TB) both fall through to ACTION_DA
    // (ModelAnimationMetadata.cpp:367-395). No profile mutation is needed to reach the
    // ACTION_DA arm — the shipped content reaches it naturally.
    ASSERT_EQ(static_cast<ModelAction>(knife->getProfile()->getWeaponAction()), ACTION_TA);
    ASSERT_FALSE(knife->getProfile()->requiresSkillIDToUse());
    const auto& actorModel = actor->getProfile()->getModel();
    ASSERT_EQ(actorModel->getAction(ACTION_TA), ACTION_DA);
    ASSERT_EQ(actorModel->getAction(ACTION_TB), ACTION_DA);

    const ObjectRef lastItemUsedBefore = actor->getAILastItemUsed();
    actor->bore_timer = 0;
    const bool actorInterruptibleBefore = actor->getGraphics().canBeInterrupted();
    const bool knifeInterruptibleBefore = knife->getGraphics().canBeInterrupted();

    // Phase A — the function contract, three times in a row with no state change between.
    // game_combat.c:220-227: ACTION_DA forces allowedtoattack = false, the reload recheck at
    // :223 is DEAD (it is always true after the :172 gate), ALERTIF_USED is published with NO
    // timer of any kind, and the function returns false.
    for (int i = 0; i < 3; ++i)
    {
        knife->clearAIAlertBits(ALERTIF_USED);
        EXPECT_FALSE(chr_do_latch_attack(actor.get(), SLOT_LEFT)) << "iteration " << i;
        EXPECT_TRUE(knife->hasAnyAIAlertBits(ALERTIF_USED)) << "iteration " << i;
        EXPECT_EQ(knife->getReloadTimer(), 0) << "iteration " << i;
        EXPECT_EQ(actor->getReloadTimer(), 0) << "iteration " << i;
    }

    // retval == false, so game_combat.c:360-363 never resets boredom, and nothing played an
    // action on the ACTION_DA path.
    EXPECT_EQ(actor->getAILastItemUsed(), lastItemUsedBefore);
    EXPECT_EQ(actor->bore_timer, 0);
    EXPECT_EQ(actor->getGraphics().canBeInterrupted(), actorInterruptibleBefore);
    EXPECT_EQ(knife->getGraphics().canBeInterrupted(), knifeInterruptibleBefore);

    // Phase B — the real caller chain. Object::updateLatchButtons (Object_interaction.cpp:373-474)
    // re-enters chr_do_latch_attack on EVERY tick the latch bit is held, because the actor's
    // reload_timer is the only gate at :468 and the ACTION_DA arm never sets it.
    actor->setLatchButton(LATCHBUTTON_LEFT, true);
    for (int i = 0; i < 3; ++i)
    {
        knife->clearAIAlertBits(ALERTIF_USED);
        actor->updateLatchButtons();
        EXPECT_TRUE(knife->hasAnyAIAlertBits(ALERTIF_USED)) << "tick " << i;
    }

    // The consumer NEVER clears the latch bit: Object_interaction.cpp:373-474 contains no
    // write to _inputLatchesPressed at all.
    EXPECT_TRUE(actor->_inputLatchesPressed[LATCHBUTTON_LEFT]);

    // QUIRK PIN — characterizes current behavior — do not fix silently.
    // This is the engine mechanism that delivers IfUsed to a held item's (or, unarmed, the
    // actor's own) script at the full 50 UPS with zero engine cooldown.
    //
    // Read it as the CHARGING half of the wizard bug, not the firing half. The ACTION_DA arm
    // publishes ALERTIF_USED *before* the animation gate at game_combat.c:268 and never fails,
    // so while it is the only arm taken the item script sees IfUsed on every single tick. For
    // missile.obj that means its IfUsed branch runs forever (selfcontent += 16, capped at
    // 1536) and its Else branch — the one that calls SpawnExactParticle — never runs at all.
    // Firing requires ticks where ALERTIF_USED is ABSENT, which only the D sub-picks that are
    // NOT ACTION_DA can produce: those fall through to the attack block, where the
    // canBeInterrupted() gate at :268 suppresses publication while an attack animation is
    // still playing and no reload timer is ever created (the D family is absent from the
    // table at :326-340). The oscillation between the two is the bug.
}

//--------------------------------------------------------------------------------------------
// T5 — QUIRK: the unarmed mana gate fails for free, and the payment is in FP8 units.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, UnarmedManaGateFailsFreeAndPaymentDeductsFP8Units_Quirk)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5711);

    ASSERT_NE(actor, nullptr);
    ASSERT_EQ(actor->getHeldObject(SLOT_LEFT), ObjectRef::Invalid);
    ASSERT_FLOAT_EQ(actor->getProfile()->getUseManaCost(), 1.0f);

    // Phase A — insufficient mana. game_combat.c:277-280 just clears allowedtoattack: no
    // reload penalty, no alert. The attack is immediately retryable on the next tick.
    drainMana(actor);
    ASSERT_LT(actor->getMana(), 1.0f);
    actor->clearAIAlertBits(ALERTIF_USED);

    EXPECT_FALSE(chr_do_latch_attack(actor.get(), SLOT_LEFT));
    EXPECT_FALSE(actor->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_EQ(actor->getReloadTimer(), 0);
    EXPECT_FLOAT_EQ(actor->getMana(), 0.0f);
    // The stealth deactivation at :263-266 is the only side effect on this arm, and the
    // follower is not stealthed, so the animation state is untouched.
    EXPECT_TRUE(actor->getGraphics().canBeInterrupted());

    // Phase B — sufficient mana.
    topUpMana(actor);
    const float manaBefore = actor->getMana();
    ASSERT_GE(manaBefore, 1.0f);

    EXPECT_TRUE(chr_do_latch_attack(actor.get(), SLOT_LEFT));

    // QUIRK PIN — characterizes current behavior — do not fix silently.
    // game_combat.c:271-281 compares a FLOAT cost (1.0 mana points) against getMana(), but
    // pays it through ICharacterState::costMana(int, ...) (Object_combat.cpp:608-655), which
    // works in FP8 (1/256) units (typedef.h:69-71). The nominal one-point cost therefore
    // deducts one 256th of a point.
    const float expectedMana = FP8_TO_FLOAT(FLOAT_TO_FP8(manaBefore) - 1);
    EXPECT_FLOAT_EQ(actor->getMana(), expectedMana);
    EXPECT_GT(actor->getMana(), manaBefore - 0.01f);
}

//--------------------------------------------------------------------------------------------
// T6 — QUIRK: an unarmed ZAP attack sets no reload at all; only the animation gate throttles.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, UnarmedZapSetsNoReloadAndOnlyAnimationGateThrottles_Quirk)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5712);

    ASSERT_NE(actor, nullptr);
    ASSERT_EQ(actor->getHeldObject(SLOT_LEFT), ObjectRef::Invalid);
    // Content premise: follower.obj declares weapon action ZAP (data.txt:131).
    ASSERT_EQ(static_cast<ModelAction>(actor->getProfile()->getWeaponAction()), ACTION_ZA);
    const auto& actorModel = actor->getProfile()->getModel();
    ASSERT_EQ(actorModel->getAction(ACTION_ZA), ACTION_ZA);
    ASSERT_EQ(actorModel->getAction(ACTION_ZB), ACTION_ZA);

    topUpMana(actor);

    EXPECT_TRUE(chr_do_latch_attack(actor.get(), SLOT_LEFT));

    // QUIRK PIN — characterizes current behavior — do not fix silently.
    // The reload table at game_combat.c:326-340 covers U/T/C/S/B/L/X/F only. The Z family is
    // absent, so base_reload_time stays at -agility (<= 0) and the :339 guard suppresses the
    // timer entirely: a successful unarmed ZAP attack has NO cooldown of its own.
    EXPECT_EQ(actor->getReloadTimer(), 0);
    EXPECT_EQ(actor->getGraphics().getCurrentAnimation(), ACTION_ZA);
    EXPECT_FALSE(actor->getGraphics().canBeInterrupted());

    const float manaAfterFirst = actor->getMana();

    // The ONLY thing that stops an immediate refire is the canBeInterrupted() check at
    // game_combat.c:268 — i.e. the attack animation still playing.
    actor->clearAIAlertBits(ALERTIF_USED);
    EXPECT_FALSE(chr_do_latch_attack(actor.get(), SLOT_LEFT));
    EXPECT_FALSE(actor->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_EQ(actor->getReloadTimer(), 0);
    EXPECT_FLOAT_EQ(actor->getMana(), manaAfterFirst);

    // QUIRK PIN — characterizes current behavior — do not fix silently.
    // The other half of an unarmed attack is character_swipe, and there the unarmed alias
    // (game_combat.c:384-388: `iweapon = ichr`, so `pweapon == pchr`) makes the ammo gate at
    // :470 and the decrement at :472-488 read and MUTATE the CHARACTER's own ammo counter.
    // A fists-only character with ammo therefore spends it on every swing.
    ASSERT_FALSE(actor->getProfile()->isStackable());
    ASSERT_EQ(actor->getAmmoMax(), 0);                       // follower.obj data.txt:5
    ASSERT_FALSE(actor->hasPerk(Ego::Perks::WAND_MASTERY));  // the :475-484 arm is not taken
    ASSERT_FALSE(actor->hasPerk(Ego::Perks::DOUBLE_SHOT));   // nor the :495-505 arm
    actor->setAmmo(5);

    character_swipe(actor->getObjRef(), SLOT_LEFT);

    EXPECT_EQ(actor->getAmmo(), 4);
}

//--------------------------------------------------------------------------------------------
// T6b — QUIRK: a REFUSED attack still breaks stealth.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, RefusedAttackStillBreaksStealth_Quirk)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5751);

    ASSERT_NE(actor, nullptr);
    ASSERT_EQ(actor->getHeldObject(SLOT_LEFT), ObjectRef::Invalid);

    // `_stealth`/`_stealthTimer` (Object.hpp:993-994) are set through the same
    // privately-inherited-member idiom the rest of this TU uses; Object::activateStealth
    // (Object_attributes_behaviors.cpp:254-337) needs the STEALTH perk plus a clear
    // line-of-sight sweep, neither of which is what this test is about.
    actor->_stealth = true;
    actor->_stealthTimer = 0;
    ASSERT_TRUE(actor->isStealthed());

    drainMana(actor);
    ASSERT_LT(actor->getMana(), actor->getProfile()->getUseManaCost());
    actor->clearAIAlertBits(ALERTIF_USED);

    EXPECT_FALSE(chr_do_latch_attack(actor.get(), SLOT_LEFT));

    // QUIRK PIN — characterizes current behavior — do not fix silently.
    // game_combat.c:263-266 calls deactivateStealth() as the FIRST statement inside the
    // `if (allowedtoattack)` block, i.e. before the canBeInterrupted() gate at :268 and before
    // the unarmed mana gate at :271-281. An attack that is then refused therefore costs the
    // player their stealth (and, via Object_attributes_behaviors.cpp:246-247, a fresh
    // one-second stealth lockout) while producing no alert, no animation and no reload.
    EXPECT_FALSE(actor->isStealthed());
    EXPECT_GE(actor->_stealthTimer, 50);   // ONESECOND, egolib_config.h:52
    EXPECT_FALSE(actor->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_EQ(actor->getReloadTimer(), 0);
    EXPECT_TRUE(actor->getGraphics().canBeInterrupted());
}

//--------------------------------------------------------------------------------------------
// T7 — armed success side-effect matrix, pinning the two-timer split.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, ArmedBashSuccessSideEffectMatrixPinsTwoTimerSplit)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5713);
    auto mace = makeObject(module, "mp_data/globalobjects/weapons/mace.obj", 5714);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(mace, nullptr);
    ASSERT_TRUE(mace->attachToObject(actor->getObjRef(), GRIP_LEFT));

    const auto& actorModel = actor->getProfile()->getModel();
    const auto& maceModel = mace->getProfile()->getModel();
    ASSERT_EQ(static_cast<ModelAction>(mace->getProfile()->getWeaponAction()), ACTION_BA);
    ASSERT_EQ(actorModel->getAction(ACTION_BA), ACTION_BA);
    ASSERT_EQ(actorModel->getAction(ACTION_BB), ACTION_BA);
    ASSERT_FALSE(mace->getProfile()->requiresSkillIDToUse());
    ASSERT_FALSE(mace->getProfile()->hasFastAttack());
    // billboardSystem is NOT installed in this fixture; the only paths that reach it are
    // perk-gated (game_combat.c:318-321), and fresh spawns have no perks.
    ASSERT_FALSE(actor->hasPerk(Ego::Perks::QUICK_STRIKE));

    actor->bore_timer = 0;
    const float agility = actor->getAttribute(Ego::Attribute::AGILITY);
    const float manaBefore = actor->getMana();
    mace->clearAIAlertBits(ALERTIF_USED);

    EXPECT_TRUE(chr_do_latch_attack(actor.get(), SLOT_LEFT));

    EXPECT_EQ(actor->getGraphics().getCurrentAnimation(), ACTION_BA);
    EXPECT_FALSE(actor->getGraphics().canBeInterrupted());
    // game_combat.c:315 -> ObjectGraphics::setAnimationSpeed clamps (ObjectGraphics_animation.cpp:66-69).
    EXPECT_FLOAT_EQ(actor->getGraphics().getAnimationSpeed(),
                    Ego::Math::constrain(0.80f + agility * 0.02f, 0.1f, 3.0f));
    EXPECT_TRUE(mace->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_EQ(actor->getAILastItemUsed(), mace->getObjRef());
    // game_combat.c:305 plays ACTION_MJ on the weapon, but mace.obj's model defines only the
    // DA and JB actions and ACTION_MJ has no actionCopyCorrect entry
    // anywhere in ModelAnimationMetadata.cpp (the nearest M-family entries are
    // `actionCopyCorrect(ACTION_MH, ACTION_MI)` at :245 and the MM/MN pair at :246-247, none of
    // which mention MJ; the ASSERT below self-verifies this), so getAction(ACTION_MJ) returns
    // the ACTION_DA fallback (ModelAnimationMetadata.cpp:367-395) — which is also the mace's
    // animation from the moment it spawns (Module_spawn.cpp:317). This assertion therefore
    // pins the FALLBACK, not the :305 play; the real :305 pin lives in
    // KursedOffHandDoesNotBlockLongbow_Quirk, whose lbow.obj model does define MJ.
    ASSERT_EQ(maceModel->getAction(ACTION_MJ), ACTION_DA)
        << "content drift: mace.obj gained an MJ action, move the :305 pin here";
    EXPECT_EQ(mace->getGraphics().getCurrentAnimation(), ACTION_DA);

    // game_combat.c:328-339: base_reload_time = (int)(-agility) + 70 for the B (Bash) family.
    const int expectedReload = static_cast<int>(-agility) + 70;
    ASSERT_GT(expectedReload, 0) << "content drift: follower agility is high enough to zero the reload";
    EXPECT_EQ(mace->getReloadTimer(), static_cast<uint16_t>(expectedReload));
    // The two-timer split: an ARMED attack parks the cooldown on the WEAPON and leaves the
    // character's own gate (Object_interaction.cpp:468) wide open.
    EXPECT_EQ(actor->getReloadTimer(), 0);
    // Armed attacks pay no mana (the cost arm at game_combat.c:271 is unarmed-only).
    EXPECT_FLOAT_EQ(actor->getMana(), manaBefore);
    // game_combat.c:360-363 -> Object::resetBoredTimer (Object_lifecycle.cpp:385-388) draws
    // Random::next<uint16_t>(250, 800): assert the range, never an exact value.
    EXPECT_GE(actor->bore_timer, 250);
    EXPECT_LE(actor->bore_timer, 800);

    // The WEAPON's own update() is what burns the weapon reload timer (Object_update.cpp:143);
    // the wielder's update() never touches it.
    GameSessionContext::get().worldUpdateCount() = 1;
    const uint16_t reloadBeforeTick = mace->getReloadTimer();
    ASSERT_GT(reloadBeforeTick, 0);
    mace->update();
    EXPECT_EQ(mace->getReloadTimer(), static_cast<uint16_t>(reloadBeforeTick - 1));

    // ... and while it is non-zero the next attack short-circuits at game_combat.c:172.
    mace->clearAIAlertBits(ALERTIF_USED);
    EXPECT_FALSE(chr_do_latch_attack(actor.get(), SLOT_LEFT));
    EXPECT_FALSE(mace->hasAnyAIAlertBits(ALERTIF_USED));
}

//--------------------------------------------------------------------------------------------
// T8 — the parry arm stays interruptible and leaves no reload or weapon animation.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, ParryActionStaysInterruptibleWithoutReloadOrWeaponAnimation)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5715);
    auto shield = makeObject(module, "mp_data/globalobjects/armor/atshield.obj", 5716);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(shield, nullptr);
    ASSERT_TRUE(shield->attachToObject(actor->getObjRef(), GRIP_LEFT));

    ASSERT_EQ(static_cast<ModelAction>(shield->getProfile()->getWeaponAction()), ACTION_PA);
    ASSERT_FALSE(shield->getProfile()->requiresSkillIDToUse());
    const auto& actorModel = actor->getProfile()->getModel();
    ASSERT_EQ(actorModel->getAction(ACTION_PA), ACTION_PA);
    ASSERT_EQ(actorModel->getAction(ACTION_PB), ACTION_PA);

    const ModelAction shieldAnimationBefore = shield->getGraphics().getCurrentAnimation();
    shield->clearAIAlertBits(ALERTIF_USED);

    EXPECT_TRUE(chr_do_latch_attack(actor.get(), SLOT_LEFT));

    // game_combat.c:291-295 — parry actions are played with actionready == true, so the
    // character stays interruptible, and the branch skips the ACTION_MJ weapon animation,
    // the animation-speed scaling and the whole reload table below it.
    EXPECT_EQ(actor->getGraphics().getCurrentAnimation(), ACTION_PA);
    EXPECT_TRUE(actor->getGraphics().canBeInterrupted());
    EXPECT_EQ(shield->getReloadTimer(), 0);
    EXPECT_EQ(shield->getGraphics().getCurrentAnimation(), shieldAnimationBefore);
    EXPECT_TRUE(shield->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_EQ(actor->getAILastItemUsed(), shield->getObjRef());
    EXPECT_EQ(actor->getReloadTimer(), 0);
}

//--------------------------------------------------------------------------------------------
// T9 — a mount that forbids rider attacks steals the attack and still reports success.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, MountForbiddingRiderAttackStealsItAndStillReturnsTrue)
{
    auto& module = beginActiveTestModule();
    auto mount = makeObject(module, "mp_data/globalobjects/pets/hotdog.obj", 5717);
    auto rider = makeObject(module, "mp_objects/follower.obj", 5718);

    ASSERT_NE(mount, nullptr);
    ASSERT_NE(rider, nullptr);
    // GRIP_ONLY == GRIP_LEFT (Logic/ObjectSlot.hpp:44).
    ASSERT_TRUE(rider->attachToObject(mount->getObjRef(), GRIP_ONLY));

    ASSERT_TRUE(mount->isMount());
    ASSERT_TRUE(mount->isAlive());
    ASSERT_FALSE(mount->isPlayer());
    ASSERT_TRUE(mount->getGraphics().canBeInterrupted());
    ASSERT_FALSE(mount->getProfile()->riderCanAttack());

    // ObjectPhysics_attachment.cpp:259-276 puts the rider into the looping, interruptible
    // riding animation.
    const ModelAction ridingAnimation = rider->getProfile()->getModel()->getAction(ACTION_MI);
    ASSERT_EQ(rider->getGraphics().getCurrentAnimation(), ridingAnimation);

    rider->bore_timer = 0;
    topUpMana(rider);
    mount->clearAIAlertBits(ALERTIF_USED);

    // game_combat.c:229-260 — the mount steals the attack: riderCanAttack() == false clears
    // allowedtoattack (so the rider's own attack block at :263 never runs), yet the mount
    // block still plays the mount's ACTION_UA, publishes the alert and sets retval = true.
    EXPECT_TRUE(chr_do_latch_attack(rider.get(), SLOT_LEFT));
    EXPECT_EQ(rider->getAILastItemUsed(), mount->getObjRef());
    EXPECT_TRUE(mount->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_EQ(rider->getGraphics().getCurrentAnimation(), ridingAnimation);
    EXPECT_EQ(rider->getReloadTimer(), 0);
    // A stolen attack still counts as a success for boredom purposes (game_combat.c:360-363).
    EXPECT_GE(rider->bore_timer, 250);
    EXPECT_LE(rider->bore_timer, 800);

    // The riderCanAttack() == true half of the same gate is pinned by
    // PermissiveMountFiresAlongsideRiderExceptOnParry below (choco.obj).
}

//--------------------------------------------------------------------------------------------
// T9b — a mount that PERMITS rider attacks fires alongside the rider, except on parries.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, PermissiveMountFiresAlongsideRiderExceptOnParry)
{
    auto& module = beginActiveTestModule();

    // ObjectProfile_data.cpp:299 parses this field INVERTED
    // (`_riderCanAttack = !vfs_get_next_bool(ctxt);  //ZF> note value is inverted intentionally`),
    // so a pet whose data.txt line 184 reads "Rider cannot attack : False" ends up with
    // riderCanAttack() == true. choco.obj is such a mount.
    auto mount = makeObject(module, "mp_data/globalobjects/pets/choco.obj", 5740);
    auto rider = makeObject(module, "mp_objects/follower.obj", 5741);

    ASSERT_NE(mount, nullptr);
    ASSERT_NE(rider, nullptr);
    ASSERT_TRUE(mount->isMount());
    ASSERT_TRUE(mount->isAlive());
    ASSERT_FALSE(mount->isPlayer());
    ASSERT_TRUE(mount->getGraphics().canBeInterrupted());
    ASSERT_TRUE(mount->getProfile()->riderCanAttack());

    ASSERT_TRUE(rider->attachToObject(mount->getObjRef(), GRIP_ONLY));
    const ModelAction ridingAnimation = rider->getProfile()->getModel()->getAction(ACTION_MI);
    ASSERT_EQ(rider->getGraphics().getCurrentAnimation(), ridingAnimation);

    // Arm A — a NON-parry action (the rider is unarmed, so follower.obj's ZAP applies).
    // game_combat.c:248 reads `!ACTION_IS_TYPE(action, P) || !riderCanAttack()`; the first
    // disjunct is true, so the mount block runs even though the rider is allowed to attack.
    // allowedtoattack was never cleared at :240, so the rider's own attack block at :263 runs
    // as well: BOTH fire in a single call.
    ASSERT_EQ(static_cast<ModelAction>(rider->getProfile()->getWeaponAction()), ACTION_ZA);
    topUpMana(rider);
    mount->clearAIAlertBits(ALERTIF_USED);
    rider->clearAIAlertBits(ALERTIF_USED);
    ASSERT_NE(rider->getAILastItemUsed(), rider->getObjRef());
    ASSERT_NE(rider->getGraphics().getCurrentAnimation(), ACTION_ZA);

    EXPECT_TRUE(chr_do_latch_attack(rider.get(), SLOT_LEFT));

    EXPECT_TRUE(mount->hasAnyAIAlertBits(ALERTIF_USED));   // published at :253
    EXPECT_TRUE(rider->hasAnyAIAlertBits(ALERTIF_USED));   // published at :351 (unarmed: self)
    // :253 sets lastItemUsed to the mount, then :344 overwrites it with the rider's weapon,
    // which for an unarmed attack is the rider itself.
    EXPECT_EQ(rider->getAILastItemUsed(), rider->getObjRef());
    EXPECT_EQ(rider->getGraphics().getCurrentAnimation(), ACTION_ZA);
    EXPECT_FALSE(rider->getGraphics().canBeInterrupted());
    EXPECT_EQ(rider->getReloadTimer(), 0);

    // Arm B — a PARRY action. Both disjuncts of :248 are now false, so the mount block is
    // skipped entirely: no ACTION_UA, no alert on the mount, no lastItemUsed pointing at it.
    auto mountB = makeObject(module, "mp_data/globalobjects/pets/choco.obj", 5742);
    auto riderB = makeObject(module, "mp_objects/follower.obj", 5743);
    auto shield = makeObject(module, "mp_data/globalobjects/armor/atshield.obj", 5744);

    ASSERT_NE(mountB, nullptr);
    ASSERT_NE(riderB, nullptr);
    ASSERT_NE(shield, nullptr);
    ASSERT_TRUE(mountB->getProfile()->riderCanAttack());
    ASSERT_TRUE(shield->attachToObject(riderB->getObjRef(), GRIP_LEFT));
    ASSERT_TRUE(riderB->attachToObject(mountB->getObjRef(), GRIP_ONLY));
    ASSERT_EQ(static_cast<ModelAction>(shield->getProfile()->getWeaponAction()), ACTION_PA);
    const auto& riderBModel = riderB->getProfile()->getModel();
    ASSERT_EQ(riderBModel->getAction(ACTION_PA), ACTION_PA);
    ASSERT_EQ(riderBModel->getAction(ACTION_PB), ACTION_PA);
    // A rider holding something sits (ACTION_MH) instead of riding
    // (ObjectPhysics_attachment.cpp:259-272).
    ASSERT_EQ(riderB->getGraphics().getCurrentAnimation(), riderBModel->getAction(ACTION_MH));

    const ModelAction mountAnimationBefore = mountB->getGraphics().getCurrentAnimation();
    mountB->clearAIAlertBits(ALERTIF_USED);
    shield->clearAIAlertBits(ALERTIF_USED);
    ASSERT_NE(riderB->getAILastItemUsed(), shield->getObjRef());
    ASSERT_NE(riderB->getGraphics().getCurrentAnimation(), ACTION_PA);
    // The T9 baseline proves this mount WOULD have played ACTION_UA if :248 let it through.
    ASSERT_NE(mountAnimationBefore, mountB->getProfile()->getModel()->getAction(ACTION_UA));

    EXPECT_TRUE(chr_do_latch_attack(riderB.get(), SLOT_LEFT));

    EXPECT_FALSE(mountB->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_EQ(mountB->getGraphics().getCurrentAnimation(), mountAnimationBefore);
    EXPECT_TRUE(shield->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_EQ(riderB->getAILastItemUsed(), shield->getObjRef());
    // The parry branch at :291-295 plays with actionready == true.
    EXPECT_EQ(riderB->getGraphics().getCurrentAnimation(), ACTION_PA);
    EXPECT_TRUE(riderB->getGraphics().canBeInterrupted());
    EXPECT_EQ(shield->getReloadTimer(), 0);
}

//--------------------------------------------------------------------------------------------
// T9c — QUIRK: a mount that forbids rider attacks swallows the attack on every axis.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, ForbiddingMountSwallowsRiderAttackOnEveryAxis_Quirk)
{
    auto& module = beginActiveTestModule();

    // QUIRK PIN — characterizes current behavior — do not fix silently.
    // game_combat.c:240 (`if (!mountProfile->riderCanAttack()) allowedtoattack = false;`) sits
    // OUTSIDE both the `isMount() && isAlive()` test at :243 and the
    // `!isPlayer() && canBeInterrupted()` test at :246. So whenever the mount itself cannot
    // act, the rider's attack is destroyed rather than merely stolen: retval stays false, no
    // alert is published anywhere, lastItemUsed is untouched, and no reload penalty is
    // charged either — the `!allowedtoattack` penalty block at :208-218 was PASSED earlier
    // while allowedtoattack was still true, so the clearing at :240 can never reach it.
    // (Had :208-218 actually run, it would have parked ONESECOND on the weapon at :211 and
    // returned at :217, making this mount block unreachable.) The rider silently loses the tick.
    struct Axis
    {
        const char* name;
        int mountSlot;
        int riderSlot;
    };
    const Axis axes[] = {
        {"dead mount",              5745, 5746},
        {"player-flagged mount",    5747, 5748},
        {"mid-animation mount",     5749, 5750},
    };

    for (const Axis& axis : axes)
    {
        SCOPED_TRACE(axis.name);

        auto mount = makeObject(module, "mp_data/globalobjects/pets/hotdog.obj", axis.mountSlot);
        auto rider = makeObject(module, "mp_objects/follower.obj", axis.riderSlot);

        ASSERT_NE(mount, nullptr);
        ASSERT_NE(rider, nullptr);
        ASSERT_FALSE(mount->getProfile()->riderCanAttack());
        ASSERT_TRUE(mount->isMount());
        ASSERT_TRUE(rider->attachToObject(mount->getObjRef(), GRIP_ONLY));

        // Vary exactly one axis away from the T9 baseline (which DOES let the mount fire).
        if (axis.mountSlot == 5745)
        {
            // `_isAlive` lives in the privately inherited ObjectState base
            // (ObjectState.hpp:164); Object_combat.cpp:562 is the only runtime writer that
            // CLEARS it (Object_lifecycle.cpp:245 sets it again on respawn).
            mount->_isAlive = false;
            ASSERT_FALSE(mount->isAlive());
        }
        else if (axis.mountSlot == 5747)
        {
            mount->setLocalPlayer(true);
            ASSERT_TRUE(mount->isPlayer());
        }
        else
        {
            Ego::Graphics::ObjectGraphicsTestAccess::setCanBeInterrupted(mount->getGraphics(), false);
            ASSERT_FALSE(mount->getGraphics().canBeInterrupted());
        }

        topUpMana(rider);
        rider->bore_timer = 0;
        mount->clearAIAlertBits(ALERTIF_USED);
        rider->clearAIAlertBits(ALERTIF_USED);
        const ObjectRef lastItemUsedBefore = rider->getAILastItemUsed();
        const ModelAction riderAnimationBefore = rider->getGraphics().getCurrentAnimation();
        const float manaBefore = rider->getMana();

        EXPECT_FALSE(chr_do_latch_attack(rider.get(), SLOT_LEFT));

        EXPECT_FALSE(mount->hasAnyAIAlertBits(ALERTIF_USED));
        EXPECT_FALSE(rider->hasAnyAIAlertBits(ALERTIF_USED));
        EXPECT_EQ(rider->getAILastItemUsed(), lastItemUsedBefore);
        EXPECT_EQ(rider->getReloadTimer(), 0);
        EXPECT_EQ(mount->getReloadTimer(), 0);
        EXPECT_EQ(rider->getGraphics().getCurrentAnimation(), riderAnimationBefore);
        EXPECT_FLOAT_EQ(rider->getMana(), manaBefore);
        // retval == false, so the boredom reset at :360-363 never runs.
        EXPECT_EQ(rider->bore_timer, 0);
    }
}

//--------------------------------------------------------------------------------------------
// T10 — the right slot is attempted only when the left slot did not "handle" the attack.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, RightSlotAttackFiresOnlyWhenLeftNotHandled)
{
    auto& module = beginActiveTestModule();

    // Phase A — the left attack succeeds, so the right slot is never attempted.
    auto actor = makeObject(module, "mp_objects/follower.obj", 5719);
    auto mace = makeObject(module, "mp_data/globalobjects/weapons/mace.obj", 5720);
    auto knife = makeObject(module, "mp_data/globalobjects/weapons/knife.obj", 5721);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(mace, nullptr);
    ASSERT_NE(knife, nullptr);
    ASSERT_TRUE(mace->attachToObject(actor->getObjRef(), GRIP_LEFT));
    ASSERT_TRUE(knife->attachToObject(actor->getObjRef(), GRIP_RIGHT));

    const auto& actorModel = actor->getProfile()->getModel();
    ASSERT_EQ(actorModel->getAction(ACTION_BA), ACTION_BA);
    ASSERT_EQ(actorModel->getAction(ACTION_BB), ACTION_BA);
    // Right-hand THRUST randomizes into the TC/TD pair (slot * 2 offset,
    // ModelAnimationMetadata.cpp:411-472); neither exists on the follower model.
    ASSERT_EQ(actorModel->getAction(ACTION_TC), ACTION_DA);
    ASSERT_EQ(actorModel->getAction(ACTION_TD), ACTION_DA);

    mace->clearAIAlertBits(ALERTIF_USED);
    knife->clearAIAlertBits(ALERTIF_USED);
    actor->setLatchButton(LATCHBUTTON_LEFT, true);
    actor->setLatchButton(LATCHBUTTON_RIGHT, true);

    actor->updateLatchButtons();

    // Object_interaction.cpp:467-473 — attack_handled short-circuits the right slot.
    EXPECT_TRUE(mace->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_FALSE(knife->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_EQ(actor->getReloadTimer(), 0);

    // Phase B — the left attack returns false (weapon cooling), so the right slot IS
    // attempted, and its ACTION_DA arm publishes ALERTIF_USED even though the right-hand
    // call's return value is discarded at Object_interaction.cpp:472.
    auto actorB = makeObject(module, "mp_objects/follower.obj", 5771);
    auto maceB = makeObject(module, "mp_data/globalobjects/weapons/mace.obj", 5772);
    auto knifeB = makeObject(module, "mp_data/globalobjects/weapons/knife.obj", 5773);

    ASSERT_NE(actorB, nullptr);
    ASSERT_NE(maceB, nullptr);
    ASSERT_NE(knifeB, nullptr);
    ASSERT_TRUE(maceB->attachToObject(actorB->getObjRef(), GRIP_LEFT));
    ASSERT_TRUE(knifeB->attachToObject(actorB->getObjRef(), GRIP_RIGHT));

    maceB->setReloadTimer(10);
    maceB->clearAIAlertBits(ALERTIF_USED);
    knifeB->clearAIAlertBits(ALERTIF_USED);
    actorB->setLatchButton(LATCHBUTTON_LEFT, true);
    actorB->setLatchButton(LATCHBUTTON_RIGHT, true);

    actorB->updateLatchButtons();

    EXPECT_FALSE(maceB->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_TRUE(knifeB->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_EQ(actorB->getReloadTimer(), 0);
}

//--------------------------------------------------------------------------------------------
// T10b — the CHARACTER's own reload timer closes both attack slots at once.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, CharacterReloadTimerBlocksBothAttackSlots)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5752);
    auto mace = makeObject(module, "mp_data/globalobjects/weapons/mace.obj", 5753);
    auto knife = makeObject(module, "mp_data/globalobjects/weapons/knife.obj", 5754);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(mace, nullptr);
    ASSERT_NE(knife, nullptr);
    ASSERT_TRUE(mace->attachToObject(actor->getObjRef(), GRIP_LEFT));
    ASSERT_TRUE(knife->attachToObject(actor->getObjRef(), GRIP_RIGHT));

    mace->clearAIAlertBits(ALERTIF_USED);
    knife->clearAIAlertBits(ALERTIF_USED);
    actor->setLatchButton(LATCHBUTTON_LEFT, true);
    actor->setLatchButton(LATCHBUTTON_RIGHT, true);

    // Both attack gates in Object::updateLatchButtons read `0 == reload_timer` off the
    // CHARACTER (Object_interaction.cpp:468 and :471), never off the weapon; the per-weapon
    // gate lives one level down at game_combat.c:172. A cooling character therefore cannot
    // attack with either hand, no matter how ready the weapons are.
    actor->setReloadTimer(10);
    ASSERT_EQ(mace->getReloadTimer(), 0);
    ASSERT_EQ(knife->getReloadTimer(), 0);
    const ObjectRef lastItemUsedBefore = actor->getAILastItemUsed();

    actor->updateLatchButtons();

    EXPECT_FALSE(mace->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_FALSE(knife->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_EQ(mace->getReloadTimer(), 0);
    EXPECT_EQ(knife->getReloadTimer(), 0);
    EXPECT_EQ(actor->getAILastItemUsed(), lastItemUsedBefore);
    EXPECT_EQ(actor->getReloadTimer(), 10);

    // Control: clear the character gate and the very same call now attacks.
    actor->setReloadTimer(0);
    actor->updateLatchButtons();
    EXPECT_TRUE(mace->hasAnyAIAlertBits(ALERTIF_USED));
}

//--------------------------------------------------------------------------------------------
// T10c — QUIRK: a grab latch in the same tick silently eats the attack latch.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, GrabLatchInSameTickSuppressesAttackLatch_Quirk)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5755);
    auto control = makeObject(module, "mp_objects/follower.obj", 5756);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(control, nullptr);
    ASSERT_EQ(actor->getHeldObject(SLOT_LEFT), ObjectRef::Invalid);
    topUpMana(actor);
    topUpMana(control);

    // Control arm: LEFT alone attacks (unarmed ZAP), proving the suppression below is caused
    // by ALTLEFT and not by anything else in the fixture.
    control->clearAIAlertBits(ALERTIF_USED);
    control->setLatchButton(LATCHBUTTON_LEFT, true);
    control->updateLatchButtons();
    ASSERT_TRUE(control->hasAnyAIAlertBits(ALERTIF_USED));
    ASSERT_EQ(control->getReloadTimer(), 0);

    actor->clearAIAlertBits(ALERTIF_USED);
    actor->setLatchButton(LATCHBUTTON_ALTLEFT, true);
    actor->setLatchButton(LATCHBUTTON_LEFT, true);

    actor->updateLatchButtons();

    // QUIRK PIN — characterizes current behavior — do not fix silently.
    // Object_interaction.cpp:441-442 sets `reload_timer = GRABDELAY` for the grab latch, and
    // the attack gates at :468/:471 read that same field LATER in the same call. Pressing
    // grab and attack on one tick therefore discards the attack entirely — no alert, no
    // animation, no feedback of any kind. The latch bits both survive (nothing in
    // updateLatchButtons clears them), so the attack does eventually land once the 25-tick
    // grab delay burns down, just not on the tick the player pressed it.
    EXPECT_FALSE(actor->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_EQ(actor->getReloadTimer(), static_cast<uint16_t>(Object::GRABDELAY));
    EXPECT_TRUE(actor->_inputLatchesPressed[LATCHBUTTON_LEFT]);
    EXPECT_TRUE(actor->_inputLatchesPressed[LATCHBUTTON_ALTLEFT]);
}

//--------------------------------------------------------------------------------------------
// T11 — a full Object::update() consumes the latch but never clears it.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, FullObjectUpdateConsumesLatchWithoutClearingIt)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5722);

    ASSERT_NE(actor, nullptr);
    ASSERT_EQ(actor->getHeldObject(SLOT_LEFT), ObjectRef::Invalid);
    topUpMana(actor);

    // Object::update() early-outs while worldUpdateCount() == 0 (Object_update.cpp:67);
    // GameSessionContext::worldUpdateCount() hands back a mutable reference
    // (ModuleUpdate.cpp:590 idiom).
    GameSessionContext::get().worldUpdateCount() = 1;
    actor->clearAIAlertBits(ALERTIF_USED);
    actor->setLatchButton(LATCHBUTTON_LEFT, true);

    actor->update();

    // Object_update.cpp:334 -> updateLatchButtons -> chr_do_latch_attack(SLOT_LEFT).
    EXPECT_TRUE(actor->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_EQ(actor->getAILastItemUsed(), actor->getObjRef());
    // Latches are LEVEL triggered: the consumer never clears them (Object_interaction.cpp:373-474
    // has no write to _inputLatchesPressed), only external resetters do.
    EXPECT_TRUE(actor->_inputLatchesPressed[LATCHBUTTON_LEFT]);
    // ZAP is absent from the reload table (game_combat.c:326-340), so no cooldown is created.
    EXPECT_EQ(actor->getReloadTimer(), 0);
}

//--------------------------------------------------------------------------------------------
// T12 — the script driver resets latches for NON-player-flagged actors only.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, ScriptDriverResetsLatchesForNonPlayerFlaggedActorsOnly)
{
    auto& module = beginActiveTestModule();
    auto nonPlayerActor = makeObject(module, "mp_objects/follower.obj", 5723);
    auto playerFlaggedActor = makeObject(module, "mp_objects/follower.obj", 5724);

    ASSERT_NE(nonPlayerActor, nullptr);
    ASSERT_NE(playerFlaggedActor, nullptr);

    for (const auto& actor : {nonPlayerActor, playerFlaggedActor})
    {
        // An empty instruction body still exercises the pre/post-loop driver logic
        // (ScriptRuntime.cpp:187 idiom). Distinct profile slots -> distinct scripts.
        actor->getProfile()->getAIScript()._instructions.clear();
        actor->setLatchButton(LATCHBUTTON_RIGHT, true);
        actor->addAIAlertBits(ALERTIF_USED);

        // Neutralize the waypoint path so applyNonPlayerMovementLatchUpdate
        // (script_driver.c:203-219) cannot republish a desired velocity.
        ai_state_t& aiState = Ego::Script::runtimeState(*actor);
        aiState.wp_valid = false;
        waypoint_list_t::clear(aiState.wp_lst);
        actor->setDesiredVelocity(Ego::Vector2f(1.0f, 1.0f));
    }

    playerFlaggedActor->setLocalPlayer(true);
    ASSERT_FALSE(nonPlayerActor->isPlayer());
    ASSERT_TRUE(playerFlaggedActor->isPlayer());

    scr_run_chr_script(nonPlayerActor->getObjRef());
    scr_run_chr_script(playerFlaggedActor->getObjRef());

    // script_driver.c:173-179 (invoked at :318) keys resetNonPlayerInputCommands on
    // RuntimeActorContext::isPlayerActor() == ITargetInfo::isPlayer(), which is the raw
    // `islocalplayer` FLAG (Object_accessors.cpp:206) — NOT playerList membership.
    EXPECT_TRUE(nonPlayerActor->_inputLatchesPressed.none());
    EXPECT_FLOAT_EQ(nonPlayerActor->getDesiredVelocity().x(), 0.0f);
    EXPECT_FLOAT_EQ(nonPlayerActor->getDesiredVelocity().y(), 0.0f);

    EXPECT_TRUE(playerFlaggedActor->_inputLatchesPressed[LATCHBUTTON_RIGHT]);

    // Both actors reach the end-of-run alert wipe at script_driver.c:353, because the
    // player flag is not one of the things that can skip a run. It is NOT an unconditional
    // wipe, though — see ScriptRunSkippedForPoofingActorLeavesLatchesAndAlertsIntact.
    EXPECT_FALSE(nonPlayerActor->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_FALSE(playerFlaggedActor->hasAnyAIAlertBits(ALERTIF_USED));
}

//--------------------------------------------------------------------------------------------
// T12b — a poofing actor's script run is skipped before the latch reset AND the alert wipe.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, ScriptRunSkippedForPoofingActorLeavesLatchesAndAlertsIntact)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5757);

    ASSERT_NE(actor, nullptr);
    ASSERT_FALSE(actor->isPlayer());
    actor->getProfile()->getAIScript()._instructions.clear();

    actor->setLatchButton(LATCHBUTTON_RIGHT, true);
    actor->addAIAlertBits(ALERTIF_USED);

    // shouldSkipScriptRun (script_driver.c:155-160) short-circuits runCharacterScript at :292,
    // i.e. BEFORE resetNonPlayerInputCommands at :318 and before the alert wipe at :353. Two
    // conditions reach it: a terminated actor, and one whose `poof_time` has elapsed.
    GameSessionContext::get().worldUpdateCount() = 1;
    ai_state_t& aiState = Ego::Script::runtimeState(*actor);
    aiState.poof_time = 0;

    scr_run_chr_script(actor->getObjRef());

    EXPECT_TRUE(actor->_inputLatchesPressed[LATCHBUTTON_RIGHT]);
    EXPECT_TRUE(actor->hasAnyAIAlertBits(ALERTIF_USED));

    // Control: with poof_time back out of range the same call performs both.
    aiState.poof_time = -1;
    scr_run_chr_script(actor->getObjRef());

    EXPECT_TRUE(actor->_inputLatchesPressed.none());
    EXPECT_FALSE(actor->hasAnyAIAlertBits(ALERTIF_USED));

    // NOTE: the OUTER filter is separate again. MainLoop::let_all_characters_think
    // (game_loop.c:474-493) never calls scr_run_chr_script for a terminated object, for an
    // inventory item that is not equipment, or for a dead object that was neither crushed nor
    // cleaned up — so those never reach either half. That loop is not exercised here.
}

//--------------------------------------------------------------------------------------------
// T13 — the player path wipes latches even without a camera, but skips unresolvable objects.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, PlayerUpdateLatchesWipesEvenWithoutCameraButSkipsUnresolvableObject)
{
    ScopedStubCameraSystem cameraGuard;

    auto& module = beginActiveTestModule();
    auto boundActor = makeObject(module, "mp_objects/follower.obj", 5725);
    auto unboundActor = makeObject(module, "mp_objects/follower.obj", 5726);

    ASSERT_NE(boundActor, nullptr);
    ASSERT_NE(unboundActor, nullptr);
    ASSERT_EQ(EngineContext::get().cameraSystem().getCamera(boundActor->getObjRef()), nullptr);

    // Arm A — a resolvable player. Player.cpp:159 resets the object's input commands BEFORE
    // the null-camera early return at :162-165, so the wipe happens even headless.
    boundActor->setLatchButton(LATCHBUTTON_LEFT, true);
    boundActor->setDesiredVelocity(Ego::Vector2f(1.0f, -1.0f));

    auto player = std::make_shared<Ego::Player>(boundActor->getObjRef(), Ego::Input::InputDevice::DeviceList[0]);
    player->updateLatches();

    EXPECT_TRUE(boundActor->_inputLatchesPressed.none());
    EXPECT_FLOAT_EQ(boundActor->getDesiredVelocity().x(), 0.0f);
    EXPECT_FLOAT_EQ(boundActor->getDesiredVelocity().y(), 0.0f);

    // Arm B — the `object->isTerminated()` half of the SAME guard (Player.cpp:156-158),
    // measured on ONE object so the assertion cannot pass vacuously: the player resolves the
    // object either way, and only the terminated flag decides whether the reset at :159 runs.
    unboundActor->setLatchButton(LATCHBUTTON_LEFT, true);
    // NOTE: setDesiredVelocity normalizes its argument, so capture what it stored rather than
    // asserting the value that was handed in.
    unboundActor->setDesiredVelocity(Ego::Vector2f(1.0f, -1.0f));
    const float desiredXBefore = unboundActor->getDesiredVelocity().x();
    ASSERT_GT(desiredXBefore, 0.0f);

    auto terminatedPlayer = std::make_shared<Ego::Player>(unboundActor->getObjRef(), Ego::Input::InputDevice::DeviceList[0]);
    unboundActor->markTerminateRequested();
    ASSERT_TRUE(unboundActor->isTerminated());

    terminatedPlayer->updateLatches();

    EXPECT_TRUE(unboundActor->_inputLatchesPressed[LATCHBUTTON_LEFT]);
    EXPECT_FLOAT_EQ(unboundActor->getDesiredVelocity().x(), desiredXBefore);

    // ... and with the flag cleared the identical call wipes both.
    // (`_terminateRequested` is ObjectState.hpp:159; Object::markTerminateRequested is the
    // only public setter and there is no public clear.)
    unboundActor->_terminateRequested = false;
    terminatedPlayer->updateLatches();

    EXPECT_TRUE(unboundActor->_inputLatchesPressed.none());
    EXPECT_FLOAT_EQ(unboundActor->getDesiredVelocity().x(), 0.0f);

    // Arm C — the `!object` half. There is no state to observe, but the early return is
    // load-bearing: without it Player::updateLatches would dereference a null Object at
    // Player.cpp:159/:162, so reaching the next statement at all is the pin.
    auto ghostPlayer = std::make_shared<Ego::Player>(ObjectRef::Invalid, Ego::Input::InputDevice::DeviceList[0]);
    ghostPlayer->updateLatches();
    SUCCEED() << "Player::updateLatches returned for an unresolvable object";
}

//--------------------------------------------------------------------------------------------
// T14 — script-side latch presses: out-of-range indices are silently swallowed.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, PressLatchButtonIgnoresOutOfRangeIndicesButReturnsTrue)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5727);
    auto target = makeObject(module, "mp_objects/follower.obj", 5728);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    ai_state_t self = makeScriptSelf(actor);
    self.setTarget(target->getObjRef());
    script_state_t state;

    // In range: index 6 (LATCHBUTTON_PACKRIGHT) is the last accepted value, because the
    // guard is `>= LATCHBUTTON_LEFT && < LATCHBUTTON_RESPAWN`
    // (script_functions_movement_locomotion.c:93).
    state.argument = LATCHBUTTON_PACKRIGHT;
    EXPECT_TRUE(scr_PressLatchButton(state, self));
    EXPECT_TRUE(actor->_inputLatchesPressed[LATCHBUTTON_PACKRIGHT]);
    EXPECT_EQ(actor->_inputLatchesPressed.count(), 1u);

    // Out of range: the function reports SUCCESS but presses nothing
    // (script_functions_movement_locomotion.c:93-98). Legacy scripts written against the old
    // bit-MASK encoding therefore fail silently; that hazard is content-side, not fixable in
    // the runtime, and is not pinned here beyond the swallow itself.
    for (const int badArgument : {static_cast<int>(LATCHBUTTON_RESPAWN), 255, -1})
    {
        state.argument = badArgument;
        EXPECT_TRUE(scr_PressLatchButton(state, self)) << "argument " << badArgument;
        EXPECT_EQ(actor->_inputLatchesPressed.count(), 1u) << "argument " << badArgument;
        EXPECT_TRUE(actor->_inputLatchesPressed[LATCHBUTTON_PACKRIGHT]) << "argument " << badArgument;
    }

    // Same matrix for the target variant (script_functions_movement_locomotion.c:133-136).
    state.argument = LATCHBUTTON_PACKRIGHT;
    EXPECT_TRUE(scr_PressTargetLatchButton(state, self));
    EXPECT_TRUE(target->_inputLatchesPressed[LATCHBUTTON_PACKRIGHT]);
    EXPECT_EQ(target->_inputLatchesPressed.count(), 1u);

    for (const int badArgument : {static_cast<int>(LATCHBUTTON_RESPAWN), 255, -1})
    {
        state.argument = badArgument;
        EXPECT_TRUE(scr_PressTargetLatchButton(state, self)) << "argument " << badArgument;
        EXPECT_EQ(target->_inputLatchesPressed.count(), 1u) << "argument " << badArgument;
        EXPECT_TRUE(target->_inputLatchesPressed[LATCHBUTTON_PACKRIGHT]) << "argument " << badArgument;
    }

    // Asymmetry: an unresolvable TARGET is reported as failure
    // (script_functions_movement_locomotion.c:126-130), while an out-of-range ARGUMENT is not.
    self.setTarget(ObjectRef::Invalid);
    state.argument = LATCHBUTTON_PACKRIGHT;
    EXPECT_FALSE(scr_PressTargetLatchButton(state, self));
}

//--------------------------------------------------------------------------------------------
// T15 — character_swipe: ammo consumption and the empty-weapon arm.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, CharacterSwipeConsumesXbowAmmoAndEmptyOnlySetsAmmoKnown)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5729);
    auto crossbow = makeObject(module, "mp_data/globalobjects/weapons/xbow.obj", 5730);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(crossbow, nullptr);
    ASSERT_TRUE(crossbow->attachToObject(actor->getObjRef(), GRIP_LEFT));

    ASSERT_FALSE(crossbow->getProfile()->isStackable());
    ASSERT_FALSE(crossbow->getProfile()->hasAttachParticleToWeapon());
    ASSERT_EQ(crossbow->getAmmo(), 50);
    ASSERT_EQ(crossbow->getAmmoMax(), 50);
    ASSERT_NE(crossbow->getProfile()->getAttackParticleProfile(), INVALID_PIP_REF);
    // The Wand-Mastery and Double-Shot arms (game_combat.c:475-505) are perk+IDSZ gated and
    // are provably not taken here (no perks on a fresh spawn, parent IDSZ is [XBOW]).
    ASSERT_FALSE(actor->hasPerk(Ego::Perks::WAND_MASTERY));
    ASSERT_FALSE(actor->hasPerk(Ego::Perks::DOUBLE_SHOT));

    flushObjectHandler(module);

    // ParticleHandler::spawnParticle pushes into _pendingParticles; getCount() covers both
    // lists (ParticleHandler.hpp:154-156) whereas beginActiveParticles() does not, so a
    // just-spawned particle is only observable through a getCount() delta.
    const size_t particlesBefore = EngineContext::get().particleHandler().getCount();

    character_swipe(actor->getObjRef(), SLOT_LEFT);

    // game_combat.c:470-489 — non-stackable weapon with ammo spends exactly one round.
    EXPECT_EQ(crossbow->getAmmo(), 49);
    EXPECT_EQ(EngineContext::get().particleHandler().getCount(), particlesBefore + 1);

    // The empty arm: game_combat.c:670-673 only flags the ammo as known.
    crossbow->setAmmo(0);
    crossbow->setAmmoKnown(false);
    const size_t particlesBeforeEmptySwipe = EngineContext::get().particleHandler().getCount();

    character_swipe(actor->getObjRef(), SLOT_LEFT);

    EXPECT_EQ(EngineContext::get().particleHandler().getCount(), particlesBeforeEmptySwipe);
    EXPECT_EQ(crossbow->getAmmo(), 0);
    EXPECT_TRUE(crossbow->isAmmoKnown());
}

//--------------------------------------------------------------------------------------------
// T15b — the X (Crossbow) reload family, the slowest row of the reload table.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, CrossbowFamilyLatchAttackParks130TickReloadOnTheWeapon)
{
    auto& module = beginActiveTestModule();
    // follower.obj has no X-family frames, so it can never reach the X row of the reload
    // table; druid.obj's model defines XA/XC.
    auto actor = makeObject(module, "mp_data/globalobjects/players/druid.obj", 5758);
    auto crossbow = makeObject(module, "mp_data/globalobjects/weapons/xbow.obj", 5759);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(crossbow, nullptr);
    ASSERT_TRUE(crossbow->attachToObject(actor->getObjRef(), GRIP_LEFT));

    ASSERT_EQ(static_cast<ModelAction>(crossbow->getProfile()->getWeaponAction()), ACTION_XA);
    ASSERT_FALSE(crossbow->getProfile()->requiresSkillIDToUse());
    ASSERT_FALSE(crossbow->getProfile()->hasFastAttack());
    // Both sub-picks of randomizeAction(ACTION_XA, SLOT_LEFT) must land on the same action or
    // the reload family would be a coin flip (ModelAnimationMetadata.cpp:411-472).
    const auto& actorModel = actor->getProfile()->getModel();
    ASSERT_EQ(actorModel->getAction(ACTION_XA), ACTION_XA);
    ASSERT_EQ(actorModel->getAction(ACTION_XB), ACTION_XA);
    // The two perk arms that would perturb the arithmetic (game_combat.c:309-312 and :317-321)
    // are provably not taken on a fresh spawn.
    ASSERT_FALSE(actor->hasPerk(Ego::Perks::CROSSBOW_MASTERY));
    ASSERT_FALSE(actor->hasPerk(Ego::Perks::QUICK_STRIKE));

    const float agility = actor->getAttribute(Ego::Attribute::AGILITY);
    crossbow->clearAIAlertBits(ALERTIF_USED);

    EXPECT_TRUE(chr_do_latch_attack(actor.get(), SLOT_LEFT));

    // game_combat.c:335 — X (Crossbow) is the heaviest row: `(int)(-agility) + 130`.
    const int expectedReload = static_cast<int>(-agility) + 130;
    ASSERT_GT(expectedReload, 0) << "content drift: druid agility now zeroes the reload";
    EXPECT_EQ(crossbow->getReloadTimer(), static_cast<uint16_t>(expectedReload));
    EXPECT_EQ(actor->getReloadTimer(), 0);
    EXPECT_TRUE(crossbow->hasAnyAIAlertBits(ALERTIF_USED));
    EXPECT_EQ(actor->getGraphics().getCurrentAnimation(), ACTION_XA);
    // Firing did NOT consume ammo: chr_do_latch_attack never calls character_swipe.
    EXPECT_EQ(crossbow->getAmmo(), 50);
}

//--------------------------------------------------------------------------------------------
// T15c — the ONLY edge from the attack chain into character_swipe is a per-frame MADFX.
//--------------------------------------------------------------------------------------------
TEST_F(LatchAttackChainFixture, AnimationActLeftFrameEffectIsTheOnlyEdgeIntoCharacterSwipe)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5760);
    auto crossbow = makeObject(module, "mp_data/globalobjects/weapons/xbow.obj", 5761);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(crossbow, nullptr);
    ASSERT_TRUE(crossbow->attachToObject(actor->getObjRef(), GRIP_LEFT));
    ASSERT_EQ(crossbow->getAmmo(), 50);

    // Premise: the follower model really does carry an MADFX_ACTLEFT frame (`ZA02ALS` and
    // `BA03SAL` at the time of writing; parsed by ModelAnimationMetadata_legacy.cpp:176-181).
    const auto& frames = actor->getProfile()->getModel()->getModel()->getFrames();
    int actLeftFrame = -1;
    for (size_t i = 0; i < frames.size(); ++i)
    {
        if (HAS_SOME_BITS(frames[i].framefx, MADFX_ACTLEFT))
        {
            actLeftFrame = static_cast<int>(i);
            break;
        }
    }
    ASSERT_GE(actLeftFrame, 0) << "content drift: follower.obj has no MADFX_ACTLEFT frame";

    using Ego::Graphics::ObjectGraphicsTestAccess;
    auto& graphics = actor->getGraphics();

    // Park the instance one quarter-step short of ilip 3, aimed at that frame.
    // ObjectGraphics::getFrameFX (ObjectGraphics.cpp:231-233) reads the TARGET frame, and
    // applyPublishedInterpolationStep (ObjectGraphics_animation.cpp:289-294) calls
    // handleAnimationFX exactly when ilip reaches 3. `_canBeInterrupted == false` is what an
    // attack animation looks like and also short-circuits updateAnimationRate
    // (ObjectGraphics_animation.cpp:386-404), keeping the step purely about the FX.
    ObjectGraphicsTestAccess::setCanBeInterrupted(graphics, false);
    ObjectGraphicsTestAccess::setSourceFrameIndex(graphics, static_cast<uint16_t>(actLeftFrame));
    ObjectGraphicsTestAccess::setTargetFrameIndex(graphics, static_cast<uint16_t>(actLeftFrame));
    ObjectGraphicsTestAccess::setAnimationProgressInteger(graphics, 2);
    ObjectGraphicsTestAccess::setAnimationProgress(graphics, 0.5f);
    ObjectGraphicsTestAccess::setAnimationRate(graphics, 1.0f);

    flushObjectHandler(module);
    const size_t particlesBefore = EngineContext::get().particleHandler().getCount();

    graphics.updateAnimation();

    // ObjectGraphics_animation.cpp:118-121 maps MADFX_ACTLEFT to
    // `character_swipe(_object.getObjRef(), SLOT_LEFT)`, which spends a round and spawns the
    // attack particle. This is the whole of the link between the two halves of the chain:
    // chr_do_latch_attack starts an action, the animation clock runs the action's frames, and
    // a frame effect fires the weapon.
    EXPECT_EQ(crossbow->getAmmo(), 49);
    EXPECT_EQ(EngineContext::get().particleHandler().getCount(), particlesBefore + 1);
    EXPECT_EQ(ObjectGraphicsTestAccess::animationProgressInteger(graphics), 3);
}

//--------------------------------------------------------------------------------------------
// T16 — DISABLED documenting reproduction of the OPEN wizard.mod play-test bug.
//--------------------------------------------------------------------------------------------
//
// This test is DISABLED on purpose. It pins the MECHANISM of an open BUG, not desired
// behavior, so it must never gate CI. Run it manually with:
//
//     ./build/products/x64/bin/egolib-tests-executable
//         --gtest_also_run_disabled_tests
//         --gtest_filter='*StuckLatchRepro*'
//
// The stuck-latch gap, in three parts:
//
//  1. Latch RESET ownership is split by the `islocalplayer` flag. The script driver refuses
//     to reset anything flagged as a player (script_driver.c:173-179), deferring to the
//     player input path; the player input path only ever touches objects reachable from a
//     resolving Ego::Player (Player.cpp:156-158, game_loop.c:146-149). An object with
//     islocalplayer == true and NO resolving playerList entry is therefore reset by NOBODY.
//
//  2. The latch CONSUMER never clears the bits it acts on: Object::updateLatchButtons
//     (Object_interaction.cpp:373-474) contains no write to _inputLatchesPressed.
//
//  3. The attack has no engine cooldown of its own. The reload table at game_combat.c:326-340
//     covers U/T/C/S/B/L/X/F; the Z family used here — and the D family the wizard content
//     actually lands on (see the WIZARD CONTENT NOTE in the file header) — are both absent,
//     so `base_reload_time` never exceeds the :339 guard and no timer is ever created. The
//     ONLY throttle left in the path is the canBeInterrupted() gate at game_combat.c:268,
//     i.e. "is the previous attack animation still playing".
//
// TWO SEPARATE SCRIPTS TURN A STUCK LATCH INTO MISSILES, and they consume ALERTIF_USED in
// OPPOSITE ways. Note that the bit does not latch: script_driver.c:353 wipes aiState.alert at
// the end of every script run, so USED is re-raised per attack rather than accumulating.
//
//   (a) the HELD WAND, data/basicdat/globalobjects/magic/missile.obj/script.txt, does NOT shoot
//       on IfUsed. Its IfUsed arm (script.txt:11-39) only CHARGES (`tmpargument = selfcontent
//       + 16`, capped at 1536, paid for with the holder's mana) and picks a state. The arm that
//       actually calls SpawnExactParticle is the Else arm (script.txt:40-89) — the ticks on
//       which ALERTIF_USED is ABSENT — which requires selfcontent > 256 and then resets it to 0.
//       This path needs an OSCILLATION between USED-present and USED-absent ticks.
//
//   (b) the PLAYER CHARACTER'S OWN script, data/basicdat/globalobjects/players/wizard.obj/
//       script.txt:26-42, spawns a homing missile DIRECTLY on each IfUsed (gated on
//       selflevel > 12 and targetmana > 255, paying CostTargetMana 256 at :35). Unarmed attacks
//       publish USED to the actor ITSELF (weapon == self), which is what feeds this half. No
//       oscillation is required here — every USED-raising tick fires.
//
// WHY THE METRIC IS "CYCLES", NOT "FIRES": the metric has to serve path (a), the stricter of
// the two. Gate (3) supplies exactly the oscillation it needs — the attack republishes USED
// whenever the animation is interruptible and goes quiet while it is not. A run in which USED
// were set on all 300 ticks would still satisfy path (b) but would produce ZERO missiles on
// path (a), so counting USED-present ticks alone would be an inverted metric for the wand.
// This loop counts USED-present -> USED-absent TRANSITIONS (one charge/fire cycle each) and
// asserts that BOTH phases were observed. The cadence depends on agility-scaled animation rates
// and content frame counts, so nothing here asserts an exact count.
//
// Known caveat: ObjectGraphics::updateAnimation routes frame FX through
// character_swipe/placeAtVertex; if that ever throws headless, this repro needs a stub. That
// is acceptable for a DISABLED documenting test.
//
TEST_F(LatchAttackChainFixture, DISABLED_StuckLatchRepro_PlayerFlaggedObjectIsResetByNobody)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5731);

    ASSERT_NE(actor, nullptr);
    topUpMana(actor);
    actor->getProfile()->getAIScript()._instructions.clear();
    actor->setLocalPlayer(true);
    ASSERT_TRUE(actor->isPlayer());

    // ONE press, as an item script's PressTargetLatchButton would do.
    actor->setLatchButton(LATCHBUTTON_LEFT, true);

    // A player that resolves to nothing: exactly the readPlayerInput skip.
    auto ghostPlayer = std::make_shared<Ego::Player>(ObjectRef::Invalid, Ego::Input::InputDevice::DeviceList[0]);

    const ObjectRef actorRef = actor->getObjRef();
    constexpr int TICKS = 300;
    int chargeTicks = 0;    // ALERTIF_USED present -> missile.obj would charge
    int quietTicks = 0;     // ALERTIF_USED absent  -> missile.obj's Else arm would fire
    int cycles = 0;         // present -> absent transitions
    bool usedLastTick = false;

    for (uint32_t tick = 1; tick <= static_cast<uint32_t>(TICKS); ++tick)
    {
        // Module_update.cpp:322-348 ordering.
        GameSessionContext::get().worldUpdateCount() = tick;
        scr_run_chr_script(actorRef);   // player-flagged: skips the reset, wipes alerts at :353
        ghostPlayer->updateLatches();   // no-op: nothing to resolve
        actor->update();
        actor->getGraphics().updateAnimation();  // the Module_update.cpp:105-108 pair

        const bool used = actor->hasAnyAIAlertBits(ALERTIF_USED);
        if (used)
        {
            ++chargeTicks;
        }
        else
        {
            ++quietTicks;
        }

        if (usedLastTick && !used)
        {
            ++cycles;
        }
        usedLastTick = used;
    }

    // Both phases must be present, and the alternation must repeat: that IS the bug.
    EXPECT_GE(cycles, 2);
    EXPECT_GT(chargeTicks, 0);
    EXPECT_GT(quietTicks, 0);
    EXPECT_LT(chargeTicks, TICKS) << "saturated USED would charge forever and fire nothing";
    EXPECT_LT(quietTicks, TICKS) << "no USED tick at all means the latch never reached the attack";

    // Nobody reset the latch, and no cooldown was ever created to slow the loop down.
    EXPECT_TRUE(actor->_inputLatchesPressed[LATCHBUTTON_LEFT]);
    EXPECT_EQ(actor->getReloadTimer(), 0);
}

} // namespace
