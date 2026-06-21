/// @file ContentParsers.cpp
/// @brief Characterization tests for legacy content parsers.
///
/// These tests exercise the spawn.txt, menu.txt (ModuleProfile),
/// wawalite.txt, data.txt (ObjectProfile), and level.mpd parsers against the
/// shipped test.mod data.  They verify that the parsers produce expected field
/// values for a known-good module so that future refactoring work does not
/// silently change parse results.
///
/// The tests require a VFS bootstrap and the real data/ directory, similar
/// to the content validator tool.

#include "gtest/gtest.h"
#include "egolib/game/Core/EngineContext.hpp"

#include "TestEnvironment.hpp"
#include "egolib/Audio/IAudioSystem.hpp"
#include "egolib/FileFormats/map_file.h"
#include "egolib/FileFormats/SpawnFile/spawn_file.h"
#include "egolib/FileFormats/SpawnFile/SpawnFileReaderImpl.hpp"
#include "egolib/FileFormats/wawalite_file.h"
#include "egolib/Graphics/AnimatedModel.hpp"
#include "egolib/Graphics/ModelAnimationMetadata.hpp"
#include "egolib/Graphics/ModelDescriptor.hpp"
#include "egolib/Graphics/ObjectModelAsset.hpp"
#include "egolib/Graphics/ObjectModelLoader.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/Image/ImageManager.hpp"
#include "egolib/Logic/PerkHandler.hpp"
#include "egolib/egoboo_setup.h"
#include "egolib/vfs.h"

#include <cstdlib>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
class RecordingAudioSystem : public IAudioSystem
{
public:
    SoundID loadSound(const std::string& fileName) override
    {
        loadedSoundPaths.push_back(fileName);
        return -1;
    }

    void playMusic(MusicID, uint16_t = 0) override {}
    void playMusic(const std::string&, uint16_t = 0) override {}
    void stopMusic() override {}
    void fadeAllSounds() override {}
    int playSound(const Ego::Vector3f&, SoundID) override { return 0; }
    void playSoundLooped(SoundID, ObjectRef) override {}
    size_t stopObjectLoopingSounds(ObjectRef, SoundID = -1) override { return 0; }
    int playSoundFull(SoundID) override { return 0; }
    SoundID getGlobalSound(GlobalSound) const override { return 0; }
    void setMaxHearingDistance(float) override {}
    void setMusicVolume(int) override {}
    void setSoundEffectVolume(int) override {}
    void update() override {}
    void loadGlobalSounds() override {}
    void loadAllMusic() override {}

    void reset()
    {
        loadedSoundPaths.clear();
    }

    std::vector<std::string> loadedSoundPaths;
};
}

// ---------------------------------------------------------------------------
// Shared test fixture that bootstraps VFS and minimal runtime services.
// ---------------------------------------------------------------------------

class ContentParserFixture : public ::testing::Test
{
protected:
    static std::unique_ptr<ContentRuntimeBootstrap> s_runtime;
    static std::unique_ptr<RecordingAudioSystem> s_audioSystem;

    static void SetUpTestSuite()
    {
        Ego::Test::configureDataDirectory();

        ContentRuntimeBootstrap::Options opts;
        opts.initializeVirtualFileSystem   = true;
        opts.initializeBaseVfsPaths        = true;
        opts.initializeLogging             = true;
        opts.configureLightweightProfileLoading = true;
        opts.initializeImageManager        = true;
        opts.initializePerkHandler         = true;
        opts.initializeProfileSystem       = true;
        opts.clearModuleVfsPathsOnShutdown = true;
        opts.clearBaseVfsPathsOnShutdown   = true;
        opts.seedRandom     = true;
        opts.randomSeed     = 0;
        opts.binaryPath     = "";
        opts.logPath        = "/debug/content-parser-tests.log";
        opts.logLevel       = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);
        s_audioSystem = std::make_unique<RecordingAudioSystem>();
        EngineContext::get().installAudioSystem(*s_audioSystem);

