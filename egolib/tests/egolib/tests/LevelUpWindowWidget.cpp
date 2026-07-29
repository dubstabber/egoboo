//********************************************************************************************
//*
//*    This file is part of Egoboo.
//*
//*    Egoboo is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Egoboo is distributed in the hope that it will be useful, but
//*    WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Egoboo.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************

/// @file LevelUpWindowWidget.cpp
/// @brief Characterization coverage for Ego::GUI::LevelUpWindow's construction SHELL (Pass 344).
///
/// This file deliberately does NOT re-test the level-up computation itself (attribute RNG draws,
/// perk grant, alerts, size growth): that is already fully characterized in CharacterLevelUp.cpp
/// against the extracted Ego::applyCharacterLevelUp. What's new here is everything LevelUpWindow
/// itself still owns after that extraction: the constructor's perk-OFFER draw (a DIFFERENT RNG
/// consumer than applyCharacterLevelUp -- it runs at window-open time, not at level-up time), the
/// resulting GUI shell, and the setHoverPerk() description/formatting state machine.
///
/// GENUINE COVERAGE BOUNDARY (not a mechanical shortcut): PerkButton -- the component that
/// actually renders one perk offer and reports which perk it holds -- is a class defined ENTIRELY
/// inside LevelUpWindow.cpp (only forward-declared in the header). Test code outside that
/// translation unit cannot name the type, so it cannot dynamic_pointer_cast to it, read its
/// `_perk` member, or drive its notifyMousePointerMoved/notifyMouseButtonPressed handlers
/// directly. Concretely, this means: (a) the identity of which perk each of the 3 offered
/// PerkButtons holds is not independently observable from outside -- only inferable from the
/// production algorithm together with the fixture's follower.obj having zero valid perks (a
/// homogeneous 5x-TOUGHNESS offer pool where EVERY draw lands on TOUGHNESS by construction,
/// regardless of RNG value); and (b) the actual hover/click event path through PerkButton is not
/// exercised. What IS characterized instead: the exact RNG draw sequence & count the constructor
/// consumes (proven via a replay oracle, mirroring CharacterLevelUp.cpp's style, WITHOUT
/// hardcoding drawn values), the resulting component shape (including the duplicate-add quirk
/// noted below), and the setHoverPerk() state machine driven directly via the private-access
/// idiom (the same formatting PerkButton::notifyMousePointerMoved would trigger, just invoked
/// without going through that opaque type).
///
/// DUPLICATE-ADD QUIRK: _descriptionLabel and _perkIncreaseLabel are each addComponent()'d TWICE
/// during construction (LevelUpWindow.cpp:209/237 and :211/239). Container::addComponent never
/// deduplicates, so both appear twice in the child list; this is pinned as-is below, not treated
/// as a bug to route around.
///
/// UNINITIALIZED-READ CAVEAT: `_desciptionLabelOffset` is not in the constructor's member
/// initializer list and is first READ (via the first setHoverPerk() call, LevelUpWindow.cpp:214)
/// before its first WRITE (inside the per-button loop, :230). This is undefined behavior in the
/// strict sense, though harmless in practice: the second setHoverPerk() call (:242) recomputes
/// both label positions from the now-initialized value, and only the POST-construction label
/// geometry is asserted here -- never the intermediate state.
///
/// DRAW PATHS REMAIN OUT OF SCOPE: LevelUpWindow::drawContainer owns the entire fade-in/shrink/
/// slide animation state machine for doLevelUp()'s results, and reaches DeferredTexture::get()
/// like every other widget's draw() path in this suite (TextureManager deadlock hazard with no
/// GL context). Nothing here calls draw()/drawContainer()/drawAll().

#include "gtest/gtest.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Logic/Attribute.hpp"
#include "egolib/Logic/IPerkHandler.hpp"
#include "egolib/Math/Random.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/InputControl/InputDevice.hpp"
#include "egolib/vfs.h"

#define protected public
#define private public
#include "egolib/game/GUI/LevelUpWindow.hpp"
#undef private
#undef protected

#include "egolib/game/GUI/Label.hpp"
#include "HeadlessUIManager.hpp"

