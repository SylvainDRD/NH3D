#include <general/engine.hpp>
#include <imgui.h>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/mesh.hpp>
#include <rendering/core/rhi.hpp>
#include <scene/ecs/components/render_component.hpp>
#include <scene/ecs/components/transform_component.hpp>
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

    Mesh meshData {
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

    engine.getResourceMapper().storeMesh("basic triangle", meshData);

    // Debug only
    scene.create(CameraComponent {}, TransformComponent {});
    scene.create(
        RenderComponent { meshData, Material { .albedo = color3 { 1.0f, 0.0f, 0.0f } } }, TransformComponent { { 0.0f, 0.0f, 0.8f } });

    const Window& window = engine.getWindow();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    double accumulatedTime = 0.0;
    float maxDeltaTime = 0.0f;
    float lastFPS = 0.0f;
    while (engine.update()) {
        accumulatedTime += engine.deltaTime();
        maxDeltaTime = std::max(maxDeltaTime, engine.deltaTime());

        vec2 mousePos = window.getMousePosition();
        io.DeltaTime = engine.deltaTime();
        io.MousePos = { mousePos.x, mousePos.y };
        io.MouseDown[ImGuiMouseButton_Left] = window.isMouseButtonPressed(MouseButton::Left);
        io.MouseDown[ImGuiMouseButton_Right] = window.isMouseButtonPressed(MouseButton::Right);
        io.MouseDown[ImGuiMouseButton_Middle] = window.isMouseButtonPressed(MouseButton::Middle);

        if (accumulatedTime > 1.0) {
            accumulatedTime = 0.0;

            lastFPS = 1.0f / maxDeltaTime;
            maxDeltaTime = 0.0f;
        }

        ImGui::NewFrame();

        ImGui::Begin("Debug Info");
        ImGui::Text("FPS: %.2f", lastFPS);

        ImGui::End();
        ImGui::Render();
    }

    return 0;
}
