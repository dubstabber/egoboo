#include "egolib/Graphics/GltfModel.hpp"

#include "egolib/Graphics/AnimatedModel.hpp"
#include "egolib/Graphics/GltfModel_internal.hpp"
#include "egolib/Log/_Include.hpp"
#include "egolib/_math.h"
#include "egolib/vfs.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{

using Ego::Graphics::AnimatedModel;
using Ego::Graphics::AnimatedModelDrawCommand;
using Ego::Graphics::AnimatedModelDrawVertex;
using Ego::Graphics::AnimatedModelFrame;
using Ego::Graphics::AnimatedModelPrimitiveMode;
using Ego::Graphics::AnimatedModelVertex;
using Ego::Graphics::AnimationMetadataInput;
using Ego::Graphics::GltfModelDetail::FramePlan;
using Ego::Graphics::GltfModelDetail::parseMetadataExtras;
using Ego::Graphics::ObjectModelLoadResult;

struct FileBuffer
{
    char* bytes = nullptr;
    size_t size = 0;

    ~FileBuffer()
    {
        std::free(bytes);
    }

    FileBuffer() = default;
    FileBuffer(const FileBuffer&) = delete;
    FileBuffer& operator=(const FileBuffer&) = delete;
};

bool readVfsFile(const std::string& path, FileBuffer& out)
{
    char* data = nullptr;
    size_t length = 0;
    if (!vfs_readEntireFile(path, &data, &length))
    {
        return false;
    }

    out.bytes = data;
    out.size = length;
    return true;
}

cgltf_result readCgltfFile(const cgltf_memory_options*, const cgltf_file_options*, const char* path, cgltf_size* size, void** data)
{
    FileBuffer buffer;
    if (!readVfsFile(path, buffer))
    {
        return cgltf_result_file_not_found;
    }

    const cgltf_size expectedSize = size ? *size : 0;
    if (expectedSize != 0 && buffer.size < expectedSize)
    {
        return cgltf_result_data_too_short;
    }

    if (size)
    {
        *size = buffer.size;
    }
    *data = buffer.bytes;
    buffer.bytes = nullptr;
    buffer.size = 0;
    return cgltf_result_success;
}

void releaseCgltfFile(const cgltf_memory_options*, const cgltf_file_options*, void* data)
{
    std::free(data);
}

const char* cgltfResultName(cgltf_result result)
{
    switch (result)
    {
        case cgltf_result_success: return "success";
        case cgltf_result_data_too_short: return "data too short";
        case cgltf_result_unknown_format: return "unknown format";
        case cgltf_result_invalid_json: return "invalid json";
        case cgltf_result_invalid_gltf: return "invalid gltf";
        case cgltf_result_invalid_options: return "invalid options";
        case cgltf_result_file_not_found: return "file not found";
        case cgltf_result_io_error: return "io error";
        case cgltf_result_out_of_memory: return "out of memory";
        case cgltf_result_legacy_gltf: return "legacy gltf";
        case cgltf_result_max_enum:
        default:
            return "unknown error";
    }
}

void logWarning(const std::string& fileName, const std::string& detail)
{
    if (Log::Target* target = Log::tryActiveTarget())
    {
        *target << Log::Entry::create(Log::Level::Warning,
                                      __FILE__,
                                      __LINE__,
                                      "unable to load glTF model `",
                                      fileName,
                                      "`: ",
                                      detail,
                                      Log::EndOfEntry);
    }
}

bool almostEqual(float a, float b)
{
    return std::fabs(a - b) <= 0.000001f;
}

bool isIdentityMatrix(const cgltf_float* matrix)
{
    for (size_t row = 0; row < 4; ++row)
    {
        for (size_t column = 0; column < 4; ++column)
        {
            const float expected = row == column ? 1.0f : 0.0f;
            if (!almostEqual(matrix[row * 4 + column], expected))
            {
                return false;
            }
        }
    }
    return true;
}

