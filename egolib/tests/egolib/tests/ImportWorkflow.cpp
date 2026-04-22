#include "gtest/gtest.h"

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#define private public
#include "egolib/Entities/_Include.hpp"
#include "egolib/Profiles/_Include.hpp"
#undef private
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/game.h"
#include "egolib/vfs.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>

namespace
{

constexpr char kImportTestRoot[] = "import-workflow-tests";

class ImportWorkflowFixture : public ::testing::Test
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
        opts.randomSeed = 19;
        opts.binaryPath = "";
        opts.logPath = "/debug/import-workflow-tests.log";
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

        game_reset_players();
        vfs_removeDirectoryAndContents("import");
        vfs_removeDirectoryAndContents(kImportTestRoot);
        setup_clear_module_vfs_paths();

        EngineContext::get().profileSystem().reset();
        EngineContext::get().profileSystem().loadModuleProfiles();
    }

    void TearDown() override
    {
        auto& session = GameSessionContext::get();
        if (session.hasActiveModule())
        {
            session.quitModule();
        }

        game_reset_players();
        vfs_removeDirectoryAndContents("import");
        vfs_removeDirectoryAndContents(kImportTestRoot);
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

        const bool began = GameSessionContext::get().beginModule(module, 29);
        EXPECT_TRUE(began);
        return GameSessionContext::get().activeModule();
    }

    ObjectProfileRef loadFollowerProfile(int slot) const
    {
        return EngineContext::get().profileSystem().loadOneProfile("mp_objects/follower.obj", slot);
    }

    void copyFixtureObjectDirectory(const std::string& destination) const
    {
        setup_init_module_vfs_paths("mp_modules/test.mod");
        vfs_removeDirectoryAndContents(destination.c_str());
        ASSERT_TRUE(vfs_copyDirectory("mp_objects/follower.obj", destination.c_str()));
        ASSERT_TRUE(vfs_exists((destination + "/data.txt").c_str()));
        setup_clear_module_vfs_paths();
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ImportWorkflowFixture::s_runtime;

TEST_F(ImportWorkflowFixture, CopyImportsTreatsEmptyListAsSuccessfulNoOp)
{
    import_list_t imports;
    import_list_t::init(imports);

    EXPECT_TRUE(game_copy_imports(imports));
    EXPECT_FALSE(vfs_exists("mp_import/temp0000.obj/data.txt"));
}

TEST_F(ImportWorkflowFixture, CopyImportsKeepsMissingSourceBehaviorAsSuccessfulNoDataCopy)
{
    import_list_t imports;
    import_list_t::init(imports);
    imports.count = 1;
    imports.lst[0].srcDir = std::string(kImportTestRoot) + "/missing-player.obj";
    imports.lst[0].slot = 3;

    EXPECT_TRUE(game_copy_imports(imports));
    EXPECT_EQ(imports.lst[0].dstDir, "/import/temp0003.obj");
    EXPECT_FALSE(vfs_exists("mp_import/temp0003.obj/data.txt"));
}

TEST_F(ImportWorkflowFixture, CopyImportsCopiesCharacterAndInventoryDirectories)
{
    const std::string sourceCharacterPath = std::string(kImportTestRoot) + "/hero.obj";
    copyFixtureObjectDirectory(sourceCharacterPath);
    copyFixtureObjectDirectory(sourceCharacterPath + "/0.obj");

    import_list_t imports;
    import_list_t::init(imports);
    imports.count = 1;
    imports.lst[0].srcDir = sourceCharacterPath;
    imports.lst[0].slot = 4;

    ASSERT_TRUE(game_copy_imports(imports));
    EXPECT_EQ(imports.lst[0].dstDir, "/import/temp0004.obj");
    EXPECT_TRUE(vfs_exists("mp_import/temp0004.obj/data.txt"));
    EXPECT_TRUE(vfs_exists("mp_import/temp0004.obj/naming.txt"));
    EXPECT_TRUE(vfs_exists("mp_import/temp0005.obj/data.txt"));
}

TEST_F(ImportWorkflowFixture, FromPlayersReturnsZeroWhenModuleHasNoPlayers)
{
    beginActiveTestModule();

    import_list_t imports;
    import_list_t::init(imports);

    EXPECT_EQ(import_list_t::from_players(imports), 0u);
    EXPECT_EQ(imports.count, 0u);
}

TEST_F(ImportWorkflowFixture, FromPlayersBuildsImportEntriesForRegisteredPlayers)
{
    GameModule& module = beginActiveTestModule();

    const ObjectProfileRef profile = loadFollowerProfile(211);
    ASSERT_NE(profile, ObjectProfileRef::Invalid);

    auto object = module.getObjectHandler().insert(profile);
    ASSERT_NE(object, nullptr);

    ASSERT_TRUE(module.addPlayer(object, Ego::Input::InputDevice::DeviceList[0]));

    import_list_t imports;
    import_list_t::init(imports);

    ASSERT_EQ(import_list_t::from_players(imports), 1u);
    ASSERT_EQ(imports.count, 1u);
    EXPECT_EQ(imports.lst[0].player, 0u);
    EXPECT_EQ(imports.lst[0].slot, 0);
    EXPECT_EQ(imports.lst[0].name, object->getName());
    EXPECT_EQ(imports.lst[0].srcDir, "mp_players/" + str_encode_path(object->getName()));
}

} // namespace
