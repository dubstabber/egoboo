#include "gtest/gtest.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <vector>

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#define private public
#include "egolib/Entities/_Include.hpp"
#undef private
#include "egolib/Entities/IObjectWorld.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Core/ISessionState.hpp"
#include "egolib/game/Module/IModuleEnvironment.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/game.h"
#include "egolib/vfs.h"

namespace
{
bool containsRef(const std::vector<ObjectRef>& refs, ObjectRef ref)
{
    return std::find(refs.begin(), refs.end(), ref) != refs.end();
}

class ObjectHandlerQueriesFixture : public ::testing::Test
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
        opts.randomSeed = 29;
        opts.binaryPath = "";
        opts.logPath = "/debug/object-handler-query-tests.log";
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

        setup_clear_module_vfs_paths();
        EngineContext::get().profileSystem().reset();
        EngineContext::get().profileSystem().loadModuleProfiles();
        setup_init_module_vfs_paths("mp_modules/test.mod");
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

    ObjectProfileRef loadProfile(const char* modulePath, const std::string& objectPath, int slot) const
    {
        setup_init_module_vfs_paths(modulePath);
        return EngineContext::get().profileSystem().loadOneProfile(objectPath, slot);
    }

    std::shared_ptr<Object> spawnObject(GameModule& module,
                                        ObjectProfileRef profile,
                                        const Ego::Vector3f& position) const
    {
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid)
        {
            return nullptr;
        }

        const ObjectRef objectRef = module.spawnObjectRef(position,
                                                          profile,
                                                          static_cast<TEAM_REF>(Team::TEAM_NULL),
                                                          0,
                                                          Facing(0),
                                                          "",
                                                          ObjectRef::Invalid);
        EXPECT_NE(objectRef, ObjectRef::Invalid);
        if (objectRef == ObjectRef::Invalid)
        {
            return nullptr;
        }

        auto object = module.getObjectHandler().getHandle(objectRef);
        EXPECT_NE(object, nullptr);
        if (object == nullptr)
        {
            return nullptr;
        }

