/// @file ScriptResidualFunctions.cpp
/// @brief Characterization tests for the 6 remaining previously-untested scr_* functions -- a
///        residual grab-bag spanning five translation units, closing out coverage of every
///        scr_* dispatch function in the codebase:
///          * script_functions_spawn_particle.c:   scr_DisaffirmCharacter, scr_ReaffirmCharacter
///          * script_functions_commerce_module.c:  scr_EndModule
///          * script_functions_commerce_passages.c: scr_FindTileInPassage
///          * script_functions_action_audio.c:     scr_SetVolumeNearestTeammate
///          * script_functions_teams.c:            scr_IfLeaderKilled

#include "gtest/gtest.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#define private public
#include "egolib/Entities/_Include.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#undef private
#include "egolib/FileFormats/map_fx.hpp"
#include "egolib/Image/ImageManager.hpp"
#include "egolib/Logic/PerkHandler.hpp"
#include "egolib/Logic/Team.hpp"
#include "egolib/Script/script.h"
#include "egolib/game/CharacterParticleOps.h"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/GameStates/GameState.hpp"
#include "egolib/game/IPlayingStateController.hpp"
#include "egolib/game/Module/Passage.hpp"
#include "egolib/game/game.h"
#include "egolib/game/script_functions.h"
#include "egolib/vfs.h"

namespace
{

/// A GameState that also implements IPlayingStateController, tracking calls to
/// endModuleInVictory() -- mirrors PlayingState's inheritance shape without depending on the
/// concrete (GUI-heavy) PlayingState.
class ControllerGameState : public GameState, public IPlayingStateController
{
public:
    void update() override {}
    void draw(Ego::GUI::DrawingContext&) override {}

    bool showMiniMap() override { return false; }
    void setMiniMapShowPlayerPosition(bool) override {}
    void addMiniMapBlip(float, float, const std::shared_ptr<const Ego::Texture>&) override {}
    void addMessageLogMessage(const std::string&) override {}
    ObjectRef getStatusCharacterRef(size_t) override { return ObjectRef::Invalid; }
    void addStatusMonitor(ObjectRef) override {}
    void displayCharacterWindow(uint8_t) override {}

    void endModuleInVictory() override { ++endModuleInVictoryCalls; }

    int endModuleInVictoryCalls = 0;

protected:
    void drawContainer(Ego::GUI::DrawingContext&) override {}
};

/// A plain GameState that does NOT implement IPlayingStateController -- used to pin the
/// dynamic_pointer_cast-fails-silently path of scr_EndModule.
class NonControllerGameState : public GameState
{
public:
    void update() override {}
    void draw(Ego::GUI::DrawingContext&) override {}

protected:
    void drawContainer(Ego::GUI::DrawingContext&) override {}
};

/// RAII helper installing a fresh headless GameEngine into EngineContext for the lifetime of a
/// single test, so a raw (non-ctest, single-process) run of every TEST_F in this fixture does
/// not hit "game engine already installed" on the second engine-using test. EngineContext's only
/// teardown entry point (clearEngine()) has a wide blast radius -- it also unconditionally clears
/// every service installed once suite-wide (ImageManager/PerkHandler/ProfileSystem via
/// ContentRuntimeBootstrap, AudioSystem/ParticleHandler via this fixture's SetUpTestSuite) -- so
/// the destructor reinstalls all five of those same singleton instances afterward. GameEngine's
/// destructor is trivial on a never-initialize()'d instance, so this is otherwise safe.
class ScopedTestEngine
{
public:
    ScopedTestEngine()
    {
        EngineContext::get().setEngine(std::make_unique<GameEngine>());
    }

    ~ScopedTestEngine()
    {
        EngineContext::get().clearEngine();
        EngineContext::get().installImageManager(Ego::ImageManager::get());
        EngineContext::get().installPerkHandler(Ego::Perks::PerkHandler::get());
        EngineContext::get().installProfileSystem(ProfileSystem::get());
        EngineContext::get().installAudioSystem(AudioSystem::get());
        EngineContext::get().installParticleHandler(ParticleHandler::get());
    }

