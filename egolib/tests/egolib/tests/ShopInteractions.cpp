#include "gtest/gtest.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#define private public
#include "egolib/Entities/_Include.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/Module/Passage.hpp"
#include "egolib/game/Shop.hpp"
#undef private
#include "egolib/vfs.h"

namespace
{

class ShopFixture : public ::testing::Test
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
        opts.randomSeed = 37;
        opts.binaryPath = "";
        opts.logPath = "/debug/shop-tests.log";
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

        const ObjectRef objectRef = module.spawnObjectRef(position, profile, static_cast<TEAM_REF>(Team::TEAM_NULL), 0, Facing(0), "", ObjectRef::Invalid);
        return module.getObjectHandler().getHandle(objectRef);
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
        const bool began = session.beginModule(module, 37);
        EXPECT_TRUE(began);
        return session.activeModule();
    }

    void flushObjectHandler(GameModule& module) const
    {
        auto refs = module.getObjectHandler().objectRefIterator();
        (void)refs;
    }

    std::shared_ptr<Object> makePricedItem(GameModule& module, int slotBase) const
    {
        static const std::vector<std::string> candidates = {
            "mp_data/globalobjects/items/torch.obj",
            "mp_data/globalobjects/items/gem.obj",
            "mp_data/globalobjects/items/shovel.obj",
            "mp_data/globalobjects/armor/atshield.obj"
        };

        for (size_t i = 0; i < candidates.size(); ++i)
        {
            auto item = makeObject(module, candidates[i], slotBase + static_cast<int>(i));
            if (item && item->isItem() && item->getPrice() > 0)
            {
                return item;
            }
        }

        ADD_FAILURE() << "unable to load a priced shop item fixture";
        return nullptr;
    }

    std::shared_ptr<Passage> addShopPassage(GameModule& module, const std::shared_ptr<Object>& owner) const
    {
        auto passage = std::make_shared<Passage>(*module.getMeshPointer(), module.getObjectHandler(), 0, 0, 1, 1, EMPTY_BIT_FIELD);
        module._passages.push_back(passage);
        passage->makeShop(owner->getObjRef());
        return passage;
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ShopFixture::s_runtime;

TEST_F(ShopFixture, MakeShopIgnoresInvalidOrMissingOwnerRefs)
{
    auto& module = beginActiveTestModule();
    Passage passage(*module.getMeshPointer(), module.getObjectHandler(), 0, 0, 1, 1, EMPTY_BIT_FIELD);

    passage.makeShop(ObjectRef::Invalid);
    EXPECT_FALSE(passage.isShop());
    EXPECT_EQ(passage.getShopOwner(), Passage::SHOP_NOOWNER);

    passage.makeShop(ObjectRef(77));
    EXPECT_FALSE(passage.isShop());
    EXPECT_EQ(passage.getShopOwner(), Passage::SHOP_NOOWNER);
}

TEST_F(ShopFixture, MakeShopMarksInPassageItemsAsShopInventory)
{
    auto& module = beginActiveTestModule();
    auto owner = makeObject(module, "mp_objects/follower.obj", 5191);
    auto item = makePricedItem(module, 5192);

    ASSERT_NE(owner, nullptr);
    ASSERT_NE(item, nullptr);

    owner->setPosition(64.0f, 64.0f, 0.0f);
    item->setPosition(64.0f, 64.0f, 0.0f);
    item->setShopItem(false);
    item->setKursed(true);
    item->setNameKnown(false);
    flushObjectHandler(module);

    Passage passage(*module.getMeshPointer(), module.getObjectHandler(), 0, 0, 1, 1, EMPTY_BIT_FIELD);
    passage.makeShop(owner->getObjRef());

    EXPECT_TRUE(passage.isShop());
    EXPECT_EQ(passage.getShopOwner(), owner->getObjRef());
    EXPECT_TRUE(item->isShopItem());
    EXPECT_FALSE(item->isKursed());
    EXPECT_TRUE(item->isNameKnown());
}

TEST_F(ShopFixture, BuyPublishesSellOrderAndTransfersMoney)
{
    auto& module = beginActiveTestModule();
    auto owner = makeObject(module, "mp_objects/follower.obj", 5201);
    auto buyer = makeObject(module, "mp_objects/follower.obj", 5202);
    auto item = makePricedItem(module, 5203);

    ASSERT_NE(owner, nullptr);
    ASSERT_NE(buyer, nullptr);
    ASSERT_NE(item, nullptr);

    addShopPassage(module, owner);
    item->setPosition(64.0f, 64.0f, 0.0f);

    const uint32_t price = static_cast<uint32_t>(item->getPrice());
    const uint16_t buyerStartMoney = buyer->getMoney();
    const uint16_t ownerStartMoney = owner->getMoney();
    buyer->giveMoney(static_cast<int>(price));

    EXPECT_TRUE(Shop::buy(buyer->getObjRef(), item->getObjRef()));
    EXPECT_EQ(Ego::Script::runtimeState(*owner).order_value, price);
    EXPECT_EQ(Ego::Script::runtimeState(*owner).order_counter, Passage::SHOP_SELL);
    EXPECT_EQ(buyer->getMoney(), buyerStartMoney);
    EXPECT_EQ(owner->getMoney(), static_cast<uint16_t>(ownerStartMoney + price));
}

TEST_F(ShopFixture, BuyRejectsWhenBuyerCannotAffordItem)
{
    auto& module = beginActiveTestModule();
    auto owner = makeObject(module, "mp_objects/follower.obj", 5211);
    auto buyer = makeObject(module, "mp_objects/follower.obj", 5212);
    auto item = makePricedItem(module, 5213);

    ASSERT_NE(owner, nullptr);
    ASSERT_NE(buyer, nullptr);
    ASSERT_NE(item, nullptr);

    addShopPassage(module, owner);
    item->setPosition(64.0f, 64.0f, 0.0f);

    const uint32_t price = static_cast<uint32_t>(item->getPrice());
    const uint16_t buyerStartMoney = buyer->getMoney();
    const uint16_t ownerStartMoney = owner->getMoney();

    EXPECT_FALSE(Shop::buy(buyer->getObjRef(), item->getObjRef()));
    EXPECT_EQ(Ego::Script::runtimeState(*owner).order_value, price);
    EXPECT_EQ(Ego::Script::runtimeState(*owner).order_counter, Passage::SHOP_NOAFFORD);
    EXPECT_EQ(buyer->getMoney(), buyerStartMoney);
    EXPECT_EQ(owner->getMoney(), ownerStartMoney);
}

TEST_F(ShopFixture, DropPublishesBuyOrderAndTransfersMoney)
{
    auto& module = beginActiveTestModule();
    auto owner = makeObject(module, "mp_objects/follower.obj", 5216);
    auto dropper = makeObject(module, "mp_objects/follower.obj", 5217);
    auto item = makePricedItem(module, 5218);

    ASSERT_NE(owner, nullptr);
    ASSERT_NE(dropper, nullptr);
    ASSERT_NE(item, nullptr);

    addShopPassage(module, owner);
    item->setPosition(64.0f, 64.0f, 0.0f);

    const uint32_t price = static_cast<uint32_t>(item->getPrice());
    const uint16_t dropperStartMoney = dropper->getMoney();
    const uint16_t ownerStartMoney = owner->getMoney();
    const uint16_t expectedOwnerMoney = static_cast<uint32_t>(ownerStartMoney) > price
                                        ? static_cast<uint16_t>(ownerStartMoney - price)
                                        : 0;

    EXPECT_TRUE(Shop::drop(dropper->getObjRef(), item->getObjRef()));
    EXPECT_EQ(Ego::Script::runtimeState(*owner).order_value, price);
    EXPECT_EQ(Ego::Script::runtimeState(*owner).order_counter, Passage::SHOP_BUY);
    EXPECT_EQ(dropper->getMoney(), static_cast<uint16_t>(dropperStartMoney + price));
    EXPECT_EQ(owner->getMoney(), expectedOwnerMoney);
}

TEST_F(ShopFixture, CanGrabItemVisibleBuyerUsesPurchasePath)
{
    auto& module = beginActiveTestModule();
    auto owner = makeObject(module, "mp_objects/follower.obj", 5219);
    auto buyer = makeObject(module, "mp_objects/follower.obj", 5220);
    auto item = makePricedItem(module, 5221);

    ASSERT_NE(owner, nullptr);
    ASSERT_NE(buyer, nullptr);
    ASSERT_NE(item, nullptr);

    addShopPassage(module, owner);
    owner->setPosition(64.0f, 64.0f, 0.0f);
    buyer->setPosition(64.0f, 64.0f, 0.0f);
    item->setPosition(64.0f, 64.0f, 0.0f);
    buyer->setAlpha(200);
    buyer->setLight(200);
    ASSERT_TRUE(owner->canSeeObject(buyer->getObjRef()));

    const uint32_t price = static_cast<uint32_t>(item->getPrice());
    const uint16_t buyerStartMoney = buyer->getMoney();
    const uint16_t ownerStartMoney = owner->getMoney();
    buyer->giveMoney(static_cast<int>(price));

    EXPECT_TRUE(Shop::canGrabItem(buyer->getObjRef(), item->getObjRef()));
    EXPECT_EQ(Ego::Script::runtimeState(*owner).order_value, price);
    EXPECT_EQ(Ego::Script::runtimeState(*owner).order_counter, Passage::SHOP_SELL);
    EXPECT_EQ(buyer->getMoney(), buyerStartMoney);
    EXPECT_EQ(owner->getMoney(), static_cast<uint16_t>(ownerStartMoney + price));
}

TEST_F(ShopFixture, StealDetectionPublishesTheftTargetAndOrder)
{
    auto& module = beginActiveTestModule();
    auto owner = makeObject(module, "mp_objects/follower.obj", 5221);
    auto thief = makeObject(module, "mp_objects/follower.obj", 5222);
    auto item = makePricedItem(module, 5223);

    ASSERT_NE(owner, nullptr);
    ASSERT_NE(thief, nullptr);
    ASSERT_NE(item, nullptr);

    addShopPassage(module, owner);
    owner->setPosition(64.0f, 64.0f, 0.0f);
    thief->setPosition(64.0f, 64.0f, 0.0f);
    item->setPosition(64.0f, 64.0f, 0.0f);
    owner->setTempAttribute(Ego::Attribute::INTELLECT, 200.0f);
    thief->setTempAttribute(Ego::Attribute::AGILITY, -100.0f);

    EXPECT_FALSE(Shop::steal(thief->getObjRef(), item->getObjRef()));
    EXPECT_EQ(Ego::Script::runtimeState(*owner).order_value, Passage::SHOP_STOLEN);
    EXPECT_EQ(Ego::Script::runtimeState(*owner).order_counter, Passage::SHOP_THEFT);
    EXPECT_EQ(owner->getAITarget(), thief->getObjRef());
}

} // namespace