        return object;
    }

    GameModule& beginActiveTestModule()
    {
        auto module = findTestModule();
        EXPECT_NE(module, nullptr);
        if (module == nullptr)
        {
            throw std::runtime_error("test.mod profile not found");
        }

        const bool began = GameSessionContext::get().beginModule(module, 29);
        EXPECT_TRUE(began);
        return GameSessionContext::get().activeModule();
    }

    static void refreshQuadTree(ObjectHandler& handler)
    {
        {
            auto refs = handler.objectRefIterator();
            (void)refs;
        }
        handler.updateQuadTree(0.0f, 0.0f, 512.0f, 512.0f);
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ObjectHandlerQueriesFixture::s_runtime;

TEST_F(ObjectHandlerQueriesFixture, ActiveObjectWorldLookupIsEmptyWithoutActiveModule)
{
    const ObjectRef ref(0);

    EXPECT_EQ(Ego::Entities::tryActiveObjectHandler(), nullptr);
    EXPECT_THROW(Ego::Entities::activeObjectHandler(), std::logic_error);
    EXPECT_EQ(Ego::Entities::tryActiveObject(ref), nullptr);
    EXPECT_EQ(Ego::Entities::tryActiveConstObject(ref), nullptr);
    EXPECT_FALSE(Ego::Entities::activeObjectExists(ref));
}

TEST_F(ObjectHandlerQueriesFixture, ActiveModuleEnvironmentAndSessionStateAreEmptyWithoutActiveModule)
{
    EXPECT_EQ(tryActiveModuleEnvironment(), nullptr);
    EXPECT_EQ(tryActiveSessionState(), nullptr);
    EXPECT_THROW(activeModuleEnvironment(), std::logic_error);
    EXPECT_THROW(activeSessionState(), std::logic_error);
}

TEST_F(ObjectHandlerQueriesFixture, ActiveModuleEnvironmentAndSessionStateMirrorActiveModuleAndSession)
{
    GameModule& module = beginActiveTestModule();
    GameSessionContext& session = GameSessionContext::get();

    ASSERT_EQ(tryActiveModuleEnvironment(), &module);
    ASSERT_EQ(&activeModuleEnvironment(), &module);
    ASSERT_EQ(tryActiveSessionState(), &session);
    ASSERT_EQ(&activeSessionState(), &session);

    EXPECT_EQ(activeModuleEnvironment().mesh().get(), module.getMeshPointer().get());
    EXPECT_EQ(&activeModuleEnvironment().water(), &module.getWater());
    EXPECT_EQ(&activeModuleEnvironment().weatherState(), &module.getWeatherState());
    EXPECT_EQ(&activeModuleEnvironment().fog(), &module.getFog());
    EXPECT_EQ(&activeModuleEnvironment().animatedTilesState(), &module.getAnimatedTilesState());

    LocalPlayerStatus status;
    status.registeredCount = 2;
    status.aliveCount = 1;
    status.deadCount = 1;
    session.publishLocalPlayerStatus(status);

    LocalPlayerPerceptionState perception;
    perception.grogLevel = 3.0f;
    perception.seeInvisibleLevel = 1.0f;
    perception.seeInvisibleMagnitude = 2.0f;
    perception.seeDarkLevel = 4.0f;
    perception.seeDarkMagnitude = 5.0f;
    session.publishLocalPlayerPerception(perception);

    const EnemySenseState enemySense(static_cast<TEAM_REF>(Team::TEAM_GOOD), IDSZ2('T', 'E', 'S', 'T'));
    session.publishEnemySense(enemySense);
    session.publishRespawnCooldown(17);
    session.worldUpdateCount() = 123;
    session.characterStatClock() = 7;
    session.enchantStatClock() = 9;

    EXPECT_EQ(&activeSessionState().playerList(), &session.playerList());
    EXPECT_EQ(activeSessionState().localPlayerCount(), session.localPlayerCount());
    EXPECT_EQ(activeSessionState().localPlayerStatus().registeredCount, 2u);
    EXPECT_EQ(activeSessionState().localPlayerStatus().aliveCount, 1u);
    EXPECT_EQ(activeSessionState().localPlayerStatus().deadCount, 1u);
    EXPECT_FLOAT_EQ(activeSessionState().localPlayerPerception().grogLevel, 3.0f);
    EXPECT_FLOAT_EQ(activeSessionState().localPlayerPerception().seeDarkMagnitude, 5.0f);
    EXPECT_EQ(activeSessionState().enemySense().team, static_cast<TEAM_REF>(Team::TEAM_GOOD));
    EXPECT_EQ(activeSessionState().enemySense().idsz, IDSZ2('T', 'E', 'S', 'T'));
    EXPECT_EQ(activeSessionState().respawnCooldown(), 17);
    EXPECT_EQ(activeSessionState().worldUpdateCount(), 123u);
    EXPECT_EQ(activeSessionState().characterStatClock(), 7u);
    EXPECT_EQ(activeSessionState().enchantStatClock(), 9u);
}

TEST_F(ObjectHandlerQueriesFixture, ActiveModuleEnvironmentAndSessionStateClearOnQuitModule)
{
    beginActiveTestModule();

    ASSERT_NE(tryActiveModuleEnvironment(), nullptr);
    ASSERT_NE(tryActiveSessionState(), nullptr);

    GameSessionContext::get().quitModule();

    EXPECT_EQ(tryActiveModuleEnvironment(), nullptr);
    EXPECT_EQ(tryActiveSessionState(), nullptr);
    EXPECT_THROW(activeModuleEnvironment(), std::logic_error);
    EXPECT_THROW(activeSessionState(), std::logic_error);
}

TEST_F(ObjectHandlerQueriesFixture, PointQueryRefsReturnNearbyNonSceneryObjects)
{
    const ObjectProfileRef followerProfile = loadProfile("mp_modules/test.mod", "mp_objects/follower.obj", 6101);
    GameModule& module = beginActiveTestModule();
    ObjectHandler& handler = module.getObjectHandler();

    auto nearA = spawnObject(module, followerProfile, Ego::Vector3f(64.0f, 64.0f, 0.0f));
    auto nearB = spawnObject(module, followerProfile, Ego::Vector3f(96.0f, 64.0f, 0.0f));
    auto farAway = spawnObject(module, followerProfile, Ego::Vector3f(256.0f, 256.0f, 0.0f));

    ASSERT_NE(nearA, nullptr);
    ASSERT_NE(nearB, nullptr);
    ASSERT_NE(farAway, nullptr);

    nearA->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    nearB->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    farAway->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));

    refreshQuadTree(handler);

    std::vector<ObjectRef> refs;
    handler.findObjectRefs(64.0f, 64.0f, 48.0f, refs, false);

    EXPECT_EQ(refs.size(), 2u);
    EXPECT_TRUE(containsRef(refs, nearA->getObjRef()));
    EXPECT_TRUE(containsRef(refs, nearB->getObjRef()));
    EXPECT_FALSE(containsRef(refs, farAway->getObjRef()));
}

