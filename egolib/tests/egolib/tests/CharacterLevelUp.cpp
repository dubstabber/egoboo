/// @file CharacterLevelUp.cpp
/// @brief Characterization tests for Ego::applyCharacterLevelUp (egolib/game/Logic/LevelUp.cpp),
///        the GUI-free level-up computation extracted from Ego::GUI::LevelUpWindow::doLevelUp.
///
///        Every outcome except the flat per-perk bonus table is asserted against a "replay
///        oracle" that re-seeds the shared Random generator from the character's captured
///        level-up seed and replays the exact same sequence of draws the production code makes
///        (8 attribute-gain draws in AttributeType order, then the 2-draw seed re-randomization).
///        mt19937 draw VALUES are stdlib-implementation-specific and are never hardcoded here.

#include "gtest/gtest.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Logic/LevelUp.hpp"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/InputControl/InputDevice.hpp"
#include "egolib/Logic/IPerkHandler.hpp"
#include "egolib/Math/Random.hpp"
#include "egolib/vfs.h"

namespace
{

/// Snapshot of every piece of character/player state that applyCharacterLevelUp reads or
/// mutates, captured immediately before the call under test.
struct LevelUpBaseline
{
    uint32_t seed = 0;
    uint8_t levelIndex = 0;
    uint32_t experience = 0;
    float targetFat = 0.0f;
    int16_t resizeTimeRemaining = 0;
    float life = 0.0f;
    float mana = 0.0f;
    bool validPerksEmpty = false;
    std::array<float, Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES> attributeValue{};      ///< getAttribute(type), pre-call
    std::array<float, Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES> baseAttributeValue{};  ///< getBaseAttribute(type), pre-call
};

LevelUpBaseline captureBaseline(const Object& character)
{
    LevelUpBaseline baseline;
    baseline.seed = character.getLevelUpSeed();
    baseline.levelIndex = character.getExperienceLevelIndex();
    baseline.experience = character.getExperience();
    baseline.targetFat = character.getTargetFat();
    baseline.resizeTimeRemaining = character.getResizeTimeRemaining();
    baseline.life = character.getLife();
    baseline.mana = character.getMana();
    baseline.validPerksEmpty = character.getValidPerks().empty();

    for (uint8_t i = 0; i < Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES; ++i)
    {
        const auto type = static_cast<Ego::Attribute::AttributeType>(i);
        baseline.attributeValue[i] = character.getAttribute(type);
        baseline.baseAttributeValue[i] = character.getBaseAttribute(type);
    }

    return baseline;
}

/// Replays LevelUpWindow.cpp's flat per-perk attribute bonus switch (LevelUp.cpp's copy of it)
/// against an oracle @a increase array. Perks that mutate a secondary attribute immediately
/// (ACROBATIC/MASTER_ACROBAT/NIGHT_VISION/SENSE_KURSES/SENSE_INVISIBLE) do not touch @a increase
/// in production either, so they are intentionally absent here; callers verify those separately.
void applyOracleFlatBonus(Ego::Perks::PerkID perkId,
                          std::array<float, Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES>& increase)
{
    using namespace Ego;
    switch (perkId)
    {
        case Perks::TOUGHNESS:
            increase[Attribute::MAX_LIFE] += 2.0f;
            break;
        case Perks::SOLDIERS_FORTITUDE:
            increase[Attribute::LIFE_REGEN] += 0.15f;
            break;
        case Perks::TROLL_BLOOD:
            increase[Attribute::LIFE_REGEN] += 0.25f;
            break;
        case Perks::GIGANTISM:
            increase[Attribute::MIGHT] += 2.00f;
            increase[Attribute::AGILITY] -= 2.00f;
            break;
        case Perks::BRUTE:
            increase[Attribute::MIGHT] += 1.00f;
            increase[Attribute::INTELLECT] -= 2.00f;
            break;
        case Perks::DRAGON_BLOOD:
            increase[Attribute::MANA_REGEN] += 0.25f;
            break;
        case Perks::POWER:
            increase[Attribute::MAX_MANA] += 2.00f;
            break;
        case Perks::PERFECTION:
            increase[Attribute::INTELLECT] += 1.00f;
            increase[Attribute::AGILITY] += 1.00f;
            break;
        case Perks::ANCIENT_BLUD:
            increase[Attribute::LIFE_REGEN] += 0.25f;
            break;
        case Perks::SPELL_MASTERY:
            increase[Attribute::SPELL_POWER] += 1.0f;
            break;
        case Perks::MYSTIC_INTELLECT:
            increase[Attribute::MAX_MANA] += 1.0f;
            increase[Attribute::MANA_REGEN] += 0.1f;
            break;
        case Perks::MEDITATION:
            increase[Attribute::MANA_REGEN] += 0.15f;
            break;
        case Perks::BOOKWORM:
            increase[Attribute::INTELLECT] += 2.00f;
            increase[Attribute::MIGHT] -= 2.00f;
            break;
        default:
            //Immediate-mutation perks (ACROBATIC family, NIGHT_VISION family) and any perk
            //without a flat bonus: nothing to add to the increase[] array.
            break;
    }
}

/// Predicts every increase[] delta applyCharacterLevelUp will produce for @a perk, by re-seeding
/// the shared Random generator from @a seed and replaying the exact same 8 draws (in
/// AttributeType order) plus the perk's type bonus and flat bonus. Leaves the shared generator
/// positioned exactly where production leaves it after its own 8 draws (i.e. ready for the
/// seed-re-randomization draws), so predictNewSeed() below can be chained immediately after.
std::array<float, Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES> predictIncrease(const Object& character,
                                                                            const Ego::Perks::Perk& perk,
                                                                            uint32_t seed)
{
    std::array<float, Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES> increase{};

    Random::setSeed(seed);
    for (uint8_t i = 0; i < Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES; ++i)
    {
        const auto type = static_cast<Ego::Attribute::AttributeType>(i);
        increase[i] = Random::next(character.getProfile()->getAttributeGain(type));
    }

    increase[perk.getType()] += 1.0f;
    applyOracleFlatBonus(perk.getID(), increase);

    return increase;
}

/// Predicts the character's next level-up seed by replaying randomizeLevelUpSeed()'s nested
/// draw. MUST be called on the same (shared) generator immediately after predictIncrease(), with
/// no other draws interleaved, to mirror production's draw order exactly.
uint32_t predictNewSeed()
{
    const uint32_t innerDraw = Random::next<uint32_t>(std::numeric_limits<uint32_t>::max());
    return Random::next<uint32_t>(innerDraw);
}

class CharacterLevelUpFixture : public ::testing::Test
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
        opts.randomSeed = 85;
        opts.binaryPath = "";
        opts.logPath = "/debug/character-level-up-tests.log";
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