    ScopedTestEngine(const ScopedTestEngine&) = delete;
    ScopedTestEngine& operator=(const ScopedTestEngine&) = delete;
};

class ScriptResidualFunctionsFixture : public ::testing::Test
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
        opts.randomSeed = 82;
        opts.binaryPath = "";
        opts.logPath = "/debug/script-residual-function-tests.log";
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

        // The particle handler is a suite-scoped singleton, not reset between individual tests
        // by anything above -- clear it so exact active/pending particle counts asserted by the
        // Disaffirm/Reaffirm tests are never contaminated by a previous test's leftover
        // particles (object refs are reused across tests since the module is reloaded fresh
        // every SetUp()).
        EngineContext::get().particleHandler().clear();
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

    GameModule& beginActiveTestModule()
    {
        auto module = findTestModule();
        EXPECT_NE(module, nullptr);
        if (module == nullptr)
        {
            throw std::runtime_error("test.mod profile not found");
        }

        auto& session = GameSessionContext::get();
        const bool began = session.beginModule(module, 82);
        EXPECT_TRUE(began);
        return session.activeModule();
    }

    ai_state_t makeScriptSelf(const std::shared_ptr<Object>& selfObject, const std::shared_ptr<Object>& targetObject) const
    {
        ai_state_t self;
        self.setSelf(selfObject ? selfObject->getObjRef() : ObjectRef::Invalid);
        self.setTarget(targetObject ? targetObject->getObjRef() : ObjectRef::Invalid);
        return self;
    }

    /// Materializes and immediately destroys a particle-handler iterator to force
    /// unlockParticles(): terminated actives are purged/pooled and any pending particles are
    /// promoted to active. A no-op side effect other than this promotion/purge.
    void flushParticleHandler() const
    {
        auto it = EngineContext::get().particleHandler().iterator();
        (void)it;
    }

    /// Spawns a torch and gives the Disaffirm/Reaffirm tests a clean, isolated particle-handler
    /// baseline to build on. Quirk discovered while writing these tests: a torch's spawn path
    /// (Object::respawn(), called as part of normal spawnObjectRef() setup) already calls
    /// reaffirm_attached_particles() once on its own -- as a side effect it both queues the
    /// torch's own attached particle into the pending list AND sets ALERTIF_REAFFIRMED, entirely
    /// independent of any AI script or explicit scr_ReaffirmCharacter call. That spawn-time
    /// behavior belongs to Object::respawn(), not to the scr_ functions under test here, so it is
    /// cleared away immediately to isolate what THIS test is pinning.
    std::shared_ptr<Object> makeTorch(GameModule& module, int slot) const
    {
        auto torch = makeObject(module, "mp_data/globalobjects/items/torch.obj", slot);
        EngineContext::get().particleHandler().clear();
        if (torch)
        {
            torch->clearAIAlertBits(ALERTIF_REAFFIRMED);
        }
        return torch;
    }

    /// Raw, side-effect-free peeks at the particle handler's internal lists. Deliberately NOT
    /// implemented via number_of_attached_particles()/iterator(): both of those construct-and-
    /// destroy a ParticleIterator, whose destructor unconditionally promotes any currently
    /// pending particles to active as a side effect -- so using them to "just check" whether a
    /// particle is still pending would itself silently flush it, corrupting the very state under
    /// observation. See the DisaffirmCharacter/ReaffirmCharacter pending-vs-active tests below.
    size_t activeParticleCount() const
    {
        return static_cast<const ParticleHandler&>(EngineContext::get().particleHandler())._activeParticles.size();
    }

    size_t pendingParticleCount() const
    {
        return static_cast<const ParticleHandler&>(EngineContext::get().particleHandler())._pendingParticles.size();
    }

    void setGridTileType(GameModule& module, int gx, int gy, uint16_t type) const
    {
        const bool ok = module.setTileTypeAtPosition(Ego::Vector2f(gx * 128.0f + 64.0f, gy * 128.0f + 64.0f), type);
        ASSERT_TRUE(ok) << "failed to set tile type at grid (" << gx << "," << gy << ")";
    }

    void clearGridRegion(GameModule& module, int x0, int y0, int x1, int y1, uint16_t baseline) const
    {
        for (int gy = y0; gy <= y1; ++gy)
        {
            for (int gx = x0; gx <= x1; ++gx)
            {
                setGridTileType(module, gx, gy, baseline);
            }
        }
    }

