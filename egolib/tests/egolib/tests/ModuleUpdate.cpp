#include "gtest/gtest.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>

#include "TestEnvironment.hpp"
#define private public
#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/game.h"
#include "egolib/game/Module/Module.hpp"
#undef private
#include "egolib/vfs.h"

namespace
{

class ModuleUpdateFixture : public ::testing::Test
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
        opts.randomSeed = 23;
        opts.binaryPath = "";
        opts.logPath = "/debug/module-update-tests.log";
        opts.logLevel = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);

        setenv("EGOBOO_DISABLE_AUDIO", "1", 1);
        AudioSystem::initialize();
        ParticleHandler::initialize();
    }

    static void TearDownTestSuite()
    {
        ParticleHandler::uninitialize();
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

        ProfileSystem::get().reset();
        ProfileSystem::get().loadModuleProfiles();
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
        for (const auto& module : ProfileSystem::get().getModuleProfiles())
        {
            if (module && module->getFolderName() == "test.mod")
            {
                return module;
            }
        }

        return nullptr;
    }

    ObjectProfileRef loadFollowerProfile(int slot) const
    {
        return ProfileSystem::get().loadOneProfile("mp_objects/follower.obj", slot);
    }

    std::shared_ptr<Object> makeObject(GameModule& module, const std::string& profilePath, int slot,
                                       const Ego::Vector3f& position = Ego::Vector3f(64.0f, 64.0f, 0.0f)) const
    {
        const ObjectProfileRef profile = ProfileSystem::get().loadOneProfile(profilePath, slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid)
        {
            return nullptr;
        }

        return module.spawnObject(position, profile, static_cast<TEAM_REF>(Team::TEAM_NULL), 0, Facing(0), "", ObjectRef::Invalid);
    }

    std::shared_ptr<Object> makeFollower(ObjectHandler& objectHandler, int slot) const
    {
        const ObjectProfileRef profile = loadFollowerProfile(slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid)
        {
            return nullptr;
        }

        return objectHandler.insert(profile);
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
};

std::unique_ptr<ContentRuntimeBootstrap> ModuleUpdateFixture::s_runtime;

TEST_F(ModuleUpdateFixture, UpdateAllObjectsTerminatesObjectAtPoofBoundary)
{
    auto& module = beginActiveTestModule();
    auto& session = GameSessionContext::get();
    auto object = makeFollower(session.objectHandler(), 4101);
    ASSERT_NE(object, nullptr);

    object->setAIPoofTime(11);

    session.worldUpdateCount() = 10;
    module.updateAllObjects();
    EXPECT_FALSE(object->isTerminated());

    session.worldUpdateCount() = 11;
    module.updateAllObjects();
    EXPECT_TRUE(object->isTerminated());
}

TEST_F(ModuleUpdateFixture, SpawnDefencePingPublishesBlockedAlertAndLastAttacker)
{
    beginActiveTestModule();
    auto& session = GameSessionContext::get();
    auto defender = makeFollower(session.objectHandler(), 4104);
    auto attacker = makeFollower(session.objectHandler(), 4105);
    ASSERT_NE(defender, nullptr);
    ASSERT_NE(attacker, nullptr);

    defender->setDamageTimer(0);
    defender->setAILastAttacker(ObjectRef::Invalid);
    defender->clearAIAlertBits(ALERTIF_BLOCKED);

    ParticleHandler::get().spawnDefencePing(defender, attacker);

    EXPECT_TRUE(defender->hasAnyAIAlertBits(ALERTIF_BLOCKED));
    EXPECT_EQ(defender->getAILastAttacker(), attacker->getObjRef());
    EXPECT_EQ(defender->getDamageTimer(), ParticleHandler::DEFENDTIME);
}

TEST_F(ModuleUpdateFixture, ReaffirmAttachedParticlesPublishesReaffirmedAlert)
{
    auto& module = beginActiveTestModule();
    auto torch = makeObject(module, "mp_data/globalobjects/items/torch.obj", 4106);
    ASSERT_NE(torch, nullptr);
    ASSERT_GT(torch->getProfile()->getAttachedParticleAmount(), 0);

    torch->clearAIAlertBits(ALERTIF_REAFFIRMED);

    const int particlesAdded = reaffirm_attached_particles(torch->getObjRef());

    EXPECT_GT(particlesAdded, 0);
    EXPECT_TRUE(torch->hasAnyAIAlertBits(ALERTIF_REAFFIRMED));
}

TEST_F(ModuleUpdateFixture, DisaffirmAttachedParticlesPublishesDisaffirmedAlert)
{
    beginActiveTestModule();
    auto& session = GameSessionContext::get();
    auto object = makeFollower(session.objectHandler(), 4107);
    ASSERT_NE(object, nullptr);

    object->clearAIAlertBits(ALERTIF_DISAFFIRMED);

    disaffirm_attached_particles(object->getObjRef());

    EXPECT_TRUE(object->hasAnyAIAlertBits(ALERTIF_DISAFFIRMED));
}

} // namespace