        // Discover modules so that test.mod is reachable.
        EngineContext::get().profileSystem().loadModuleProfiles();
    }

    static void TearDownTestSuite()
    {
        EngineContext::get().clearAudioSystem();
        s_audioSystem.reset();
        s_runtime.reset();
    }

    /// Helper: find the ModuleProfile for a given module directory name.
    static std::shared_ptr<ModuleProfile> findModule(const std::string& dirName)
    {
        for (const auto& mod : EngineContext::get().profileSystem().getModuleProfiles())
        {
            if (mod && mod->getFolderName() == dirName)
            {
                return mod;
            }
        }
        return nullptr;
    }

    /// Helper: mount a module's VFS paths so its content is reachable.
    static void mountModule(const ModuleProfile& mod)
    {
        setup_init_module_vfs_paths(mod.getPath());
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ContentParserFixture::s_runtime;
std::unique_ptr<RecordingAudioSystem> ContentParserFixture::s_audioSystem;

std::vector<uint8_t> makeSyntheticModelBuffer()
{
    std::vector<uint8_t> buffer;

    const auto append = [&buffer](const void* data, size_t size)
    {
        const auto* bytes = static_cast<const uint8_t*>(data);
        buffer.insert(buffer.end(), bytes, bytes + size);
    };
    const auto appendFloat = [&append](float value)
    {
        append(&value, sizeof(value));
    };
    const auto appendUInt16 = [&append](uint16_t value)
    {
        append(&value, sizeof(value));
    };

    const std::array<std::array<float, 3>, 3> positions =
    {{
        {{0.0f, 0.0f, 0.0f}},
        {{1.0f, 0.0f, 0.0f}},
        {{0.0f, 1.0f, 0.0f}}
    }};
    const std::array<std::array<float, 3>, 3> normals =
    {{
        {{0.0f, 0.0f, 1.0f}},
        {{0.0f, 0.0f, 1.0f}},
        {{0.0f, 0.0f, 1.0f}}
    }};
    const std::array<std::array<float, 2>, 3> uvs =
    {{
        {{0.0f, 0.0f}},
        {{1.0f, 0.0f}},
        {{0.0f, 1.0f}}
    }};

    for (const auto& position : positions)
    {
        for (const float value : position)
        {
            appendFloat(value);
        }
    }
    for (const auto& normal : normals)
    {
        for (const float value : normal)
        {
            appendFloat(value);
        }
    }
    for (const auto& uv : uvs)
    {
        for (const float value : uv)
        {
            appendFloat(value);
        }
    }
    appendUInt16(0);
    appendUInt16(1);
    appendUInt16(2);

    while (buffer.size() % 4 != 0)
    {
        buffer.push_back(0);
    }

    return buffer;
}

std::string base64Encode(const std::vector<uint8_t>& data)
{
    static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    for (size_t i = 0; i < data.size(); i += 3)
    {
        const uint32_t a = data[i];
        const uint32_t b = (i + 1 < data.size()) ? data[i + 1] : 0;
        const uint32_t c = (i + 2 < data.size()) ? data[i + 2] : 0;
        const uint32_t triple = (a << 16) | (b << 8) | c;

        encoded.push_back(alphabet[(triple >> 18) & 0x3F]);
        encoded.push_back(alphabet[(triple >> 12) & 0x3F]);
        encoded.push_back(i + 1 < data.size() ? alphabet[(triple >> 6) & 0x3F] : '=');
        encoded.push_back(i + 2 < data.size() ? alphabet[triple & 0x3F] : '=');
    }
    return encoded;
}

std::string makeSyntheticModelJson(const std::string& bufferUri, bool includeExtras)
{
    std::string json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":104";
    if (!bufferUri.empty())
    {
        json += ",\"uri\":\"" + bufferUri + "\"";
    }
    json +=
        "}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
        "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36},"
        "{\"buffer\":0,\"byteOffset\":72,\"byteLength\":24},"
        "{\"buffer\":0,\"byteOffset\":96,\"byteLength\":6,\"target\":34963}"
        "],"
        "\"accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
        "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
        "{\"bufferView\":2,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},"
        "{\"bufferView\":3,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}"
        "],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1,\"TEXCOORD_0\":2},\"indices\":3}]}]";

    if (includeExtras)
    {
        json +=
            ",\"extras\":{\"egoboo\":{"
            "\"version\":1,"
            "\"frames\":[{\"name\":\"DA0\",\"mesh\":0,\"framefx\":256}],"
            "\"actions\":{\"DA\":[0,0]},"
            "\"healAliases\":[[\"DA\",\"WA\"]]"
            "}}";
    }

    json += "}";
    return json;
}

void writeTextFile(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream out(path);
    out << text;
}

void writeBinaryFile(const std::filesystem::path& path, const std::vector<uint8_t>& data)
{
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

void appendU32(std::vector<uint8_t>& data, uint32_t value)
{
    data.push_back(static_cast<uint8_t>(value & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void appendPaddedTextChunk(std::vector<uint8_t>& data, const std::string& text)
{
    data.insert(data.end(), text.begin(), text.end());
    while (data.size() % 4 != 0)
    {
        data.push_back(' ');
    }
}

void writeSyntheticGltf(const std::filesystem::path& objectDir, const std::string& fileName, bool includeExtras)
{
    const std::vector<uint8_t> buffer = makeSyntheticModelBuffer();
    const std::string uri = "data:application/octet-stream;base64," + base64Encode(buffer);
    writeTextFile(objectDir / fileName, makeSyntheticModelJson(uri, includeExtras));
}

void writeSyntheticGlb(const std::filesystem::path& objectDir, bool includeExtras)
{
    const std::vector<uint8_t> buffer = makeSyntheticModelBuffer();
    std::string json = makeSyntheticModelJson("", includeExtras);

    std::vector<uint8_t> jsonChunk;
    appendPaddedTextChunk(jsonChunk, json);

    std::vector<uint8_t> binChunk = buffer;
    while (binChunk.size() % 4 != 0)
    {
        binChunk.push_back(0);
    }

    std::vector<uint8_t> glb;
    appendU32(glb, 0x46546C67);
    appendU32(glb, 2);
    appendU32(glb, static_cast<uint32_t>(12 + 8 + jsonChunk.size() + 8 + binChunk.size()));
    appendU32(glb, static_cast<uint32_t>(jsonChunk.size()));
    appendU32(glb, 0x4E4F534A);
    glb.insert(glb.end(), jsonChunk.begin(), jsonChunk.end());
    appendU32(glb, static_cast<uint32_t>(binChunk.size()));
    appendU32(glb, 0x004E4942);
    glb.insert(glb.end(), binChunk.begin(), binChunk.end());

    writeBinaryFile(objectDir / "tris.glb", glb);
}

void expectSyntheticModelLoaded(const Ego::Graphics::ObjectModelLoadResult& result)
{
    ASSERT_NE(result.model, nullptr);
    ASSERT_TRUE(result.animationMetadata.has_value());

    const std::shared_ptr<Ego::Graphics::AnimatedModel>& model = result.model;
    EXPECT_EQ(model->getVertexCount(), 3u);
    ASSERT_EQ(model->getFrames().size(), 1u);
    EXPECT_STREQ(model->getFrames()[0].name, "DA0");
    EXPECT_EQ(model->getFrames()[0].framefx, static_cast<BIT_FIELD>(MADFX_FOOTFALL));
    ASSERT_EQ(model->getFrames()[0].vertexList.size(), 3u);
    EXPECT_FLOAT_EQ(model->getFrames()[0].vertexList[1].pos[kX], 1.0f);
    EXPECT_FLOAT_EQ(model->getFrames()[0].vertexList[2].pos[kY], 1.0f);

    ASSERT_FALSE(model->getDrawCommands().empty());
    const Ego::Graphics::AnimatedModelDrawCommand& command = model->getDrawCommands().front();
    EXPECT_EQ(command.primitiveMode, Ego::Graphics::AnimatedModelPrimitiveMode::Triangles);
    ASSERT_EQ(command.data.size(), 3u);
    EXPECT_EQ(command.data[0].vertexIndex, 0);
    EXPECT_EQ(command.data[1].vertexIndex, 1);
    EXPECT_EQ(command.data[2].vertexIndex, 2);

    Ego::Graphics::ModelAnimationMetadata metadata;
    metadata.initializeFromActionData(*model, *result.animationMetadata);
    EXPECT_TRUE(metadata.isActionValid(ACTION_DA));
    EXPECT_EQ(metadata.getFirstFrame(ACTION_DA), 0);
    EXPECT_EQ(metadata.getLastFrame(ACTION_DA), 0);
}

// ===========================================================================
//  spawn.txt parser tests
// ===========================================================================

class SpawnParserTest : public ContentParserFixture {};

TEST_F(SpawnParserTest, TestModSpawnEntryCount)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr) << "test.mod not found in module profiles";
    mountModule(*mod);

    SpawnFileReaderImpl reader;
    auto entries = reader.read("mp_data/spawn.txt");

    // test.mod/gamedat/spawn.txt has 45 spawn entries (known from validator).
    EXPECT_EQ(entries.size(), 45u);
}

TEST_F(SpawnParserTest, TestModFirstEntryIsPlayer1)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    SpawnFileReaderImpl reader;
    auto entries = reader.read("mp_data/spawn.txt");
    ASSERT_GE(entries.size(), 1u);

    const auto& first = entries[0];
    EXPECT_EQ(first.spawn_comment, "Player1");
    EXPECT_EQ(first.slot, 0);
    EXPECT_TRUE(first.stat);
}

TEST_F(SpawnParserTest, TestModFirstEntryPosition)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    SpawnFileReaderImpl reader;
    auto entries = reader.read("mp_data/spawn.txt");
    ASSERT_GE(entries.size(), 1u);

    const auto& first = entries[0];
    // Positions in spawn.txt are in tile units; the parser may multiply by
    // GRID_FSIZE (128.0).  We verify the raw parsed values match expectations.
    // The file says 37.6 37.6 0.0 for xpos ypos zpos.
    EXPECT_NEAR(first.pos[kX], 37.6f * 128.0f, 1.0f);
    EXPECT_NEAR(first.pos[kY], 37.6f * 128.0f, 1.0f);
    EXPECT_NEAR(first.pos[kZ], 0.0f, 0.1f);
}

TEST_F(SpawnParserTest, TestModAttachmentTypes)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    SpawnFileReaderImpl reader;
    auto entries = reader.read("mp_data/spawn.txt");
    ASSERT_GE(entries.size(), 5u);

    // Entry 0 is the Player1 base — no attachment.
    EXPECT_EQ(entries[0].attach, ATTACH_NONE);
    // Entry 1: "L" = ATTACH_LEFT
    EXPECT_EQ(entries[1].attach, ATTACH_LEFT);
    // Entry 2: "R" = ATTACH_RIGHT
    EXPECT_EQ(entries[2].attach, ATTACH_RIGHT);
    // Entry 3: "I" = ATTACH_INVENTORY
    EXPECT_EQ(entries[3].attach, ATTACH_INVENTORY);
}

