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
#include "egolib/game/game.h"
#undef private
#include "egolib/vfs.h"

namespace
{

class GameTextFixture : public ::testing::Test
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
        opts.randomSeed = 31;
        opts.binaryPath = "";
        opts.logPath = "/debug/game-text-tests.log";
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

    std::shared_ptr<Object> makeObject(ObjectHandler& objectHandler, const std::string& profilePath, int slot) const
    {
        const ObjectProfileRef profile = loadProfile(profilePath, slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid)
        {
            return nullptr;
        }

        return objectHandler.insert(profile);
    }

    ObjectHandler& beginActiveTestModule()
    {
        auto module = findTestModule();
        EXPECT_NE(module, nullptr);
        if (module == nullptr)
        {
            throw std::runtime_error("test.mod profile not found");
        }

        const bool began = GameSessionContext::get().beginModule(module, 31);
        EXPECT_TRUE(began);
        return GameSessionContext::get().objectHandler();
    }
};

std::unique_ptr<ContentRuntimeBootstrap> GameTextFixture::s_runtime;

TEST_F(GameTextFixture, ExpandEscapeCodesUsesScriptableOwnerAndTargetRefs)
{
    ObjectHandler& objectHandler = beginActiveTestModule();
    auto speaker = makeObject(objectHandler, "mp_objects/follower.obj", 5101);
    auto owner = makeObject(objectHandler, "mp_objects/follower.obj", 5102);
    auto target = makeObject(objectHandler, "mp_objects/follower.obj", 5103);

    ASSERT_NE(speaker, nullptr);
    ASSERT_NE(owner, nullptr);
    ASSERT_NE(target, nullptr);

    owner->setNameKnown(true);
    target->setNameKnown(true);
    target->setGender(Gender::Female);

    IScriptable& scriptableSpeaker = *speaker;
    scriptableSpeaker.setAIOwner(owner->getObjRef());
    scriptableSpeaker.setAITarget(target->getObjRef());

    script_state_t scriptState;
    scriptState.distance = 12;

    const std::string text = "%t|%o|%s|%g|%0";
    const std::string expanded = expandEscapeCodes(speaker, scriptState, text);
    const std::string expected = target->getName()
                               + "|" + owner->getName(true, false, false)
                               + "|" + target->getProfile()->getClassName()
                               + "|her|"
                               + target->getProfile()->getSkinInfo(0).name;

    EXPECT_EQ(expanded, expected);
}

} // namespace
