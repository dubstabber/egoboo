#include "gtest/gtest.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "TestEnvironment.hpp"
#define private public
#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/game.h"
#include "egolib/game/Module/Module.hpp"
#undef private
#include "egolib/Script/IScriptSystem.hpp"
#include "egolib/vfs.h"

namespace
{

class RecordingParticleHandler : public IParticleHandler
{
public:
    struct GlobalSpawnCall
    {
        Ego::Vector3f position;
        Facing facing;
        LocalParticleProfileRef particleProfile;
        int multispawn;
        bool onlyOverWater;
    };

    struct LocalSpawnCall
    {
        Ego::Vector3f position;
        Facing facing;
        ObjectProfileRef profile;
        LocalParticleProfileRef particleProfile;
        ObjectRef attachment;
        uint16_t vertexOffset;
        TEAM_REF team;
        ObjectRef origin;
        ParticleRef particleOrigin;
        int multispawn;
        ObjectRef target;
    };

    void updateAllParticles() override {}

    void download(egoboo_config_t&) override {}

    void upload(egoboo_config_t&) override {}

    size_t getDisplayLimit() const override
    {
        return 0;
    }

    void setDisplayLimit(size_t) override {}

    void clear() override
    {
        _particles.clear();
        _particlesByRef.clear();
        _count = 0;
        _freeCount = 0;
        globalSpawnCalls.clear();
        localSpawnCalls.clear();
    }

    const std::shared_ptr<Ego::Particle>& operator[](ParticleRef index) override
    {
        const auto it = _particlesByRef.find(index);
        return it == _particlesByRef.end() ? Ego::Particle::INVALID_PARTICLE : it->second;
    }

    std::shared_ptr<Ego::Particle> spawnLocalParticle(const Ego::Vector3f& position,
                                                      const Facing& facing,
                                                      ObjectProfileRef profile,
                                                      const LocalParticleProfileRef& particleProfile,
                                                      ObjectRef attachment,
                                                      uint16_t vertexOffset,
                                                      TEAM_REF team,
                                                      ObjectRef origin,
                                                      ParticleRef particleOrigin,
                                                      int multispawn,
                                                      ObjectRef target) override
    {
        localSpawnCalls.push_back({position, facing, profile, particleProfile, attachment, vertexOffset, team, origin, particleOrigin, multispawn, target});
        return Ego::Particle::INVALID_PARTICLE;
    }

    std::shared_ptr<Ego::Particle> spawnParticle(const Ego::Vector3f&,
                                                 const Facing&,
                                                 ObjectProfileRef,
                                                 PIP_REF,
                                                 ObjectRef,
                                                 uint16_t,
                                                 TEAM_REF,
                                                 ObjectRef,
                                                 ParticleRef,
                                                 int,
                                                 ObjectRef,
                                                 bool) override
    {
        return Ego::Particle::INVALID_PARTICLE;
    }

    std::shared_ptr<Ego::Particle> spawnGlobalParticle(const Ego::Vector3f& position,
                                                       const Facing& facing,
                                                       const LocalParticleProfileRef& particleProfile,
                                                       int multispawn,
                                                       bool onlyOverWater = false) override
    {
        globalSpawnCalls.push_back({position, facing, particleProfile, multispawn, onlyOverWater});
        return Ego::Particle::INVALID_PARTICLE;
    }

    size_t getCount() const override
    {
        return _count;
    }

    size_t getFreeCount() const override
    {
        return _freeCount;
    }

    std::shared_ptr<const Ego::Texture> getLightParticleTexture() override
    {
        return nullptr;
    }

    std::shared_ptr<const Ego::Texture> getTransparentParticleTexture() override
    {
        return nullptr;
    }

    void spawnPoof(ObjectRef) override {}

    void spawnDefencePing(ObjectRef, ObjectRef) override {}

    void addActiveParticle(const std::shared_ptr<Ego::Particle>& particle)
    {
        _particles.push_back(particle);
        _particlesByRef[particle->getParticleID()] = particle;
        _count = _particles.size();
    }

    void setFreeCount(size_t freeCount)
    {
        _freeCount = freeCount;
    }

    std::vector<GlobalSpawnCall> globalSpawnCalls;
    std::vector<LocalSpawnCall> localSpawnCalls;

protected:
    ParticleList::const_iterator beginActiveParticles() override
    {
        return _particles.cbegin();
    }

    ParticleList::const_iterator endActiveParticles() override
    {
        return _particles.cend();
    }

    void lockParticles() override {}