// ===========================================================================
//  menu.txt / ModuleProfile parser tests
// ===========================================================================

class ModuleProfileParserTest : public ContentParserFixture {};

TEST_F(ModuleProfileParserTest, TestModProfileExists)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr) << "test.mod not found in module profiles";
}

TEST_F(ModuleProfileParserTest, TestModProfileName)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(mod->getName(), "Test Module");
}

TEST_F(ModuleProfileParserTest, TestModProfileImports)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    // menu.txt says "Number of imports ( 0 to 4 ) :4"
    EXPECT_EQ(mod->getImportAmount(), 4);
}

TEST_F(ModuleProfileParserTest, TestModProfileExporting)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    // "Exporting ( True or False ) :True"
    EXPECT_TRUE(mod->isExportAllowed());
}

TEST_F(ModuleProfileParserTest, TestModProfileMaxPlayers)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    // test.mod supports up to 4 players.
    EXPECT_GE(mod->getMaxPlayers(), 1);
    EXPECT_LE(mod->getMaxPlayers(), 4);
}

// ===========================================================================
//  wawalite.txt parser tests
// ===========================================================================

class WawaliteParserTest : public ContentParserFixture {};

TEST_F(WawaliteParserTest, TestModWawaliteLoads)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    wawalite_data_t data;
    auto result = wawalite_data_read("mp_data/wawalite.txt", &data);
    EXPECT_NE(result, nullptr);
}

TEST_F(WawaliteParserTest, TestModWawaliteGravity)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    wawalite_data_t data;
    auto result = wawalite_data_read("mp_data/wawalite.txt", &data);
    ASSERT_NE(result, nullptr);

    // Physics gravity should be a reasonable value (not zero, not absurd).
    EXPECT_GT(data.phys.gravity, -20.0f);
    EXPECT_LT(data.phys.gravity, 0.0f);
}

// ===========================================================================
//  data.txt / ObjectProfile parser tests
// ===========================================================================

class ObjectProfileParserTest : public ContentParserFixture {};

namespace
{

void setSyntheticFrameName(Ego::Graphics::AnimatedModelFrame& frame, const char* name)
{
    std::strncpy(frame.name, name, sizeof(frame.name));
    frame.name[sizeof(frame.name) - 1] = '\0';
}

Ego::Graphics::AnimatedModel makeSyntheticModel(std::initializer_list<const char*> frameNames)
{
    Ego::Graphics::AnimatedModel model;
    model.getFrames().resize(frameNames.size());

    size_t index = 0;
    for (const char* frameName : frameNames)
    {
        setSyntheticFrameName(model.getFrames()[index], frameName);
        ++index;
    }

    return model;
}

} // namespace

class ModelAnimationMetadataTest : public ContentParserFixture {};

