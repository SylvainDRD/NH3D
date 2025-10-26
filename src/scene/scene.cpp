#include "scene.hpp"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <scene/ecs/components/mesh_component.hpp>
#define TINYGLTF_NO_INCLUDE_JSON
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

namespace NH3D {

Scene::Scene()
{
    _entityMasks.reserve(400'000);
    _availableEntities.reserve(2'000);
}

Scene::Scene(const std::filesystem::path& filePath)
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

        // TODO: move that into scene loading

        return {};
    }

}

}