    void unlockParticles() override {}

private:
    size_t _count = 0;
    size_t _freeCount = 0;
    ParticleList _particles;
    std::unordered_map<ParticleRef, std::shared_ptr<Ego::Particle>> _particlesByRef;
};

class ScopedInstalledParticleHandler
{
public:
    explicit ScopedInstalledParticleHandler(IParticleHandler& handler)
    {
        EngineContext::get().clearParticleHandler();
        EngineContext::get().installParticleHandler(handler);
    }

    ~ScopedInstalledParticleHandler()
    {
        EngineContext::get().clearParticleHandler();
        EngineContext::get().installParticleHandler(ParticleHandler::get());
    }

    ScopedInstalledParticleHandler(const ScopedInstalledParticleHandler&) = delete;
    ScopedInstalledParticleHandler& operator=(const ScopedInstalledParticleHandler&) = delete;
};

class RecordingScriptSystem : public Ego::Script::IScriptSystem
{
public:
    void runCharacterScript(Object* object) override
    {
        runRefs.push_back(object != nullptr ? object->getObjRef() : ObjectRef::Invalid);
    }

    void setAlerts(ObjectRef character) override
    {
        alertRefs.push_back(character);
    }

    void endScriptingSystem() override {}

    std::vector<ObjectRef> runRefs;
    std::vector<ObjectRef> alertRefs;
};

class ScopedInstalledScriptSystem
{
public:
    explicit ScopedInstalledScriptSystem(Ego::Script::IScriptSystem& system) :
        _previous(Ego::Script::tryActiveScriptSystem())
    {
        Ego::Script::installScriptSystem(&system);
    }

    ~ScopedInstalledScriptSystem()
    {
        Ego::Script::installScriptSystem(_previous);
    }

    ScopedInstalledScriptSystem(const ScopedInstalledScriptSystem&) = delete;
    ScopedInstalledScriptSystem& operator=(const ScopedInstalledScriptSystem&) = delete;

private:
    Ego::Script::IScriptSystem* _previous;
};

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

    ObjectProfileRef loadFollowerProfile(int slot) const
    {
        return EngineContext::get().profileSystem().loadOneProfile("mp_objects/follower.obj", slot);
    }

