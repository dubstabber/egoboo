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

} // namespace