    std::pair<std::shared_ptr<Passage>, int> addPassage(GameModule& module, int x0, int y0, int x1, int y1) const
    {
        auto passage = std::make_shared<Passage>(*module.getMeshPointer(), module.getObjectHandler(), x0, y0, x1, y1, EMPTY_BIT_FIELD);
        module._passages.push_back(passage);
        return {passage, static_cast<int>(module._passages.size() - 1)};
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ScriptResidualFunctionsFixture::s_runtime;

//--------------------------------------------------------------------------------------------
// script_functions_spawn_particle.c: scr_ReaffirmCharacter, scr_DisaffirmCharacter
//--------------------------------------------------------------------------------------------

TEST_F(ScriptResidualFunctionsFixture, ReaffirmCharacterSpawnsIntoPendingListAndSetsAlertBeforeAnyFlush)
{
    auto& module = beginActiveTestModule();
    auto torch = makeTorch(module, 6801);
    ASSERT_NE(torch, nullptr);
    ASSERT_EQ(torch->getProfile()->getAttachedParticleAmount(), 1);

    ai_state_t self = makeScriptSelf(torch, nullptr);
    script_state_t state;
    state.argument = 12345; // unread by this function -- proven unchanged below
    state.distance = 6789;

    EXPECT_TRUE(scr_ReaffirmCharacter(state, self));
    // The alert is published on the REAL object's ai_state (via IScriptable, resolved
    // separately by ref inside reaffirm_attached_particles), not on the local `self` passed in.
    EXPECT_TRUE(torch->hasAnyAIAlertBits(ALERTIF_REAFFIRMED));
    EXPECT_EQ(state.argument, 12345);
    EXPECT_EQ(state.distance, 6789);

    // Not flushed yet: the just-spawned particle is still pending, invisible to the
    // active-only count.
    EXPECT_EQ(activeParticleCount(), 0u);
    EXPECT_EQ(pendingParticleCount(), 1u);

    flushParticleHandler();
    EXPECT_EQ(activeParticleCount(), 1u);
    EXPECT_EQ(pendingParticleCount(), 0u);
    EXPECT_EQ(number_of_attached_particles(torch->getObjRef()), 1);
}

TEST_F(ScriptResidualFunctionsFixture, ReaffirmCharacterSetsNoAlertOnceAlreadyAtCapacityButStillReturnsTrue)
{
    auto& module = beginActiveTestModule();
    auto torch = makeTorch(module, 6802);
    ASSERT_NE(torch, nullptr);

    ai_state_t self = makeScriptSelf(torch, nullptr);
    script_state_t state;

    ASSERT_TRUE(scr_ReaffirmCharacter(state, self));
    flushParticleHandler();
    ASSERT_EQ(activeParticleCount(), 1u);

    // Already at capacity (count >= amount): reaffirm_attached_particles bails before spawning
    // OR setting the alert, but the scr_ wrapper still discards the count and returns true.
    torch->clearAIAlertBits(ALERTIF_REAFFIRMED);
    EXPECT_TRUE(scr_ReaffirmCharacter(state, self));
    EXPECT_FALSE(torch->hasAnyAIAlertBits(ALERTIF_REAFFIRMED));
    EXPECT_EQ(activeParticleCount(), 1u);
    EXPECT_EQ(pendingParticleCount(), 0u);
}

TEST_F(ScriptResidualFunctionsFixture, ReaffirmCharacterIsANoOpAlertWiseForProfileWithZeroAttachedParticleAmount)
{
    auto& module = beginActiveTestModule();
    auto follower = makeObject(module, "mp_objects/follower.obj", 6803);
    ASSERT_NE(follower, nullptr);
    ASSERT_EQ(follower->getProfile()->getAttachedParticleAmount(), 0);

    follower->clearAIAlertBits(ALERTIF_REAFFIRMED);
    ai_state_t self = makeScriptSelf(follower, nullptr);
    script_state_t state;

    EXPECT_TRUE(scr_ReaffirmCharacter(state, self));
    EXPECT_FALSE(follower->hasAnyAIAlertBits(ALERTIF_REAFFIRMED));
    EXPECT_EQ(number_of_attached_particles(follower->getObjRef()), 0);
}

TEST_F(ScriptResidualFunctionsFixture, ReaffirmCharacterDoubleSpawnsWhenCalledTwiceBeforeAnyFlush)
{
    auto& module = beginActiveTestModule();
    auto torch = makeTorch(module, 6804);
    ASSERT_NE(torch, nullptr);

    ai_state_t self = makeScriptSelf(torch, nullptr);
    script_state_t state;

    EXPECT_TRUE(scr_ReaffirmCharacter(state, self));
    EXPECT_TRUE(torch->hasAnyAIAlertBits(ALERTIF_REAFFIRMED));
    EXPECT_EQ(activeParticleCount(), 0u);
    EXPECT_EQ(pendingParticleCount(), 1u);

    // Quirk pinned deliberately: with no flush between the two calls, the SECOND call's own
    // internal number_of_attached_particles capacity check still only scans the active list (0
    // items) -- but as a side effect of that very check's own iterator unlocking, it promotes the
    // FIRST call's still-pending particle to active before the second spawnParticle() call
    // appends its own new particle to pending. So this call also succeeds and sets the alert.
    torch->clearAIAlertBits(ALERTIF_REAFFIRMED);
    EXPECT_TRUE(scr_ReaffirmCharacter(state, self));
    EXPECT_TRUE(torch->hasAnyAIAlertBits(ALERTIF_REAFFIRMED));
    EXPECT_EQ(activeParticleCount(), 1u);
    EXPECT_EQ(pendingParticleCount(), 1u);

    flushParticleHandler();
    EXPECT_EQ(activeParticleCount(), 2u);
    EXPECT_EQ(pendingParticleCount(), 0u);
}

TEST_F(ScriptResidualFunctionsFixture, DisaffirmCharacterTerminatesActiveAttachedParticlesAndSetsAlertUnconditionally)
{
    auto& module = beginActiveTestModule();
    auto torch = makeTorch(module, 6805);
    ASSERT_NE(torch, nullptr);

    ai_state_t reaffirmSelf = makeScriptSelf(torch, nullptr);
    script_state_t state;
    ASSERT_TRUE(scr_ReaffirmCharacter(state, reaffirmSelf));
    flushParticleHandler();
    ASSERT_EQ(activeParticleCount(), 1u);

    torch->clearAIAlertBits(ALERTIF_DISAFFIRMED);
    ai_state_t disaffirmSelf = makeScriptSelf(torch, nullptr);
    EXPECT_TRUE(scr_DisaffirmCharacter(state, disaffirmSelf));
    EXPECT_TRUE(torch->hasAnyAIAlertBits(ALERTIF_DISAFFIRMED));

    // The termination + pool-return already happened synchronously inside the call: the
    // termination loop's own temporary particle-handler iterator is destroyed (and unlocks)
    // before disaffirm_attached_particles returns, so no external flush is needed here.
    EXPECT_EQ(activeParticleCount(), 0u);
    EXPECT_EQ(pendingParticleCount(), 0u);
}

TEST_F(ScriptResidualFunctionsFixture, DisaffirmCharacterSetsAlertEvenWhenNoParticlesWereEverAttached)
{
    auto& module = beginActiveTestModule();
    auto follower = makeObject(module, "mp_objects/follower.obj", 6806);
    ASSERT_NE(follower, nullptr);

    follower->clearAIAlertBits(ALERTIF_DISAFFIRMED);
    ai_state_t self = makeScriptSelf(follower, nullptr);
    script_state_t state;

    EXPECT_TRUE(scr_DisaffirmCharacter(state, self));
    EXPECT_TRUE(follower->hasAnyAIAlertBits(ALERTIF_DISAFFIRMED));
}

TEST_F(ScriptResidualFunctionsFixture, DisaffirmCharacterLeavesStillPendingParticlesAliveAndPromotesThemToActive)
{
    auto& module = beginActiveTestModule();
    auto torch = makeTorch(module, 6807);
    ASSERT_NE(torch, nullptr);

    ai_state_t self = makeScriptSelf(torch, nullptr);
    script_state_t state;

    ASSERT_TRUE(scr_ReaffirmCharacter(state, self));
    // Raw peek (side-effect free): the spawned particle is still pending, invisible to the
    // disaffirm termination loop's active-only iterator. Deliberately not using
    // number_of_attached_particles() here -- calling it would itself flush (promote) the pending
    // particle before disaffirm ever runs, defeating the point of this test.
    ASSERT_EQ(activeParticleCount(), 0u);
    ASSERT_EQ(pendingParticleCount(), 1u);

    torch->clearAIAlertBits(ALERTIF_DISAFFIRMED);
    EXPECT_TRUE(scr_DisaffirmCharacter(state, self));
    EXPECT_TRUE(torch->hasAnyAIAlertBits(ALERTIF_DISAFFIRMED));

    // Quirk pinned deliberately: the pending particle was never terminated by the disaffirm
    // loop (it only iterates the active list) -- disaffirm's own iterator destructor unlocked
    // and promoted it to active as a side effect, so it SURVIVES disaffirm.
    EXPECT_EQ(activeParticleCount(), 1u);
    EXPECT_EQ(pendingParticleCount(), 0u);
}

TEST_F(ScriptResidualFunctionsFixture, DisaffirmAndReaffirmReturnFalseForUnresolvedSelfAndLeaveActiveAttachedParticlesUntouched)
{
    auto& module = beginActiveTestModule();
    auto torch = makeTorch(module, 6808);
    ASSERT_NE(torch, nullptr);

    ai_state_t self = makeScriptSelf(torch, nullptr);
    script_state_t state;
    ASSERT_TRUE(scr_ReaffirmCharacter(state, self));
    flushParticleHandler();
    ASSERT_EQ(activeParticleCount(), 1u);

    const ObjectRef torchRef = torch->getObjRef();
    torch->requestTerminate();

    ai_state_t staleSelf;
    staleSelf.setSelf(torchRef);

    // Both calls would produce a visible side effect if the resolveSpawnSelfContext guard were
    // skipped: disaffirm_attached_particles(torchRef) would terminate the still-active particle
    // regardless of whether the owning object itself is terminated (its termination loop only
    // checks the particle's own attachedObjectID, never the owner's liveness) -- so this is a
    // real, non-vacuous guard test, not merely "self is Invalid".
    EXPECT_FALSE(scr_DisaffirmCharacter(state, staleSelf));
    EXPECT_FALSE(scr_ReaffirmCharacter(state, staleSelf));
    EXPECT_EQ(activeParticleCount(), 1u);
    EXPECT_EQ(pendingParticleCount(), 0u);
}

//--------------------------------------------------------------------------------------------
// script_functions_commerce_module.c: scr_EndModule
//--------------------------------------------------------------------------------------------

TEST_F(ScriptResidualFunctionsFixture, EndModuleReturnsTrueAsSilentNoOpWhenNoEngineIsInstalled)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6809);
    ASSERT_NE(actor, nullptr);

