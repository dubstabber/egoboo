#include "gtest/gtest.h"

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Module/Module_spawn_realization.hpp"
#include "egolib/vfs.h"

#include <cstdlib>
#include <memory>
#include <vector>

namespace
{

class ModuleSpawnRealizationFixture : public ::testing::Test
{
protected:
    static std::unique_ptr<ContentRuntimeBootstrap> s_runtime;

    ObjectHandler _objectHandler;
    import_list_t _importList;
    pro_import_t _importData;

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
        opts.randomSeed = 9;
        opts.binaryPath = "";
        opts.logPath = "/debug/module-spawn-realization-tests.log";
        opts.logLevel = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);

        setenv("EGOBOO_DISABLE_AUDIO", "1", 1);
        AudioSystem::initialize();
    }

    static void TearDownTestSuite()
    {
        AudioSystem::uninitialize();
        s_runtime.reset();
    }

    void SetUp() override
    {
        ProfileSystem::get().reset();
        ProfileSystem::get().loadModuleProfiles();
        mountTestModule();
        import_list_t::init(_importList);
        _importData.slot = -1;
        _importData.player = 0;
        _importData.slot_lst.fill(INVALID_PRO_REF);
        _importData.max_slot = -1;
    }

    std::shared_ptr<ModuleProfile> mountTestModule()
    {
        for (const auto& module : ProfileSystem::get().getModuleProfiles())
        {
            if (module && module->getFolderName() == "test.mod")
            {
                setup_init_module_vfs_paths(module->getPath());
                return module;
            }
        }
        return nullptr;
    }

    ObjectProfileRef loadProfile(const std::string& objectName, int slot)
    {
        return ProfileSystem::get().loadOneProfile("mp_objects/" + objectName, slot);
    }

    std::shared_ptr<Object> makeObject(const std::string& objectName, int slot)
    {
        const ObjectProfileRef profile = loadProfile(objectName, slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        return _objectHandler.insert(profile);
    }

    module_spawn_realization::SpawnRealizationState makeState() const
    {
        module_spawn_realization::SpawnRealizationState state;
        state.importList = &_importList;
        state.importData = &_importData;
        state.playerAmount = MAX_PLAYER;
        state.isProfileLoaded = [](ObjectProfileRef profile)
        {
            return ProfileSystem::get().isLoaded(profile);
        };
        return state;
    }

    spawn_file_info_t makeEntry(int slot, REF_T attach = ATTACH_NONE) const
    {
        spawn_file_info_t entry;
        entry.do_spawn = true;
        entry.slot = slot;
        entry.attach = attach;
        entry.spawn_name = "follower.obj";
        return entry;
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ModuleSpawnRealizationFixture::s_runtime;

TEST_F(ModuleSpawnRealizationFixture, AttachedSpawnWithoutParentReturnsNullAndSkipsSpawn)
{
    auto state = makeState();
    bool spawnCalled = false;

    module_spawn_realization::SpawnRealizationOps ops;
    ops.spawnObject = [&](const spawn_file_info_t&)
    {
        spawnCalled = true;
        return std::shared_ptr<Object>();
    };

    auto result = module_spawn_realization::realizeSpawnEntry(makeEntry(70, ATTACH_LEFT), nullptr, state, ops);

    EXPECT_EQ(result, nullptr);
    EXPECT_FALSE(spawnCalled);
}

TEST_F(ModuleSpawnRealizationFixture, AttachNoneCallsMatrixSetupAndReturnsSpawnedObject)
{
    auto state = makeState();
    std::vector<ObjectRef> matrixCalls;

    module_spawn_realization::SpawnRealizationOps ops;
    ops.spawnObject = [&](const spawn_file_info_t& entry)
    {
        loadProfile("follower.obj", entry.slot);
        return _objectHandler.insert(ObjectProfileRef(entry.slot));
    };
    ops.makeCharacterMatrix = [&](const std::shared_ptr<Object>& object)
    {
        matrixCalls.push_back(object->getObjRef());
    };

    auto result = module_spawn_realization::realizeSpawnEntry(makeEntry(71), nullptr, state, ops);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(matrixCalls.size(), 1u);
    EXPECT_EQ(matrixCalls.front(), result->getObjRef());
}

TEST_F(ModuleSpawnRealizationFixture, InventoryAttachMarksGrabbedAlertWhenChildSurvives)
{
    auto parent = makeObject("follower.obj", 72);
    ASSERT_NE(parent, nullptr);

    auto state = makeState();
    bool attachCalled = false;
    size_t recordedSlot = INVEN_COUNT;

    module_spawn_realization::SpawnRealizationOps ops;
    ops.spawnObject = [&](const spawn_file_info_t& entry)
    {
        loadProfile("follower.obj", entry.slot);
        auto object = _objectHandler.insert(ObjectProfileRef(entry.slot));
        object->ai.alert = 0;
        return object;
    };
    ops.attachInventoryItem = [&](const std::shared_ptr<Object>& parentObject, const std::shared_ptr<Object>&)
    {
        attachCalled = true;
        recordedSlot = parentObject->getInventory().getFirstFreeSlotNumber();
    };

    auto result = module_spawn_realization::realizeSpawnEntry(makeEntry(73, ATTACH_INVENTORY), parent, state, ops);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(attachCalled);
    EXPECT_EQ(recordedSlot, 0u);
    EXPECT_TRUE(HAS_SOME_BITS(result->ai.alert, ALERTIF_GRABBED));
}

TEST_F(ModuleSpawnRealizationFixture, InventoryAttachReturnsNullWhenMergeTerminatesChild)
{
    auto parent = makeObject("follower.obj", 74);
    ASSERT_NE(parent, nullptr);

    auto state = makeState();
    bool attachCalled = false;
    bool mergedIntoStack = false;
    size_t recordedSlot = INVEN_COUNT;

    module_spawn_realization::SpawnRealizationOps ops;
    ops.spawnObject = [&](const spawn_file_info_t& entry)
    {
        loadProfile("follower.obj", entry.slot);
        return _objectHandler.insert(ObjectProfileRef(entry.slot));
    };
    ops.attachInventoryItem = [&](const std::shared_ptr<Object>& parentObject, const std::shared_ptr<Object>& object)
    {
        attachCalled = true;
        recordedSlot = parentObject->getInventory().getFirstFreeSlotNumber();
        mergedIntoStack = true;
        object->ai.alert = 0;
    };
    ops.isObjectTerminated = [&](const std::shared_ptr<Object>&)
    {
        return mergedIntoStack;
    };

    auto result = module_spawn_realization::realizeSpawnEntry(makeEntry(75, ATTACH_INVENTORY), parent, state, ops);

    EXPECT_EQ(result, nullptr);
    EXPECT_TRUE(attachCalled);
    EXPECT_EQ(recordedSlot, 0u);
}

TEST_F(ModuleSpawnRealizationFixture, AttachLeftUsesLeftGrip)
{
    auto parent = makeObject("follower.obj", 76);
    ASSERT_NE(parent, nullptr);

    auto state = makeState();
    std::vector<grip_offset_t> grips;

    module_spawn_realization::SpawnRealizationOps ops;
    ops.spawnObject = [&](const spawn_file_info_t& entry)
    {
        loadProfile("follower.obj", entry.slot);
        return _objectHandler.insert(ObjectProfileRef(entry.slot));
    };
    ops.attachToGrip = [&](const std::shared_ptr<Object>&, const std::shared_ptr<Object>&, grip_offset_t grip)
    {
        grips.push_back(grip);
        return true;
    };

    auto result = module_spawn_realization::realizeSpawnEntry(makeEntry(77, ATTACH_LEFT), parent, state, ops);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(grips.size(), 1u);
    EXPECT_EQ(grips.front(), GRIP_LEFT);
}

TEST_F(ModuleSpawnRealizationFixture, AttachRightUsesRightGrip)
{
    auto parent = makeObject("follower.obj", 78);
    ASSERT_NE(parent, nullptr);

    auto state = makeState();
    std::vector<grip_offset_t> grips;

    module_spawn_realization::SpawnRealizationOps ops;
    ops.spawnObject = [&](const spawn_file_info_t& entry)
    {
        loadProfile("follower.obj", entry.slot);
        return _objectHandler.insert(ObjectProfileRef(entry.slot));
    };
    ops.attachToGrip = [&](const std::shared_ptr<Object>&, const std::shared_ptr<Object>&, grip_offset_t grip)
    {
        grips.push_back(grip);
        return true;
    };

    auto result = module_spawn_realization::realizeSpawnEntry(makeEntry(79, ATTACH_RIGHT), parent, state, ops);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(grips.size(), 1u);
    EXPECT_EQ(grips.front(), GRIP_RIGHT);
}

TEST_F(ModuleSpawnRealizationFixture, NonImportPlayerParentIdentifiesStartupEquipment)
{
    auto parent = makeObject("follower.obj", 80);
    ASSERT_NE(parent, nullptr);
    parent->islocalplayer = true;

    auto state = makeState();
    state.importValid = false;

    module_spawn_realization::SpawnRealizationOps ops;
    ops.spawnObject = [&](const spawn_file_info_t& entry)
    {
        loadProfile("follower.obj", entry.slot);
        auto object = _objectHandler.insert(ObjectProfileRef(entry.slot));
        object->nameknown = false;
        object->iskursed = true;
        return object;
    };

    auto result = module_spawn_realization::realizeSpawnEntry(makeEntry(81, ATTACH_NONE), parent, state, ops);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->nameknown);
    EXPECT_FALSE(result->iskursed);
}

TEST_F(ModuleSpawnRealizationFixture, StatSpawnWithoutImportsAddsNextLocalPlayerAndIdentifiesSpawn)
{
    auto state = makeState();
    state.importAmount = 0;
    state.playerAmount = 4;

    size_t playerCount = 0;
    size_t localPlayerCount = 2;
    std::vector<size_t> deviceIndexes;

    module_spawn_realization::SpawnRealizationOps ops;
    ops.spawnObject = [&](const spawn_file_info_t& entry)
    {
        loadProfile("follower.obj", entry.slot);
        auto object = _objectHandler.insert(ObjectProfileRef(entry.slot));
        object->nameknown = false;
        return object;
    };
    ops.currentPlayerCount = [&]() { return playerCount; };
    ops.currentLocalPlayerCount = [&]() { return localPlayerCount; };
    ops.addPlayer = [&](const std::shared_ptr<Object>& object, size_t deviceIndex)
    {
        ++playerCount;
        ++localPlayerCount;
        deviceIndexes.push_back(deviceIndex);
        object->islocalplayer = true;
        object->is_which_player = static_cast<PLA_REF>(deviceIndex);
        return true;
    };

    auto entry = makeEntry(82, ATTACH_NONE);
    entry.stat = true;
    auto result = module_spawn_realization::realizeSpawnEntry(entry, nullptr, state, ops);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(deviceIndexes.size(), 1u);
    EXPECT_EQ(deviceIndexes.front(), 2u);
    EXPECT_TRUE(result->nameknown);
    EXPECT_TRUE(result->isPlayer());
}

TEST_F(ModuleSpawnRealizationFixture, StatSpawnWithImportsMatchesLocalPlayerNumberFromImportSlot)
{
    auto state = makeState();
    state.importAmount = 2;
    state.playerAmount = 4;

    _importList.count = 1;
    _importList.lst[0].slot = 123;
    _importList.lst[0].local_player_num = 3;
    _importData.max_slot = 5;
    _importData.slot_lst[5] = 123;

    size_t playerCount = 0;
    std::vector<size_t> deviceIndexes;

    module_spawn_realization::SpawnRealizationOps ops;
    ops.spawnObject = [&](const spawn_file_info_t& entry)
    {
        loadProfile("follower.obj", entry.slot);
        return _objectHandler.insert(ObjectProfileRef(entry.slot));
    };
    ops.currentPlayerCount = [&]() { return playerCount; };
    ops.currentLocalPlayerCount = []() { return 0u; };
    ops.addPlayer = [&](const std::shared_ptr<Object>& object, size_t deviceIndex)
    {
        ++playerCount;
        deviceIndexes.push_back(deviceIndex);
        object->islocalplayer = true;
        object->is_which_player = static_cast<PLA_REF>(deviceIndex);
        return true;
    };

    auto entry = makeEntry(5, ATTACH_NONE);
    entry.stat = true;
    auto result = module_spawn_realization::realizeSpawnEntry(entry, nullptr, state, ops);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(deviceIndexes.size(), 1u);
    EXPECT_EQ(deviceIndexes.front(), 3u);
    EXPECT_TRUE(result->isPlayer());
}

TEST_F(ModuleSpawnRealizationFixture, StatSpawnWithNoImportMatchSkipsPlayerBinding)
{
    auto state = makeState();
    state.importAmount = 2;
    state.playerAmount = 4;

    _importList.count = 1;
    _importList.lst[0].slot = 222;
    _importList.lst[0].local_player_num = 1;
    _importData.max_slot = 6;
    _importData.slot_lst[6] = 999;

    size_t addPlayerCalls = 0;

    module_spawn_realization::SpawnRealizationOps ops;
    ops.spawnObject = [&](const spawn_file_info_t& entry)
    {
        loadProfile("follower.obj", entry.slot);
        auto object = _objectHandler.insert(ObjectProfileRef(entry.slot));
        object->islocalplayer = false;
        return object;
    };
    ops.currentPlayerCount = []() { return 0u; };
    ops.currentLocalPlayerCount = []() { return 0u; };
    ops.addPlayer = [&](const std::shared_ptr<Object>&, size_t)
    {
        ++addPlayerCalls;
        return true;
    };

    auto entry = makeEntry(6, ATTACH_NONE);
    entry.stat = true;
    auto result = module_spawn_realization::realizeSpawnEntry(entry, nullptr, state, ops);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(addPlayerCalls, 0u);
    EXPECT_FALSE(result->isPlayer());
}

} // namespace
