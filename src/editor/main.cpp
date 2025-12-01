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

    auto& resourceMapper = engine.getResourceMapper();
    const auto& [cubeVertexData, cubeIndices] = resourceMapper.loadMesh(NH3D_DIR "cube.glb");

    Mesh meshData {
        .vertexBuffer = rhi.createBuffer({ .size = static_cast<uint32>(cubeVertexData.size() * sizeof(VertexData)),
            .usageFlags = BufferUsageFlagBits::STORAGE_BUFFER_BIT | BufferUsageFlagBits::DST_TRANSFER_BIT,
            .memoryUsage = BufferMemoryUsage::GPU_ONLY,
            .initialData
            = { reinterpret_cast<const byte*>(cubeVertexData.data()), static_cast<uint32>(cubeVertexData.size() * sizeof(VertexData)) } }),
        .indexBuffer = rhi.createBuffer({ .size = static_cast<uint32>(cubeIndices.size() * sizeof(uint32)),
            .usageFlags = BufferUsageFlagBits::INDEX_BUFFER_BIT | BufferUsageFlagBits::DST_TRANSFER_BIT,
            .memoryUsage = BufferMemoryUsage::GPU_ONLY,
            .initialData
            = { reinterpret_cast<const byte*>(cubeIndices.data()), static_cast<uint32>(cubeIndices.size() * sizeof(uint32)) } }),
        .objectAABB = AABB::fromMesh(cubeVertexData, cubeIndices),
    };

    // resourceMapper.storeMesh("...", meshData);

    scene.create(CameraComponent {}, TransformComponent {});
    scene.create(
        RenderComponent { meshData, Material { .albedo = color3 { 1.0f, 0.0f, 0.0f } } }, TransformComponent { { 0.0f, 0.0f, 10.0f } });

    const Window& window = engine.getWindow();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    std::array<float, 128> frameTimes {};
    int32 frameTimesIndex = 0;

    float accumulatedTime = 0.0f;
    float maxDeltaTime = 0.0f;
    float lastFPS = 0.0f;
    while (engine.update()) {
        accumulatedTime += engine.deltaTime();
        maxDeltaTime = std::max(maxDeltaTime, engine.deltaTime());

        vec2 mousePos = window.getMousePosition();
        io.DeltaTime = engine.deltaTime();
        io.MousePos = { mousePos.x, mousePos.y };
        io.MouseDown[ImGuiMouseButton_Left] = window.isMouseButtonPressed(MouseButton::Left);
        io.MouseDown[ImGuiMouseButton_Middle] = window.isMouseButtonPressed(MouseButton::Middle);

        if (accumulatedTime > 0.125f) {
            accumulatedTime = 0.0f;

            lastFPS = 1.0f / maxDeltaTime;

            frameTimes[frameTimesIndex] = maxDeltaTime * 1000.0f; // in ms
            frameTimesIndex = (frameTimesIndex + 1) & (frameTimes.size() - 1);

            maxDeltaTime = 0.0f;
        }

        ImGui::NewFrame();

        ImGui::Begin("Debug Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("FPS: %.2f", lastFPS);
        ImGui::PlotLines("Overall Frame Time (ms)", frameTimes.data(), frameTimes.size(), (frameTimesIndex + 1) & (frameTimes.size() - 1),
            nullptr, 0.0f, 16.0f, ImVec2(0, 80));

        for (const auto& [e, _, transform] : scene.makeView<const RenderComponent&, TransformComponent&>()) {
            vec3 position = transform.position();
            ImGui::SliderFloat3("Position", &position.x, -10.0f, 10.0f);
            transform.setPosition(scene, e, position);
        }

        ImGui::End();
        ImGui::Render();
    }

    return 0;
}
