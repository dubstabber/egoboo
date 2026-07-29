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

/// @file CharacterWindowWidget.cpp
/// @brief Characterization coverage for Ego::GUI::CharacterWindow (Pass 344).
///
/// CharacterWindow constructs headlessly against a LIVE module/object world: unlike
/// ModuleSelector (fabricated ModuleProfile stand-ins are enough), CharacterWindow's title and
/// every tab body resolve the observed character through Ego::Entities::tryActiveObject, which
/// requires a real GameSessionContext module to be active. The fixture below is therefore the
/// CharacterLevelUp.cpp/CameraTracking.cpp ContentRuntimeBootstrap + GameSessionContext mold,
/// plus the Ego::Test::HeadlessUIManager seam (Pass 343) so the widget's GUI construction path
/// (Labels, Images, Buttons -- all DeferredTexture-backed and therefore lazy, never touching a
/// real texture/GL) runs to completion without a renderer.
///
/// DRAW PATHS REMAIN OUT OF SCOPE: CharacterWindow::drawContainer/draw/drawAll and every child
/// widget's draw() eventually reach DeferredTexture::get()/get_ptr(), which deadlocks
/// TextureManager's condition variable with no GL context (see CharacterStatusWidget.cpp and
/// GuiHeadlessUIManagerStub.cpp for the same hazard already documented elsewhere in this suite).
/// Nothing here calls any draw* method.
///
/// The level-up NUMBER-CRUNCHING itself (attribute RNG draws, perk grant, alerts) is already
/// fully characterized in CharacterLevelUp.cpp against the extracted Ego::applyCharacterLevelUp;
/// this file does not re-test that. It only exercises the GUI shell around it: the level-up
/// button's visibility rules and its role as a parent for a sibling LevelUpWindow.

#include "gtest/gtest.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Logic/Attribute.hpp"
#include "egolib/Logic/IPerkHandler.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/InputControl/InputDevice.hpp"
#include "egolib/vfs.h"

#define protected public
#define private public
#include "egolib/game/GUI/CharacterWindow.hpp"
#include "egolib/game/Logic/Player.hpp"
#undef private
#undef protected

#include "egolib/game/GUI/Button.hpp"
#include "egolib/game/GUI/Image.hpp"
#include "egolib/game/GUI/Label.hpp"
#include "egolib/game/GUI/ScrollableList.hpp"
#include "egolib/game/GUI/TabPanel.hpp"
#include "HeadlessUIManager.hpp"