TEST_F(ObjectHandlerQueriesFixture, AreaQueryRefsPreserveSceneryFiltering)
{
    const ObjectProfileRef followerProfile = loadProfile("mp_modules/test.mod", "mp_objects/follower.obj", 6102);
    const ObjectProfileRef rockProfile = loadProfile("mp_modules/archaeologist.mod", "mp_objects/rock.obj", 6103);
    GameModule& module = beginActiveTestModule();
    ObjectHandler& handler = module.getObjectHandler();

    auto follower = spawnObject(module, followerProfile, Ego::Vector3f(64.0f, 64.0f, 0.0f));
    auto rock = spawnObject(module, rockProfile, Ego::Vector3f(72.0f, 64.0f, 0.0f));

    ASSERT_NE(follower, nullptr);
    ASSERT_NE(rock, nullptr);

    follower->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));

    refreshQuadTree(handler);

    const Ego::AxisAlignedBox2f searchArea(Ego::Point2f(32.0f, 32.0f), Ego::Point2f(96.0f, 96.0f));

    std::vector<ObjectRef> refsWithoutScenery;
    handler.findObjectRefs(searchArea, refsWithoutScenery, false);
    EXPECT_EQ(refsWithoutScenery.size(), 1u);
    EXPECT_TRUE(containsRef(refsWithoutScenery, follower->getObjRef()));
    EXPECT_FALSE(containsRef(refsWithoutScenery, rock->getObjRef()));

    std::vector<ObjectRef> refsWithScenery;
    handler.findObjectRefs(searchArea, refsWithScenery, true);
    EXPECT_EQ(refsWithScenery.size(), 2u);
    EXPECT_TRUE(containsRef(refsWithScenery, follower->getObjRef()));
    EXPECT_TRUE(containsRef(refsWithScenery, rock->getObjRef()));
}

TEST_F(ObjectHandlerQueriesFixture, QueryRefsCanBeResolvedSafelyAfterRemoval)
{
    const ObjectProfileRef followerProfile = loadProfile("mp_modules/test.mod", "mp_objects/follower.obj", 6104);
    GameModule& module = beginActiveTestModule();
    ObjectHandler& handler = module.getObjectHandler();

    auto nearA = spawnObject(module, followerProfile, Ego::Vector3f(64.0f, 64.0f, 0.0f));
    auto nearB = spawnObject(module, followerProfile, Ego::Vector3f(96.0f, 64.0f, 0.0f));
    auto farAway = spawnObject(module, followerProfile, Ego::Vector3f(256.0f, 256.0f, 0.0f));

    ASSERT_NE(nearA, nullptr);
    ASSERT_NE(nearB, nullptr);
    ASSERT_NE(farAway, nullptr);

    nearA->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    nearB->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    farAway->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));

    refreshQuadTree(handler);

    std::vector<ObjectRef> refs{nearA->getObjRef(), nearB->getObjRef()};

    const ObjectRef removedRef = refs.back();
    std::vector<ObjectRef> expectedResolvedRefs = refs;
    expectedResolvedRefs.pop_back();

    ASSERT_TRUE(handler.remove(removedRef));

    std::vector<ObjectRef> resolvedRefs;
    for (const ObjectRef& ref : refs)
    {
        if (const Object* object = handler.get(ref))
        {
            resolvedRefs.push_back(object->getObjRef());
        }
    }

    EXPECT_EQ(resolvedRefs, expectedResolvedRefs);
}

TEST_F(ObjectHandlerQueriesFixture, ExplicitLookupsDistinguishBorrowedAndOwningAccess)
{
    const ObjectProfileRef followerProfile = loadProfile("mp_modules/test.mod", "mp_objects/follower.obj", 6107);
    GameModule& module = beginActiveTestModule();
    ObjectHandler& handler = module.getObjectHandler();

    auto follower = spawnObject(module, followerProfile, Ego::Vector3f(64.0f, 64.0f, 0.0f));
    ASSERT_NE(follower, nullptr);

    const ObjectRef liveRef = follower->getObjRef();
    EXPECT_EQ(handler.get(liveRef), follower.get());
    EXPECT_EQ(handler.getHandle(liveRef).get(), follower.get());

    EXPECT_EQ(handler.get(ObjectRef::Invalid), nullptr);
    EXPECT_EQ(handler.getHandle(ObjectRef::Invalid), nullptr);

    const ObjectRef neverSpawnedRef(liveRef.get() + 1000);
    EXPECT_EQ(handler.get(neverSpawnedRef), nullptr);
    EXPECT_EQ(handler.getHandle(neverSpawnedRef), nullptr);

    ASSERT_TRUE(handler.remove(liveRef));
    EXPECT_EQ(handler.get(liveRef), nullptr);
    EXPECT_EQ(handler.getHandle(liveRef), nullptr);
}

