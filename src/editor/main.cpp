#include <general/engine.hpp>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/gpu_mesh.hpp>
#include <rendering/core/rhi.hpp>
#include <scene/ecs/components/render_component.hpp>
#include <scene/scene.hpp>
#include <vector>

using namespace NH3D;

int main()
{
    Engine engine;

    Scene& scene = engine.getMainScene();
    IRHI& rhi = engine.getRHI();

    const std::vector<VertexData> vertexData = {
        {
            .position = vec3 { -1.f, 1.f, 0.f },
            .normal = vec3 { 1.0f, 0.0f, 0.0f },
            .uv = vec2 { 1.0f, 0.0f },
        },
        {
            .position = vec3 { 1.f, 1.f, 0.f },
            .normal = vec3 { 0.0f, 1.0f, 0.0f },
            .uv = vec2 { 0.0f, 0.0f },
        },
        {
            .position = vec3 { 0.f, -1.f, 0.f },
            .normal = vec3 { 0.0f, 0.0f, 1.0f },
            .uv = vec2 { 0.5f, 1.0f },
        },
    };
    const std::vector<uint32> indices = { 0, 1, 2 };

    GPUMesh meshData {
        .vertexBuffer = rhi.createBuffer({ .size = vertexData.size() * sizeof(VertexData),
            .usage = BufferUsageFlagBits::STORAGE_BUFFER_BIT | BufferUsageFlagBits::DST_TRANSFER_BIT,
            .memory = BufferMemoryUsage::GPU_ONLY,
            .initialData = reinterpret_cast<const byte*>(vertexData.data()) }),
        .indexBuffer = rhi.createBuffer({ .size = indices.size() * sizeof(uint32),
            .usage = BufferUsageFlagBits::INDEX_BUFFER_BIT | BufferUsageFlagBits::DST_TRANSFER_BIT,
            .memory = BufferMemoryUsage::GPU_ONLY,
            .initialData = reinterpret_cast<const byte*>(indices.data()) }),
    };

    engine.getResourceMapper().storeMesh("basic triangle", meshData);

    // Debug only
    scene.create(RenderComponent { meshData });

    while (engine.update())
        ;

    return 0;
}