namespace {

using Ego::Colour4f;
using Ego::GUI::Label;
using Ego::GUI::LevelUpWindow;

/// @brief Mirrors LevelUpWindow.cpp's inline ordinal-suffix switch (1st/2nd/3rd/Nth) exactly, so
///        the classLevelLabel text assertion below pins the ALGORITHM rather than hardcoding
///        follower.obj's specific starting level.
std::string ordinalSuffix(int level) {
    switch (level) {
        case 1: return "st";
        case 2: return "nd";
        case 3: return "rd";
        default: return "th";
    }
}

/// @brief Colour4f has no operator==; compare component-wise instead.
::testing::AssertionResult ColoursEqual(const Colour4f& actual, const Colour4f& expected) {
    if (actual.get_r() == expected.get_r() && actual.get_g() == expected.get_g()
        && actual.get_b() == expected.get_b() && actual.get_a() == expected.get_a()) {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure()
        << "actual (" << actual.get_r() << ", " << actual.get_g() << ", " << actual.get_b() << ", " << actual.get_a()
        << ") != expected (" << expected.get_r() << ", " << expected.get_g() << ", " << expected.get_b() << ", "
        << expected.get_a() << ")";
}

class LevelUpWindowWidgetFixture : public ::testing::Test {
protected:
    static std::unique_ptr<ContentRuntimeBootstrap> s_runtime;

    static void SetUpTestSuite() {
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
        opts.randomSeed = 87;
        opts.binaryPath = "";
        opts.logPath = "/debug/level-up-window-widget-tests.log";
        opts.logLevel = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);

        setenv("EGOBOO_DISABLE_AUDIO", "1", 1);
        AudioSystem::initialize(EngineContext::get().config(), EngineContext::get().logTarget());
        EngineContext::get().installAudioSystem(AudioSystem::get());
        ParticleHandler::initialize();
        EngineContext::get().installParticleHandler(ParticleHandler::get());
    }

    static void TearDownTestSuite() {
        EngineContext::get().clearParticleHandler();
        ParticleHandler::uninitialize();
        EngineContext::get().clearAudioSystem();
        AudioSystem::uninitialize();
        s_runtime.reset();
    }

    void SetUp() override {
        auto& session = GameSessionContext::get();
        if (session.hasActiveModule()) {
            session.quitModule();
        }

        EngineContext::get().profileSystem().reset();
        EngineContext::get().profileSystem().loadModuleProfiles();
        setup_init_module_vfs_paths("mp_modules/test.mod");
    }

    void TearDown() override {
        auto& session = GameSessionContext::get();
        if (session.hasActiveModule()) {
            session.quitModule();
        }

        setup_clear_module_vfs_paths();
    }

    std::shared_ptr<ModuleProfile> findTestModule() const {
        for (const auto& module : EngineContext::get().profileSystem().getModuleProfiles()) {
            if (module && module->getFolderName() == "test.mod") {
                return module;
            }
        }
        return nullptr;
    }

    GameModule& beginActiveTestModule() {
        auto module = findTestModule();
        EXPECT_NE(module, nullptr);
        if (module == nullptr) {
            throw std::runtime_error("test.mod profile not found");
        }

        auto& session = GameSessionContext::get();
        const bool began = session.beginModule(module, 87);
        EXPECT_TRUE(began);
        return session.activeModule();
    }

    /// Spawns a follower.obj: its (content-wide, [POOL]-less) empty valid-perk pool is what makes
    /// the constructor's perk-offer pool homogeneous (5x TOUGHNESS after padding), which is what
    /// lets this file characterize the offer WITHOUT being able to name PerkButton (see header).
    std::shared_ptr<Object> spawnFollower(GameModule& module, int slot) const {
        const ObjectProfileRef profile = EngineContext::get().profileSystem().loadOneProfile("mp_objects/follower.obj", slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid) {
            return nullptr;
        }

        const ObjectRef objectRef = module.spawnObjectRef(Ego::Vector3f(64.0f, 64.0f, 0.0f), profile,
                                                           static_cast<TEAM_REF>(Team::TEAM_NULL), 0, Facing(0),
                                                           "", ObjectRef::Invalid);
        return module.getObjectHandler().getHandle(objectRef);
    }
};

std::unique_ptr<ContentRuntimeBootstrap> LevelUpWindowWidgetFixture::s_runtime;

TEST_F(LevelUpWindowWidgetFixture, ConstructsHeadlesslyCenteredOnScreenWithTheOrdinalLevelClassLabel) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    auto& module = beginActiveTestModule();
    auto character = spawnFollower(module, 7201);
    ASSERT_NE(character, nullptr);
    ASSERT_TRUE(character->getValidPerks().empty());  // precondition: homogeneous offer pool