    ASSERT_EQ(EngineContext::get().tryEngine(), nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, nullptr);

    EXPECT_TRUE(scr_EndModule(state, self));
}

TEST_F(ScriptResidualFunctionsFixture, EndModulePushesVictoryOnControllerCurrentStateAndReturnsTrue)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6810);
    ASSERT_NE(actor, nullptr);

    ScopedTestEngine engineGuard;
    GameEngine& engine = EngineContext::get().engine();
    auto controllerState = std::make_shared<ControllerGameState>();
    engine.pushGameState(controllerState);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, nullptr);

    EXPECT_TRUE(scr_EndModule(state, self));
    EXPECT_EQ(controllerState->endModuleInVictoryCalls, 1);
}

TEST_F(ScriptResidualFunctionsFixture, EndModuleIsSilentNoOpWhenCurrentStateDoesNotImplementPlayingController)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6811);
    ASSERT_NE(actor, nullptr);

    ScopedTestEngine engineGuard;
    GameEngine& engine = EngineContext::get().engine();
    auto plainState = std::make_shared<NonControllerGameState>();
    engine.pushGameState(plainState);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, nullptr);

    // dynamic_pointer_cast<IPlayingStateController> on a non-controller current state yields
    // nullptr, so pushModuleEndVictoryScreen()'s body is skipped entirely -- still a true return.
    EXPECT_TRUE(scr_EndModule(state, self));
}