TEST_F(ModelAnimationMetadataTest, LegacyFrameNamesBuildActionRangesAndEffects)
{
    Ego::Graphics::AnimatedModel model = makeSyntheticModel({
        "DA0",
        "WA0 F",
        "WA1 ALGR",
        "WB0 LA",
        "KC0 P"
    });

    Ego::Graphics::ModelAnimationMetadata metadata;
    metadata.initializeFromLegacyFrames(model, "synthetic", "mp_missing/copy.txt");

    EXPECT_TRUE(metadata.isActionValid(ACTION_DA));
    EXPECT_TRUE(metadata.isActionValid(ACTION_WA));
    EXPECT_TRUE(metadata.isActionValid(ACTION_WB));
    EXPECT_TRUE(metadata.isActionValid(ACTION_KC));
    EXPECT_FALSE(metadata.isActionValid(ACTION_WC));

    EXPECT_EQ(metadata.getFirstFrame(ACTION_WA), 1);
    EXPECT_EQ(metadata.getLastFrame(ACTION_WA), 2);
    EXPECT_EQ(metadata.getFirstFrame(ACTION_WB), 3);
    EXPECT_EQ(metadata.getLastFrame(ACTION_WB), 3);

    EXPECT_TRUE(HAS_SOME_BITS(model.getFrames()[1].framefx, MADFX_FOOTFALL));
    EXPECT_TRUE(HAS_SOME_BITS(model.getFrames()[2].framefx, MADFX_ACTLEFT));
    EXPECT_TRUE(HAS_SOME_BITS(model.getFrames()[2].framefx, MADFX_GRABRIGHT));
    EXPECT_TRUE(HAS_SOME_BITS(model.getFrames()[3].framefx, MADFX_ACTLEFT));
    EXPECT_TRUE(HAS_SOME_BITS(model.getFrames()[4].framefx, MADFX_POOF));

    const BIT_FIELD walkFx = metadata.getMadFX(model, ACTION_WA);
    EXPECT_TRUE(HAS_SOME_BITS(walkFx, MADFX_FOOTFALL));
    EXPECT_TRUE(HAS_SOME_BITS(walkFx, MADFX_ACTLEFT));
    EXPECT_TRUE(HAS_SOME_BITS(walkFx, MADFX_GRABRIGHT));
}

TEST_F(ModelAnimationMetadataTest, MissingWalkActionsFallbackThroughLegacyChain)
{
    Ego::Graphics::AnimatedModel model = makeSyntheticModel({
        "DA0",
        "WB0",
        "WB1"
    });

    Ego::Graphics::ModelAnimationMetadata metadata;
    metadata.initializeFromLegacyFrames(model, "synthetic", "mp_missing/copy.txt");

    EXPECT_FALSE(metadata.isActionValid(ACTION_WA));
    EXPECT_FALSE(metadata.isActionValid(ACTION_WC));
    EXPECT_EQ(metadata.getAction(ACTION_WA), ACTION_DA);
    EXPECT_EQ(metadata.getAction(ACTION_WC), ACTION_WB);
}

TEST_F(ModelAnimationMetadataTest, WalkLipMappingsUseActionFrameProgress)
{
    Ego::Graphics::AnimatedModel model = makeSyntheticModel({
        "DA0",
        "WA0",
        "WA1",
        "WA2",
        "WB0",
        "WC0"
    });

    Ego::Graphics::ModelAnimationMetadata metadata;
    metadata.initializeFromLegacyFrames(model, "synthetic", "mp_missing/copy.txt");

    EXPECT_EQ(model.getFrames()[1].framelip, 0);
    EXPECT_EQ(model.getFrames()[2].framelip, 5);
    EXPECT_EQ(model.getFrames()[3].framelip, 10);

    EXPECT_EQ(metadata.getFrameLipToWalkFrame(LIPWA, 0), 1);
    EXPECT_EQ(metadata.getFrameLipToWalkFrame(LIPWA, 15), 3);
    EXPECT_EQ(metadata.getFrameLipToWalkFrame(LIPWB, 15), 4);
    EXPECT_EQ(metadata.getFrameLipToWalkFrame(LIPWC, 15), 5);
}

TEST_F(ModelAnimationMetadataTest, CopyFileHealingMapsActionFamilies)
{
#ifdef _WIN32
    const int processId = _getpid();
#else
    const int processId = getpid();
#endif
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("egoboo-model-metadata-" + std::to_string(processId));
    const std::filesystem::path objectDir = root / "copy.obj";
    std::filesystem::create_directories(objectDir);
    Ego::Test::scheduleTestDirectoryCleanup(root);

    {
        std::ofstream(objectDir / "copy.txt") << ": BASH ZAP\n";
    }

    ASSERT_NE(0, vfs_add_mount_point(root.string(), Ego::FsPath(""), Ego::VfsPath("mp_modelmetadata"), 1));

    Ego::Graphics::AnimatedModel model = makeSyntheticModel({
        "DA0",
        "BA0",
        "BB0"
    });

    Ego::Graphics::ModelAnimationMetadata metadata;
    metadata.initializeFromLegacyFrames(model, "synthetic", "mp_modelmetadata/copy.obj/copy.txt");

    EXPECT_TRUE(metadata.isActionValid(ACTION_BA));
    EXPECT_TRUE(metadata.isActionValid(ACTION_BB));
    EXPECT_FALSE(metadata.isActionValid(ACTION_ZA));
    EXPECT_FALSE(metadata.isActionValid(ACTION_ZB));
    EXPECT_EQ(metadata.getAction(ACTION_ZA), ACTION_BA);
    EXPECT_EQ(metadata.getAction(ACTION_ZB), ACTION_BB);

    vfs_remove_mount_point(Ego::VfsPath("mp_modelmetadata"));
}