bool hasUnsupportedNodeTransform(const cgltf_data& data)
{
    for (cgltf_size i = 0; i < data.nodes_count; ++i)
    {
        const cgltf_node& node = data.nodes[i];
        if (!node.mesh)
        {
            continue;
        }

        if (node.has_matrix && !isIdentityMatrix(node.matrix))
        {
            return true;
        }
        if (node.has_translation &&
            (!almostEqual(node.translation[0], 0.0f) ||
             !almostEqual(node.translation[1], 0.0f) ||
             !almostEqual(node.translation[2], 0.0f)))
        {
            return true;
        }
        if (node.has_scale &&
            (!almostEqual(node.scale[0], 1.0f) ||
             !almostEqual(node.scale[1], 1.0f) ||
             !almostEqual(node.scale[2], 1.0f)))
        {
            return true;
        }
        if (node.has_rotation &&
            (!almostEqual(node.rotation[0], 0.0f) ||
             !almostEqual(node.rotation[1], 0.0f) ||
             !almostEqual(node.rotation[2], 0.0f) ||
             !almostEqual(node.rotation[3], 1.0f)))
        {
            return true;
        }
    }
    return false;
}

bool validateAccessor(const cgltf_accessor* accessor,
                      cgltf_type type,
                      cgltf_component_type componentType,
                      const std::string& name,
                      std::string& error)
{
    if (!accessor)
    {
        error = "missing " + name + " accessor";
        return false;
    }
    if (accessor->type != type || accessor->component_type != componentType || accessor->is_sparse)
    {
        error = "unsupported " + name + " accessor layout";
        return false;
    }
    if (!accessor->buffer_view || !accessor->buffer_view->buffer || !accessor->buffer_view->buffer->data)
    {
        error = name + " accessor has no loaded buffer data";
        return false;
    }
    if (accessor->buffer_view->has_meshopt_compression)
    {
        error = name + " accessor uses unsupported meshopt compression";
        return false;
    }
    return true;
}

bool validateOptionalAccessor(const cgltf_accessor* accessor,
                              cgltf_type type,
                              cgltf_component_type componentType,
                              const std::string& name,
                              std::string& error)
{
    return !accessor || validateAccessor(accessor, type, componentType, name, error);
}

bool validateIndexAccessor(const cgltf_accessor* accessor, std::string& error)
{
    if (!accessor)
    {
        return true;
    }
    if (accessor->type != cgltf_type_scalar || accessor->is_sparse)
    {
        error = "unsupported index accessor layout";
        return false;
    }
    if (accessor->component_type != cgltf_component_type_r_8u &&
        accessor->component_type != cgltf_component_type_r_16u &&
        accessor->component_type != cgltf_component_type_r_32u)
    {
        error = "unsupported index component type";
        return false;
    }
    if (!accessor->buffer_view || !accessor->buffer_view->buffer || !accessor->buffer_view->buffer->data)
    {
        error = "index accessor has no loaded buffer data";
        return false;
    }
    if (accessor->buffer_view->has_meshopt_compression)
    {
        error = "index accessor uses unsupported meshopt compression";
        return false;
    }
    return true;
}

void normalizeNormal(float normal[3])
{
    const float length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
    if (length <= 0.000001f)
    {
        normal[0] = 0.0f;
        normal[1] = 0.0f;
        normal[2] = 0.0f;
        return;
    }

    normal[0] /= length;
    normal[1] /= length;
    normal[2] /= length;
}