TEST_F(ScriptResidualFunctionsFixture, EndModuleReturnsFalseForUnresolvedSelfAndDoesNotInvokeControllerEvenWhenOneIsActive)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6812);
    ASSERT_NE(actor, nullptr);

    ScopedTestEngine engineGuard;
    GameEngine& engine = EngineContext::get().engine();
    auto controllerState = std::make_shared<ControllerGameState>();
    engine.pushGameState(controllerState);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, nullptr);
    self.setSelf(ObjectRef::Invalid);

    // A live IPlayingStateController IS active -- if the resolveSelfContext guard were skipped,
    // this call would visibly increment endModuleInVictoryCalls (proven by the sibling test
    // above using the exact same controller setup). It does not.
    EXPECT_FALSE(scr_EndModule(state, self));
    EXPECT_EQ(controllerState->endModuleInVictoryCalls, 0);
}

//--------------------------------------------------------------------------------------------
// script_functions_commerce_passages.c: scr_FindTileInPassage
//--------------------------------------------------------------------------------------------

TEST_F(ScriptResidualFunctionsFixture, FindTileInPassageWritesTileCenterOnFirstRowMatchIgnoringUpperImgBits)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6813);
    ASSERT_NE(actor, nullptr);

    clearGridRegion(module, 20, 20, 25, 25, 0);
    setGridTileType(module, 22, 20, 9);

    // Pollute the upper byte of the matched tile's _img with an unrelated value (e.g. an
    // animation tile-set index) -- the lower-8-bit compare in ::FindTileInPassage must ignore it.
    const Index1D matchIndex = module.getMeshPointer()->getTileIndex(Ego::Vector2f(22 * 128.0f + 64.0f, 20 * 128.0f + 64.0f));
    ASSERT_NE(matchIndex, Index1D::Invalid);
    module.getMeshPointer()->getTileInfo(matchIndex)._img |= TILE_SET_UPPER_BITS(0x3);

    auto [passage, passageId] = addPassage(module, 20, 20, 24, 24);
    (void)passage;

    script_state_t state;
    state.x = 20 * 128;
    state.y = 20 * 128;
    state.distance = 9;
    state.argument = passageId;
    ai_state_t self = makeScriptSelf(actor, nullptr);

    EXPECT_TRUE(scr_FindTileInPassage(state, self));
    EXPECT_EQ(state.x, 22 * 128 + 64);
    EXPECT_EQ(state.y, 20 * 128 + 64);
}