TEST_F(ModelAnimationMetadataTest, ActionDataMatchesLegacyFramePipeline)
{
    // The neutral initializeFromActionData path must reproduce the legacy
    // frame-name pipeline exactly when fed the same action ranges and effects.
    const std::initializer_list<const char*> frameNames = {
        "DA0",
        "WA0 F",
        "WA1 ALGR",
        "WA2",
        "WB0 LA",
        "WC0",
        "KC0 P"
    };

    // Legacy path: recover everything from MD2 frame-name strings.
    Ego::Graphics::AnimatedModel legacyModel = makeSyntheticModel(frameNames);
    Ego::Graphics::ModelAnimationMetadata legacy;
    legacy.initializeFromLegacyFrames(legacyModel, "synthetic", "mp_missing/copy.txt");

    // Neutral path: same frames, per-frame effects copied from the legacy result
    // (a loader sets these on the model), action ranges supplied structurally.
    Ego::Graphics::AnimatedModel neutralModel = makeSyntheticModel(frameNames);
    ASSERT_EQ(neutralModel.getFrames().size(), legacyModel.getFrames().size());
    for (size_t i = 0; i < neutralModel.getFrames().size(); ++i)
    {
        neutralModel.getFrames()[i].framefx = legacyModel.getFrames()[i].framefx;
    }

    Ego::Graphics::AnimationMetadataInput input;
    input.actions[ACTION_DA] = {true, 0, 0};
    input.actions[ACTION_WA] = {true, 1, 3};
    input.actions[ACTION_WB] = {true, 4, 4};
    input.actions[ACTION_WC] = {true, 5, 5};
    input.actions[ACTION_KC] = {true, 6, 6};

    Ego::Graphics::ModelAnimationMetadata neutral;
    neutral.initializeFromActionData(neutralModel, input);

    // Action validity, fallback resolution, ranges and effects identical for every action.
    for (int action = 0; action < ACTION_COUNT; ++action)
    {
        EXPECT_EQ(legacy.isActionValid(action), neutral.isActionValid(action)) << "action " << action;
        EXPECT_EQ(legacy.getAction(action), neutral.getAction(action)) << "action " << action;
        if (legacy.isActionValid(action))
        {
            EXPECT_EQ(legacy.getFirstFrame(action), neutral.getFirstFrame(action)) << "action " << action;
            EXPECT_EQ(legacy.getLastFrame(action), neutral.getLastFrame(action)) << "action " << action;
            EXPECT_EQ(legacy.getMadFX(legacyModel, action), neutral.getMadFX(neutralModel, action)) << "action " << action;
        }
    }

    // Walk-lip interpolation tables identical.
    for (int lip = 0; lip < LIP_COUNT; ++lip)
    {
        for (int framelip = 0; framelip < 16; ++framelip)
        {
            EXPECT_EQ(legacy.getFrameLipToWalkFrame(lip, framelip), neutral.getFrameLipToWalkFrame(lip, framelip))
                << "lip " << lip << " framelip " << framelip;
        }
    }

    // Per-frame framelip write-backs identical.
    for (size_t i = 0; i < legacyModel.getFrames().size(); ++i)
    {
        EXPECT_EQ(legacyModel.getFrames()[i].framelip, neutralModel.getFrames()[i].framelip) << "frame " << i;
    }
}

TEST_F(ModelAnimationMetadataTest, ActionDataIngestsWithoutFrameNames)
{
    // The glTF-style case: frames carry no legacy MD2 names; action ranges and
    // per-frame effects come from structured data, not frame-name parsing.
    Ego::Graphics::AnimatedModel model;
    model.getFrames().resize(4);
    for (Ego::Graphics::AnimatedModelFrame& frame : model.getFrames())
    {
        frame.framefx = EMPTY_BIT_FIELD;
    }
    model.getFrames()[1].framefx = MADFX_FOOTFALL;

    Ego::Graphics::AnimationMetadataInput input;
    input.actions[ACTION_WA] = {true, 0, 3};

    Ego::Graphics::ModelAnimationMetadata metadata;
    metadata.initializeFromActionData(model, input);

    EXPECT_TRUE(metadata.isActionValid(ACTION_WA));
    EXPECT_TRUE(metadata.isActionValid(ACTION_DA));   // engine-seeded default-stand action
    EXPECT_EQ(metadata.getFirstFrame(ACTION_WA), 0);
    EXPECT_EQ(metadata.getLastFrame(ACTION_WA), 3);
    EXPECT_TRUE(HAS_SOME_BITS(metadata.getMadFX(model, ACTION_WA), MADFX_FOOTFALL));

    // Walk-lip progression derived purely from the supplied frame range.
    EXPECT_EQ(metadata.getFrameLipToWalkFrame(LIPWA, 0), 0);
    EXPECT_EQ(metadata.getFrameLipToWalkFrame(LIPWA, 15), 3);
}

TEST_F(ObjectProfileParserTest, TestModFollowerProfileLoads)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    // Load follower.obj in lightweight mode (no 3D model, sounds, particles).
    auto profile = ObjectProfile::loadFromFile(
        "mp_objects/follower.obj", ObjectProfileRef(37), true);
    ASSERT_NE(profile, nullptr) << "follower.obj failed to load";
}

TEST_F(ObjectProfileParserTest, TestModFollowerProfileLoadsNonLightweightWithInstalledAudioService)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    s_audioSystem->reset();
    auto profile = ObjectProfile::loadFromFile(
        "mp_objects/follower.obj", ObjectProfileRef(37), false);
    ASSERT_NE(profile, nullptr) << "follower.obj failed to load in full mode";
    ASSERT_NE(profile->getModel(), nullptr);
    ASSERT_NE(profile->getModel()->getModel(), nullptr);
    EXPECT_TRUE(profile->getModel()->isActionValid(ACTION_DA));
    EXPECT_GT(profile->getModel()->getModel()->getVertexCount(), 0u);
    EXPECT_FALSE(profile->getModel()->getModel()->getFrames().empty());
    EXPECT_FALSE(profile->getModel()->getModel()->getDrawCommands().empty());
    ASSERT_FALSE(s_audioSystem->loadedSoundPaths.empty());
    EXPECT_EQ(s_audioSystem->loadedSoundPaths.front(), "mp_objects/follower.obj/sound0");
}