    GameModule& beginActiveTestModule()
    {
        auto module = findTestModule();
        EXPECT_NE(module, nullptr);
        if (module == nullptr)
        {
            throw std::runtime_error("test.mod profile not found");
        }

        auto& session = GameSessionContext::get();
        const bool began = session.beginModule(module, 85);
        EXPECT_TRUE(began);
        return session.activeModule();
    }

    /// Spawns follower.obj at @a slot and registers it as a player (mandatory: doLevelUp /
    /// applyCharacterLevelUp index the player list by getPlayerNumber(), which is only set by
    /// GameModule::addPlayer).
    std::shared_ptr<Object> spawnFollowerAsPlayer(GameModule& module, int slot) const
    {
        const ObjectProfileRef profile = EngineContext::get().profileSystem().loadOneProfile("mp_objects/follower.obj", slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid)
        {
            return nullptr;
        }

        const ObjectRef objectRef = module.spawnObjectRef(Ego::Vector3f(64.0f, 64.0f, 0.0f), profile,
                                                          static_cast<TEAM_REF>(Team::TEAM_NULL), 0, Facing(0),
                                                          "", ObjectRef::Invalid);
        auto object = module.getObjectHandler().getHandle(objectRef);
        if (!object)
        {
            return nullptr;
        }

        const bool registered = module.addPlayer(object->getObjRef(), Ego::Input::InputDevice::DeviceList[0]);
        EXPECT_TRUE(registered);
        return object;
    }
};

std::unique_ptr<ContentRuntimeBootstrap> CharacterLevelUpFixture::s_runtime;

