#include "gtest/gtest.h"

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#define private public
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/Profiles/_Include.hpp"
#undef private
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/game.h"
#include "egolib/vfs.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace
{

// Releases a gtest stdout capture even if the code under test escapes via an exception (or an
// ASSERT_* failure) before the test body reaches its own GetCapturedStdout() call. Mirrors
// WawaliteReadContract.cpp's ScopedStdoutCapture -- without this, a capture left open by an
// early ASSERT_* failure would leak into every later test's own CaptureStdout() call.
class ScopedStdoutCapture
{
public:
    ScopedStdoutCapture() { testing::internal::CaptureStdout(); }
    ~ScopedStdoutCapture()
    {
        if (!released)
        {
            testing::internal::GetCapturedStdout();
        }
    }

    std::string release()
    {
        released = true;
        return testing::internal::GetCapturedStdout();
    }

private:
    bool released = false;
};

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

        game_reset_players();
        vfs_removeDirectoryAndContents("import");
        vfs_removeDirectoryAndContents("players");
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
        vfs_removeDirectoryAndContents("players");
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

    ObjectProfileRef loadProfile(const std::string& path, int slot) const
    {
        return EngineContext::get().profileSystem().loadOneProfile(path, slot);
    }

    std::shared_ptr<Object> makeObject(GameModule& module, const std::string& path, int slot) const
    {
        const ObjectProfileRef profile = loadProfile(path, slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid)
        {
            throw std::runtime_error("profile load failed");
        }

        auto object = module.getObjectHandler().insert(profile);
        EXPECT_NE(object, nullptr);
        if (!object)
        {
            throw std::runtime_error("object insert failed");
        }

        return object;
    }

    void copyFixtureObjectDirectory(const std::string& destination) const
    {
        setup_init_module_vfs_paths("mp_modules/test.mod");
        vfs_removeDirectoryAndContents(destination.c_str());
        ASSERT_TRUE(vfs_copyDirectory("mp_objects/follower.obj", destination.c_str()));
        ASSERT_TRUE(vfs_exists((destination + "/data.txt").c_str()));
        setup_clear_module_vfs_paths();
    }

    std::string playerExportRoot(const Object& object) const
    {
        return fs_getUserDirectory() + "/players/" + str_encode_path(object.getName());
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

TEST_F(ImportWorkflowFixture, ModuleSpawnAndPlayerBindingExposeRefFirstPath)
{
    GameModule& module = beginActiveTestModule();
    const ObjectProfileRef profile = loadFollowerProfile(411);
    ASSERT_NE(profile, ObjectProfileRef::Invalid);

    const ObjectRef objectRef = module.spawnObjectRef(Ego::Vector3f(64.0f, 64.0f, 0.0f),
                                                      profile,
                                                      static_cast<TEAM_REF>(Team::TEAM_NULL),
                                                      0,
                                                      Facing(0),
                                                      "",
                                                      ObjectRef::Invalid);
    ASSERT_NE(objectRef, ObjectRef::Invalid);

    ASSERT_TRUE(module.addPlayer(objectRef, Ego::Input::InputDevice::DeviceList[0]));
    Object* object = module.getObjectHandler().get(objectRef);
    ASSERT_NE(object, nullptr);
    EXPECT_TRUE(object->isPlayer());
    ASSERT_NE(module.getPlayer(0), nullptr);
    EXPECT_EQ(module.getPlayer(0)->getObjectRef(), objectRef);
}

TEST_F(ImportWorkflowFixture, FromPlayersBuildsImportEntriesForRegisteredPlayers)
{
    GameModule& module = beginActiveTestModule();

    const ObjectProfileRef profile = loadFollowerProfile(211);
    ASSERT_NE(profile, ObjectProfileRef::Invalid);

    auto object = module.getObjectHandler().insert(profile);
    ASSERT_NE(object, nullptr);

    ASSERT_TRUE(module.addPlayer(object->getObjRef(), Ego::Input::InputDevice::DeviceList[0]));

    import_list_t imports;
    import_list_t::init(imports);

    ASSERT_EQ(import_list_t::from_players(imports), 1u);
    ASSERT_EQ(imports.count, 1u);
    EXPECT_EQ(imports.lst[0].player, 0u);
    EXPECT_EQ(imports.lst[0].slot, 0);
    EXPECT_EQ(imports.lst[0].name, object->getName());
    EXPECT_EQ(imports.lst[0].srcDir, "mp_players/" + str_encode_path(object->getName()));
}

TEST_F(ImportWorkflowFixture, FromPlayersSkipsMissingAndTerminatedPlayersWithoutDisturbingLiveOrder)
{
    GameModule& module = beginActiveTestModule();

    const ObjectProfileRef profile = loadFollowerProfile(214);
    ASSERT_NE(profile, ObjectProfileRef::Invalid);

    auto first = module.getObjectHandler().insert(profile);
    auto missing = module.getObjectHandler().insert(profile);
    auto terminated = module.getObjectHandler().insert(profile);
    auto last = module.getObjectHandler().insert(profile);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(missing, nullptr);
    ASSERT_NE(terminated, nullptr);
    ASSERT_NE(last, nullptr);

    first->setName("First Live Player");
    missing->setName("Missing Player");
    terminated->setName("Terminated Player");
    last->setName("Last Live Player");

    ASSERT_TRUE(module.addPlayer(first->getObjRef(), Ego::Input::InputDevice::DeviceList[0]));
    ASSERT_TRUE(module.addPlayer(missing->getObjRef(), Ego::Input::InputDevice::DeviceList[1]));
    ASSERT_TRUE(module.addPlayer(terminated->getObjRef(), Ego::Input::InputDevice::DeviceList[2]));
    ASSERT_TRUE(module.addPlayer(last->getObjRef(), Ego::Input::InputDevice::DeviceList[3]));

    ASSERT_NE(module.getPlayer(1), nullptr);
    ASSERT_TRUE(module.getObjectHandler().remove(missing->getObjRef()));
    terminated->requestTerminate();

    import_list_t imports;
    import_list_t::init(imports);

    ASSERT_EQ(import_list_t::from_players(imports), 2u);
    ASSERT_EQ(imports.count, 2u);
    EXPECT_EQ(imports.lst[0].player, 0u);
    EXPECT_EQ(imports.lst[0].slot, 0);
    EXPECT_EQ(imports.lst[0].name, first->getName());
    EXPECT_EQ(imports.lst[0].srcDir, "mp_players/" + str_encode_path(first->getName()));
    EXPECT_EQ(imports.lst[1].player, 3u);
    EXPECT_EQ(imports.lst[1].slot, 3 * MAX_IMPORT_PER_PLAYER);
    EXPECT_EQ(imports.lst[1].name, last->getName());
    EXPECT_EQ(imports.lst[1].srcDir, "mp_players/" + str_encode_path(last->getName()));
}

TEST_F(ImportWorkflowFixture, ExportAllPlayersReturnsFalseWhenExportIsDisabled)
{
    GameModule& module = beginActiveTestModule();
    auto player = makeObject(module, "mp_objects/follower.obj", 212);
    ASSERT_NE(player, nullptr);

    player->setName("Export Disabled Player");
    ASSERT_TRUE(module.addPlayer(player->getObjRef(), Ego::Input::InputDevice::DeviceList[0]));

    module.setExportValid(false);

    const std::string exportRoot = playerExportRoot(*player);
    EXPECT_FALSE(export_all_players(false));
    EXPECT_FALSE(std::filesystem::exists(exportRoot + "/data.txt"));
}

TEST_F(ImportWorkflowFixture, ExportAllPlayersExportsCharacterDirectoryForLivePlayer)
{
    GameModule& module = beginActiveTestModule();
    auto player = makeObject(module, "mp_objects/follower.obj", 213);
    ASSERT_NE(player, nullptr);

    player->setName("Export Hero");
    ASSERT_TRUE(module.addPlayer(player->getObjRef(), Ego::Input::InputDevice::DeviceList[0]));

    const std::string exportRoot = playerExportRoot(*player);
    ASSERT_TRUE(export_all_players(false));
    EXPECT_TRUE(std::filesystem::exists(exportRoot + "/data.txt"));
    EXPECT_TRUE(std::filesystem::exists(exportRoot + "/naming.txt"));
}

TEST_F(ImportWorkflowFixture, ExportAllPlayersExportsHeldItemsIntoSlotDirectories)
{
    GameModule& module = beginActiveTestModule();
    auto player = makeObject(module, "mp_objects/follower.obj", 214);
    auto leftItem = makeObject(module, "mp_data/globalobjects/weapons/stiletto.obj", 215);
    auto rightItem = makeObject(module, "mp_data/globalobjects/armor/atshield.obj", 216);
    ASSERT_NE(player, nullptr);
    ASSERT_NE(leftItem, nullptr);
    ASSERT_NE(rightItem, nullptr);

    player->setName("Held Export Hero");
    ASSERT_TRUE(module.addPlayer(player->getObjRef(), Ego::Input::InputDevice::DeviceList[0]));
    player->setHeldObject(SLOT_LEFT, leftItem->getObjRef());
    player->setHeldObject(SLOT_RIGHT, rightItem->getObjRef());

    const std::string exportRoot = playerExportRoot(*player);
    ASSERT_TRUE(export_all_players(false));
    EXPECT_TRUE(std::filesystem::exists(exportRoot + "/" + std::to_string(SLOT_LEFT) + ".obj/data.txt"));
    EXPECT_TRUE(std::filesystem::exists(exportRoot + "/" + std::to_string(SLOT_RIGHT) + ".obj/data.txt"));
}

TEST_F(ImportWorkflowFixture, ExportAllPlayersSkipsNonCarryableInventoryItemsWithoutBreakingDenseNumbering)
{
    GameModule& module = beginActiveTestModule();
    auto player = makeObject(module, "mp_objects/follower.obj", 217);
    auto skippedItem = makeObject(module, "mp_data/globalobjects/weapons/stiletto.obj", 218);
    auto exportedItem = makeObject(module, "mp_data/globalobjects/weapons/xbow.obj", 219);
    ASSERT_NE(player, nullptr);
    ASSERT_NE(skippedItem, nullptr);
    ASSERT_NE(exportedItem, nullptr);

    player->setName("Inventory Export Hero");
    skippedItem->setName("Skipped Inventory Item");
    exportedItem->setName("Carryable Inventory Item");

    skippedItem->getProfile()->_isItem = true;
    skippedItem->getProfile()->_canCarryToNextModule = false;
    exportedItem->getProfile()->_isItem = true;
    exportedItem->getProfile()->_canCarryToNextModule = true;

    ASSERT_TRUE(module.addPlayer(player->getObjRef(), Ego::Input::InputDevice::DeviceList[0]));
    player->setInventoryItemRef(0, skippedItem->getObjRef());
    player->setInventoryItemRef(2, exportedItem->getObjRef());

    const std::string exportRoot = playerExportRoot(*player);
    const std::string firstInventoryExport = exportRoot + "/" + std::to_string(SLOT_COUNT) + ".obj";
    const std::string secondInventoryExport = exportRoot + "/" + std::to_string(SLOT_COUNT + 1) + ".obj";

    ASSERT_TRUE(export_all_players(false));
    EXPECT_TRUE(std::filesystem::exists(firstInventoryExport + "/data.txt"));
    EXPECT_FALSE(std::filesystem::exists(secondInventoryExport + "/data.txt"));
}

TEST_F(ImportWorkflowFixture, ExportAllPlayersTreatsNoPlayersAsSuccessfulNoOp)
{
    GameModule& module = beginActiveTestModule();
    module.setExportValid(true);

    EXPECT_TRUE(export_all_players(false));
}

//--------------------------------------------------------------------------------------------
// export_all_players / export_one_character: silent-drop defects
//--------------------------------------------------------------------------------------------

/// The per-file copy loop inside export_one_character (game_export.c) used to discard
/// vfs_copyFile's return value, so a character exported with one or more uncopyable files
/// (missing/unreadable model, script, icon, ...) still reported ExportCharacterResult::Exported.
/// This forces a deterministic, filesystem-level vfs_copyFile failure without racing the
/// loop's own `if (!vfs_exists(tofile))` existence guard: data.txt and naming.txt are
/// pre-written as ordinary, writable files directly into the item's export directory (the
/// per-character export writes both unconditionally, and on POSIX, opening an *existing* file
/// for write does not require write permission on its containing directory -- only *creating*
/// a new directory entry does), and then that directory's own write permission is stripped.
/// Every other file the copy loop tries to create there is new, so vfs_copyFile fails for each
/// one, and export_one_character's own aggregate must report Error.
/// @remark POSIX-only trigger: a stripped directory-write bit is bypassed by CAP_DAC_OVERRIDE
/// when running as root, and does not block new-file creation on Windows/NTFS semantics
/// either. Skips itself under a root effective UID rather than failing outright.
TEST_F(ImportWorkflowFixture, ExportAllPlayersReportsErrorWhenAHeldItemFileCannotBeCopied)
{
    namespace fs = std::filesystem;

#ifndef _WIN32
    if (geteuid() == 0)
    {
        GTEST_SKIP() << "directory write-permission trick does not block root (CAP_DAC_OVERRIDE)";
    }
#endif

    GameModule& module = beginActiveTestModule();
    auto player = makeObject(module, "mp_objects/follower.obj", 220);
    auto leftItem = makeObject(module, "mp_data/globalobjects/weapons/stiletto.obj", 221);
    ASSERT_NE(player, nullptr);
    ASSERT_NE(leftItem, nullptr);

    player->setName("Copy Failure Hero");
    ASSERT_TRUE(module.addPlayer(player->getObjRef(), Ego::Input::InputDevice::DeviceList[0]));
    player->setHeldObject(SLOT_LEFT, leftItem->getObjRef());

    const std::string exportRoot = playerExportRoot(*player);
    const std::string itemDir = exportRoot + "/" + std::to_string(SLOT_LEFT) + ".obj";

    ASSERT_TRUE(fs::create_directories(itemDir));
    {
        std::ofstream dataFile(itemDir + "/data.txt");
        ASSERT_TRUE(dataFile.good());
        dataFile << "pre-existing\n";
    }
    {
        std::ofstream namingFile(itemDir + "/naming.txt");
        ASSERT_TRUE(namingFile.good());
        namingFile << ":pre-existing\n:STOP\n\n";
    }
    ASSERT_TRUE(fs::exists(itemDir + "/data.txt"));
    ASSERT_TRUE(fs::exists(itemDir + "/naming.txt"));
    ASSERT_FALSE(fs::exists(itemDir + "/script.txt"))
        << "precondition: script.txt must not already exist, or the copy loop's own "
           "vfs_exists() guard would skip it and this test would prove nothing";

    fs::permissions(itemDir, fs::perms::owner_read | fs::perms::owner_exec, fs::perm_options::replace);

    // Always restore write permission before TearDown() tries to remove the directory tree,
    // even if an assertion below fails first.
    struct RestorePermissions
    {
        std::string path;
        ~RestorePermissions()
        {
            std::filesystem::permissions(path, std::filesystem::perms::owner_all,
                                          std::filesystem::perm_options::replace);
        }
    } restore{itemDir};

    EXPECT_FALSE(export_all_players(false));

    // The directory really was write-blocked: script.txt (a real file in stiletto.obj that the
    // copy loop would otherwise pick up) could not be created.
    EXPECT_FALSE(fs::exists(itemDir + "/script.txt"));
}

/// export_one_character_name_vfs()'s bool return (RandomName::exportName, which refuses to
/// write naming.txt for an empty name) used to be discarded too. An item with its name field
/// explicitly known-but-empty is the only headlessly-reachable, filesystem-trick-free trigger
/// for that specific false return.
/// @remark Deliberately does NOT assert that naming.txt is absent afterward: the per-file copy
/// loop that runs later in the same export_one_character call sees that the character-specific
/// naming.txt was never written and (correctly, and unrelated to this defect) falls back to
/// copying the source item's own template naming.txt over it, since that file also does not
/// yet exist at the destination. The Warning is the only reliable signal that the name export
/// itself failed.
TEST_F(ImportWorkflowFixture, ExportAllPlayersReportsErrorWhenAHeldItemHasNoExportableName)
{
    GameModule& module = beginActiveTestModule();
    auto player = makeObject(module, "mp_objects/follower.obj", 224);
    auto leftItem = makeObject(module, "mp_data/globalobjects/weapons/stiletto.obj", 225);
    ASSERT_NE(player, nullptr);
    ASSERT_NE(leftItem, nullptr);

    player->setName("Naming Failure Hero");
    leftItem->setName("");
    leftItem->setNameKnown(true);
    ASSERT_EQ(leftItem->getName(), "");

    ASSERT_TRUE(module.addPlayer(player->getObjRef(), Ego::Input::InputDevice::DeviceList[0]));
    player->setHeldObject(SLOT_LEFT, leftItem->getObjRef());

    const std::string exportRoot = playerExportRoot(*player);
    const std::string itemDir = exportRoot + "/" + std::to_string(SLOT_LEFT) + ".obj";

    ScopedStdoutCapture capture;
    const bool exportedAll = export_all_players(false);
    const std::string out = capture.release();

    EXPECT_FALSE(exportedAll);
    EXPECT_TRUE(std::filesystem::exists(itemDir + "/data.txt"));
    EXPECT_NE(out.find("WARNING: "), std::string::npos) << out;
    EXPECT_NE(out.find("unable to save"), std::string::npos) << out;
    // The Warning logs the VFS-relative path, not the absolute filesystem path itemDir uses, so
    // match on the tail both share.
    EXPECT_NE(out.find(std::to_string(SLOT_LEFT) + ".obj/naming.txt"), std::string::npos) << out;
}

/// The trap: export_one_character_quest_vfs() returns false BY DESIGN for a non-player object
/// (an inventory item has no Ego::Player to source a quest log from). That false must not be
/// folded into export_one_character's aggregate, or every carryable item export would
/// manufacture a spurious Error even though every file it touches was written successfully.
TEST_F(ImportWorkflowFixture, ExportAllPlayersDoesNotTreatNonPlayerQuestExportFalseAsAnError)
{
    GameModule& module = beginActiveTestModule();
    auto player = makeObject(module, "mp_objects/follower.obj", 228);
    auto item = makeObject(module, "mp_data/globalobjects/weapons/xbow.obj", 229);
    ASSERT_NE(player, nullptr);
    ASSERT_NE(item, nullptr);

    player->setName("Quest Trap Hero");
    item->setName("Quest Trap Item");
    item->getProfile()->_isItem = true;
    item->getProfile()->_canCarryToNextModule = true;

    ASSERT_TRUE(module.addPlayer(player->getObjRef(), Ego::Input::InputDevice::DeviceList[0]));
    player->setInventoryItemRef(0, item->getObjRef());

    const std::string exportRoot = playerExportRoot(*player);
    const std::string itemDir = exportRoot + "/" + std::to_string(SLOT_COUNT) + ".obj";

    EXPECT_TRUE(export_all_players(false));
    EXPECT_TRUE(std::filesystem::exists(itemDir + "/data.txt"));
    // export_one_character_quest_vfs() only ever writes quest.txt for a player; confirms the
    // false it returned here for a non-player was not mistaken for a failure.
    EXPECT_FALSE(std::filesystem::exists(itemDir + "/quest.txt"));
}

/// Repairer regression: the inventory loop's `number` used to advance only on
/// ExportCharacterResult::Exported, so an Error'd inventory item never consumed its slot
/// number. export_one_character only creates/clears a directory for a *fresh* chr_obj_index,
/// and its per-file copy loop skips any file that already exists at the destination -- so the
/// very next inventory item was exported into the failed item's already-populated directory,
/// interleaving both items' files into one "chimera" that reported Exported even though it was
/// never a complete, self-consistent object. Confirms the failed item keeps its own directory
/// (with only its own files) and the following item gets its own, separate directory.
TEST_F(ImportWorkflowFixture, ExportAllPlayersDoesNotReuseAFailedInventoryItemsSlotDirectory)
{
    GameModule& module = beginActiveTestModule();
    auto player = makeObject(module, "mp_objects/follower.obj", 236);
    auto namingFailureItem = makeObject(module, "mp_data/globalobjects/weapons/stiletto.obj", 237);
    auto followingItem = makeObject(module, "mp_data/globalobjects/weapons/xbow.obj", 238);
    ASSERT_NE(player, nullptr);
    ASSERT_NE(namingFailureItem, nullptr);
    ASSERT_NE(followingItem, nullptr);

    player->setName("Slot Reuse Hero");
    // An empty-but-known name makes export_one_character_name_vfs() fail deterministically
    // (RandomName::exportName refuses to write naming.txt for an empty name), which downgrades
    // this item's export to Error without needing any filesystem trickery.
    namingFailureItem->setName("");
    namingFailureItem->setNameKnown(true);
    followingItem->setName("Following Item");

    namingFailureItem->getProfile()->_isItem = true;
    namingFailureItem->getProfile()->_canCarryToNextModule = true;
    followingItem->getProfile()->_isItem = true;
    followingItem->getProfile()->_canCarryToNextModule = true;

    ASSERT_TRUE(module.addPlayer(player->getObjRef(), Ego::Input::InputDevice::DeviceList[0]));
    player->setInventoryItemRef(0, namingFailureItem->getObjRef());
    player->setInventoryItemRef(1, followingItem->getObjRef());

    const std::string exportRoot = playerExportRoot(*player);
    const std::string firstInventoryExport = exportRoot + "/" + std::to_string(SLOT_COUNT) + ".obj";
    const std::string secondInventoryExport = exportRoot + "/" + std::to_string(SLOT_COUNT + 1) + ".obj";

    EXPECT_FALSE(export_all_players(false));

    // The naming-failure item's own directory still holds its own (stiletto-only) file...
    EXPECT_TRUE(std::filesystem::exists(firstInventoryExport + "/enchant.txt"));
    // ...and must NOT also contain the following item's (xbow-only) file: that would mean the
    // following item's export landed inside the naming-failure item's directory instead of its
    // own -- the chimera-directory regression this test guards against.
    EXPECT_FALSE(std::filesystem::exists(firstInventoryExport + "/part2.txt"));

    // The following item gets its own, separate directory with its own (xbow-only) file.
    ASSERT_TRUE(std::filesystem::exists(secondInventoryExport + "/data.txt"));
    EXPECT_TRUE(std::filesystem::exists(secondInventoryExport + "/part2.txt"));
}

//--------------------------------------------------------------------------------------------
// GameSessionContext::finishModule: discarded export_all_players() aggregate
//--------------------------------------------------------------------------------------------

/// finishModule() (GameSessionContext.cpp) used to call export_all_players(false) and drop its
/// return value entirely -- the function's own bool return reflects only game_copy_imports(),
/// so a failed player export was invisible to every caller. Confirms the new summary Warning
/// (naming the module) is now logged when export fails, and is silent when it does not.
TEST_F(ImportWorkflowFixture, FinishModuleLogsAWarningNamingTheModuleWhenPlayerExportFails)
{
    GameModule& module = beginActiveTestModule();
    auto player = makeObject(module, "mp_objects/follower.obj", 230);
    auto leftItem = makeObject(module, "mp_data/globalobjects/weapons/stiletto.obj", 231);
    ASSERT_NE(player, nullptr);
    ASSERT_NE(leftItem, nullptr);

    player->setName("Finish Module Hero");
    leftItem->setName("");
    leftItem->setNameKnown(true);

    ASSERT_TRUE(module.addPlayer(player->getObjRef(), Ego::Input::InputDevice::DeviceList[0]));
    player->setHeldObject(SLOT_LEFT, leftItem->getObjRef());

    const std::string moduleName = module.getName();

    ScopedStdoutCapture capture;
    const bool finished = GameSessionContext::get().finishModule();
    const std::string out = capture.release();

    // finishModule()'s own return value is driven by game_copy_imports(), not by export
    // success -- unchanged by this fix, and asserted here so the Warning check below cannot be
    // mistaken for a change to that contract.
    EXPECT_TRUE(finished);
    EXPECT_NE(out.find("WARNING: "), std::string::npos) << out;
    EXPECT_NE(out.find("failed to export"), std::string::npos) << out;
    EXPECT_NE(out.find(moduleName), std::string::npos) << out;
}

TEST_F(ImportWorkflowFixture, FinishModuleDoesNotLogAWarningWhenPlayerExportSucceeds)
{
    GameModule& module = beginActiveTestModule();
    auto player = makeObject(module, "mp_objects/follower.obj", 232);
    ASSERT_NE(player, nullptr);

    player->setName("Finish Module Success Hero");
    ASSERT_TRUE(module.addPlayer(player->getObjRef(), Ego::Input::InputDevice::DeviceList[0]));

    ScopedStdoutCapture capture;
    const bool finished = GameSessionContext::get().finishModule();
    const std::string out = capture.release();

    EXPECT_TRUE(finished);
    EXPECT_EQ(out.find("failed to export"), std::string::npos) << out;
}

} // namespace
