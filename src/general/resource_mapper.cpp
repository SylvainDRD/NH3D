#include "resource_mapper.hpp"

namespace NH3D {

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