bool appendPrimitiveVertices(const cgltf_primitive& primitive,
                             AnimatedModelFrame& frame,
                             size_t& vertexBase,
                             std::string& error)
{
    const cgltf_accessor* positions = cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
    const cgltf_accessor* normals = cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0);

    if (!validateAccessor(positions, cgltf_type_vec3, cgltf_component_type_r_32f, "POSITION", error) ||
        !validateOptionalAccessor(normals, cgltf_type_vec3, cgltf_component_type_r_32f, "NORMAL", error))
    {
        return false;
    }
    if (normals && normals->count != positions->count)
    {
        error = "NORMAL count does not match POSITION count";
        return false;
    }

    const size_t oldSize = frame.vertexList.size();
    frame.vertexList.resize(oldSize + positions->count);

    bool boundingBoxFound = oldSize != 0;
    for (cgltf_size i = 0; i < positions->count; ++i)
    {
        float position[3] = {0.0f, 0.0f, 0.0f};
        if (!cgltf_accessor_read_float(positions, i, position, 3))
        {
            error = "failed to read POSITION accessor";
            return false;
        }

        float normal[3] = {0.0f, 0.0f, 0.0f};
        if (normals && !cgltf_accessor_read_float(normals, i, normal, 3))
        {
            error = "failed to read NORMAL accessor";
            return false;
        }
        normalizeNormal(normal);

        AnimatedModelVertex& vertex = frame.vertexList[oldSize + i];
        vertex.pos[kX] = position[0];
        vertex.pos[kY] = position[1];
        vertex.pos[kZ] = position[2];
        vertex.nrm[kX] = normal[0];
        vertex.nrm[kY] = normal[1];
        vertex.nrm[kZ] = normal[2];
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

    vertexBase = oldSize;
    return true;
}

bool appendPrimitiveCommand(const cgltf_primitive& primitive,
                            size_t vertexBase,
                            AnimatedModel& model,
                            std::string& error)
{
    const cgltf_accessor* positions = cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
    const cgltf_accessor* texcoords = cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, 0);

    if (!validateAccessor(positions, cgltf_type_vec3, cgltf_component_type_r_32f, "POSITION", error) ||
        !validateOptionalAccessor(texcoords, cgltf_type_vec2, cgltf_component_type_r_32f, "TEXCOORD_0", error) ||
        !validateIndexAccessor(primitive.indices, error))
    {
        return false;
    }
    if (texcoords && texcoords->count != positions->count)
    {
        error = "TEXCOORD_0 count does not match POSITION count";
        return false;
    }

    const cgltf_size indexCount = primitive.indices ? primitive.indices->count : positions->count;
    if (indexCount == 0 || indexCount % 3 != 0)
    {
        error = "triangle primitive index count must be a non-zero multiple of 3";
        return false;
    }

    AnimatedModelDrawCommand command;
    command.primitiveMode = AnimatedModelPrimitiveMode::Triangles;
    command.data.reserve(indexCount);

    for (cgltf_size i = 0; i < indexCount; ++i)
    {
        const cgltf_size sourceIndex = primitive.indices ? cgltf_accessor_read_index(primitive.indices, i) : i;
        if (sourceIndex >= positions->count)
        {
            error = "primitive index is outside POSITION range";
            return false;
        }

        float uv[2] = {0.0f, 0.0f};
        if (texcoords && !cgltf_accessor_read_float(texcoords, sourceIndex, uv, 2))
        {
            error = "failed to read TEXCOORD_0 accessor";
            return false;
        }

        const size_t modelIndex = vertexBase + sourceIndex;
        if (modelIndex > static_cast<size_t>(std::numeric_limits<int32_t>::max()))
        {
            error = "model has too many vertices for the draw command index type";
            return false;
        }

        AnimatedModelDrawVertex vertex;
        vertex.vertexIndex = static_cast<int32_t>(modelIndex);
        vertex.s = uv[0];
        vertex.t = uv[1];
        command.data.push_back(vertex);
    }

    model.appendDrawCommand(std::move(command));
    return true;
}

bool appendMeshFrame(const cgltf_mesh& mesh,
                     AnimatedModel& model,
                     AnimatedModelFrame& frame,
                     bool buildCommands,
                     std::string& error)
{
    if (mesh.primitives_count == 0)
    {
        error = "mesh has no primitives";
        return false;
    }

    for (cgltf_size i = 0; i < mesh.primitives_count; ++i)
    {
        const cgltf_primitive& primitive = mesh.primitives[i];
        if (primitive.type != cgltf_primitive_type_triangles)
        {
            error = "only triangle primitives are supported";
            return false;
        }
        if (primitive.targets_count != 0 || primitive.has_draco_mesh_compression)
        {
            error = "morph targets and compressed primitives are not supported";
            return false;
        }

        size_t vertexBase = 0;
        if (!appendPrimitiveVertices(primitive, frame, vertexBase, error))
        {
            return false;
        }
        if (buildCommands && !appendPrimitiveCommand(primitive, vertexBase, model, error))
        {
            return false;
        }
    }

    return true;
}