    auto window = std::make_shared<LevelUpWindow>(character->getObjRef());
    ASSERT_FALSE(window->isDestroyed());

    // Fixed size (510, 340), centered on the 640x480 stub screen: position = (320,240) - (255,170).
    EXPECT_FLOAT_EQ(window->getWidth(), 510.0f);
    EXPECT_FLOAT_EQ(window->getHeight(), 340.0f);
    EXPECT_FLOAT_EQ(window->getX(), 65.0f);
    EXPECT_FLOAT_EQ(window->getY(), 70.0f);

    // classLevelLabel is the second top-level child (after the character icon Image); its exact
    // text is a content-independent oracle over the character's actual name/level/gender/class.
    auto children = std::vector<std::shared_ptr<Ego::GUI::Component>>();
    {
        auto it = window->iterator();
        children.assign(it.begin(), it.end());
    }
    ASSERT_GE(children.size(), 2u);
    auto classLevelLabel = std::dynamic_pointer_cast<Label>(children[1]);
    ASSERT_NE(classLevelLabel, nullptr);

    std::ostringstream expected;
    expected << character->getName() << " is now a ";
    const int levelOrdinal = character->getExperienceLevel() + 1;
    expected << levelOrdinal << ordinalSuffix(levelOrdinal) << " level ";
    if (character->getGender() == Gender::Male) {
        expected << "male ";
    } else if (character->getGender() == Gender::Female) {
        expected << "female ";
    }
    expected << character->getProfile()->getClassName() << '!';
    EXPECT_EQ(classLevelLabel->getText(), expected.str());
}

TEST_F(LevelUpWindowWidgetFixture, ConstructionDestroysImmediatelyForAnInvalidCharacterReferenceWithNoComponentsAdded) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    beginActiveTestModule();

    // Unlike CharacterWindow (which adds its 3 Tabs before the invalid-character gate),
    // LevelUpWindow's gate (LevelUpWindow.cpp:145-149) runs before ANY addComponent() call.
    auto window = std::make_shared<LevelUpWindow>(ObjectRef::Invalid);
    EXPECT_TRUE(window->isDestroyed());
    EXPECT_EQ(window->getComponentCount(), 0u);
}

TEST_F(LevelUpWindowWidgetFixture, PerkOfferConsumesExactlyThreeRandomDrawsOverTheSeededPoolAndYieldsTenComponents) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    auto& module = beginActiveTestModule();
    auto character = spawnFollower(module, 7202);
    ASSERT_NE(character, nullptr);
    ASSERT_FALSE(character->hasPerk(Ego::Perks::JACK_OF_ALL_TRADES));  // NR_OF_PERKS == 3 branch

    const uint32_t seed = character->getLevelUpSeed();

    auto window = std::make_shared<LevelUpWindow>(character->getObjRef());
    ASSERT_FALSE(window->isDestroyed());

    // Immediately capture a marker draw from wherever the constructor left the shared generator.
    const uint32_t actualMarker = Random::next<uint32_t>(std::numeric_limits<uint32_t>::max());

    // Oracle: replay the constructor's exact draw sequence from the SAME captured seed. The pool
    // starts at 5 entries (character->getValidPerks() is empty, padded to 5x TOUGHNESS), and each
    // of the 3 iterations draws Random::next(poolSize - 1) then erases one entry: sizes 5,4,3 ->
    // draws next(4), next(3), next(2). If the real constructor drew a different COUNT or RANGE of
    // values, this marker draw -- immediately following on the same shared generator -- would
    // desync and very likely disagree.
    Random::setSeed(seed);
    Random::next<size_t>(4);
    Random::next<size_t>(3);
    Random::next<size_t>(2);
    const uint32_t oracleMarker = Random::next<uint32_t>(std::numeric_limits<uint32_t>::max());

    EXPECT_EQ(actualMarker, oracleMarker);

    // 10 top-level components for a non-JACK_OF_ALL_TRADES character: icon, classLevelLabel,
    // selectPerkLabel, _descriptionLabel (added twice), _perkIncreaseLabel (added twice), and
    // exactly 3 PerkButtons. See the DUPLICATE-ADD QUIRK note in this file's header.
    EXPECT_EQ(window->getComponentCount(), 10u);
}