TEST_F(ScriptResidualFunctionsFixture, FindTileInPassageLeavesRegistersUnchangedOnFailureContradictingItsOwnDocstring)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6814);
    ASSERT_NE(actor, nullptr);

    clearGridRegion(module, 20, 20, 25, 25, 0);
    auto [passage, passageId] = addPassage(module, 20, 20, 24, 24);
    (void)passage;

    script_state_t state;
    state.x = 99;
    state.y = 88;
    state.distance = 250; // never planted anywhere in the cleared region
    state.argument = passageId;
    ai_state_t self = makeScriptSelf(actor, nullptr);

    // Docstring claims "both [x and y] will be set to 0 if no tile is found" -- pinned here as
    // false: on failure the registers are left completely untouched, never zeroed.
    EXPECT_FALSE(scr_FindTileInPassage(state, self));
    EXPECT_EQ(state.x, 99);
    EXPECT_EQ(state.y, 88);
}

TEST_F(ScriptResidualFunctionsFixture, FindTileInPassageReturnsFalseForUnresolvedSelfWithoutScanningEvenWhenAMatchExists)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6815);
    ASSERT_NE(actor, nullptr);

    clearGridRegion(module, 20, 20, 25, 25, 0);
    setGridTileType(module, 22, 20, 9);
    auto [passage, passageId] = addPassage(module, 20, 20, 24, 24);
    (void)passage;

    script_state_t state;
    state.x = 20 * 128;
    state.y = 20 * 128;
    state.distance = 9; // would match the planted tile if the guard were skipped
    state.argument = passageId;
    ai_state_t self = makeScriptSelf(actor, nullptr);
    self.setSelf(ObjectRef::Invalid);

    EXPECT_FALSE(scr_FindTileInPassage(state, self));
    EXPECT_EQ(state.x, 20 * 128);
    EXPECT_EQ(state.y, 20 * 128);
}

