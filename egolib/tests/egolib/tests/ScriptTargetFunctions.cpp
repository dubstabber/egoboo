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
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/game/Module/Module.hpp"
#undef private
#include "egolib/Script/script.h"
#include "egolib/game/script_functions.h"
#include "egolib/vfs.h"

namespace
{

class ScriptTargetFunctionsFixture : public ::testing::Test
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
        opts.randomSeed = 41;
        opts.binaryPath = "";
        opts.logPath = "/debug/script-target-function-tests.log";
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

        return module.spawnObject(position, profile, static_cast<TEAM_REF>(Team::TEAM_NULL), 0, Facing(0), "", ObjectRef::Invalid);
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
        const bool began = session.beginModule(module, 41);
        EXPECT_TRUE(began);
        return session.activeModule();
    }

    ai_state_t makeScriptSelf(const std::shared_ptr<Object>& selfObject, const std::shared_ptr<Object>& targetObject) const
    {
        ai_state_t self;
        self.setSelf(selfObject ? selfObject->getObjRef() : ObjectRef::Invalid);
        self.setTarget(targetObject ? targetObject->getObjRef() : ObjectRef::Invalid);
        return self;
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ScriptTargetFunctionsFixture::s_runtime;

TEST_F(ScriptTargetFunctionsFixture, OrderTargetPublishesOrderThroughScriptableRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5301);
    auto target = makeObject(module, "mp_objects/follower.obj", 5302);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    script_state_t state;
    state.argument = 77;
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_TRUE(scr_OrderTarget(state, self));
    EXPECT_TRUE(target->hasAnyAIAlertBits(ALERTIF_ORDERED));
    EXPECT_EQ(Ego::Script::runtimeState(*target).order_value, 77u);
    EXPECT_EQ(Ego::Script::runtimeState(*target).order_counter, 0);
}

TEST_F(ScriptTargetFunctionsFixture, SetTargetToTargetHandsReadsThroughInventoryHolderRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5305);
    auto target = makeObject(module, "mp_objects/follower.obj", 5306);
    auto leftHandItem = makeObject(module, "mp_objects/follower.obj", 5307);
    auto rightHandItem = makeObject(module, "mp_objects/follower.obj", 5308);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(leftHandItem, nullptr);
    ASSERT_NE(rightHandItem, nullptr);

    IInventoryHolder& inventoryHolder = *target;
    inventoryHolder.setHeldObject(SLOT_LEFT, leftHandItem->getObjRef());
    inventoryHolder.setHeldObject(SLOT_RIGHT, rightHandItem->getObjRef());

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_TRUE(scr_SetTargetToTargetLeftHand(state, self));
    EXPECT_EQ(self.getTarget(), leftHandItem->getObjRef());

    self.setTarget(target->getObjRef());
    EXPECT_TRUE(scr_SetTargetToTargetRightHand(state, self));
    EXPECT_EQ(self.getTarget(), rightHandItem->getObjRef());
}

TEST_F(ScriptTargetFunctionsFixture, SetTargetToRiderReadsThroughInventoryHolderRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5309);
    auto rider = makeObject(module, "mp_objects/follower.obj", 5310);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(rider, nullptr);

    IInventoryHolder& inventoryHolder = *actor;
    inventoryHolder.setHeldObject(SLOT_LEFT, rider->getObjRef());

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, nullptr);

    EXPECT_TRUE(scr_SetTargetToRider(state, self));
    EXPECT_EQ(self.getTarget(), rider->getObjRef());
}

