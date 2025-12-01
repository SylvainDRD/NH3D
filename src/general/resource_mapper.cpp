#include "resource_mapper.hpp"
#include <cstring>
#include <filesystem>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <nlohmann/json.hpp>
#include <sys/types.h>
#define TINYGLTF_NO_INCLUDE_JSON
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

namespace NH3D {

bool ResourceMapper::loadModel(IRHI& rhi, const std::filesystem::path& path, MeshData& meshData, const vec3u swizzle) const
{
    static std::vector<VertexData> vertexData;
    static std::vector<uint32> indices;

    // TODO: remove that
    static std::filesystem::path previousPath;
    if (path == previousPath) {
        goto naughtyshit;
    }

    {
        // TODO: remove that
        previousPath = path;
        NH3D_DEBUGLOG("TODO: remove that shit and properly cache");

        tinygltf::TinyGLTF loader;

        tinygltf::Model model;
        std::string error;
        std::string warning;

        bool result;
        if (path.extension() == ".glb") {
            result = loader.LoadBinaryFromFile(&model, &error, &warning, path.string());
        } else {
            result = loader.LoadASCIIFromFile(&model, &error, &warning, path.string());
        }

        if (!warning.empty()) {
            NH3D_WARN("tinyGLTF import warning: " << warning);
        }

        if (!error.empty()) {
            NH3D_ERROR("tinyGLTF import error: " << error);
        }

        if (!result) {
            NH3D_ERROR("Failed to import GLB/GLTF file from " << path);
            return false;
        }

        const tinygltf::Scene& scene = model.scenes[model.defaultScene];
        if (scene.nodes.size() != 1) {
            NH3D_ERROR("Failed to import GLB/GLTF file from " << path << ": multi nodes not supported");
            return false;
        }

        const tinygltf::Node& node = model.nodes[scene.nodes[0]];
        const tinygltf::Mesh& mesh = model.meshes[node.mesh];
        if (mesh.primitives.size() != 1) {
            NH3D_ERROR("Failed to import GLB/GLTF file from " << path << ": multi-meshes not supported");
            return false;
        }

        tinygltf::Primitive primitive = mesh.primitives[0];
        if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
            NH3D_ERROR("Failed to import GLB/GLTF file from " << path << ": only triangle meshes are supported");
            return false;
        }

        if (primitive.attributes.find("POSITION") == primitive.attributes.end()
            || primitive.attributes.find("NORMAL") == primitive.attributes.end()
            || primitive.attributes.find("TEXCOORD_0") == primitive.attributes.end()) {
            NH3D_ERROR("Failed to import GLB/GLTF file from " << path << ": mesh attributes missing");
            return false;
        }

        if (primitive.indices < 0) {
            NH3D_ERROR("Failed to import GLB/GLTF file from " << path << ": mesh indices missing");
            return false;
        }

        const tinygltf::Accessor& positionAccessor = model.accessors[primitive.attributes["POSITION"]];
        const uint32 vertexCount = static_cast<uint32>(positionAccessor.count);

        const tinygltf::BufferView& positionView = model.bufferViews[positionAccessor.bufferView];
        const float* positions = reinterpret_cast<const float*>(
            &(model.buffers[positionView.buffer].data[positionAccessor.byteOffset + positionView.byteOffset]));
        const uint32 positionByteStride = positionAccessor.ByteStride(positionView)
            ? (positionAccessor.ByteStride(positionView) / sizeof(float))
            : tinygltf::GetNumComponentsInType(TINYGLTF_PARAMETER_TYPE_FLOAT_VEC3);

        const tinygltf::Accessor& normalAccessor = model.accessors[primitive.attributes["NORMAL"]];
        const tinygltf::BufferView& normalView = model.bufferViews[normalAccessor.bufferView];
        const float* normals
            = reinterpret_cast<const float*>(&(model.buffers[normalView.buffer].data[normalAccessor.byteOffset + normalView.byteOffset]));
        const uint32 normalByteStride = normalAccessor.ByteStride(normalView)
            ? (normalAccessor.ByteStride(normalView) / sizeof(float))
            : tinygltf::GetNumComponentsInType(TINYGLTF_PARAMETER_TYPE_FLOAT_VEC3);

        const tinygltf::Accessor& uvAccessor = model.accessors[primitive.attributes["TEXCOORD_0"]];
        const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
        const float* uvs = reinterpret_cast<const float*>(&(model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
        const uint32 uvByteStride = uvAccessor.ByteStride(uvView) ? (uvAccessor.ByteStride(uvView) / sizeof(float))
                                                                  : tinygltf::GetNumComponentsInType(TINYGLTF_PARAMETER_TYPE_FLOAT_VEC2);

        vertexData.resize(vertexCount);

        for (size_t i = 0; i < vertexCount; ++i) {
            VertexData vertex;
            vertex.position[swizzle.x] = positions[i * positionByteStride + 0];
            vertex.position[swizzle.y] = positions[i * positionByteStride + 1];
            vertex.position[swizzle.z] = positions[i * positionByteStride + 2];
            vertex.normal[swizzle.x] = normals[i * normalByteStride + 0];
            vertex.normal[swizzle.y] = normals[i * normalByteStride + 1];
            vertex.normal[swizzle.z] = normals[i * normalByteStride + 2];
            vertex.uv.x = uvs[i * uvByteStride + 0];
            vertex.uv.y = uvs[i * uvByteStride + 1];

            vertexData[i] = vertex;
        }

        const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
        const tinygltf::BufferView& indexView = model.bufferViews[indexAccessor.bufferView];
        const void* indexData = &(model.buffers[indexView.buffer].data[indexAccessor.byteOffset + indexView.byteOffset]);

        indices.resize(indexAccessor.count);

        // TODO: support returning more index types
        switch (indexAccessor.componentType) {
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
            const uint32* buffer = static_cast<const uint32*>(indexData);
            memcpy(indices.data(), buffer, indexAccessor.count * sizeof(uint32)); // Can memcpy because both are uint32
            break;
        }
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
            const uint16* buffer = static_cast<const uint16*>(indexData);
            for (size_t i = 0; i < indexAccessor.count; i++) {
                indices[i] = buffer[i];
            }
            break;
        }
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
            const uint8* buffer = static_cast<const uint8*>(indexData);
            for (size_t i = 0; i < indexAccessor.count; i++) {
                indices[i] = buffer[i];
            }
            break;
        }
        default:
            NH3D_ABORT("Unsupported index component type in gltf mesh");
            return false;
        }
    }