namespace {

using Ego::Colour4f;
using Ego::GUI::Button;
using Ego::GUI::CharacterWindow;
using Ego::GUI::Component;
using Ego::GUI::Container;
using Ego::GUI::Image;
using Ego::GUI::Label;
using Ego::GUI::ScrollableList;
using Ego::GUI::Tab;

/// @brief Collects @a container's current top-level children in insertion order.
std::vector<std::shared_ptr<Component>> collectChildren(Container& container) {
    auto it = container.iterator();
    return std::vector<std::shared_ptr<Component>>(it.begin(), it.end());
}

std::shared_ptr<Button> findButtonWithText(Container& container, const std::string& text) {
    for (const auto& child : collectChildren(container)) {
        auto button = std::dynamic_pointer_cast<Button>(child);
        if (button && button->getText() == text) {
            return button;
        }
    }
    return nullptr;
}

std::set<std::string> collectLabelTexts(Container& container) {
    std::set<std::string> texts;
    for (const auto& child : collectChildren(container)) {
        auto label = std::dynamic_pointer_cast<Label>(child);
        if (label) {
            texts.insert(label->getText());
        }
    }
    return texts;
}

/// @brief Like collectLabelTexts, but keyed by text so callers can also inspect each label's
///        actual rendered colour (per-AttributeType texts are unique within one describeEnchant-
///        Effects() call, so this loses nothing collectLabelTexts's std::set already assumed).
std::unordered_map<std::string, std::shared_ptr<Label>> collectLabelsByText(Container& container) {
    std::unordered_map<std::string, std::shared_ptr<Label>> labels;
    for (const auto& child : collectChildren(container)) {
        auto label = std::dynamic_pointer_cast<Label>(child);
        if (label) {
            labels[label->getText()] = label;
        }
    }
    return labels;
}

/// @brief Colour4f has no operator==; compare component-wise instead (mirrors
///        LevelUpWindowWidget.cpp's identical helper).
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

/// @brief Mirrors CharacterWindow::describeEnchantEffects's algorithm exactly (per-AttributeType
///        accumulation across a bucket of enchants, the near-zero epsilon skip, the
///        unhandled_switch_case_error swallow for attributes Attribute::toString cannot name, and
///        the precise ostringstream format) so the test pins the ALGORITHM rather than
///        hardcoding shipped enchant.txt numbers -- the same oracle-replay style
///        CharacterLevelUp.cpp uses for the RNG-driven level-up computation.
std::set<std::string> describeEnchantEffectsOracle(const std::vector<std::shared_ptr<Ego::Enchantment>>& enchantments) {
    std::unordered_map<Ego::Attribute::AttributeType, float> effects;
    for (const auto& enchant : enchantments) {
        for (const Ego::EnchantModifier& modifier : enchant->getModifiers()) {
            effects[modifier._type] += modifier._value;
        }
    }

    std::set<std::string> labels;
    for (const auto& element : effects) {
        if (std::abs(element.second) < std::numeric_limits<float>::epsilon()) {
            continue;
        }
        try {
            std::ostringstream out;
            out << Ego::Attribute::toString(element.first) << std::setprecision(2) << ": " << element.second;
            labels.insert(out.str());
        } catch (idlib::unhandled_switch_case_error&) {
            continue;
        }
    }
    return labels;
}

/// @brief Companion to describeEnchantEffectsOracle: mirrors the SAME accumulation algorithm but
///        yields the label-colouring rule (CharacterWindow.cpp:565) instead of discarding it --
///        green() for a net-positive accumulated effect, red() for net-negative -- keyed by the
///        same label text describeEnchantEffectsOracle produces, so a caller can cross-reference
///        collectLabelsByText()'s actual Label objects against this without hardcoding which
///        attributes end up positive or negative for any particular enchant.txt content.
std::unordered_map<std::string, Colour4f> describeEnchantEffectsColourOracle(
    const std::vector<std::shared_ptr<Ego::Enchantment>>& enchantments) {
    std::unordered_map<Ego::Attribute::AttributeType, float> effects;
    for (const auto& enchant : enchantments) {
        for (const Ego::EnchantModifier& modifier : enchant->getModifiers()) {
            effects[modifier._type] += modifier._value;
        }
    }

    std::unordered_map<std::string, Colour4f> colours;
    for (const auto& element : effects) {
        if (std::abs(element.second) < std::numeric_limits<float>::epsilon()) {
            continue;
        }
        try {
            std::ostringstream out;
            out << Ego::Attribute::toString(element.first) << std::setprecision(2) << ": " << element.second;
            colours[out.str()] = element.second > 0 ? Colour4f::green() : Colour4f::red();
        } catch (idlib::unhandled_switch_case_error&) {
            continue;
        }
    }
    return colours;
}

class CharacterWindowWidgetFixture : public ::testing::Test {
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
        opts.randomSeed = 86;
        opts.binaryPath = "";
        opts.logPath = "/debug/character-window-widget-tests.log";
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
        const bool began = session.beginModule(module, 86);
        EXPECT_TRUE(began);
        return session.activeModule();
    }

    std::shared_ptr<Object> spawnObject(GameModule& module, const std::string& profilePath, int slot) const {
        const ObjectProfileRef profile = EngineContext::get().profileSystem().loadOneProfile(profilePath, slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid) {
            return nullptr;
        }

        const ObjectRef objectRef = module.spawnObjectRef(Ego::Vector3f(64.0f, 64.0f, 0.0f), profile,
                                                           static_cast<TEAM_REF>(Team::TEAM_NULL), 0, Facing(0),
                                                           "", ObjectRef::Invalid);
        return module.getObjectHandler().getHandle(objectRef);
    }

    /// Spawns a non-player follower.obj (avoids the CharacterWindow constructor's player-only
    /// side effects: inventory mode flip and the LEVEL UP button).
    std::shared_ptr<Object> spawnFollower(GameModule& module, int slot) const {
        return spawnObject(module, "mp_objects/follower.obj", slot);
    }

    /// Spawns follower.obj at @a slot and registers it as a player (mirrors CharacterLevelUp.cpp).
    std::shared_ptr<Object> spawnFollowerAsPlayer(GameModule& module, int slot) const {
        auto object = spawnObject(module, "mp_objects/follower.obj", slot);
        if (!object) {
            return nullptr;
        }

        const bool registered = module.addPlayer(object->getObjRef(), Ego::Input::InputDevice::DeviceList[0]);
        EXPECT_TRUE(registered);
        return object;
    }

