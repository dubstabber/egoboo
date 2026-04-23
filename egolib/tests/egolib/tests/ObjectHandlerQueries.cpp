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
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/game.h"
#include "egolib/vfs.h"

namespace
{
std::vector<ObjectRef> collectRefs(const std::vector<std::shared_ptr<Object>>& objects)
{
    std::vector<ObjectRef> refs;
    refs.reserve(objects.size());
    for (const auto& object : objects)
    {
        refs.push_back(object ? object->getObjRef() : ObjectRef::Invalid);
    }
    return refs;
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

    std::shared_ptr<Object> spawnObject(ObjectHandler& handler,
                                        ObjectProfileRef profile,
                                        const Ego::Vector3f& position) const
    {
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid)
        {
            return nullptr;
        }

        auto object = handler.insert(profile);
        EXPECT_NE(object, nullptr);
        if (object == nullptr)
        {
            return nullptr;
        }

        object->setPosition(position);
        return object;
    }

    ObjectHandler& beginActiveTestModule()
    {
        auto module = findTestModule();
        EXPECT_NE(module, nullptr);
        if (module == nullptr)
        {
            throw std::runtime_error("test.mod profile not found");
        }

        const bool began = GameSessionContext::get().beginModule(module, 29);
        EXPECT_TRUE(began);
        return GameSessionContext::get().objectHandler();
    }

    static void refreshQuadTree(ObjectHandler& handler)
    {
        {
            auto objects = handler.iterator();
            (void)objects;
        }
        handler.updateQuadTree(0.0f, 0.0f, 512.0f, 512.0f);
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ObjectHandlerQueriesFixture::s_runtime;

TEST_F(ObjectHandlerQueriesFixture, PointQueryRefsMatchLegacySharedPtrResults)
{
    const ObjectProfileRef followerProfile = loadProfile("mp_modules/test.mod", "mp_objects/follower.obj", 6101);
    ObjectHandler& handler = beginActiveTestModule();

    auto nearA = spawnObject(handler, followerProfile, Ego::Vector3f(64.0f, 64.0f, 0.0f));
    auto nearB = spawnObject(handler, followerProfile, Ego::Vector3f(96.0f, 64.0f, 0.0f));
    auto farAway = spawnObject(handler, followerProfile, Ego::Vector3f(256.0f, 256.0f, 0.0f));

    ASSERT_NE(nearA, nullptr);
    ASSERT_NE(nearB, nullptr);
    ASSERT_NE(farAway, nullptr);

    nearA->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    nearB->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    farAway->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));

    refreshQuadTree(handler);

    const auto legacy = handler.findObjects(64.0f, 64.0f, 48.0f, false);
    std::vector<ObjectRef> refs;
    handler.findObjectRefs(64.0f, 64.0f, 48.0f, refs, false);

    EXPECT_EQ(refs, collectRefs(legacy));
    EXPECT_EQ(std::find(refs.begin(), refs.end(), farAway->getObjRef()), refs.end());
}

TEST_F(ObjectHandlerQueriesFixture, AreaQueryRefsMatchLegacySharedPtrResultsAndPreserveSceneryFiltering)
{
    const ObjectProfileRef followerProfile = loadProfile("mp_modules/test.mod", "mp_objects/follower.obj", 6102);
    const ObjectProfileRef rockProfile = loadProfile("mp_modules/archaeologist.mod", "mp_objects/rock.obj", 6103);
    ObjectHandler& handler = beginActiveTestModule();

    auto follower = spawnObject(handler, followerProfile, Ego::Vector3f(64.0f, 64.0f, 0.0f));
    auto rock = spawnObject(handler, rockProfile, Ego::Vector3f(72.0f, 64.0f, 0.0f));

    ASSERT_NE(follower, nullptr);
    ASSERT_NE(rock, nullptr);

    follower->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));

    refreshQuadTree(handler);

    const Ego::AxisAlignedBox2f searchArea(Ego::Point2f(32.0f, 32.0f), Ego::Point2f(96.0f, 96.0f));

    std::vector<std::shared_ptr<Object>> legacyWithoutScenery;
    handler.findObjects(searchArea, legacyWithoutScenery, false);
    std::vector<ObjectRef> refsWithoutScenery;
    handler.findObjectRefs(searchArea, refsWithoutScenery, false);
    EXPECT_EQ(refsWithoutScenery, collectRefs(legacyWithoutScenery));

    std::vector<std::shared_ptr<Object>> legacyWithScenery;
    handler.findObjects(searchArea, legacyWithScenery, true);
    std::vector<ObjectRef> refsWithScenery;
    handler.findObjectRefs(searchArea, refsWithScenery, true);
    EXPECT_EQ(refsWithScenery, collectRefs(legacyWithScenery));
}

TEST_F(ObjectHandlerQueriesFixture, QueryRefsCanBeResolvedSafelyAfterRemoval)
{
    const ObjectProfileRef followerProfile = loadProfile("mp_modules/test.mod", "mp_objects/follower.obj", 6104);
    ObjectHandler& handler = beginActiveTestModule();

    auto nearA = spawnObject(handler, followerProfile, Ego::Vector3f(64.0f, 64.0f, 0.0f));
    auto nearB = spawnObject(handler, followerProfile, Ego::Vector3f(96.0f, 64.0f, 0.0f));
    auto farAway = spawnObject(handler, followerProfile, Ego::Vector3f(256.0f, 256.0f, 0.0f));

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
}  // namespace