TEST_F(ScriptTargetFunctionsFixture, SelfTargetSelectionReadsThroughScriptableRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5313);
    auto attacker = makeObject(module, "mp_objects/follower.obj", 5314);
    auto bumped = makeObject(module, "mp_objects/follower.obj", 5315);
    auto hit = makeObject(module, "mp_objects/follower.obj", 5316);
    auto lastItemUsed = makeObject(module, "mp_objects/follower.obj", 5317);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(attacker, nullptr);
    ASSERT_NE(bumped, nullptr);
    ASSERT_NE(hit, nullptr);
    ASSERT_NE(lastItemUsed, nullptr);

    IScriptable& scriptableActor = *actor;
    scriptableActor.setAILastAttacker(attacker->getObjRef());
    ASSERT_TRUE(scriptableActor.recordAIBump(bumped->getObjRef()));
    scriptableActor.setAILastHit(hit->getObjRef());
    scriptableActor.setAILastItemUsed(lastItemUsed->getObjRef());

    ai_state_t self = makeScriptSelf(actor, nullptr);
    script_state_t state;

    EXPECT_TRUE(scr_SetTargetToWhoeverAttacked(state, self));
    EXPECT_EQ(self.getTarget(), attacker->getObjRef());

    self.setTarget(ObjectRef::Invalid);
    EXPECT_TRUE(scr_SetTargetToWhoeverBumped(state, self));
    EXPECT_EQ(self.getTarget(), bumped->getObjRef());

    self.setTarget(ObjectRef::Invalid);
    EXPECT_TRUE(scr_SetTargetToWhoeverWasHit(state, self));
    EXPECT_EQ(self.getTarget(), hit->getObjRef());

    self.setTarget(ObjectRef::Invalid);
    EXPECT_TRUE(scr_SetTargetToLastItemUsed(state, self));
    EXPECT_EQ(self.getTarget(), lastItemUsed->getObjRef());
}

TEST_F(ScriptTargetFunctionsFixture, SetTargetToWhoeverAttackedFailsWhenScriptableRefIsMissing)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 53120);

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, nullptr);

    EXPECT_FALSE(scr_SetTargetToWhoeverAttacked(state, self));
    EXPECT_EQ(self.getTarget(), ObjectRef::Invalid);
}

TEST_F(ScriptTargetFunctionsFixture, SetTargetToWhoeverIsHoldingReadsThroughTargetInfoRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5318);
    auto holder = makeObject(module, "mp_objects/follower.obj", 5319);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(holder, nullptr);

    actor->setHolderRef(holder->getObjRef());

    ai_state_t self = makeScriptSelf(actor, nullptr);
    script_state_t state;

    EXPECT_TRUE(scr_SetTargetToWhoeverIsHolding(state, self));
    EXPECT_EQ(self.getTarget(), holder->getObjRef());
}

TEST_F(ScriptTargetFunctionsFixture, SetTargetToWhoeverIsHoldingFailsWithoutHolder)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 53121);

    ASSERT_NE(actor, nullptr);

    ai_state_t self = makeScriptSelf(actor, nullptr);
    script_state_t state;

    EXPECT_FALSE(scr_SetTargetToWhoeverIsHolding(state, self));
    EXPECT_EQ(self.getTarget(), ObjectRef::Invalid);
}

TEST_F(ScriptTargetFunctionsFixture, TargetStateAndContentQueriesReadThroughScriptableRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5311);
    auto target = makeObject(module, "mp_objects/follower.obj", 5312);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    IScriptable& scriptableTarget = *target;
    scriptableTarget.setAIStateValue(31);
    scriptableTarget.setAIContent(32);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_TRUE(scr_GetTargetState(state, self));
    EXPECT_EQ(state.argument, 31);

    state.argument = 0;
    EXPECT_TRUE(scr_GetTargetContent(state, self));
    EXPECT_EQ(state.argument, 32);
}

TEST_F(ScriptTargetFunctionsFixture, TargetDamageTypeQueryReadsThroughScriptableRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5321);
    auto target = makeObject(module, "mp_objects/follower.obj", 5322);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    IScriptable& scriptableTarget = *target;
    scriptableTarget.setAILastDamageType(DamageType::DAMAGE_FIRE);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_TRUE(scr_GetTargetDamageType(state, self));
    EXPECT_EQ(state.argument, static_cast<int>(DamageType::DAMAGE_FIRE));
}

TEST_F(ScriptTargetFunctionsFixture, IfTargetKilledReturnsFalseForLiveTargetAndTrueForDeadTarget)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5331);
    auto target = makeObject(module, "mp_objects/follower.obj", 5332);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_FALSE(scr_IfTargetKilled(state, self));

    IDamageable& damageableTarget = *target;
    damageableTarget.kill(actor, true);
    ASSERT_FALSE(target->isAlive());

    EXPECT_TRUE(scr_IfTargetKilled(state, self));
}