    /// Loads @a objectPath as an enchant spawner and attaches @a enchantPath's enchant profile to
    /// @a target. Mirrors ScriptSystemsFunctions.cpp's addHealRemovableEnchant, generalized to a
    /// caller-chosen object/enchant pair so this file can pick specific [NAME]-bucketed profiles.
    std::shared_ptr<Ego::Enchantment> addEnchant(GameModule& module, const std::shared_ptr<Object>& target,
                                                  const std::string& objectPath, const std::string& enchantPath,
                                                  int slot) const {
        auto source = spawnObject(module, objectPath, slot);
        EXPECT_NE(source, nullptr);
        if (!source) {
            return nullptr;
        }

        const auto enchantRef = EngineContext::get().profileSystem().loadEnchantProfile(enchantPath, INVALID_EVE_REF);
        auto enchant = target->addEnchant(enchantRef, source->getProfileID().get(), source->getObjRef(), source->getObjRef());
        EXPECT_NE(enchant, nullptr);
        return enchant;
    }
};

std::unique_ptr<ContentRuntimeBootstrap> CharacterWindowWidgetFixture::s_runtime;

TEST_F(CharacterWindowWidgetFixture, ConstructsHeadlesslyWithTheCharacterTabActiveByDefault) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    auto& module = beginActiveTestModule();
    auto character = spawnFollower(module, 7101);
    ASSERT_NE(character, nullptr);

    auto window = std::make_shared<CharacterWindow>(character->getObjRef());
    ASSERT_FALSE(window->isDestroyed());

    // Title resolution (resolveCharacterWindowTitle, CharacterWindow.cpp:31-39): a resolvable
    // character's title is the character's own display name, further upper-cased wholesale by
    // InternalWindow::TitleBar's constructor -- not hardcoded here, but replayed via the same
    // public getName() call the production code itself makes.
    std::string expectedTitle = character->getName();
    std::transform(expectedTitle.begin(), expectedTitle.end(), expectedTitle.begin(), ::toupper);
    EXPECT_EQ(window->_titleBar->_title, expectedTitle);

    // Top level: 3 Tabs + 3 tab-switch Buttons (buildXTab() calls populate INSIDE each Tab, not
    // directly into the window).
    EXPECT_EQ(window->getComponentCount(), 6u);

    EXPECT_TRUE(window->_characterStatisticsTab->isEnabled());
    EXPECT_TRUE(window->_characterStatisticsTab->isVisible());
    EXPECT_FALSE(window->_knownPerksTab->isEnabled());
    EXPECT_FALSE(window->_knownPerksTab->isVisible());
    EXPECT_FALSE(window->_activeEnchantsTab->isEnabled());
    EXPECT_FALSE(window->_activeEnchantsTab->isVisible());

    // Clicking the "Perks" tab-switch button flips exclusivity to the perks tab.
    auto perksTabButton = findButtonWithText(*window, window->_knownPerksTab->getTitle());
    ASSERT_NE(perksTabButton, nullptr);
    perksTabButton->doClick();
    EXPECT_FALSE(window->_characterStatisticsTab->isVisible());
    EXPECT_TRUE(window->_knownPerksTab->isVisible());
    EXPECT_FALSE(window->_activeEnchantsTab->isVisible());

    // Clicking the "Enchants" tab-switch button flips exclusivity to the enchants tab (the third,
    // previously-unexercised onClick lambda, CharacterWindow.cpp:156-163).
    auto enchantsTabButton = findButtonWithText(*window, window->_activeEnchantsTab->getTitle());
    ASSERT_NE(enchantsTabButton, nullptr);
    enchantsTabButton->doClick();
    EXPECT_FALSE(window->_characterStatisticsTab->isVisible());
    EXPECT_FALSE(window->_knownPerksTab->isVisible());
    EXPECT_TRUE(window->_activeEnchantsTab->isVisible());
    EXPECT_FALSE(window->_characterStatisticsTab->isEnabled());
    EXPECT_FALSE(window->_knownPerksTab->isEnabled());
    EXPECT_TRUE(window->_activeEnchantsTab->isEnabled());

    // Clicking "Character" again (CharacterWindow.cpp:126-133) returns exclusivity to the
    // character tab, exercising the return transition from a non-default tab.
    auto characterTabButton = findButtonWithText(*window, window->_characterStatisticsTab->getTitle());
    ASSERT_NE(characterTabButton, nullptr);
    characterTabButton->doClick();
    EXPECT_TRUE(window->_characterStatisticsTab->isVisible());
    EXPECT_FALSE(window->_knownPerksTab->isVisible());
    EXPECT_FALSE(window->_activeEnchantsTab->isVisible());
}

