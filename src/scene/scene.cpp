#include "scene.hpp"
#include "misc/utils.hpp"
#include "scene/ecs/entity.hpp"
#include "scene/ecs/subtree_view.hpp"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <scene/ecs/components/render_component.hpp>
#define TINYGLTF_NO_INCLUDE_JSON
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

namespace NH3D {

Scene::Scene(IRHI& rhi)
{
    _entityMasks.reserve(400'000);
    _availableEntities.reserve(2'000);
}

Scene::Scene(IRHI& rhi, const std::filesystem::path& filePath)
{
    _entityMasks.reserve(400'000);
    _availableEntities.reserve(2'000);

    // TODO: pre-allocate a loading struct with strings for errors & warnings/tinygltf::Model/vectors for mesh data pre-allocated
}

namespace _private {
    using namespace tinygltf;

    std::pair<std::vector<VertexData>, std::vector<uint32_t>> loadGLBMeshData(const std::filesystem::path& glbPath)
    {
        using namespace tinygltf;

        TinyGLTF loader;

        Model model;
        std::string error;
        std::string warning;

        bool result = loader.LoadASCIIFromFile(&model, &error, &warning, glbPath.string());

        if (!warning.empty()) {
            NH3D_WARN("tinyGLTF import warning: " << warning);
        }

        if (!error.empty()) {
            NH3D_ERROR("tinyGLTF import error: " << error);
        }

        if (!result) {
            NH3D_ERROR("Failed to import GLB/GLTF file from " << glbPath);
        }

        // TODO

        return {};
    }

}

[[nodiscard]] uint32 Scene::size() const
{
    return static_cast<uint32>(_entityMasks.size() - _availableEntities.size());
}

[[nodiscard]] bool Scene::isValidEntity(const Entity entity) const
{
    return entity < _entityMasks.size() && (_entityMasks[entity] & SparseSetMap::InvalidEntityMask) == 0;
}

void Scene::remove(const Entity entity)
{
    NH3D_ASSERT(entity < _entityMasks.size(), "Attempting to delete a non-existant entity");
    NH3D_ASSERT((_entityMasks[entity] & SparseSetMap::InvalidEntityMask) == 0, "Attempting to delete an invalid entity");

    SubtreeView subtree = getSubtree(entity);
    for (const Entity e : subtree) {
        _setMap.remove(e, _entityMasks[e]);
        _entityMasks[e] = SparseSetMap::InvalidEntityMask;
        _availableEntities.emplace_back(e);
    }
    _hierarchy.deleteSubtree(entity);
}

void Scene::setParent(const Entity entity, const Entity parent)
{
    NH3D_ASSERT(entity != InvalidEntity, "Unexpected invalid entity");
    _hierarchy.setParent(entity, parent);
}

[[nodiscard]] bool Scene::isLeaf(const Entity entity) const
{
    NH3D_ASSERT(isValidEntity(entity), "Attempting to check leaf status of an invalid entity");

    return _hierarchy.isLeaf(entity);
}

[[nodiscard]] Entity Scene::getMainCamera() const { return _mainCamera; }

void Scene::setMainCamera(const Entity entity)
{
    NH3D_ASSERT(isValidEntity(entity), "Attempting to set main camera to an invalid entity");
    NH3D_ASSERT(checkComponents<CameraComponent>(entity), "Attempting to set main camera to an entity without CameraComponent");

    _mainCamera = entity;
}

void Scene::setVisibleFlag(const Entity entity, const bool flag)
{
    NH3D_ASSERT(isValidEntity(entity), "Attempting to set visible flag of an invalid entity");
    _setMap.setFlag<RenderComponent>(entity, flag);
}

[[nodiscard]] bool Scene::isVisible(const Entity entity)
{
    NH3D_ASSERT(isValidEntity(entity), "Attempting to get visible flag of an invalid entity");
    return _setMap.getFlag<RenderComponent>(entity);
}

[[nodiscard]] const void* Scene::getRawVisibleFlags() { return _setMap.getRawFlags<RenderComponent>(); }

}