TEST_F(ObjectProfileParserTest, ObjectModelResolverFindsCurrentMd2Fallback)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    const Ego::Graphics::ObjectModelAsset modelAsset =
        Ego::Graphics::resolveObjectModelAsset("mp_objects/follower.obj");
    ASSERT_TRUE(modelAsset.exists);
    EXPECT_EQ(modelAsset.format, Ego::Graphics::ObjectModelFormat::Md2);
    EXPECT_EQ(modelAsset.path, "mp_objects/follower.obj/tris.md2");
}

TEST_F(ObjectProfileParserTest, ObjectModelResolverPrefersGltfThenGlbThenMd2)
{
#ifdef _WIN32
    const int processId = _getpid();
#else
    const int processId = getpid();
#endif
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("egoboo-model-resolver-" + std::to_string(processId));
    const std::filesystem::path objectDir = root / "synthetic.obj";
    std::filesystem::create_directories(objectDir);
    Ego::Test::scheduleTestDirectoryCleanup(root);

    {
        std::ofstream(objectDir / "tris.md2").put('\0');
    }

    ASSERT_NE(0, vfs_add_mount_point(root.string(), Ego::FsPath(""), Ego::VfsPath("mp_modelresolver"), 1));

    const std::string objectPath = "mp_modelresolver/synthetic.obj";
    Ego::Graphics::ObjectModelAsset modelAsset = Ego::Graphics::resolveObjectModelAsset(objectPath);
    ASSERT_TRUE(modelAsset.exists);
    EXPECT_EQ(modelAsset.format, Ego::Graphics::ObjectModelFormat::Md2);

    {
        std::ofstream(objectDir / "tris.glb").put('\0');
    }
    modelAsset = Ego::Graphics::resolveObjectModelAsset(objectPath);
    ASSERT_TRUE(modelAsset.exists);
    EXPECT_EQ(modelAsset.format, Ego::Graphics::ObjectModelFormat::Glb);

    {
        std::ofstream(objectDir / "tris.gltf").put('\0');
    }
    modelAsset = Ego::Graphics::resolveObjectModelAsset(objectPath);
    ASSERT_TRUE(modelAsset.exists);
    EXPECT_EQ(modelAsset.format, Ego::Graphics::ObjectModelFormat::Gltf);

    const Ego::Graphics::ObjectModelAsset md2Fallback =
        Ego::Graphics::resolveObjectModelAsset(objectPath, Ego::Graphics::ObjectModelFormat::Md2);
    ASSERT_TRUE(md2Fallback.exists);
    EXPECT_EQ(md2Fallback.format, Ego::Graphics::ObjectModelFormat::Md2);

    const Ego::Graphics::ObjectModelAsset loadableAsset =
        Ego::Graphics::resolveLoadableObjectModelAsset(objectPath);
    ASSERT_TRUE(loadableAsset.exists);
    EXPECT_EQ(loadableAsset.format, Ego::Graphics::ObjectModelFormat::Gltf);
    EXPECT_EQ(loadableAsset.path, objectPath + "/tris.gltf");

    vfs_remove_mount_point(Ego::VfsPath("mp_modelresolver"));
}

TEST_F(ObjectProfileParserTest, ObjectModelLoaderReportsCurrentLoadability)
{
    EXPECT_TRUE(Ego::Graphics::canLoadObjectModelFormat(Ego::Graphics::ObjectModelFormat::Md2));
    EXPECT_TRUE(Ego::Graphics::canLoadObjectModelFormat(Ego::Graphics::ObjectModelFormat::Gltf));
    EXPECT_TRUE(Ego::Graphics::canLoadObjectModelFormat(Ego::Graphics::ObjectModelFormat::Glb));
    EXPECT_FALSE(Ego::Graphics::canLoadObjectModelFormat(Ego::Graphics::ObjectModelFormat::Unknown));
}

TEST_F(ObjectProfileParserTest, ObjectModelAssetHelpersReportCurrentSearchOrder)
{
    const std::vector<Ego::Graphics::ObjectModelFormat>& order =
        Ego::Graphics::getObjectModelSearchOrder();
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], Ego::Graphics::ObjectModelFormat::Gltf);
    EXPECT_EQ(order[1], Ego::Graphics::ObjectModelFormat::Glb);
    EXPECT_EQ(order[2], Ego::Graphics::ObjectModelFormat::Md2);

    EXPECT_STREQ(Ego::Graphics::getObjectModelFileName(Ego::Graphics::ObjectModelFormat::Gltf), "tris.gltf");
    EXPECT_STREQ(Ego::Graphics::getObjectModelFileName(Ego::Graphics::ObjectModelFormat::Glb), "tris.glb");
    EXPECT_STREQ(Ego::Graphics::getObjectModelFileName(Ego::Graphics::ObjectModelFormat::Md2), "tris.md2");
    EXPECT_STREQ(Ego::Graphics::getObjectModelFileName(Ego::Graphics::ObjectModelFormat::Unknown), "");
    EXPECT_EQ(Ego::Graphics::describeObjectModelSearchOrder(), "tris.gltf, tris.glb, tris.md2");
}

TEST_F(ObjectProfileParserTest, ObjectModelLoadableResolverReturnsPreferredGltfAsset)
{
#ifdef _WIN32
    const int processId = _getpid();
#else
    const int processId = getpid();
#endif
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("egoboo-model-loadable-resolver-" + std::to_string(processId));
    const std::filesystem::path objectDir = root / "future.obj";
    std::filesystem::create_directories(objectDir);
    Ego::Test::scheduleTestDirectoryCleanup(root);

    {
        std::ofstream(objectDir / "tris.glb").put('\0');
        std::ofstream(objectDir / "tris.gltf").put('\0');
    }

    ASSERT_NE(0, vfs_add_mount_point(root.string(), Ego::FsPath(""), Ego::VfsPath("mp_modelloadable"), 1));

    const std::string objectPath = "mp_modelloadable/future.obj";
    const Ego::Graphics::ObjectModelAsset preferredAsset =
        Ego::Graphics::resolveObjectModelAsset(objectPath);
    ASSERT_TRUE(preferredAsset.exists);
    EXPECT_EQ(preferredAsset.format, Ego::Graphics::ObjectModelFormat::Gltf);
    EXPECT_EQ(preferredAsset.path, objectPath + "/tris.gltf");

    const Ego::Graphics::ObjectModelAsset loadableAsset =
        Ego::Graphics::resolveLoadableObjectModelAsset(objectPath);
    ASSERT_TRUE(loadableAsset.exists);
    EXPECT_EQ(loadableAsset.format, Ego::Graphics::ObjectModelFormat::Gltf);
    EXPECT_EQ(loadableAsset.path, objectPath + "/tris.gltf");
    EXPECT_EQ(Ego::Graphics::loadObjectModelAsset(preferredAsset), nullptr);

    vfs_remove_mount_point(Ego::VfsPath("mp_modelloadable"));
}