TEST_F(CharacterWindowWidgetFixture, ConstructionDestroysImmediatelyForAnInvalidCharacterReference) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    beginActiveTestModule();

    // No object has been spawned at all: Ego::Entities::tryActiveObject(ObjectRef::Invalid)
    // resolves to nullptr, so the constructor's invalid-character gate fires before any tab body
    // is built. The 3 Tab components already added earlier in the constructor remain (destroy()
    // does not clear them), but nothing else does.
    auto window = std::make_shared<CharacterWindow>(ObjectRef::Invalid);
    EXPECT_TRUE(window->isDestroyed());
    EXPECT_EQ(window->getComponentCount(), 3u);

    // Title resolution's unresolvable-reference fallback (resolveCharacterWindowTitle,
    // CharacterWindow.cpp:31-39): the literal "Character" fallback, upper-cased wholesale by
    // InternalWindow::TitleBar's constructor exactly as the resolvable-character path is.
    EXPECT_EQ(window->_titleBar->_title, "CHARACTER");
}

TEST_F(CharacterWindowWidgetFixture, CharacterStatisticTabBuildsAllExpectedLabelsAndExactlySixInventorySlots) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    auto& module = beginActiveTestModule();
    auto character = spawnFollower(module, 7102);
    ASSERT_NE(character, nullptr);

    auto window = std::make_shared<CharacterWindow>(character->getObjRef());
    ASSERT_FALSE(window->isDestroyed());

    // icon(1) + classLevelLabel(1) + "ATTRIBUTES" header(1) + 8 attribute label/value pairs(16) +
    // "DEFENCES" header(1) + 8 resistance label/value/percent triples(24) + 6 InventorySlots(6).
    // No LEVEL UP button: this is a non-player character.
    const size_t expected = 1 + 1 + 1 + 2 * Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES + 1 + 3 * DAMAGE_COUNT + 6;
    EXPECT_EQ(window->_characterStatisticsTab->getComponentCount(), expected);

    size_t imageCount = 0;
    size_t inventorySlotCount = 0;
    for (const auto& child : collectChildren(*window->_characterStatisticsTab)) {
        if (std::dynamic_pointer_cast<Image>(child)) {
            ++imageCount;
        }
        // InventorySlot is a Component but not a Button/Label/Image; identify it by exclusion
        // against the three named types above plus Button (there are no Buttons in this tab for
        // a non-player character).
        if (!std::dynamic_pointer_cast<Image>(child) && !std::dynamic_pointer_cast<Label>(child)
            && !std::dynamic_pointer_cast<Button>(child)) {
            ++inventorySlotCount;
        }
    }
    EXPECT_EQ(imageCount, 1u);                      // the character's profile icon
    EXPECT_EQ(inventorySlotCount, character->getInventoryMaxItems());
    EXPECT_EQ(character->getInventoryMaxItems(), 6u);  // Inventory::MAXNUMINPACK

    // The class/level label mentions the character's class name (exact wording -- level ordinal,
    // gender, "Dead " prefix -- is covered by the LevelUpWindow ordinal-suffix pin instead).
    auto labelTexts = collectLabelTexts(*window->_characterStatisticsTab);
    const bool foundClassName = std::any_of(labelTexts.begin(), labelTexts.end(), [&](const std::string& text) {
        return text.find(character->getProfile()->getClassName()) != std::string::npos;
    });
    EXPECT_TRUE(foundClassName);
}