TEST_F(ScriptTargetFunctionsFixture, TargetInfoPredicatesReadThroughRoleSurface)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5333);
    auto target = makeObject(module, "mp_objects/follower.obj", 5334);
    auto heldWeapon = makeObject(module, "mp_data/globalobjects/weapons/sword.obj", 5335);
    auto inventoryWeapon = makeObject(module, "mp_data/globalobjects/weapons/sword.obj", 5336);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(heldWeapon, nullptr);
    ASSERT_NE(inventoryWeapon, nullptr);

    actor->setTeamRef(static_cast<TEAM_REF>(Team::TEAM_EVIL));
    actor->setBaseTeamRef(static_cast<TEAM_REF>(Team::TEAM_EVIL));
    target->setTeamRef(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    target->setBaseTeamRef(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    target->setLocalPlayer(true);
    target->setPlayerNumber(0);
    target->setGender(Gender::Female);
    target->setPlatform(true);
    target->setBaseAttribute(Ego::Attribute::MAX_LIFE, 200.0f);
    target->setBaseAttribute(Ego::Attribute::MAX_MANA, 200.0f);
    target->setBaseAttribute(Ego::Attribute::SEE_INVISIBLE, 1.0f);
    target->setBaseAttribute(Ego::Attribute::SENSE_KURSES, 1.0f);
    target->setLife(1.0f);
    target->setMana(0.0f);
    target->setGrogTimer(7);
    target->setDazeTimer(9);
    target->setHeldObject(SLOT_LEFT, heldWeapon->getObjRef());
    inventoryWeapon->setEquipped(true);
    ASSERT_TRUE(Inventory::add_item(static_cast<IInventoryHolder&>(*target), inventoryWeapon, target->getFirstFreeInventorySlot(), true));

    const IDSZ2 targetTypeId = target->getProfile()->getIDSZ(IDSZ_TYPE);
    const IDSZ2 heldWeaponTypeId = heldWeapon->getProfile()->getIDSZ(IDSZ_TYPE);
    const IDSZ2 specialId = target->getProfile()->getIDSZ(IDSZ_SPECIAL);
    const IDSZ2 vulnerabilityId = target->getProfile()->getIDSZ(IDSZ_VULNERABILITY);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);

    state.argument = targetTypeId.toUint32();
    EXPECT_TRUE(scr_IfTargetHasID(state, self));

    state.argument = heldWeaponTypeId.toUint32();
    EXPECT_TRUE(scr_IfTargetHasItemID(state, self));
    EXPECT_TRUE(scr_IfTargetHoldingItemID(state, self));
    EXPECT_TRUE(scr_IfTargetHasItemIDEquipped(state, self));

    state.argument = target->getProfile()->getIDSZ(IDSZ_SKILL).toUint32();
    EXPECT_EQ(scr_IfTargetHasSkillID(state, self), target->hasSkillIDSZ(target->getProfile()->getIDSZ(IDSZ_SKILL)));

    state.argument = specialId.toUint32();
    EXPECT_EQ(scr_IfTargetHasSpecialID(state, self), target->getProfile()->getIDSZ(IDSZ_SPECIAL) == specialId);

    state.argument = vulnerabilityId.toUint32();
    EXPECT_EQ(scr_IfTargetHasVulnerabilityID(state, self), target->getProfile()->getIDSZ(IDSZ_VULNERABILITY) == vulnerabilityId);

    state.argument = target->getProfile()->getIDSZ(IDSZ_PARENT).toUint32();
    EXPECT_EQ(scr_IfTargetHasAnyID(state, self), target->getProfile()->hasIDSZ(target->getProfile()->getIDSZ(IDSZ_PARENT)));

    EXPECT_TRUE(scr_IfTargetIsOnOtherTeam(state, self));
    EXPECT_TRUE(scr_IfTargetIsOnHatedTeam(state, self));
    EXPECT_FALSE(scr_IfTargetIsOnSameTeam(state, self));
    EXPECT_TRUE(scr_IfTargetIsHurt(state, self));
    EXPECT_TRUE(scr_IfTargetIsAPlayer(state, self));
    EXPECT_TRUE(scr_IfTargetIsAlive(state, self));
    EXPECT_FALSE(scr_IfTargetIsMale(state, self));
    EXPECT_TRUE(scr_IfTargetIsFemale(state, self));
    EXPECT_TRUE(scr_IfTargetCanSeeInvisible(state, self));
    EXPECT_TRUE(scr_IfTargetCanSeeKurses(state, self));
    EXPECT_TRUE(scr_IfTargetHasNotFullMana(state, self));
    EXPECT_TRUE(scr_IfTargetIsFlying(state, self) == target->isFlying());
    EXPECT_TRUE(scr_IfTargetIsAPlatform(state, self));

    EXPECT_TRUE(scr_GetTargetGrogTime(state, self));
    EXPECT_EQ(state.argument, 7);

    EXPECT_TRUE(scr_GetTargetDazeTime(state, self));
    EXPECT_EQ(state.argument, 9);
}