naughtyshit:
    meshData.mesh = Mesh {
        .vertexBuffer = rhi.createBuffer({ .size = static_cast<uint32>(vertexData.size() * sizeof(VertexData)),
            .usageFlags = BufferUsageFlagBits::STORAGE_BUFFER_BIT | BufferUsageFlagBits::DST_TRANSFER_BIT,
            .memoryUsage = BufferMemoryUsage::GPU_ONLY,
            .initialData
            = { reinterpret_cast<const byte*>(vertexData.data()), static_cast<uint32>(vertexData.size() * sizeof(VertexData)) } }),
        .indexBuffer = rhi.createBuffer({ .size = static_cast<uint32>(indices.size() * sizeof(uint32)),
            .usageFlags = BufferUsageFlagBits::INDEX_BUFFER_BIT | BufferUsageFlagBits::DST_TRANSFER_BIT,
            .memoryUsage = BufferMemoryUsage::GPU_ONLY,
            .initialData = { reinterpret_cast<const byte*>(indices.data()), static_cast<uint32>(indices.size() * sizeof(uint32)) } }),
        .objectAABB = AABB::fromMesh(vertexData, indices),
    };
    // TODO: load texture
    meshData.material = Material { .albedo = color3 { 0.6f, 0.0f, 0.0f } };

    return true;
}

void ResourceMapper::storeMesh(const std::string& name, const Mesh& mesh) { _meshMap[name] = mesh; }

Mesh ResourceMapper::getMesh(const std::string& name) const
{
    auto it = _meshMap.find(name);
    if (it == _meshMap.end()) {
        return Mesh {
            .vertexBuffer = InvalidHandle<Buffer>,
            .indexBuffer = InvalidHandle<Buffer>,
        };
    }
    return it->second;
}

void ResourceMapper::storeTexture(const std::string& name, Handle<Texture> texture) { _textureMap[name] = texture; }

Handle<Texture> ResourceMapper::getTexture(const std::string& name) const
{
    auto it = _textureMap.find(name);
    if (it == _textureMap.end()) {
        return InvalidHandle<Texture>;
    }
    return it->second;
}

void ResourceMapper::storeShader(const std::string& name, Handle<Shader> shader) { _shaderMap[name] = shader; }
Handle<Shader> ResourceMapper::getShader(const std::string& name) const
{
    auto it = _shaderMap.find(name);
    if (it == _shaderMap.end()) {
        return InvalidHandle<Shader>;
    }
    return it->second;
}

} // namespace NH3D