TEST_F(CharacterWindowWidgetFixture, KnownPerksTabListsOnlyLearnedPerksAndClickRevealsTheirDescription) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    auto& module = beginActiveTestModule();
    auto character = spawnFollower(module, 7103);
    ASSERT_NE(character, nullptr);
    ASSERT_FALSE(character->hasPerk(Ego::Perks::TOUGHNESS));

    character->addPerk(Ego::Perks::TOUGHNESS);
    ASSERT_TRUE(character->hasPerk(Ego::Perks::TOUGHNESS));

    auto window = std::make_shared<CharacterWindow>(character->getObjRef());
    ASSERT_FALSE(window->isDestroyed());

    auto tabChildren = collectChildren(*window->_knownPerksTab);
    ASSERT_EQ(tabChildren.size(), 4u);
    auto perksKnownList = std::dynamic_pointer_cast<ScrollableList>(tabChildren[0]);
    auto perkIcon = std::dynamic_pointer_cast<Image>(tabChildren[1]);
    auto newPerkLabel = std::dynamic_pointer_cast<Label>(tabChildren[2]);
    auto perkDescriptionLabel = std::dynamic_pointer_cast<Label>(tabChildren[3]);
    ASSERT_NE(perksKnownList, nullptr);
    ASSERT_NE(perkIcon, nullptr);
    ASSERT_NE(newPerkLabel, nullptr);
    ASSERT_NE(perkDescriptionLabel, nullptr);

    EXPECT_FALSE(perkIcon->isVisible());
    EXPECT_EQ(newPerkLabel->getText(), "No Perk Selected");
    EXPECT_EQ(perkDescriptionLabel->getText(), "Select a perk to view details...");

    const Ego::Perks::Perk& perk = Ego::Perks::activePerkHandler().getPerk(Ego::Perks::TOUGHNESS);

    // ScrollableList's own ctor adds a "+"/"-" pair of scroll Buttons before any perk IconButton;
    // exactly one perk was granted, so exactly one further Button (the IconButton) is present.
    ASSERT_EQ(perksKnownList->getComponentCount(), 3u);
    auto perkButton = findButtonWithText(*perksKnownList, perk.getName());
    ASSERT_NE(perkButton, nullptr);

    perkButton->doClick();
    EXPECT_TRUE(perkIcon->isVisible());
    EXPECT_EQ(perkDescriptionLabel->getText(), perk.getDescription());
}