TEST_F(ObjectProfileParserTest, ObjectModelLoaderLoadsSyntheticGltf)
{
#ifdef _WIN32
    const int processId = _getpid();
#else
    const int processId = getpid();
#endif
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("egoboo-model-gltf-" + std::to_string(processId));
    const std::filesystem::path objectDir = root / "synthetic.obj";
    std::filesystem::create_directories(objectDir);
    Ego::Test::scheduleTestDirectoryCleanup(root);

    writeSyntheticGltf(objectDir, "tris.gltf", true);

    ASSERT_NE(0, vfs_add_mount_point(root.string(), Ego::FsPath(""), Ego::VfsPath("mp_modelgltf"), 1));

    const std::string objectPath = "mp_modelgltf/synthetic.obj";
    const Ego::Graphics::ObjectModelAsset asset =
        Ego::Graphics::resolveObjectModelAsset(objectPath);
    ASSERT_TRUE(asset.exists);
    EXPECT_EQ(asset.format, Ego::Graphics::ObjectModelFormat::Gltf);

    expectSyntheticModelLoaded(Ego::Graphics::loadObjectModel(asset));

    Ego::ModelDescriptor descriptor(objectPath);
    EXPECT_TRUE(descriptor.isActionValid(ACTION_DA));
    ASSERT_NE(descriptor.getModel(), nullptr);
    EXPECT_EQ(descriptor.getModel()->getVertexCount(), 3u);

    vfs_remove_mount_point(Ego::VfsPath("mp_modelgltf"));
}

TEST_F(ObjectProfileParserTest, ObjectModelLoaderLoadsSyntheticGlb)
{
#ifdef _WIN32
    const int processId = _getpid();
#else
    const int processId = getpid();
#endif
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("egoboo-model-glb-" + std::to_string(processId));
    const std::filesystem::path objectDir = root / "synthetic.obj";
    std::filesystem::create_directories(objectDir);
    Ego::Test::scheduleTestDirectoryCleanup(root);

    writeSyntheticGlb(objectDir, true);

    ASSERT_NE(0, vfs_add_mount_point(root.string(), Ego::FsPath(""), Ego::VfsPath("mp_modelglb"), 1));

    const std::string objectPath = "mp_modelglb/synthetic.obj";
    const Ego::Graphics::ObjectModelAsset asset =
        Ego::Graphics::resolveObjectModelAsset(objectPath);
    ASSERT_TRUE(asset.exists);
    EXPECT_EQ(asset.format, Ego::Graphics::ObjectModelFormat::Glb);

    expectSyntheticModelLoaded(Ego::Graphics::loadObjectModel(asset));

    vfs_remove_mount_point(Ego::VfsPath("mp_modelglb"));
}

TEST_F(ObjectProfileParserTest, ObjectModelLoaderUsesSingleFrameFallbackWithoutExtras)
{
#ifdef _WIN32
    const int processId = _getpid();
#else
    const int processId = getpid();
#endif
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("egoboo-model-gltf-no-extras-" + std::to_string(processId));
    const std::filesystem::path objectDir = root / "synthetic.obj";
    std::filesystem::create_directories(objectDir);
    Ego::Test::scheduleTestDirectoryCleanup(root);

    writeSyntheticGltf(objectDir, "tris.gltf", false);

    ASSERT_NE(0, vfs_add_mount_point(root.string(), Ego::FsPath(""), Ego::VfsPath("mp_modelgltfnoextras"), 1));

    const std::string objectPath = "mp_modelgltfnoextras/synthetic.obj";
    const Ego::Graphics::ObjectModelLoadResult result =
        Ego::Graphics::loadObjectModel(Ego::Graphics::resolveObjectModelAsset(objectPath));

    ASSERT_NE(result.model, nullptr);
    ASSERT_TRUE(result.animationMetadata.has_value());
    ASSERT_EQ(result.model->getFrames().size(), 1u);
    EXPECT_STREQ(result.model->getFrames()[0].name, "DA");

    Ego::Graphics::ModelAnimationMetadata metadata;
    metadata.initializeFromActionData(*result.model, *result.animationMetadata);
    EXPECT_TRUE(metadata.isActionValid(ACTION_DA));

    vfs_remove_mount_point(Ego::VfsPath("mp_modelgltfnoextras"));
}

TEST_F(ObjectProfileParserTest, ModelDescriptorMissingModelErrorNamesAllCandidates)
{
#ifdef _WIN32
    const int processId = _getpid();
#else
    const int processId = getpid();
#endif
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("egoboo-model-missing-" + std::to_string(processId));
    const std::filesystem::path objectDir = root / "missing.obj";
    std::filesystem::create_directories(objectDir);
    Ego::Test::scheduleTestDirectoryCleanup(root);

    ASSERT_NE(0, vfs_add_mount_point(root.string(), Ego::FsPath(""), Ego::VfsPath("mp_modelmissing"), 1));

    try
    {
        Ego::ModelDescriptor descriptor("mp_modelmissing/missing.obj");
        FAIL() << "ModelDescriptor should reject object folders without a model asset";
    }
    catch (const std::runtime_error& error)
    {
        const std::string message = error.what();
        EXPECT_NE(message.find("expected one of: tris.gltf, tris.glb, tris.md2"), std::string::npos);
    }

    vfs_remove_mount_point(Ego::VfsPath("mp_modelmissing"));
}