bool buildAnimatedModel(const cgltf_data& data,
                        const std::vector<FramePlan>& framePlans,
                        AnimatedModel& model,
                        std::string& error)
{
    if (data.extensions_required_count != 0)
    {
        error = "required glTF extensions are not supported";
        return false;
    }
    if (data.skins_count != 0)
    {
        error = "skins are not supported";
        return false;
    }
    if (hasUnsupportedNodeTransform(data))
    {
        error = "non-identity node transforms are not supported";
        return false;
    }
    if (data.meshes_count == 0)
    {
        error = "file contains no meshes";
        return false;
    }

    std::vector<AnimatedModelFrame>& frames = model.getFrames();
    frames.resize(framePlans.size());

    size_t expectedVertexCount = 0;
    for (size_t frameIndex = 0; frameIndex < framePlans.size(); ++frameIndex)
    {
        const FramePlan& plan = framePlans[frameIndex];
        if (plan.meshIndex >= data.meshes_count)
        {
            error = "extras.egoboo frame references a missing mesh";
            return false;
        }

        AnimatedModelFrame& frame = frames[frameIndex];
        std::strncpy(frame.name, plan.name.c_str(), sizeof(frame.name) - 1);
        frame.name[sizeof(frame.name) - 1] = '\0';
        frame.framefx = plan.framefx;

        if (!appendMeshFrame(data.meshes[plan.meshIndex], model, frame, frameIndex == 0, error))
        {
            return false;
        }

        if (frame.vertexList.empty())
        {
            error = "frame has no vertices";
            return false;
        }
        if (frameIndex == 0)
        {
            expectedVertexCount = frame.vertexList.size();
            model.setVertexCount(expectedVertexCount);
        }
        else if (frame.vertexList.size() != expectedVertexCount)
        {
            error = "all glTF frames must have the same vertex count";
            return false;
        }
    }

    return true;
}

ObjectModelLoadResult loadGltfModel(const std::string& fileName)
{
    ObjectModelLoadResult result;

    FileBuffer source;
    if (!readVfsFile(fileName, source))
    {
        logWarning(fileName, "unable to read file through VFS");
        return result;
    }

    cgltf_options options = {};
    options.file.read = readCgltfFile;
    options.file.release = releaseCgltfFile;

    cgltf_data* rawData = nullptr;
    cgltf_result parseResult = cgltf_parse(&options, source.bytes, source.size, &rawData);
    if (parseResult != cgltf_result_success)
    {
        logWarning(fileName, std::string("parse failed: ") + cgltfResultName(parseResult));
        return result;
    }

    std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(rawData, cgltf_free);

    const cgltf_result bufferResult = cgltf_load_buffers(&options, data.get(), fileName.c_str());
    if (bufferResult != cgltf_result_success)
    {
        logWarning(fileName, std::string("buffer load failed: ") + cgltfResultName(bufferResult));
        return result;
    }

    const cgltf_result validationResult = cgltf_validate(data.get());
    if (validationResult != cgltf_result_success)
    {
        logWarning(fileName, std::string("validation failed: ") + cgltfResultName(validationResult));
        return result;
    }

    std::vector<FramePlan> framePlans;
    AnimationMetadataInput metadata;
    std::string error;
    if (!parseMetadataExtras(*data, framePlans, metadata, error))
    {
        logWarning(fileName, error);
        return result;
    }

    auto model = std::make_shared<AnimatedModel>();
    if (!buildAnimatedModel(*data, framePlans, *model, error))
    {
        logWarning(fileName, error);
        return result;
    }

    result.model = std::move(model);
    result.animationMetadata = metadata;
    return result;
}

} // namespace

namespace Ego
{
namespace Graphics
{

ObjectModelLoadResult GltfModel::loadFromFile(const std::string& fileName)
{
    return loadGltfModel(fileName);
}

} // namespace Graphics
} // namespace Ego