TEST_F(CharacterWindowWidgetFixture, ActiveEnchantsTabMergesEnchantsByNameWithCountPrefixAndUnderscoreReplacement) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    auto& module = beginActiveTestModule();
    auto character = spawnFollower(module, 7104);
    ASSERT_NE(character, nullptr);

    // stiletto.obj's enchant has no [NAME] tag -> bucketed as "Miscellaneous".
    auto miscEnchant = addEnchant(module, character,
                                   "mp_data/globalobjects/weapons/stiletto.obj",
                                   "mp_data/globalobjects/weapons/stiletto.obj/enchant.txt", 7110);
    // ppotion.obj and mushroom.obj both carry [NAME] Poison -> merged into one "x2 Poison" bucket.
    auto poisonEnchant1 = addEnchant(module, character,
                                      "mp_data/globalobjects/potions/ppotion.obj",
                                      "mp_data/globalobjects/potions/ppotion.obj/enchant.txt", 7111);
    auto poisonEnchant2 = addEnchant(module, character,
                                      "mp_data/globalobjects/items/mushroom.obj",
                                      "mp_data/globalobjects/items/mushroom.obj/enchant.txt", 7112);
    // dexamulet.obj carries [NAME] Magic_Item -> underscore replaced with a space for display.
    auto magicEnchant = addEnchant(module, character,
                                    "mp_data/globalobjects/magic_item/dexamulet.obj",
                                    "mp_data/globalobjects/magic_item/dexamulet.obj/enchant.txt", 7113);
    ASSERT_NE(miscEnchant, nullptr);
    ASSERT_NE(poisonEnchant1, nullptr);
    ASSERT_NE(poisonEnchant2, nullptr);
    ASSERT_NE(magicEnchant, nullptr);
    ASSERT_TRUE(character->hasActiveEnchants());

    auto window = std::make_shared<CharacterWindow>(character->getObjRef());
    ASSERT_FALSE(window->isDestroyed());

    auto tabChildren = collectChildren(*window->_activeEnchantsTab);
    ASSERT_EQ(tabChildren.size(), 3u);
    auto activeEnchantsList = std::dynamic_pointer_cast<ScrollableList>(tabChildren[0]);
    auto enchantNameLabel = std::dynamic_pointer_cast<Label>(tabChildren[1]);
    auto enchantEffectsList = std::dynamic_pointer_cast<ScrollableList>(tabChildren[2]);
    ASSERT_NE(activeEnchantsList, nullptr);
    ASSERT_NE(enchantNameLabel, nullptr);
    ASSERT_NE(enchantEffectsList, nullptr);

    // 3 distinct buckets: Miscellaneous (1 enchant, no count prefix), "x2 Poison" (2 enchants,
    // merged), "Magic Item" (1 enchant, underscore replaced, no count prefix).
    auto miscBucketButton = findButtonWithText(*activeEnchantsList, "Miscellaneous");
    auto poisonBucketButton = findButtonWithText(*activeEnchantsList, "x2 Poison");
    auto magicBucketButton = findButtonWithText(*activeEnchantsList, "Magic Item");
    ASSERT_NE(miscBucketButton, nullptr);
    ASSERT_NE(poisonBucketButton, nullptr);
    ASSERT_NE(magicBucketButton, nullptr);
    // ScrollableList's own "+"/"-" scroll buttons plus exactly these 3 bucket buttons.
    EXPECT_EQ(activeEnchantsList->getComponentCount(), 5u);

    poisonBucketButton->doClick();
    EXPECT_EQ(enchantNameLabel->getText(), "Poison");
    EXPECT_EQ(collectLabelTexts(*enchantEffectsList),
              describeEnchantEffectsOracle({poisonEnchant1, poisonEnchant2}));
    // Label colouring (CharacterWindow.cpp:565): green() for a net-positive accumulated effect,
    // red() for net-negative. Both ppotion.obj and mushroom.obj drain life each second, so the
    // merged Poison bucket's LIFE_REGEN entry is net-negative -> red, not hardcoded here.
    {
        auto labelsByText = collectLabelsByText(*enchantEffectsList);
        auto colourOracle = describeEnchantEffectsColourOracle({poisonEnchant1, poisonEnchant2});
        ASSERT_FALSE(colourOracle.empty());
        for (const auto& [text, expectedColour] : colourOracle) {
            ASSERT_EQ(labelsByText.count(text), 1u) << "missing label: " << text;
            EXPECT_TRUE(ColoursEqual(labelsByText[text]->getColour(), expectedColour)) << "for label: " << text;
        }
    }

    miscBucketButton->doClick();
    EXPECT_EQ(enchantNameLabel->getText(), "Miscellaneous");
    EXPECT_EQ(collectLabelTexts(*enchantEffectsList), describeEnchantEffectsOracle({miscEnchant}));
    {
        auto labelsByText = collectLabelsByText(*enchantEffectsList);
        auto colourOracle = describeEnchantEffectsColourOracle({miscEnchant});
        ASSERT_FALSE(colourOracle.empty());
        for (const auto& [text, expectedColour] : colourOracle) {
            ASSERT_EQ(labelsByText.count(text), 1u) << "missing label: " << text;
            EXPECT_TRUE(ColoursEqual(labelsByText[text]->getColour(), expectedColour)) << "for label: " << text;
        }
    }

    magicBucketButton->doClick();
    EXPECT_EQ(enchantNameLabel->getText(), "Magic Item");
    EXPECT_EQ(collectLabelTexts(*enchantEffectsList), describeEnchantEffectsOracle({magicEnchant}));
    // Sanity: dexamulet's enchant carries several attributes Attribute::toString CAN name
    // (channel life, speed, defence, agility), so this bucket is non-empty -- the swallow/skip
    // logic is not accidentally eating everything.
    EXPECT_FALSE(collectLabelTexts(*enchantEffectsList).empty());
    {
        // Sanity that BOTH colours actually occur somewhere across this file's fixtures (not just
        // one, which would leave half the green()/red() rule unpinned): dexamulet's positive
        // attribute boosts complement the poison bucket's negative one above.
        auto labelsByText = collectLabelsByText(*enchantEffectsList);
        auto colourOracle = describeEnchantEffectsColourOracle({magicEnchant});
        ASSERT_FALSE(colourOracle.empty());
        bool sawGreen = false;
        for (const auto& [text, expectedColour] : colourOracle) {
            ASSERT_EQ(labelsByText.count(text), 1u) << "missing label: " << text;
            EXPECT_TRUE(ColoursEqual(labelsByText[text]->getColour(), expectedColour)) << "for label: " << text;
            sawGreen = sawGreen || ColoursEqual(expectedColour, Colour4f::green());
        }
        EXPECT_TRUE(sawGreen);
    }
}

