#include "egolib/Graphics/MD2Model.hpp"

#include "egolib/FileFormats/id_md2.h"
#include "egolib/Log/_Include.hpp"
#include "egolib/_math.h"
#include "egolib/vfs.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace
{

/// The 162 legacy MD2/Quake2 quantized normals plus one zero sentinel (index 162).
/// This palette is MD2-format-private: the runtime AnimatedModel carries explicit
/// normals and a precomputed environment-map coordinate, not a palette index.
constexpr size_t LEGACY_NORMAL_COUNT = 163;

const float LEGACY_MODEL_NORMALS[LEGACY_NORMAL_COUNT][3] =
{
#include "egolib/FileFormats/id_normals.inl"
    , {0, 0, 0}
};

float legacyNormal(size_t normal, size_t index)
{
    normal = std::min(normal, LEGACY_NORMAL_COUNT - 1);
    index = std::min<size_t>(index, 2);
    return LEGACY_MODEL_NORMALS[normal][index];
}

/// Count/offset validation caps for MD2Model::loadFromFile.
///
/// A scratch script scanned every shipped tris.md2 under data/ (956 files, 2026-08-17) - the
/// only filename the runtime ever resolves for the Md2 format (see
/// getObjectModelFileName, ObjectModelAsset.cpp) - for the largest value of each
/// header field actually in use, then each cap below was set to whichever is larger: the classic
/// Quake2/MD2 format ceiling (id_md2.h's MD2_MAX_* constants) or roughly 10x that shipped
/// maximum. The classic ceilings are not safe to use uniformly: the largest shipped num_st is
/// 2448 (data/basicdat/globalobjects/misc/swampplant.obj/tris.md2), which already exceeds
/// MD2_MAX_TEXCOORDS (2048) - this content set routinely exceeds that one classic limit, so
/// enforcing it here would reject a real shipped asset. The shipped-max-derived headroom
/// dominates every field once the offset/count math is done (10x a nonzero shipped maximum
/// outgrows these particular classic ceilings before landing anywhere near a header value that
/// could plausibly wrap or exhaust memory). (data/ also holds 6 stray non-tris.md2 files that the
/// runtime never loads - arch.MD2, 2x OLDtris.md2, tris_old.md2, tris_new.md2, weapon.md2 - the
/// largest of which reaches num_frames 198; every cap below still clears that with room to spare.)
///   num_skins:    shipped max 6    (pets/blacksheep.obj)   -> cap 64
///   num_vertices: shipped max 986  (misc/smithy.obj)       -> cap 10000
///   num_st:       shipped max 2448 (misc/swampplant.obj)   -> cap 25000
///   num_tris:     shipped max 958  (misc/smithy.obj)       -> cap 10000
///   num_frames:   shipped max 141  (players/rogue.obj)     -> cap 1500
///   size_glcmds:  shipped max 4478 (misc/smithy.obj)       -> cap 50000
constexpr int32_t MAX_HEADER_SKINS = 64;
constexpr int32_t MAX_HEADER_VERTICES = 10000;
constexpr int32_t MAX_HEADER_TEXCOORDS = 25000;
constexpr int32_t MAX_HEADER_TRIANGLES = 10000;
constexpr int32_t MAX_HEADER_FRAMES = 1500;
constexpr int32_t MAX_HEADER_GLCMDS_SIZE = 50000;

} // namespace