TEST_F(ScriptResidualFunctionsFixture, FindTileInPassageSearchesOneRowAndColumnBeyondTheNominalPassageSpan)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6816);
    ASSERT_NE(actor, nullptr);

    clearGridRegion(module, 20, 20, 25, 25, 0);
    // Passage nominally spans (20,20)-(24,24); plant the match one row/column past that -- the
    // AABB used for the scan is built from (x1+1, y1+1), so the overscan cell IS reachable.
    setGridTileType(module, 25, 25, 9);
    auto [passage, passageId] = addPassage(module, 20, 20, 24, 24);
    (void)passage;

    script_state_t state;
    state.x = 20 * 128;
    state.y = 20 * 128;
    state.distance = 9;
    state.argument = passageId;
    ai_state_t self = makeScriptSelf(actor, nullptr);

    EXPECT_TRUE(scr_FindTileInPassage(state, self));
    EXPECT_EQ(state.x, 25 * 128 + 64);
    EXPECT_EQ(state.y, 25 * 128 + 64);
}

TEST_F(ScriptResidualFunctionsFixture, FindTileInPassageRefindsTheSameTileWhenReCalledWithTheReturnedCenterCoordinates)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6817);
    ASSERT_NE(actor, nullptr);

    clearGridRegion(module, 20, 20, 25, 25, 0);
    setGridTileType(module, 22, 20, 9);
    auto [passage, passageId] = addPassage(module, 20, 20, 24, 24);
    (void)passage;

    script_state_t state;
    state.x = 20 * 128;
    state.y = 20 * 128;
    state.distance = 9;
    state.argument = passageId;
    ai_state_t self = makeScriptSelf(actor, nullptr);

    ASSERT_TRUE(scr_FindTileInPassage(state, self));
    const int foundX = state.x;
    const int foundY = state.y;

    // "Call multiple times" iteration is entirely caller-driven: feeding the just-returned
    // center coordinates straight back in re-finds the SAME tile rather than advancing.
    EXPECT_TRUE(scr_FindTileInPassage(state, self));
    EXPECT_EQ(state.x, foundX);
    EXPECT_EQ(state.y, foundY);
}

TEST_F(ScriptResidualFunctionsFixture, FindTileInPassageIgnoresStateXOnNonFirstRows)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6818);
    ASSERT_NE(actor, nullptr);

    clearGridRegion(module, 20, 20, 25, 25, 0);
    // Planted on row 22 (not the first scanned row) at the leftmost column of the passage.
    setGridTileType(module, 20, 22, 9);
    auto [passage, passageId] = addPassage(module, 20, 20, 24, 24);
    (void)passage;

    script_state_t state;
    // A large starting x that would skip column 20 entirely if honored on every row -- it only
    // applies to the first scanned row (row 20), which contains no match, so the "remaining
    // rows" loop restarts x at the passage's own left edge for every subsequent row, including
    // row 22.
    state.x = 24 * 128;
    state.y = 20 * 128;
    state.distance = 9;
    state.argument = passageId;
    ai_state_t self = makeScriptSelf(actor, nullptr);

    EXPECT_TRUE(scr_FindTileInPassage(state, self));
    EXPECT_EQ(state.x, 20 * 128 + 64);
    EXPECT_EQ(state.y, 22 * 128 + 64);
}

//--------------------------------------------------------------------------------------------
// script_functions_action_audio.c: scr_SetVolumeNearestTeammate
//--------------------------------------------------------------------------------------------

TEST_F(ScriptResidualFunctionsFixture, SetVolumeNearestTeammateReturnsTrueForResolvedSelfAndTouchesNoRegisters)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6819);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    state.argument = -999;
    state.distance = 12345;
    state.x = 1;
    state.y = 2;
    state.turn = 3;
    ai_state_t self = makeScriptSelf(actor, nullptr);

    // Despite the docstring's "lets insects buzz correctly" / tmpargument=sound, tmpdistance=
    // distance framing, this is a documented no-current-runtime-implementation stub: the guard
    // is the entire body. No audio system call is made, no registers are read or written.
    EXPECT_TRUE(scr_SetVolumeNearestTeammate(state, self));
    EXPECT_EQ(state.argument, -999);
    EXPECT_EQ(state.distance, 12345);
    EXPECT_EQ(state.x, 1);
    EXPECT_EQ(state.y, 2);
    EXPECT_EQ(state.turn, 3);
}