/// Runs the shared assertion battery that applies to every level-up call regardless of which
/// perk was chosen: RNG-replay-exact increase[]/displayedValue[]/new-seed, level/XP bookkeeping,
/// the ALERTIF_LEVELUP alert, the level-up indicator round trip, current-life/mana coupling, the
/// fat/size-Might coupling, and the unconditional (pool-independent) perk grant.
void assertCommonLevelUpOutcome(Object& character,
                                const std::shared_ptr<Ego::Player>& player,
                                const Ego::Perks::Perk& perk,
                                const LevelUpBaseline& baseline,
                                const std::array<float, Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES>& predictedIncrease,
                                uint32_t predictedNewSeed,
                                const Ego::LevelUpReport& report)
{
    for (uint8_t i = 0; i < Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES; ++i)
    {
        SCOPED_TRACE(Ego::Attribute::toString(static_cast<Ego::Attribute::AttributeType>(i)));
        EXPECT_FLOAT_EQ(report.increase[i], predictedIncrease[i]);
        EXPECT_FLOAT_EQ(report.displayedValue[i], baseline.attributeValue[i]);
        EXPECT_FLOAT_EQ(character.getBaseAttribute(static_cast<Ego::Attribute::AttributeType>(i)),
                        baseline.baseAttributeValue[i] + predictedIncrease[i]);
    }

    //Level bumps by exactly 1; total experience is untouched.
    EXPECT_EQ(character.getExperienceLevelIndex(), static_cast<uint8_t>(baseline.levelIndex + 1));
    EXPECT_EQ(character.getExperience(), baseline.experience);

    //ALERTIF_LEVELUP is raised.
    EXPECT_TRUE(character.hasAnyAIAlertBits(ALERTIF_LEVELUP));

    //The level-up indicator round trips true -> false.
    EXPECT_FALSE(player->hasUnspentLevel());

    //Anti-save-scum reseed: the stored seed changes to the deterministically-replayed value.
    EXPECT_EQ(character.getLevelUpSeed(), predictedNewSeed);

    //Current life/mana track base MAX_LIFE/MAX_MANA increases exactly.
    EXPECT_FLOAT_EQ(character.getLife(), baseline.life + predictedIncrease[Ego::Attribute::MAX_LIFE]);
    EXPECT_FLOAT_EQ(character.getMana(), baseline.mana + predictedIncrease[Ego::Attribute::MAX_MANA]);

    //Might slightly increases character size: fires exactly when the Might delta is nonzero.
    if (predictedIncrease[Ego::Attribute::MIGHT] != 0.0f)
    {
        const float expectedFat = baseline.targetFat +
                                  character.getProfile()->getSizeGainPerMight() * 0.1f * predictedIncrease[Ego::Attribute::MIGHT];
        EXPECT_FLOAT_EQ(character.getTargetFat(), expectedFat);
        EXPECT_EQ(character.getResizeTimeRemaining(), static_cast<int16_t>(baseline.resizeTimeRemaining + Object::SIZETIME));
    }
    else
    {
        EXPECT_FLOAT_EQ(character.getTargetFat(), baseline.targetFat);
        EXPECT_EQ(character.getResizeTimeRemaining(), baseline.resizeTimeRemaining);
    }

    //The perk is granted unconditionally, with no pool/requirement check -- even though
    //follower.obj's pool is empty (no [POOL] expansions), so this perk was never "valid".
    EXPECT_TRUE(baseline.validPerksEmpty);
    EXPECT_TRUE(character.hasPerk(perk.getID()));
}

/// Drives one full level-up call for @a perk against @a object (already spawned + registered as
/// a player), asserting the shared outcome battery. Returns the produced report so callers can
/// layer additional perk-specific assertions on top.
Ego::LevelUpReport runLevelUpScenario(Object& object, const Ego::Perks::Perk& perk)
{
    auto& playerList = GameSessionContext::get().activeModule().getPlayerList();
    const auto player = playerList[object.getPlayerNumber()];
    EXPECT_NE(player, nullptr);
    if (!player)
    {
        throw std::runtime_error("player registration missing");
    }

    //Arrange the indicator round trip: pretend the player has an unspent level pending.
    player->setLevelUpIndicator(true);
    EXPECT_TRUE(player->hasUnspentLevel());

    const LevelUpBaseline baseline = captureBaseline(object);

    const std::array<float, Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES> predictedIncrease =
        predictIncrease(object, perk, baseline.seed);
    const uint32_t predictedNewSeed = predictNewSeed();

    //Pollute the shared generator between the oracle replay and the real call: the real call
    //re-seeds internally, so this must not change the outcome (risk #1/#2 in the extraction plan).
    for (int i = 0; i < 5; ++i)
    {
        Random::next(0, 1000);
    }

    const Ego::LevelUpReport report = Ego::applyCharacterLevelUp(object, perk, playerList);

    assertCommonLevelUpOutcome(object, player, perk, baseline, predictedIncrease, predictedNewSeed, report);

    return report;
}

} // namespace