TEST_F(ScriptTargetFunctionsFixture, TeamTargetSelectionUsesLeaderAndSissyRefs)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5337);
    auto leader = makeObject(module, "mp_objects/follower.obj", 5338);
    auto sissy = makeObject(module, "mp_objects/follower.obj", 5339);
    auto target = makeObject(module, "mp_objects/follower.obj", 5340);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(leader, nullptr);
    ASSERT_NE(sissy, nullptr);
    ASSERT_NE(target, nullptr);

    constexpr TEAM_REF teamRef = static_cast<TEAM_REF>(Team::TEAM_GOOD);
    actor->setTeamRef(teamRef);
    actor->setBaseTeamRef(teamRef);
    leader->setTeamRef(teamRef);
    leader->setBaseTeamRef(teamRef);
    sissy->setTeamRef(teamRef);
    sissy->setBaseTeamRef(teamRef);

    auto& team = module.getTeamList()[teamRef];
    team.setLeader(leader);
    team.callForHelp(sissy);

    IScriptable& scriptableLeader = *leader;
    scriptableLeader.setAITarget(target->getObjRef());

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, nullptr);

    EXPECT_TRUE(scr_SetTargetToLeader(state, self));
    EXPECT_EQ(self.getTarget(), leader->getObjRef());

    self.setTarget(ObjectRef::Invalid);
    EXPECT_TRUE(scr_SetTargetToTargetOfLeader(state, self));
    EXPECT_EQ(self.getTarget(), target->getObjRef());

    self.setTarget(ObjectRef::Invalid);
    EXPECT_TRUE(scr_SetTargetToWhoeverCalledForHelp(state, self));
    EXPECT_EQ(self.getTarget(), sissy->getObjRef());
    EXPECT_EQ(module.getTeamLeaderRef(teamRef), leader->getObjRef());
    EXPECT_EQ(module.getTeamCallerForHelpRef(teamRef), sissy->getObjRef());

    team.setLeader(nullptr);
    team._sissy.reset();

    self.setTarget(ObjectRef::Invalid);
    EXPECT_FALSE(scr_SetTargetToLeader(state, self));
    EXPECT_EQ(self.getTarget(), ObjectRef::Invalid);

    self.setTarget(ObjectRef::Invalid);
    EXPECT_FALSE(scr_SetTargetToTargetOfLeader(state, self));
    EXPECT_EQ(self.getTarget(), ObjectRef::Invalid);

    self.setTarget(ObjectRef::Invalid);
    EXPECT_FALSE(scr_SetTargetToWhoeverCalledForHelp(state, self));
    EXPECT_EQ(self.getTarget(), ObjectRef::Invalid);
    EXPECT_EQ(module.getTeamLeaderRef(teamRef), ObjectRef::Invalid);
    EXPECT_EQ(module.getTeamCallerForHelpRef(teamRef), ObjectRef::Invalid);
}

TEST_F(ScriptTargetFunctionsFixture, TargetAnimationPredicatesReadThroughRoleSurface)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5345);
    auto target = makeObject(module, "mp_objects/follower.obj", 5346);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);

    target->inst._currentAnimation = ACTION_PA;
    target->inst._nextAnimation = ACTION_PA;
    EXPECT_TRUE(scr_IfTargetIsDefending(state, self));

    target->inst._currentAnimation = ACTION_UA;
    target->inst._nextAnimation = ACTION_UA;
    EXPECT_TRUE(scr_IfTargetIsAttacking(state, self));

    target->setKursed(true);
    EXPECT_TRUE(scr_IfTargetIsKursed(state, self));

    target->inst._currentAnimation = ACTION_DA;
    target->inst._nextAnimation = ACTION_DA;
    EXPECT_TRUE(scr_IfTargetIsSneaking(state, self));

    target->inst._currentAnimation = ACTION_WA;
    target->inst._nextAnimation = ACTION_WA;
    EXPECT_TRUE(scr_IfTargetIsSneaking(state, self));

    target->inst._currentAnimation = ACTION_MG;
    target->inst._nextAnimation = ACTION_MG;
    target->setKursed(false);
    EXPECT_FALSE(scr_IfTargetIsDefending(state, self));
    EXPECT_FALSE(scr_IfTargetIsAttacking(state, self));
    EXPECT_FALSE(scr_IfTargetIsKursed(state, self));
    EXPECT_FALSE(scr_IfTargetIsSneaking(state, self));
}