TEST_F(ObjectProfileParserTest, ModelDescriptorInvalidGltfNamesPreferredAsset)
{
#ifdef _WIN32
    const int processId = _getpid();
#else
    const int processId = getpid();
#endif
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("egoboo-model-unsupported-" + std::to_string(processId));
    const std::filesystem::path objectDir = root / "future.obj";
    std::filesystem::create_directories(objectDir);
    Ego::Test::scheduleTestDirectoryCleanup(root);

    {
        std::ofstream(objectDir / "tris.gltf").put('\0');
    }

    ASSERT_NE(0, vfs_add_mount_point(root.string(), Ego::FsPath(""), Ego::VfsPath("mp_modelunsupported"), 1));

    try
    {
        Ego::ModelDescriptor descriptor("mp_modelunsupported/future.obj");
        FAIL() << "ModelDescriptor should reject malformed glTF assets";
    }
    catch (const std::runtime_error& error)
    {
        const std::string message = error.what();
        EXPECT_NE(message.find("Unable to load model file: mp_modelunsupported/future.obj/tris.gltf"), std::string::npos);
        EXPECT_NE(message.find("(glTF)"), std::string::npos);
    }

    vfs_remove_mount_point(Ego::VfsPath("mp_modelunsupported"));
}

TEST_F(ObjectProfileParserTest, TestModFollowerClassName)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    auto profile = ObjectProfile::loadFromFile(
        "mp_objects/follower.obj", ObjectProfileRef(37), true);
    ASSERT_NE(profile, nullptr);

    // data.txt says "Healer" (first char capitalized by parser).
    EXPECT_EQ(profile->getClassName(), "Healer");
}

TEST_F(ObjectProfileParserTest, TestModFollowerGender)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    auto profile = ObjectProfile::loadFromFile(
        "mp_objects/follower.obj", ObjectProfileRef(37), true);
    ASSERT_NE(profile, nullptr);

    // data.txt says "FEMALE".
    EXPECT_EQ(profile->getGender(), GenderProfile::Female);
}

TEST_F(ObjectProfileParserTest, TestModFollowerPhysicalAttributes)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    auto profile = ObjectProfile::loadFromFile(
        "mp_objects/follower.obj", ObjectProfileRef(37), true);
    ASSERT_NE(profile, nullptr);

    // Size: 1.10
    EXPECT_NEAR(profile->getSize(), 1.10f, 0.01f);
    // Shadow size: 25
    EXPECT_NEAR(profile->getShadowSize(), 25.0f, 0.5f);
    // Bump height: 90
    EXPECT_NEAR(profile->getBumpHeight(), 90.0f, 0.5f);
    // Weight: 90
    EXPECT_EQ(profile->getWeight(), 90);
    // Jump power: 10.0
    EXPECT_NEAR(profile->getJumpPower(), 10.0f, 0.01f);
    // Max ammo: 0
    EXPECT_EQ(profile->getMaxAmmo(), 0);
}

TEST_F(ObjectProfileParserTest, TestModFollowerFlags)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    auto profile = ObjectProfile::loadFromFile(
        "mp_objects/follower.obj", ObjectProfileRef(37), true);
    ASSERT_NE(profile, nullptr);

    // data.txt flags for follower.obj:
    EXPECT_FALSE(profile->isItem());
    EXPECT_FALSE(profile->isMount());
    EXPECT_FALSE(profile->isStackable());
    EXPECT_FALSE(profile->isInvincible());
    EXPECT_FALSE(profile->isPlatform());
    EXPECT_TRUE(profile->isNameKnown());
    EXPECT_TRUE(profile->canGrabMoney());
}

TEST_F(ObjectProfileParserTest, GlobalIronBallTransferBlendingFlag)
{
    auto profile = ObjectProfile::loadFromFile(
        "mp_data/globalobjects/misc/ironball.obj", ObjectProfileRef(51), true);
    ASSERT_NE(profile, nullptr);

    EXPECT_FALSE(profile->transferBlending());
}

// ===========================================================================
//  level.mpd parser tests
// ===========================================================================

class MapFileParserTest : public ContentParserFixture {};

TEST_F(MapFileParserTest, TestModLevelMpdLoads)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    map_t map;
    bool loaded = map.load("mp_data/level.mpd");
    EXPECT_TRUE(loaded) << "level.mpd failed to parse";
}

TEST_F(MapFileParserTest, TestModLevelMpdDimensions)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    map_t map;
    ASSERT_TRUE(map.load("mp_data/level.mpd"));

    // The map must have non-zero tile dimensions.
    EXPECT_GT(map._info.getTileCountX(), 0u);
    EXPECT_GT(map._info.getTileCountY(), 0u);
    EXPECT_GT(map._info.getVertexCount(), 0u);

    // The total tile count should be tileX * tileY.
    EXPECT_EQ(map._info.getTileCount(),
              map._info.getTileCountX() * map._info.getTileCountY());
}

TEST_F(MapFileParserTest, TestModLevelMpdTileMemory)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    map_t map;
    ASSERT_TRUE(map.load("mp_data/level.mpd"));

    // After a successful load, tile and vertex vectors must match the info.
    EXPECT_EQ(map._mem.tiles.size(),
              static_cast<size_t>(map._info.getTileCount()));
    EXPECT_EQ(map._mem.vertices.size(),
              static_cast<size_t>(map._info.getVertexCount()));
}

TEST_F(MapFileParserTest, TestModLevelMpdSaneBounds)
{
    auto mod = findModule("test.mod");
    ASSERT_NE(mod, nullptr);
    mountModule(*mod);

    map_t map;
    ASSERT_TRUE(map.load("mp_data/level.mpd"));

    // Tile dimensions should be within engine limits.
    EXPECT_LE(map._info.getTileCountX(), MAP_TILE_MAX_X);
    EXPECT_LE(map._info.getTileCountY(), MAP_TILE_MAX_Y);
    EXPECT_LE(map._info.getVertexCount(), MAP_VERTICES_MAX);
}