TEST_F(CharacterLevelUpFixture, FollowerAttributeGainIntervalsMatchProfileData)
{
    auto& module = beginActiveTestModule();
    auto object = spawnFollowerAsPlayer(module, 7001);
    ASSERT_NE(object, nullptr);

    const auto& profile = *object->getProfile();

    EXPECT_FLOAT_EQ(profile.getAttributeGain(Ego::Attribute::MAX_LIFE).lower(), 1.0f);
    EXPECT_FLOAT_EQ(profile.getAttributeGain(Ego::Attribute::MAX_LIFE).upper(), 2.0f);
    EXPECT_FLOAT_EQ(profile.getAttributeGain(Ego::Attribute::MAX_MANA).lower(), 1.0f);
    EXPECT_FLOAT_EQ(profile.getAttributeGain(Ego::Attribute::MAX_MANA).upper(), 2.0f);
    EXPECT_FLOAT_EQ(profile.getAttributeGain(Ego::Attribute::MANA_REGEN).lower(), 0.0f);
    EXPECT_FLOAT_EQ(profile.getAttributeGain(Ego::Attribute::MANA_REGEN).upper(), 1.0f);
    EXPECT_FLOAT_EQ(profile.getAttributeGain(Ego::Attribute::SPELL_POWER).lower(), 0.0f);
    EXPECT_FLOAT_EQ(profile.getAttributeGain(Ego::Attribute::SPELL_POWER).upper(), 1.0f);
    EXPECT_FLOAT_EQ(profile.getAttributeGain(Ego::Attribute::MIGHT).lower(), 0.0f);
    EXPECT_FLOAT_EQ(profile.getAttributeGain(Ego::Attribute::MIGHT).upper(), 1.0f);
    EXPECT_FLOAT_EQ(profile.getAttributeGain(Ego::Attribute::INTELLECT).lower(), 0.5f);
    EXPECT_FLOAT_EQ(profile.getAttributeGain(Ego::Attribute::INTELLECT).upper(), 2.5f);
    EXPECT_FLOAT_EQ(profile.getAttributeGain(Ego::Attribute::AGILITY).lower(), 1.0f);
    EXPECT_FLOAT_EQ(profile.getAttributeGain(Ego::Attribute::AGILITY).upper(), 2.0f);

    //Life regeneration gain per level is unimplemented content-wide: exactly [0,0].
    EXPECT_FLOAT_EQ(profile.getAttributeGain(Ego::Attribute::LIFE_REGEN).lower(), 0.0f);
    EXPECT_FLOAT_EQ(profile.getAttributeGain(Ego::Attribute::LIFE_REGEN).upper(), 0.0f);

    //follower.obj has no [POOL] expansions, so it can never validly learn any perk.
    EXPECT_TRUE(object->getValidPerks().empty());
}

TEST_F(CharacterLevelUpFixture, ToughnessAppliesTypeBonusFlatLifeBonusAndCommonOutcome)
{
    auto& module = beginActiveTestModule();
    auto object = spawnFollowerAsPlayer(module, 7002);
    ASSERT_NE(object, nullptr);

    const Ego::Perks::Perk& perk = Ego::Perks::activePerkHandler().getPerk(Ego::Perks::TOUGHNESS);
    ASSERT_EQ(perk.getType(), Ego::Attribute::MIGHT);

    const Ego::LevelUpReport report = runLevelUpScenario(*object, perk);

    //TOUGHNESS: +1 Might (type bonus) and a flat +2 Max Life on top of the drawn gains.
    //(No further per-attribute value assertions here beyond the shared battery: the flat bonus
    //is already folded into predictedIncrease/report.increase by applyOracleFlatBonus.)
    (void)report;
}

TEST_F(CharacterLevelUpFixture, SoldiersFortitudeGrantsExactLifeRegenBonus)
{
    auto& module = beginActiveTestModule();
    auto object = spawnFollowerAsPlayer(module, 7003);
    ASSERT_NE(object, nullptr);

    const Ego::Perks::Perk& perk = Ego::Perks::activePerkHandler().getPerk(Ego::Perks::SOLDIERS_FORTITUDE);
    ASSERT_EQ(perk.getType(), Ego::Attribute::MIGHT);

    const Ego::LevelUpReport report = runLevelUpScenario(*object, perk);

    //Follower's Life Regeneration gain interval is exactly [0,0] (unimplemented content-wide),
    //so the only possible contribution is the perk's flat +0.15 bonus.
    EXPECT_FLOAT_EQ(report.increase[Ego::Attribute::LIFE_REGEN], 0.15f);
}