TEST_F(ScriptResidualFunctionsFixture, SetVolumeNearestTeammateReturnsFalseForUnresolvedSelf)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6820);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    state.argument = 7;
    state.distance = 42;
    ai_state_t self = makeScriptSelf(actor, nullptr);
    self.setSelf(ObjectRef::Invalid);

    // Honest disclosure: this function has NO downstream behavior at all, even on the resolved
    // path (see the sibling test above) -- it is an empty TODO stub beyond the guard. There is
    // therefore no operand/world setup that could make "guard skipped" observably different from
    // "guard passed" other than the boolean return itself; the guard is architecturally
    // indistinguishable from a downstream failure because there is no downstream to fail. This
    // test pins exactly that: the return-value flip, nothing more.
    EXPECT_FALSE(scr_SetVolumeNearestTeammate(state, self));
    EXPECT_EQ(state.argument, 7);
    EXPECT_EQ(state.distance, 42);
}

//--------------------------------------------------------------------------------------------
// script_functions_teams.c: scr_IfLeaderKilled
//--------------------------------------------------------------------------------------------

TEST_F(ScriptResidualFunctionsFixture, IfLeaderKilledReturnsTrueWhenBitSetAndDoesNotClearItOnRepeatedCalls)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6821);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, nullptr);
    self.alert = ALERTIF_LEADERKILLED;

    // Non-consuming: unlike many alert-driven predicates, this one never clears the bit --
    // repeated calls keep returning true.
    EXPECT_TRUE(scr_IfLeaderKilled(state, self));
    EXPECT_TRUE(HAS_SOME_BITS(self.alert, ALERTIF_LEADERKILLED));
    EXPECT_TRUE(scr_IfLeaderKilled(state, self));
}

TEST_F(ScriptResidualFunctionsFixture, IfLeaderKilledReturnsFalseWhenBitClearOrOnlyUnrelatedBitsSet)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6822);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, nullptr);

    self.alert = 0;
    EXPECT_FALSE(scr_IfLeaderKilled(state, self));

    // Neighbouring bits alone (12 and 14) must not trip the exact 1<<13 mask.
    self.alert = ALERTIF_REAFFIRMED; // bit 12
    EXPECT_FALSE(scr_IfLeaderKilled(state, self));

    self.alert = (1u << 14);
    EXPECT_FALSE(scr_IfLeaderKilled(state, self));
}

TEST_F(ScriptResidualFunctionsFixture, IfLeaderKilledIsPureAlertTestIndependentOfActualTeamLeaderLiveness)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6823);
    ASSERT_NE(actor, nullptr);

    constexpr TEAM_REF goodTeam = static_cast<TEAM_REF>(Team::TEAM_GOOD);
    actor->setTeam(goodTeam);
    Team& team = module.getTeamList()[static_cast<size_t>(goodTeam)];

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, nullptr);

    // Team leader is alive (actor is its own leader) but the alert bit is set anyway -- the
    // function does not consult team state at all, only the alert bit.
    team.setLeaderRef(actor->getObjRef());
    self.alert = ALERTIF_LEADERKILLED;
    EXPECT_TRUE(scr_IfLeaderKilled(state, self));

    // Conversely, no leader at all (Invalid) with the bit clear still reads false.
    team.setLeaderRef(ObjectRef::Invalid);
    self.alert = 0;
    EXPECT_FALSE(scr_IfLeaderKilled(state, self));
}

TEST_F(ScriptResidualFunctionsFixture, IfLeaderKilledReturnsFalseForUnresolvedSelfEvenWhenAlertBitIsSet)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6824);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, nullptr);
    self.alert = ALERTIF_LEADERKILLED; // would satisfy the predicate outright if the guard were skipped
    self.setSelf(ObjectRef::Invalid);

    EXPECT_FALSE(scr_IfLeaderKilled(state, self));
}

} // namespace