std::shared_ptr<Ego::Graphics::AnimatedModel> MD2Model::loadFromFile(const std::string& fileName)
{
    id_md2_header_t md2Header;

    // RAII wrapper, mirroring the vfs_FILE idiom in wawalite_file.c's wawalite_data_write and
    // script.c's RuntimeStatistics::append: every early return below closes the file
    // automatically instead of relying on a manual vfs_close() at each exit point.
    std::shared_ptr<vfs_FILE> file(vfs_openRead(fileName),
                                    [](vfs_FILE* f) { if (f) vfs_close(f); });
    if (!file)
    {
        Log::activeTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to open model file ", "`", fileName, "`", Log::EndOfEntry);
        return nullptr;
    }

    vfs_FILE* const f = file.get();

    // Same nullptr + log_warning contract as the magic/version check below - shared here so a
    // truncated read (checked immediately) and an invalid magic/version/count/offset (checked
    // further down) report identically.
    const auto logInvalidHeader = [&fileName]()
    {
        Log::activeTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "model ", "`", fileName, "`", " does not have valid header or identifier", Log::EndOfEntry);
    };

    if (vfs_read(&md2Header, sizeof(md2Header), 1, f) != 1)
    {
        // A short read leaves md2Header as uninitialized stack garbage - do not look at any of
        // its fields, just reject the file the same way an invalid magic number is rejected.
        logInvalidHeader();
        return nullptr;
    }

    md2Header.ident            = Endian_FileToHost(md2Header.ident);
    md2Header.version          = Endian_FileToHost(md2Header.version);
    md2Header.skinwidth        = Endian_FileToHost(md2Header.skinwidth);
    md2Header.skinheight       = Endian_FileToHost(md2Header.skinheight);
    md2Header.framesize        = Endian_FileToHost(md2Header.framesize);
    md2Header.num_skins        = Endian_FileToHost(md2Header.num_skins);
    md2Header.num_vertices     = Endian_FileToHost(md2Header.num_vertices);
    md2Header.num_st           = Endian_FileToHost(md2Header.num_st);
    md2Header.num_tris         = Endian_FileToHost(md2Header.num_tris);
    md2Header.size_glcmds      = Endian_FileToHost(md2Header.size_glcmds);
    md2Header.num_frames       = Endian_FileToHost(md2Header.num_frames);
    md2Header.offset_skins     = Endian_FileToHost(md2Header.offset_skins);
    md2Header.offset_st        = Endian_FileToHost(md2Header.offset_st);
    md2Header.offset_tris      = Endian_FileToHost(md2Header.offset_tris);
    md2Header.offset_frames    = Endian_FileToHost(md2Header.offset_frames);
    md2Header.offset_glcmds    = Endian_FileToHost(md2Header.offset_glcmds);
    md2Header.offset_end       = Endian_FileToHost(md2Header.offset_end);

    if (md2Header.ident != MD2_MAGIC_NUMBER || md2Header.version != MD2_VERSION)
    {
        logInvalidHeader();
        return nullptr;
    }

    // Every container size below is taken directly from one of these six int32_t header
    // fields (size_glcmds bounds the per-command resizes further down, inside the glcmds loop -
    // see the matching guard there). A hostile or corrupt file can make any of them negative
    // (wraps to an enormous size_t and throws std::length_error) or absurdly large-but-positive
    // (throws std::bad_alloc) before a single byte of geometry is read. Reject up front instead,
    // using the same nullptr + log_warning contract as an invalid magic number.
    if (md2Header.num_skins < 0    || md2Header.num_skins > MAX_HEADER_SKINS
     || md2Header.num_vertices < 0 || md2Header.num_vertices > MAX_HEADER_VERTICES
     || md2Header.num_st < 0       || md2Header.num_st > MAX_HEADER_TEXCOORDS
     || md2Header.num_tris < 0     || md2Header.num_tris > MAX_HEADER_TRIANGLES
     || md2Header.num_frames < 0   || md2Header.num_frames > MAX_HEADER_FRAMES
     || md2Header.size_glcmds < 0  || md2Header.size_glcmds > MAX_HEADER_GLCMDS_SIZE)
    {
        logInvalidHeader();
        return nullptr;
    }

    // Offsets are only ever used to vfs_seek() ahead of a vfs_read() into a buffer already
    // sized from the (now-validated) counts above, never as a raw pointer or index, so a bad
    // offset cannot corrupt memory: vfs_seek()/PHYSFS_seek() on an out-of-range offset simply
    // fails (or lands past EOF), and the following vfs_read() then returns fewer bytes than
    // requested (vfs_io.c's vfs_read, PHYSFS_readBytes) rather than reading out of bounds. This
    // check only rejects an offset that starts past the end of the file; an offset landing
    // exactly at EOF, or an in-bounds offset whose section runs past EOF, still falls through to
    // the pre-existing short-read tolerance (the following vfs_read() simply returns fewer bytes
    // than requested). offset_glcmds is checked only when size_glcmds > 0, matching the guard
    // already below that skips the whole glcmds section otherwise.
    const long fileLength = vfs_fileLength(f);
    if (md2Header.offset_skins < 0  || md2Header.offset_skins > fileLength
     || md2Header.offset_st < 0     || md2Header.offset_st > fileLength
     || md2Header.offset_tris < 0   || md2Header.offset_tris > fileLength
     || md2Header.offset_frames < 0 || md2Header.offset_frames > fileLength
     || (md2Header.size_glcmds > 0 && (md2Header.offset_glcmds < 0 || md2Header.offset_glcmds > fileLength)))
    {
        logInvalidHeader();
        return nullptr;
    }

    auto model = std::make_shared<Ego::Graphics::AnimatedModel>();
    model->setVertexCount(md2Header.num_vertices);

    std::vector<id_md2_texcoord_t> texCoords(md2Header.num_st);
    vfs_seek(f, md2Header.offset_st);
    for (id_md2_texcoord_t& texCoord : texCoords)
    {
        vfs_read(&texCoord, sizeof(texCoord), 1, f);
        texCoord.s = Endian_FileToHost(texCoord.s);
        texCoord.t = Endian_FileToHost(texCoord.t);
    }

    std::vector<id_md2_triangle_t> triangles(md2Header.num_tris);
    vfs_seek(f, md2Header.offset_tris);
    vfs_read(triangles.data(), sizeof(id_md2_triangle_t), md2Header.num_tris, f);
    for (id_md2_triangle_t& triangle : triangles)
    {
        for (size_t vertex = 0; vertex < 3; ++vertex)
        {
            triangle.vertex[vertex] = Endian_FileToHost(triangle.vertex[vertex]);
            triangle.st[vertex] = Endian_FileToHost(triangle.st[vertex]);
        }
    }

    std::vector<id_md2_skin_t> skins(md2Header.num_skins);
    vfs_seek(f, md2Header.offset_skins);
    vfs_read(skins.data(), sizeof(id_md2_skin_t), md2Header.num_skins, f);

    auto& frames = model->getFrames();
    frames.resize(md2Header.num_frames);
    for (Ego::Graphics::AnimatedModelFrame& frame : frames)
    {
        frame.vertexList.resize(md2Header.num_vertices);
    }

    vfs_seek(f, md2Header.offset_frames);
    for (Ego::Graphics::AnimatedModelFrame& frame : frames)
    {
        id_md2_frame_header_t frameHeader;
        vfs_read(&frameHeader, sizeof(frameHeader), 1, f);

        frameHeader.scale[0] = Endian_FileToHost(frameHeader.scale[0]);
        frameHeader.scale[1] = Endian_FileToHost(frameHeader.scale[1]);
        frameHeader.scale[2] = Endian_FileToHost(frameHeader.scale[2]);

        frameHeader.translate[0] = Endian_FileToHost(frameHeader.translate[0]);
        frameHeader.translate[1] = Endian_FileToHost(frameHeader.translate[1]);
        frameHeader.translate[2] = Endian_FileToHost(frameHeader.translate[2]);

        bool boundingBoxFound = false;
        for (Ego::Graphics::AnimatedModelVertex& vertex : frame.vertexList)
        {
            id_md2_vertex_t frameVertex;
            vfs_read(&frameVertex, sizeof(id_md2_vertex_t), 1, f);

            vertex.pos[kX] = frameVertex.v[0] * frameHeader.scale[0] + frameHeader.translate[0];
            vertex.pos[kY] = frameVertex.v[1] * frameHeader.scale[1] + frameHeader.translate[1];
            vertex.pos[kZ] = frameVertex.v[2] * frameHeader.scale[2] + frameHeader.translate[2];

            const size_t normalIndex = std::min<size_t>(frameVertex.normalIndex, MD2_MAX_NORMALS);
            vertex.nrm[kX] = legacyNormal(normalIndex, 0);
            vertex.nrm[kY] = legacyNormal(normalIndex, 1);
            vertex.nrm[kZ] = legacyNormal(normalIndex, 2);

            // Precompute the environment-map U coordinate from the (pre-scale) normal,
            // identical to the retired indextoenvirox[normalIndex] table lookup.
            vertex.envU = std::atan2(vertex.nrm[kY], vertex.nrm[kX]) * idlib::inv_two_pi<float>();

            const oct_vec_v2_t ovec(vertex.pos);
            if (!boundingBoxFound)
            {
                frame.bb = oct_bb_t(ovec);
                boundingBoxFound = true;
            }
            else
            {
                frame.bb.join(ovec);
            }
        }

        strncpy(frame.name, frameHeader.name, 16);
    }

    if (md2Header.size_glcmds > 0)
    {
        vfs_seek(f, md2Header.offset_glcmds);

        int32_t cmdSize = 0;
        while (cmdSize < md2Header.size_glcmds)
        {
            int32_t commands;
            if (vfs_read(&commands, sizeof(int32_t), 1, f) != 1)
            {
                // Same short-read contract as the header itself: a truncated glcmds section
                // gives no trustworthy count to read further, so reject rather than act on
                // whatever happens to be left in `commands`.
                logInvalidHeader();
                return nullptr;
            }
            cmdSize += sizeof(int32_t) / sizeof(int32_t);

            commands = Endian_FileToHost(commands);
            if (0 == commands || cmdSize == md2Header.size_glcmds)
            {
                break;
            }

            // `commands` is per-iteration file data, not a header field, so it is not covered by
            // the header's count/offset guard above - but it feeds a container size exactly the
            // same way, so it needs the same treatment. Two hazards: negating INT32_MIN is
            // signed-overflow UB (there is no positive int32_t magnitude for it), and an
            // otherwise-in-range commandCount can still ask for more id_glcmd_packed_t entries
            // than the (now-capped) size_glcmds has room left for. size_glcmds itself is a
            // natural cap on any single command's word count: a command block can never
            // legitimately hold more packets than the section has words remaining for.
            if (commands == std::numeric_limits<int32_t>::min())
            {
                logInvalidHeader();
                return nullptr;
            }

            const int32_t commandCount = commands > 0 ? commands : -commands;
            const int32_t glcmdsWordsPerCommand =
                static_cast<int32_t>(sizeof(id_glcmd_packed_t) / sizeof(int32_t));
            const int32_t remainingGlcmdsWords = md2Header.size_glcmds - cmdSize;
            if (commandCount < 0 || commandCount > remainingGlcmdsWords / glcmdsWordsPerCommand)
            {
                logInvalidHeader();
                return nullptr;
            }

            Ego::Graphics::AnimatedModelDrawCommand command;
            command.primitiveMode = commands > 0
                                  ? Ego::Graphics::AnimatedModelPrimitiveMode::TriangleStrip
                                  : Ego::Graphics::AnimatedModelPrimitiveMode::TriangleFan;
            command.data.resize(commandCount);

            std::vector<id_glcmd_packed_t> rawCommandData(commandCount);
            vfs_read(rawCommandData.data(), sizeof(id_glcmd_packed_t), commandCount, f);
            cmdSize += (sizeof(id_glcmd_packed_t) * commandCount) / sizeof(uint32_t);

            for (size_t i = 0; i < rawCommandData.size(); ++i)
            {
                id_glcmd_packed_t& raw = rawCommandData[i];
                raw.index = Endian_FileToHost(raw.index);
                raw.s = Endian_FileToHost(raw.s);
                raw.t = Endian_FileToHost(raw.t);

                command.data[i].vertexIndex = raw.index;
                command.data[i].s = raw.s - (0.5f / 64.0f);
                command.data[i].t = raw.t - (0.5f / 64.0f);
            }

            model->prependDrawCommand(std::move(command));
        }
    }

    return model;
}