TEST_F(CharacterLevelUpFixture, GigantismAppliesMightAndAgilityDeltas)
{
    auto& module = beginActiveTestModule();
    auto object = spawnFollowerAsPlayer(module, 7004);
    ASSERT_NE(object, nullptr);

    const Ego::Perks::Perk& perk = Ego::Perks::activePerkHandler().getPerk(Ego::Perks::GIGANTISM);
    ASSERT_EQ(perk.getType(), Ego::Attribute::MIGHT);

    //GIGANTISM: +1 Might (type bonus) + flat +2 Might, flat -2 Agility. Follower's Agility gain
    //draws from [1,2], so the net Agility delta (draw - 2) is at or below roughly zero -- the
    //negative-increase path -- and is asserted exactly against the replay oracle either way.
    runLevelUpScenario(*object, perk);
}

TEST_F(CharacterLevelUpFixture, BruteAppliesMightAndIntellectDeltas)
{
    auto& module = beginActiveTestModule();
    auto object = spawnFollowerAsPlayer(module, 7005);
    ASSERT_NE(object, nullptr);

    const Ego::Perks::Perk& perk = Ego::Perks::activePerkHandler().getPerk(Ego::Perks::BRUTE);
    ASSERT_EQ(perk.getType(), Ego::Attribute::MIGHT);

    //BRUTE: +1 Might (type bonus) + flat +1 Might, flat -2 Intellect (can net negative against
    //follower's [0.5,2.5] Intellect gain draw).
    runLevelUpScenario(*object, perk);
}

TEST_F(CharacterLevelUpFixture, PowerGrantsIntellectTypeBonusAndFlatManaBonus)
{
    auto& module = beginActiveTestModule();
    auto object = spawnFollowerAsPlayer(module, 7006);
    ASSERT_NE(object, nullptr);

    const Ego::Perks::Perk& perk = Ego::Perks::activePerkHandler().getPerk(Ego::Perks::POWER);
    ASSERT_EQ(perk.getType(), Ego::Attribute::INTELLECT);

    //POWER: +1 Intellect (type bonus) + flat +2 Max Mana. The shared battery already asserts the
    //current-mana coupling against predictedIncrease[MAX_MANA].
    runLevelUpScenario(*object, perk);
}

TEST_F(CharacterLevelUpFixture, AcrobaticGrantsImmediateNumberOfJumpsBonus)
{
    auto& module = beginActiveTestModule();
    auto object = spawnFollowerAsPlayer(module, 7007);
    ASSERT_NE(object, nullptr);

    const Ego::Perks::Perk& perk = Ego::Perks::activePerkHandler().getPerk(Ego::Perks::ACROBATIC);
    ASSERT_EQ(perk.getType(), Ego::Attribute::AGILITY);

    const float jumpsBefore = object->getBaseAttribute(Ego::Attribute::NUMBER_OF_JUMPS);

    runLevelUpScenario(*object, perk);

    //ACROBATIC mutates a secondary attribute immediately, outside of the report/increase[] array.
    EXPECT_FLOAT_EQ(object->getBaseAttribute(Ego::Attribute::NUMBER_OF_JUMPS), jumpsBefore + 1.0f);
}

TEST_F(CharacterLevelUpFixture, NightVisionGrantsImmediateDarkvisionBonus)
{
    auto& module = beginActiveTestModule();
    auto object = spawnFollowerAsPlayer(module, 7008);
    ASSERT_NE(object, nullptr);

    const Ego::Perks::Perk& perk = Ego::Perks::activePerkHandler().getPerk(Ego::Perks::NIGHT_VISION);
    ASSERT_EQ(perk.getType(), Ego::Attribute::INTELLECT);

    const float darkvisionBefore = object->getBaseAttribute(Ego::Attribute::DARKVISION);

    runLevelUpScenario(*object, perk);

    //NIGHT_VISION mutates a secondary attribute immediately, outside of the report/increase[] array.
    EXPECT_FLOAT_EQ(object->getBaseAttribute(Ego::Attribute::DARKVISION), darkvisionBefore + 1.0f);
}

TEST_F(CharacterLevelUpFixture, DefaultCasePerkAppliesOnlyTypeBonusAndGrantsUnconditionally)
{
    auto& module = beginActiveTestModule();
    auto object = spawnFollowerAsPlayer(module, 7009);
    ASSERT_NE(object, nullptr);

    //WEAPON_PROFICIENCY has no case in the flat-bonus switch: only the universal +1 type bonus
    //applies. It is also outside follower.obj's (empty) perk pool, exercising the unconditional
    //no-validation grant.
    const Ego::Perks::Perk& perk = Ego::Perks::activePerkHandler().getPerk(Ego::Perks::WEAPON_PROFICIENCY);
    ASSERT_EQ(perk.getType(), Ego::Attribute::MIGHT);

    ASSERT_FALSE(object->hasPerk(Ego::Perks::WEAPON_PROFICIENCY));

    runLevelUpScenario(*object, perk);
}