    std::shared_ptr<Object> makeObject(GameModule& module, const std::string& profilePath, int slot,
                                       const Ego::Vector3f& position = Ego::Vector3f(64.0f, 64.0f, 0.0f)) const
    {
        const ObjectProfileRef profile = EngineContext::get().profileSystem().loadOneProfile(profilePath, slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid)
        {
            return nullptr;
        }

        const ObjectRef objectRef = module.spawnObjectRef(position, profile, static_cast<TEAM_REF>(Team::TEAM_NULL), 0, Facing(0), "", ObjectRef::Invalid);
        return module.getObjectHandler().getHandle(objectRef);
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

    void flushObjectHandler(GameModule& module) const
    {
        auto refs = module.getObjectHandler().objectRefIterator();
        (void)refs;
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

TEST_F(ModuleUpdateFixture, UpdatePitsKillsLiveObjectBelowPitDepthAndSkipsInvincibleObjects)
{
    auto& module = beginActiveTestModule();
    auto fallen = makeObject(module, "mp_objects/follower.obj", 4102,
                             Ego::Vector3f(64.0f, 64.0f, GameModule::PITDEPTH - 8.0f));
    auto invincible = makeObject(module, "mp_objects/follower.obj", 4103,
                                 Ego::Vector3f(96.0f, 64.0f, GameModule::PITDEPTH - 8.0f));

    ASSERT_NE(fallen, nullptr);
    ASSERT_NE(invincible, nullptr);

    fallen->setInvincible(false);
    invincible->setInvincible(true);
    flushObjectHandler(module);

    module.enablePitsKill();
    module._pitsClock = 1;

    module.updatePits();

    EXPECT_FALSE(fallen->isAlive());
    EXPECT_TRUE(invincible->isAlive());
    EXPECT_EQ(module._pitsClock, GameModule::PIT_CLOCK_RATE);
}

TEST_F(ModuleUpdateFixture, UpdateDamageTilesDamagesEligibleObjectsAndSkipsInvincibleObjects)
{
    auto& module = beginActiveTestModule();
    auto victim = makeObject(module, "mp_objects/follower.obj", 41031,
                             Ego::Vector3f(64.0f, 64.0f, 0.0f));
    auto invincible = makeObject(module, "mp_objects/follower.obj", 41032,
                                 Ego::Vector3f(96.0f, 64.0f, 0.0f));

    ASSERT_NE(victim, nullptr);
    ASSERT_NE(invincible, nullptr);

    victim->setInvincible(false);
    victim->setDamageTimer(0);
    invincible->setInvincible(true);
    invincible->setDamageTimer(0);
    flushObjectHandler(module);

    module.getMeshPointer()->add_fx(victim->getTile(), MAPFX_DAMAGE);
    module.getMeshPointer()->add_fx(invincible->getTile(), MAPFX_DAMAGE);
    module._damageTile.amount = IPair(512, 0);
    module._damageTile.damagetype = DAMAGE_DIRECT;
    module._damageTile.part_gpip = LocalParticleProfileRef::Invalid;
    GameSessionContext::get().worldUpdateCount() = 1;

    const float victimLifeBefore = victim->getLife();
    const float invincibleLifeBefore = invincible->getLife();

    module.updateDamageTiles();

    EXPECT_FLOAT_EQ(victim->getLife(), victimLifeBefore - 2.0f);
    EXPECT_EQ(victim->getDamageTimer(), static_cast<uint8_t>(GameModule::DAMAGETILETIME));
    EXPECT_FLOAT_EQ(invincible->getLife(), invincibleLifeBefore);
    EXPECT_EQ(invincible->getDamageTimer(), static_cast<uint8_t>(0));
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

    EngineContext::get().particleHandler().spawnDefencePing(defender->getObjRef(), attacker->getObjRef());

    EXPECT_TRUE(defender->hasAnyAIAlertBits(ALERTIF_BLOCKED));
    EXPECT_EQ(defender->getAILastAttacker(), attacker->getObjRef());
    EXPECT_EQ(defender->getDamageTimer(), ParticleHandler::DEFENDTIME);
}

TEST_F(ModuleUpdateFixture, SpawnDefencePingClearsLastAttackerWhenAttackerRefIsStale)
{
    beginActiveTestModule();
    auto& session = GameSessionContext::get();
    auto defender = makeFollower(session.objectHandler(), 4108);
    ASSERT_NE(defender, nullptr);

    defender->setDamageTimer(0);
    defender->setAILastAttacker(ObjectRef(77));
    defender->clearAIAlertBits(ALERTIF_BLOCKED);

    EngineContext::get().particleHandler().spawnDefencePing(defender->getObjRef(), ObjectRef(77));

    EXPECT_TRUE(defender->hasAnyAIAlertBits(ALERTIF_BLOCKED));
    EXPECT_EQ(defender->getAILastAttacker(), ObjectRef::Invalid);
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

TEST_F(ModuleUpdateFixture, LetAllCharactersThinkDispatchesEligibleLiveObjectsThroughScriptSystem)
{
    auto& module = beginActiveTestModule();
    module.getObjectHandler().clear();

    auto eligible = makeObject(module, "mp_objects/follower.obj", 4109,
                               Ego::Vector3f(64.0f, 64.0f, 0.0f));
    auto dead = makeObject(module, "mp_objects/follower.obj", 4110,
                           Ego::Vector3f(96.0f, 64.0f, 0.0f));
    auto removed = makeObject(module, "mp_objects/follower.obj", 4111,
                              Ego::Vector3f(128.0f, 64.0f, 0.0f));

    ASSERT_NE(eligible, nullptr);
    ASSERT_NE(dead, nullptr);
    ASSERT_NE(removed, nullptr);
    flushObjectHandler(module);

    dead->_isAlive = false;
    removed->requestTerminate();

    RecordingScriptSystem scriptSystem;
    ScopedInstalledScriptSystem scopedScriptSystem(scriptSystem);

    MainLoop::let_all_characters_think();

    ASSERT_EQ(scriptSystem.alertRefs.size(), 1u);
    ASSERT_EQ(scriptSystem.runRefs.size(), 1u);
    EXPECT_EQ(scriptSystem.alertRefs.front(), eligible->getObjRef());
    EXPECT_EQ(scriptSystem.runRefs.front(), eligible->getObjRef());
}

TEST_F(ModuleUpdateFixture, ObjectUpdateRoutesWaterSplashThroughInstalledParticleService)
{
    auto& module = beginActiveTestModule();
    auto object = makeObject(module, "mp_objects/follower.obj", 41071, Ego::Vector3f(64.0f, 64.0f, 0.0f));
    ASSERT_NE(object, nullptr);

    module.getMeshPointer()->add_fx(object->getTile(), MAPFX_WATER);
    module.getWater()._is_water = true;
    module.getWater().set_douse_level(16.0f);
    object->setInWater(false);
    GameSessionContext::get().worldUpdateCount() = 1;

    RecordingParticleHandler handler;
    ScopedInstalledParticleHandler scopedHandler(handler);

    object->update();

    ASSERT_EQ(handler.globalSpawnCalls.size(), 1u);
    EXPECT_EQ(handler.globalSpawnCalls.front().particleProfile.get(), PIP_SPLASH);
    EXPECT_EQ(handler.globalSpawnCalls.front().position.x(), object->getPosX());
    EXPECT_EQ(handler.globalSpawnCalls.front().position.y(), object->getPosY());
    EXPECT_FLOAT_EQ(handler.globalSpawnCalls.front().position.z(), module.getWater().get_level() + 10.0f);
    EXPECT_TRUE(object->isInWater());
}

TEST_F(ModuleUpdateFixture, ParticleInitializeResolvesOwnerThroughInstalledParentParticleService)
{
    beginActiveTestModule();
    EngineContext::get().profileSystem().loadGlobalParticleProfiles();

    auto& session = GameSessionContext::get();
    auto owner = makeFollower(session.objectHandler(), 4109);
    ASSERT_NE(owner, nullptr);

    auto parent = std::make_shared<Ego::Particle>();
    parent->_particleID = ParticleRef(41);
    parent->_isTerminated = false;
    parent->owner_ref = owner->getObjRef();

    RecordingParticleHandler handler;
    handler.addActiveParticle(parent);

    ScopedInstalledParticleHandler scopedHandler(handler);

    Ego::Particle particle;
    const bool initialized = particle.initialize(ParticleRef(42),
                                                 Ego::Vector3f(64.0f, 64.0f, 0.0f),
                                                 Facing(0),
                                                 ObjectProfileRef::Invalid,
                                                 PIP_RIPPLE,
                                                 ObjectRef::Invalid,
                                                 GRIP_LAST,
                                                 static_cast<TEAM_REF>(Team::TEAM_NULL),
                                                 ObjectRef::Invalid,
                                                 parent->getParticleID(),
                                                 0,
                                                 ObjectRef::Invalid,
                                                 false);

    ASSERT_TRUE(initialized);
    EXPECT_EQ(particle.owner_ref, owner->getObjRef());
}

TEST_F(ModuleUpdateFixture, ParticleGetOwnerUsesInstalledParticleServiceForParentFallback)
{
    beginActiveTestModule();

    auto& session = GameSessionContext::get();
    auto owner = makeFollower(session.objectHandler(), 4110);
    ASSERT_NE(owner, nullptr);

    auto parent = std::make_shared<Ego::Particle>();
    parent->_particleID = ParticleRef(51);
    parent->_isTerminated = false;
    parent->owner_ref = owner->getObjRef();
    parent->parent_ref = ParticleRef::Invalid;

    RecordingParticleHandler handler;
    handler.addActiveParticle(parent);

    ScopedInstalledParticleHandler scopedHandler(handler);

    Ego::Particle child;
    child._particleID = ParticleRef(52);
    child._isTerminated = false;
    child.owner_ref = ObjectRef::Invalid;
    child.parent_ref = parent->getParticleID();

    EXPECT_EQ(child.getOwner(), owner->getObjRef());
}

TEST_F(ModuleUpdateFixture, ParticleDestroyRoutesEndSpawnThroughInstalledParticleService)
{
    beginActiveTestModule();

    RecordingParticleHandler handler;
    ScopedInstalledParticleHandler scopedHandler(handler);

    auto profile = std::make_shared<ParticleProfile>();
    profile->endspawn._amount = 2;
    profile->endspawn._lpip = LocalParticleProfileRef(PIP_RIPPLE);
    profile->endspawn._facingAdd = 64;
    profile->end_sound = -1;

    Ego::Particle particle;
    particle._particleID = ParticleRef(61);
    particle._isTerminated = true;
    particle._particleProfile = profile;
    particle._spawnerProfile = ObjectProfileRef::Invalid;
    particle.facing = Facing(10);

    particle.destroy();

    ASSERT_EQ(handler.globalSpawnCalls.size(), 2u);
    EXPECT_TRUE(handler.localSpawnCalls.empty());
    EXPECT_EQ(handler.globalSpawnCalls.front().particleProfile.get(), PIP_RIPPLE);
    EXPECT_EQ(handler.globalSpawnCalls.front().multispawn, 0);
    EXPECT_EQ(handler.globalSpawnCalls.back().multispawn, 1);
}

} // namespace