TEST_F(LevelUpWindowWidgetFixture, PerkOfferConsumesExactlyFiveRandomDrawsForAJackOfAllTradesCharacterAndYieldsTwelveComponents) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    auto& module = beginActiveTestModule();
    auto character = spawnFollower(module, 7204);
    ASSERT_NE(character, nullptr);
    ASSERT_TRUE(character->getValidPerks().empty());  // still homogeneous: JACK_OF_ALL_TRADES
                                                        // isn't itself a [POOL] entry, so it does
                                                        // not appear in getValidPerks()

    // Granting JACK_OF_ALL_TRADES (LevelUpWindow.cpp:220) is what selects the 5-perk-offer
    // branch instead of the default 3-perk one exercised above.
    character->addPerk(Ego::Perks::JACK_OF_ALL_TRADES);
    ASSERT_TRUE(character->hasPerk(Ego::Perks::JACK_OF_ALL_TRADES));

    const uint32_t seed = character->getLevelUpSeed();

    auto window = std::make_shared<LevelUpWindow>(character->getObjRef());
    ASSERT_FALSE(window->isDestroyed());

    const uint32_t actualMarker = Random::next<uint32_t>(std::numeric_limits<uint32_t>::max());

    // Oracle: same replay style as the 3-perk branch above, but 5 iterations over the same
    // 5-entry padded pool: sizes 5,4,3,2,1 -> draws next(4), next(3), next(2), next(1), next(0).
    Random::setSeed(seed);
    Random::next<size_t>(4);
    Random::next<size_t>(3);
    Random::next<size_t>(2);
    Random::next<size_t>(1);
    Random::next<size_t>(0);
    const uint32_t oracleMarker = Random::next<uint32_t>(std::numeric_limits<uint32_t>::max());

    EXPECT_EQ(actualMarker, oracleMarker);

    // 12 top-level components: the same 7 fixed components as the 3-perk branch (icon,
    // classLevelLabel, selectPerkLabel, _descriptionLabel x2, _perkIncreaseLabel x2) plus 5
    // PerkButtons instead of 3.
    EXPECT_EQ(window->getComponentCount(), 12u);
}

TEST_F(LevelUpWindowWidgetFixture, SetHoverPerkStateMachineTogglesBetweenTheDefaultPromptAndAPerkSpecificDescription) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    auto& module = beginActiveTestModule();
    auto character = spawnFollower(module, 7203);
    ASSERT_NE(character, nullptr);

    auto window = std::make_shared<LevelUpWindow>(character->getObjRef());
    ASSERT_FALSE(window->isDestroyed());

    // Post-construction default state (both in-ctor setHoverPerk(NR_OF_PERKS) calls leave this).
    EXPECT_EQ(window->getCurrentPerk(), Ego::Perks::NR_OF_PERKS);
    EXPECT_EQ(window->_descriptionLabel->getText(), "Select your new perk...");
    EXPECT_EQ(window->_perkIncreaseLabel->getText(), "Hover mouse over a perk to see the benefits.");
    EXPECT_TRUE(ColoursEqual(window->_perkIncreaseLabel->getColour(), Colour4f::purple()));

    const Ego::Perks::Perk& toughness = Ego::Perks::activePerkHandler().getPerk(Ego::Perks::TOUGHNESS);
    window->setHoverPerk(Ego::Perks::TOUGHNESS);

    EXPECT_EQ(window->getCurrentPerk(), Ego::Perks::TOUGHNESS);
    EXPECT_EQ(window->_descriptionLabel->getText(), toughness.getDescription());
    EXPECT_EQ(window->_perkIncreaseLabel->getText(),
              toughness.getName() + "\n+1 " + Ego::Attribute::toString(toughness.getType()));
    EXPECT_TRUE(ColoursEqual(window->_perkIncreaseLabel->getColour(), toughness.getColour()));

    // Toggling back to "no perk" restores the default prompt.
    window->setHoverPerk(Ego::Perks::NR_OF_PERKS);
    EXPECT_EQ(window->getCurrentPerk(), Ego::Perks::NR_OF_PERKS);
    EXPECT_EQ(window->_descriptionLabel->getText(), "Select your new perk...");
    EXPECT_TRUE(ColoursEqual(window->_perkIncreaseLabel->getColour(), Colour4f::purple()));
}

} // namespace