TEST_F(ObjectHandlerQueriesFixture, ActiveObjectWorldLookupMatchesActiveModule)
{
    const ObjectProfileRef followerProfile = loadProfile("mp_modules/test.mod", "mp_objects/follower.obj", 6108);
    GameModule& module = beginActiveTestModule();
    ObjectHandler& handler = module.getObjectHandler();

    auto follower = spawnObject(module, followerProfile, Ego::Vector3f(64.0f, 64.0f, 0.0f));
    ASSERT_NE(follower, nullptr);

    const ObjectRef liveRef = follower->getObjRef();
    EXPECT_EQ(Ego::Entities::tryActiveObjectHandler(), &handler);
    EXPECT_EQ(&Ego::Entities::activeObjectHandler(), &handler);
    EXPECT_EQ(Ego::Entities::tryActiveObject(liveRef), follower.get());
    EXPECT_EQ(Ego::Entities::tryActiveConstObject(liveRef), follower.get());
    EXPECT_TRUE(Ego::Entities::activeObjectExists(liveRef));

    EXPECT_EQ(Ego::Entities::tryActiveObject(ObjectRef::Invalid), nullptr);
    EXPECT_EQ(Ego::Entities::tryActiveConstObject(ObjectRef::Invalid), nullptr);
    EXPECT_FALSE(Ego::Entities::activeObjectExists(ObjectRef::Invalid));

    ASSERT_TRUE(handler.remove(liveRef));
    EXPECT_EQ(Ego::Entities::tryActiveObject(liveRef), nullptr);
    EXPECT_EQ(Ego::Entities::tryActiveConstObject(liveRef), nullptr);
    EXPECT_FALSE(Ego::Entities::activeObjectExists(liveRef));
}

TEST_F(ObjectHandlerQueriesFixture, FullRefIteratorMatchesSpawnInsertionOrder)
{
    const ObjectProfileRef followerProfile = loadProfile("mp_modules/test.mod", "mp_objects/follower.obj", 6105);
    GameModule& module = beginActiveTestModule();
    ObjectHandler& handler = module.getObjectHandler();

    auto nearA = spawnObject(module, followerProfile, Ego::Vector3f(64.0f, 64.0f, 0.0f));
    auto nearB = spawnObject(module, followerProfile, Ego::Vector3f(96.0f, 64.0f, 0.0f));
    auto farAway = spawnObject(module, followerProfile, Ego::Vector3f(256.0f, 256.0f, 0.0f));

    ASSERT_NE(nearA, nullptr);
    ASSERT_NE(nearB, nullptr);
    ASSERT_NE(farAway, nullptr);

    refreshQuadTree(handler);

    std::vector<ObjectRef> refIteratorRefs;
    {
        auto refs = handler.objectRefIterator();
        refIteratorRefs.assign(refs.begin(), refs.end());
    }

    ASSERT_GE(refIteratorRefs.size(), 3u);
    const auto spawnedRefs = refIteratorRefs.end() - 3;
    EXPECT_EQ(spawnedRefs[0], nearA->getObjRef());
    EXPECT_EQ(spawnedRefs[1], nearB->getObjRef());
    EXPECT_EQ(spawnedRefs[2], farAway->getObjRef());
}

TEST_F(ObjectHandlerQueriesFixture, FullRefIteratorRemainsStableAcrossDeferredRemoval)
{
    const ObjectProfileRef followerProfile = loadProfile("mp_modules/test.mod", "mp_objects/follower.obj", 6106);
    GameModule& module = beginActiveTestModule();
    ObjectHandler& handler = module.getObjectHandler();

    auto nearA = spawnObject(module, followerProfile, Ego::Vector3f(64.0f, 64.0f, 0.0f));
    auto nearB = spawnObject(module, followerProfile, Ego::Vector3f(96.0f, 64.0f, 0.0f));
    auto farAway = spawnObject(module, followerProfile, Ego::Vector3f(256.0f, 256.0f, 0.0f));

    ASSERT_NE(nearA, nullptr);
    ASSERT_NE(nearB, nullptr);
    ASSERT_NE(farAway, nullptr);

    refreshQuadTree(handler);

    std::vector<ObjectRef> expectedRefs;
    {
        auto refs = handler.objectRefIterator();
        expectedRefs.assign(refs.begin(), refs.end());
    }

    std::vector<ObjectRef> iteratedRefs;
    {
        auto refs = handler.objectRefIterator();
        for (const ObjectRef& ref : refs)
        {
            iteratedRefs.push_back(ref);
            if (ref == nearA->getObjRef())
            {
                ASSERT_TRUE(handler.remove(nearB->getObjRef()));
            }
        }
    }

    EXPECT_EQ(iteratedRefs, expectedRefs);
    EXPECT_EQ(handler.get(nearB->getObjRef()), nullptr);

    std::vector<ObjectRef> expectedPostRemovalRefs = expectedRefs;
    expectedPostRemovalRefs.erase(
        std::remove(expectedPostRemovalRefs.begin(), expectedPostRemovalRefs.end(), nearB->getObjRef()),
        expectedPostRemovalRefs.end());

    std::vector<ObjectRef> postRemovalRefs;
    {
        auto refs = handler.objectRefIterator();
        postRemovalRefs.assign(refs.begin(), refs.end());
    }

    EXPECT_EQ(postRemovalRefs, expectedPostRemovalRefs);
}
}  // namespace
