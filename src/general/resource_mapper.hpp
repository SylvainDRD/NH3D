#pragma once

#include <filesystem>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/buffer.hpp>
#include <rendering/core/handle.hpp>
#include <rendering/core/mesh.hpp>
#include <rendering/core/shader.hpp>
#include <rendering/core/texture.hpp>
#include <unordered_map>

namespace NH3D {

struct MeshData {
    std::vector<VertexData> vertexData;
    std::vector<uint32> indices;
    uint32 materialIndex;
};

class ResourceMapper {
    NH3D_NO_COPY(ResourceMapper)
public:
    ResourceMapper() = default;
    ~ResourceMapper() = default;

    // TODO: support multiple index types and return AABB
    // TODO: pass RV as parameters and give the reponsibility of memory management to the caller (right now the vectors are static, bad
    // things will happen if called multiple times)
    [[nodiscard]]
    std::pair<const std::vector<VertexData>&, const std::vector<uint32>&> loadMesh(
        const std::filesystem::path& path, const vec3u swizzle = { 0, 2, 1 }) const;

    void storeMesh(const std::string& name, const Mesh& mesh);
    [[nodiscard]] Mesh getMesh(const std::string& name) const;

    void storeTexture(const std::string& name, Handle<Texture> texture);
    [[nodiscard]] Handle<Texture> getTexture(const std::string& name) const;

    void storeShader(const std::string& name, Handle<Shader> shader);
    [[nodiscard]] Handle<Shader> getShader(const std::string& name) const;

private:
    // Only stores handles for move resilience
    std::unordered_map<std::string, Mesh> _meshMap;
    std::unordered_map<std::string, Handle<Texture>> _textureMap;
    std::unordered_map<std::string, Handle<Shader>> _shaderMap;
};

} // namespace NH3D