TEST_F(ScriptTargetFunctionsFixture, MountAndWeaponQueriesUseTargetInfoRoleSurface)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5341);
    auto mount = makeObject(module, "mp_data/globalobjects/magic/mount.obj", 5342);
    auto rider = makeObject(module, "mp_objects/follower.obj", 5343);
    auto weapon = makeObject(module, "mp_data/globalobjects/weapons/sword.obj", 5344);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(mount, nullptr);
    ASSERT_NE(rider, nullptr);
    ASSERT_NE(weapon, nullptr);

    mount->setHeldObject(SLOT_LEFT, rider->getObjRef());
    rider->setHolderRef(mount->getObjRef());

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, rider);

    EXPECT_TRUE(scr_IfTargetIsMounted(state, self));

    self.setTarget(mount->getObjRef());
    EXPECT_TRUE(scr_IfTargetIsAMount(state, self));
    EXPECT_EQ(scr_IfTargetCanOpenStuff(state, self),
              rider->getProfile()->canOpenStuff() || mount->getProfile()->canOpenStuff());

    self.setTarget(weapon->getObjRef());
    EXPECT_TRUE(scr_IfTargetIsAWeapon(state, self));
}

TEST_F(ScriptTargetFunctionsFixture, TargetQuestQueryReadsThroughTargetInfoRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5351);
    auto target = makeObject(module, "mp_objects/follower.obj", 5352);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_TRUE(module.addPlayer(target, Ego::Input::InputDevice::DeviceList[0]));

    const IDSZ2 questId('T', 'Q', 'S', 'T');
    module.getPlayer(target->getPlayerNumber())->getQuestLog().setQuestProgress(questId, 7);

    script_state_t state;
    state.argument = questId.toUint32();
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_TRUE(scr_IfTargetHasQuest(state, self));
    EXPECT_EQ(state.distance, 7);
}

TEST_F(ScriptTargetFunctionsFixture, TargetOwnerPredicateReadsThroughTargetInfoRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5361);
    auto target = makeObject(module, "mp_objects/follower.obj", 5362);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);
    self.owner = target->getObjRef();

    EXPECT_TRUE(scr_IfTargetIsOwner(state, self));

    self.owner = actor->getObjRef();
    EXPECT_FALSE(scr_IfTargetIsOwner(state, self));
}

TEST_F(ScriptTargetFunctionsFixture, FacingQueriesReadThroughPhysicalRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5371, Ego::Vector3f(64.0f, 64.0f, 0.0f));
    auto target = makeObject(module, "mp_objects/follower.obj", 5372, Ego::Vector3f(128.0f, 64.0f, 0.0f));

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    actor->setFacingZ(FACE_EAST);
    target->setFacingZ(FACE_WEST);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_TRUE(scr_IfFacingTarget(state, self));
    EXPECT_TRUE(scr_IfTargetIsFacingSelf(state, self));

    target->setPosition(Ego::Vector3f(64.0f, 128.0f, 0.0f));
    target->setFacingZ(FACE_SOUTH);

    EXPECT_FALSE(scr_IfFacingTarget(state, self));
    EXPECT_FALSE(scr_IfTargetIsFacingSelf(state, self));
}

TEST_F(ScriptTargetFunctionsFixture, DressyAndSpellPredicatesPreserveActorProfileSemantics)
{
    auto& module = beginActiveTestModule();
    auto healer = makeObject(module, "mp_data/globalobjects/players/healer.obj", 5381);
    auto rogue = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 5382);

    ASSERT_NE(healer, nullptr);
    ASSERT_NE(rogue, nullptr);
    ASSERT_TRUE(healer->setSkin(0));
    ASSERT_TRUE(rogue->setSkin(2));
    rogue->getProfile()->_skinInfo[2].dressy = false;

    script_state_t state;
    ai_state_t self = makeScriptSelf(healer, rogue);

    EXPECT_TRUE(scr_IfTargetIsDressedUp(state, self));
    EXPECT_TRUE(scr_IfTargetIsASpell(state, self));

    self.setSelf(rogue->getObjRef());
    self.setTarget(healer->getObjRef());

    EXPECT_FALSE(scr_IfTargetIsDressedUp(state, self));
    EXPECT_FALSE(scr_IfTargetIsASpell(state, self));
}

} // namespace