TEST_F(CharacterWindowWidgetFixture, PlayerConstructionShowsTheLevelUpButtonAndTogglesInventoryModeAcrossItsLifetime) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    auto& module = beginActiveTestModule();
    auto character = spawnFollowerAsPlayer(module, 7105);
    ASSERT_NE(character, nullptr);

    auto& playerList = GameSessionContext::get().activeModule().getPlayerList();
    auto player = playerList[character->getPlayerNumber()];
    ASSERT_NE(player, nullptr);
    EXPECT_FALSE(player->_inventoryMode);

    player->setLevelUpIndicator(true);
    ASSERT_TRUE(player->hasUnspentLevel());

    {
        auto window = std::make_shared<CharacterWindow>(character->getObjRef());
        ASSERT_FALSE(window->isDestroyed());

        // Player character: the character tab gains the LEVEL UP button, visible because the
        // player has an unspent level.
        ASSERT_NE(window->_levelUpButton, nullptr);
        EXPECT_TRUE(window->_levelUpButton->isVisible());

        // Constructor side effect: a CharacterWindow observing a player consumes that player's
        // input for inventory management for as long as the window is open.
        EXPECT_TRUE(player->_inventoryMode);
    }

    // The window (and its shared_ptr) is gone: the destructor reverted inventory mode.
    EXPECT_FALSE(player->_inventoryMode);
}

TEST_F(CharacterWindowWidgetFixture, NotifyMousePointerMovedTracksLevelUpButtonVisibilityAsUnspentLevelChanges) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    auto& module = beginActiveTestModule();
    auto character = spawnFollowerAsPlayer(module, 7106);
    ASSERT_NE(character, nullptr);

    auto& playerList = GameSessionContext::get().activeModule().getPlayerList();
    auto player = playerList[character->getPlayerNumber()];
    ASSERT_NE(player, nullptr);
    player->setLevelUpIndicator(true);

    auto window = std::make_shared<CharacterWindow>(character->getObjRef());
    ASSERT_FALSE(window->isDestroyed());
    ASSERT_TRUE(window->_levelUpButton->isVisible());

    // A position far outside the window overlaps no child, so InternalWindow's underlying
    // Container::notifyMousePointerMoved propagation naturally returns false (no Button claims a
    // point it doesn't contain, and Label/Image don't override the default at all) -- the
    // level-up-button visibility recompute at CharacterWindow.cpp:288-294 runs as a SIDE EFFECT
    // on every call regardless of that return value.
    const Ego::Events::MousePointerMovedEvent farAway(Ego::Point2f(-1000.0f, -1000.0f));

    player->setLevelUpIndicator(false);
    EXPECT_FALSE(window->notifyMousePointerMoved(farAway));
    EXPECT_FALSE(window->_levelUpButton->isVisible());

    player->setLevelUpIndicator(true);
    EXPECT_FALSE(window->notifyMousePointerMoved(farAway));
    EXPECT_TRUE(window->_levelUpButton->isVisible());
}

TEST_F(CharacterWindowWidgetFixture, NotifyMousePointerMovedForcesLevelUpButtonInvisibleWhenTheObservedPlayerBecomesUnresolvable) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    auto& module = beginActiveTestModule();
    auto character = spawnFollowerAsPlayer(module, 7109);
    ASSERT_NE(character, nullptr);

    auto& playerList = GameSessionContext::get().activeModule().getPlayerList();
    auto player = playerList[character->getPlayerNumber()];
    ASSERT_NE(player, nullptr);
    player->setLevelUpIndicator(true);

    auto window = std::make_shared<CharacterWindow>(character->getObjRef());
    ASSERT_FALSE(window->isDestroyed());
    ASSERT_TRUE(window->_levelUpButton->isVisible());

    // Distinguish the two ways tryObservedPlayer() can go null: unlike the "character removed"
    // test below, the CHARACTER object here stays alive and resolvable -- only the tryObserved-
    // Player(_playerNumber) lookup at CharacterWindow.cpp:289 starts failing, e.g. as if the
    // session's playerList had shrunk out from under this player index. MAX_PLAYER is 4 (so
    // INVALID_PLA_REF == 4); only one player is registered (index 0), so PLA_REF{1} is a
    // still-"valid" (non-invalid) index that is nonetheless out of range against a 1-entry
    // playerList -- exactly the tryObservedPlayer() null-without-INVALID_PLA_REF case.
    window->_playerNumber = static_cast<PLA_REF>(1);

    const Ego::Events::MousePointerMovedEvent farAway(Ego::Point2f(-1000.0f, -1000.0f));
    EXPECT_FALSE(window->notifyMousePointerMoved(farAway));
    EXPECT_FALSE(window->_levelUpButton->isVisible());
    // The window itself survives: only the button toggles, unlike the character-removed case.
    EXPECT_FALSE(window->isDestroyed());
}

