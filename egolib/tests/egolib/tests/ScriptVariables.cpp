#include "gtest/gtest.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/script_variables.h"
#include "egolib/vfs.h"

namespace
{

class ScriptVariablesFixture : public ::testing::Test
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
        opts.randomSeed = 67;
        opts.binaryPath = "";
        opts.logPath = "/debug/script-variables-tests.log";
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

    std::shared_ptr<Object> makeObject(GameModule& module,
                                       const std::string& profilePath,
                                       int slot,
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
        const bool began = session.beginModule(module, 67);
        EXPECT_TRUE(began);
        return session.activeModule();
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ScriptVariablesFixture::s_runtime;

TEST_F(ScriptVariablesFixture, SpatialReadersUseRoleSeamsAndPreserveFallbacks)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 5901);
    auto target = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 5902);
    auto owner = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 5903);
    auto leader = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 5904);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(owner, nullptr);
    ASSERT_NE(leader, nullptr);

    actor->setPosition(10.0f, 20.0f, 30.0f);
    actor->setSpawnPosition(Ego::Vector3f(4.0f, 6.0f, 8.0f));
    actor->setVelocity(Ego::Vector3f(1.0f, 2.0f, 3.0f));
    actor->setFacingZ(Facing(111));
    actor->setHoldingWeight(12);
    actor->setBaseTeamRef(static_cast<TEAM_REF>(Team::TEAM_GOOD));

    target->setPosition(30.0f, 45.0f, 50.0f);
    target->setVelocity(Ego::Vector3f(-4.0f, 5.0f, -6.0f));
    target->setFacingZ(Facing(222));

    owner->setPosition(16.0f, 23.0f, 32.0f);
    owner->setFacingZ(Facing(333));

    leader->setPosition(13.0f, 17.0f, 19.0f);
    leader->setFacingZ(Facing(444));

    script_state_t scriptState{};
    scriptState.x = 40;
    scriptState.y = 55;

    ai_state_t aiState{};
    aiState.setSelf(actor->getObjRef());

    EXPECT_EQ(load_VARSELFX(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 10);
    EXPECT_EQ(load_VARSELFY(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 20);
    EXPECT_EQ(load_VARSELFTURN(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 111);
    EXPECT_EQ(load_VARSELFZ(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 30);
    EXPECT_EQ(load_VARWEIGHT(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 12);
    EXPECT_EQ(load_VARSELFSPAWNX(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 4);
    EXPECT_EQ(load_VARSELFSPAWNY(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 6);
    EXPECT_EQ(load_VARSPAWNDISTANCE(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 20);
    EXPECT_EQ(load_VARSELFALTITUDE(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()),
              static_cast<int32_t>(actor->getPosZ() - actor->getFloorElevation()));
    EXPECT_EQ(load_VARSELFMORALE(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()),
              module.getTeamList()[Team::TEAM_GOOD].getMorale());

    EXPECT_EQ(load_VARTARGETX(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 30);
    EXPECT_EQ(load_VARTARGETY(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 45);
    EXPECT_EQ(load_VARTARGETDISTANCE(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 45);
    EXPECT_EQ(load_VARTARGETTURN(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 222);
    EXPECT_EQ(load_VARTARGETSPEEDX(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 4);
    EXPECT_EQ(load_VARTARGETSPEEDY(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 5);
    EXPECT_EQ(load_VARTARGETSPEEDZ(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 6);
    EXPECT_EQ(load_VARTARGETZ(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 50);
    EXPECT_EQ(load_VARTARGETALTITUDE(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()),
              static_cast<int32_t>(target->getPosZ() - target->getFloorElevation()));

    EXPECT_EQ(load_VARLEADERX(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 13);
    EXPECT_EQ(load_VARLEADERY(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 17);
    EXPECT_EQ(load_VARLEADERDISTANCE(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 6);
    EXPECT_EQ(load_VARLEADERTURN(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 444);

    EXPECT_EQ(load_VAROWNERX(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 16);
    EXPECT_EQ(load_VAROWNERY(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 23);
    EXPECT_EQ(load_VAROWNERDISTANCE(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 9);
    EXPECT_EQ(load_VAROWNERTURN(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()), 333);

    EXPECT_EQ(load_VARTARGETTURNTO(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()),
              Ego::Math::clipBits<16>(FACING_T(vec_to_facing(20.0f, 25.0f))));
    EXPECT_EQ(load_VAROWNERTURNTO(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()),
              Ego::Math::clipBits<16>(FACING_T(vec_to_facing(6.0f, 3.0f))));
    EXPECT_EQ(load_VARXYTURNTO(scriptState, aiState, actor.get(), target.get(), owner.get(), leader.get()),
              Ego::Math::clipBits<16>(FACING_T(vec_to_facing(30.0f, 35.0f))));

    EXPECT_EQ(load_VARTARGETX(scriptState, aiState, actor.get(), nullptr, owner.get(), nullptr), 0);
    EXPECT_EQ(load_VARTARGETDISTANCE(scriptState, aiState, actor.get(), nullptr, owner.get(), nullptr), 0x7FFFFFFF);
    EXPECT_EQ(load_VAROWNERX(scriptState, aiState, actor.get(), target.get(), nullptr, leader.get()), 0);
    EXPECT_EQ(load_VAROWNERDISTANCE(scriptState, aiState, actor.get(), target.get(), nullptr, leader.get()), 0x7FFFFFFF);
    EXPECT_EQ(load_VAROWNERTURNTO(scriptState, aiState, actor.get(), target.get(), nullptr, leader.get()), 0);
    EXPECT_EQ(load_VARLEADERX(scriptState, aiState, actor.get(), target.get(), owner.get(), nullptr), 10);
    EXPECT_EQ(load_VARLEADERY(scriptState, aiState, actor.get(), target.get(), owner.get(), nullptr), 20);
    EXPECT_EQ(load_VARLEADERTURN(scriptState, aiState, actor.get(), target.get(), owner.get(), nullptr), 111);
    EXPECT_EQ(load_VARLEADERDISTANCE(scriptState, aiState, actor.get(), target.get(), owner.get(), nullptr), 0x7FFFFFFF);
}

TEST_F(ScriptVariablesFixture, NumericReadersUseRoleSeamsForStatsEconomyAndProfileIds)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 5911);
    auto target = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 5912);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    const SKIN_T targetSkin = target->getProfile()->isValidSkin(1) ? 1 : 0;
    ASSERT_TRUE(target->setSkin(targetSkin));

    actor->setLife(9.5f);
    actor->setMana(4.25f);
    actor->setAmmo(7);
    actor->giveMoney(123);
    actor->setExperience(321);
    actor->setExperienceLevelIndex(5);
    actor->setReloadTimer(9);
    actor->setBaseAttribute(Ego::Attribute::MIGHT, 17.0f);
    actor->setBaseAttribute(Ego::Attribute::INTELLECT, 14.0f);
    actor->setBaseAttribute(Ego::Attribute::AGILITY, 11.0f);
    actor->setBaseAttribute(Ego::Attribute::SPELL_POWER, 3.5f);
    actor->setBaseAttribute(Ego::Attribute::ACCELERATION, 1.75f);

    target->setLife(12.5f);
    target->setMana(6.5f);
    target->setAmmo(8);
    target->giveMoney(456);
    target->setExperience(654);
    target->setExperienceLevelIndex(4);
    target->setReloadTimer(13);
    target->setTeamRef(static_cast<TEAM_REF>(Team::TEAM_EVIL));
    target->setBaseAttribute(Ego::Attribute::MIGHT, 21.0f);
    target->setBaseAttribute(Ego::Attribute::INTELLECT, 18.0f);
    target->setBaseAttribute(Ego::Attribute::AGILITY, 16.0f);
    target->setBaseAttribute(Ego::Attribute::SPELL_POWER, 4.5f);
    target->setBaseAttribute(Ego::Attribute::MAX_LIFE, 33.0f);

    script_state_t scriptState{};
    ai_state_t aiState{};

    EXPECT_EQ(load_VARSELFLIFE(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr), FLOAT_TO_FP8(actor->getLife()));
    EXPECT_EQ(load_VARSELFSTR(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr),
              FLOAT_TO_FP8(actor->getAttribute(Ego::Attribute::MIGHT)));
    EXPECT_EQ(load_VARSELFINT(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr),
              FLOAT_TO_FP8(actor->getAttribute(Ego::Attribute::INTELLECT)));
    EXPECT_EQ(load_VARSELFDEX(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr),
              FLOAT_TO_FP8(actor->getAttribute(Ego::Attribute::AGILITY)));
    EXPECT_EQ(load_VARSELFMANAFLOW(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr),
              FLOAT_TO_FP8(actor->getAttribute(Ego::Attribute::SPELL_POWER)));
    EXPECT_EQ(load_VARSELFMONEY(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr), actor->getMoney());
    EXPECT_EQ(load_VARSELFACCEL(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr),
              static_cast<int32_t>(actor->getAttribute(Ego::Attribute::ACCELERATION) * 100.0f));
    EXPECT_EQ(load_VARSELFAMMO(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr), actor->getAmmo());
    EXPECT_EQ(load_VARSELFLEVEL(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr), actor->getExperienceLevelIndex());
    EXPECT_EQ(load_VARSELFID(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr),
              actor->getProfile()->getIDSZ(IDSZ_TYPE).toUint32());
    EXPECT_EQ(load_VARSELFHATEID(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr),
              actor->getProfile()->getIDSZ(IDSZ_HATE).toUint32());

    EXPECT_EQ(load_VARTARGETSTR(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr),
              FLOAT_TO_FP8(target->getAttribute(Ego::Attribute::MIGHT)));
    EXPECT_EQ(load_VARTARGETINT(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr),
              FLOAT_TO_FP8(target->getAttribute(Ego::Attribute::INTELLECT)));
    EXPECT_EQ(load_VARTARGETDEX(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr),
              FLOAT_TO_FP8(target->getAttribute(Ego::Attribute::AGILITY)));
    EXPECT_EQ(load_VARTARGETLIFE(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr),
              FLOAT_TO_FP8(target->getLife()));
    EXPECT_EQ(load_VARTARGETMANAFLOW(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr),
              FLOAT_TO_FP8(target->getAttribute(Ego::Attribute::SPELL_POWER)));
    EXPECT_EQ(load_VARTARGETLEVEL(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr), target->getExperienceLevelIndex());
    EXPECT_EQ(load_VARTARGETEXP(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr),
              static_cast<int32_t>(target->getExperience()));
    EXPECT_EQ(load_VARTARGETAMMO(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr), target->getAmmo());
    EXPECT_EQ(load_VARTARGETMONEY(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr), target->getMoney());
    EXPECT_EQ(load_VARTARGETRELOADTIME(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr), target->getReloadTimer());
    EXPECT_EQ(load_VARTARGETMAXLIFE(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr),
              FLOAT_TO_FP8(target->getAttribute(Ego::Attribute::MAX_LIFE)));
    EXPECT_EQ(load_VARTARGETTEAM(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr),
              static_cast<int32_t>(target->getTeamRef()));
    EXPECT_EQ(load_VARTARGETARMOR(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr), targetSkin);
}

TEST_F(ScriptVariablesFixture, ManaReadersPreserveChannelLifeBehaviorThroughCharacterStateRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 5921);
    auto target = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 5922);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    actor->setLife(5.0f);
    actor->setMana(2.0f);
    actor->setBaseAttribute(Ego::Attribute::CHANNEL_LIFE, 1.0f);

    target->setLife(7.0f);
    target->setMana(3.0f);
    target->setBaseAttribute(Ego::Attribute::CHANNEL_LIFE, 1.0f);

    script_state_t scriptState{};
    ai_state_t aiState{};

    EXPECT_EQ(load_VARSELFMANA(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr),
              FLOAT_TO_FP8(actor->getMana()) + FLOAT_TO_FP8(actor->getLife()));
    EXPECT_EQ(load_VARTARGETMANA(scriptState, aiState, actor.get(), target.get(), nullptr, nullptr),
              FLOAT_TO_FP8(target->getMana()) + FLOAT_TO_FP8(target->getLife()));

    EXPECT_EQ(load_VARTARGETMANA(scriptState, aiState, actor.get(), nullptr, nullptr, nullptr), 0);
}

} // namespace
