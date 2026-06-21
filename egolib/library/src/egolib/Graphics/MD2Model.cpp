#include "egolib/Graphics/MD2Model.hpp"

#include "egolib/FileFormats/id_md2.h"
#include "egolib/Log/_Include.hpp"
#include "egolib/_math.h"
#include "egolib/vfs.h"

#include <algorithm>
#include <cmath>
#include <cstring>
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

} // namespace

std::shared_ptr<Ego::Graphics::AnimatedModel> MD2Model::loadFromFile(const std::string& fileName)
{
    id_md2_header_t md2Header;

    vfs_FILE* f = vfs_openRead(fileName);
    if (!f)
    {
        Log::activeTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to open model file ", "`", fileName, "`", Log::EndOfEntry);
        return nullptr;
    }

    vfs_read(&md2Header, sizeof(md2Header), 1, f);

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
        vfs_close(f);
        Log::activeTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "model ", "`", fileName, "`", " does not have valid header or identifier", Log::EndOfEntry);
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
            vfs_read(&commands, sizeof(int32_t), 1, f);
            cmdSize += sizeof(int32_t) / sizeof(int32_t);

            commands = Endian_FileToHost(commands);
            if (0 == commands || cmdSize == md2Header.size_glcmds)
            {
                break;
            }

            Ego::Graphics::AnimatedModelDrawCommand command;
            const int32_t commandCount = commands > 0 ? commands : -commands;
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

    vfs_close(f);
    return model;
}