TEST_F(CharacterWindowWidgetFixture, NotifyMousePointerMovedDestroysTheWindowWhenTheObservedCharacterIsRemoved) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    auto& module = beginActiveTestModule();
    auto character = spawnFollower(module, 7107);
    ASSERT_NE(character, nullptr);
    const ObjectRef characterRef = character->getObjRef();

    auto window = std::make_shared<CharacterWindow>(characterRef);
    ASSERT_FALSE(window->isDestroyed());

    ASSERT_TRUE(module.getObjectHandler().remove(characterRef));
    character.reset();

    const Ego::Events::MousePointerMovedEvent anywhere(Ego::Point2f(0.0f, 0.0f));
    EXPECT_FALSE(window->notifyMousePointerMoved(anywhere));
    EXPECT_TRUE(window->isDestroyed());
}

TEST_F(CharacterWindowWidgetFixture, LevelUpButtonClickWithoutAParentContainerSelfDestroysInsteadOfOpeningALevelUpWindow) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    auto& module = beginActiveTestModule();
    auto character = spawnFollowerAsPlayer(module, 7115);
    ASSERT_NE(character, nullptr);

    auto& playerList = GameSessionContext::get().activeModule().getPlayerList();
    auto player = playerList[character->getPlayerNumber()];
    ASSERT_NE(player, nullptr);
    player->setLevelUpIndicator(true);

    // Deliberately never attach this window to any parent Container (contrast with the sibling
    // test below, which uses a bare Tab as a minimal concrete parent): the LEVEL UP button's
    // onClick bail-out gate (CharacterWindow.cpp:409-412, `getParent() == nullptr`) then fires
    // instead of opening a LevelUpWindow.
    auto window = std::make_shared<CharacterWindow>(character->getObjRef());
    ASSERT_FALSE(window->isDestroyed());
    ASSERT_EQ(window->getParent(), nullptr);
    ASSERT_NE(window->_levelUpButton, nullptr);
    ASSERT_TRUE(window->_levelUpButton->isVisible());

    window->_levelUpButton->doClick();

    EXPECT_TRUE(window->isDestroyed());
    EXPECT_EQ(window->_levelUpWindow.lock(), nullptr);
}

TEST_F(CharacterWindowWidgetFixture, LevelUpButtonClickOpensASiblingLevelUpWindowAndHidesUntilItCloses) {
    Ego::Test::HeadlessUIManager uiManager;
    Ego::Test::ScopedActiveUIManager guard(uiManager);

    auto& module = beginActiveTestModule();
    auto character = spawnFollowerAsPlayer(module, 7108);
    ASSERT_NE(character, nullptr);

    auto& playerList = GameSessionContext::get().activeModule().getPlayerList();
    auto player = playerList[character->getPlayerNumber()];
    ASSERT_NE(player, nullptr);
    player->setLevelUpIndicator(true);

    // A LEVEL UP button click's onClick handler bails out (self-destroys) unless the window
    // already has a parent Container; use a bare Tab as a minimal concrete parent.
    auto parent = std::make_shared<Tab>("root");
    auto window = std::make_shared<CharacterWindow>(character->getObjRef());
    ASSERT_FALSE(window->isDestroyed());
    parent->addComponent(window);

    ASSERT_NE(window->_levelUpButton, nullptr);
    ASSERT_TRUE(window->_levelUpButton->isVisible());
    const size_t parentComponentCountBefore = parent->getComponentCount();

    window->_levelUpButton->doClick();

    EXPECT_FALSE(window->_levelUpButton->isVisible());
    EXPECT_EQ(parent->getComponentCount(), parentComponentCountBefore + 1);
    auto levelUpWindow = window->_levelUpWindow.lock();
    ASSERT_NE(levelUpWindow, nullptr);
    EXPECT_FALSE(levelUpWindow->isDestroyed());

    // CharacterWindow's destructor (not Component::destroy(), which only unparents and flags
    // _destroyed while other owners remain) cascades destroy() to the LevelUpWindow it opened.
    // Dropping BOTH owning references -- the parent's child-list entry and this local variable
    // -- is what actually runs ~CharacterWindow.
    parent->removeComponent(window);
    window.reset();
    EXPECT_TRUE(levelUpWindow->isDestroyed());
}

} // namespace
