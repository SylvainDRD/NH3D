#include "general/resource_mapper.hpp"
#include "rendering/core/enums.hpp"
#include <general/engine.hpp>
#include <imgui.h>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/mesh.hpp>
#include <rendering/core/rhi.hpp>
#include <scene/ecs/components/render_component.hpp>
#include <scene/ecs/components/transform_component.hpp>
#include <scene/scene.hpp>

using namespace NH3D;

int main()
{
    Engine engine;

    Scene& scene = engine.getMainScene();
    IRHI& rhi = engine.getRHI();

    scene.create(CameraComponent {}, TransformComponent {});

    auto& resourceMapper = engine.getResourceMapper();

    std::vector<byte> textureData(256 * 256 * 4);
    for (uint32 r = 0; r < 256; ++r) {
        for (uint32 g = 0; g < 256; ++g) {
            const size_t index = (static_cast<size_t>(r) * 256 + static_cast<size_t>(g)) * 4;
            textureData[index + 0] = r;
            textureData[index + 1] = g;
            textureData[index + 2] = (r + g) >> 1;
            textureData[index + 3] = 255;
        }
    }

    Handle<Texture> textures[20]; // simulate 20 different textures
    for (int i = 0; i < std::size(textures); ++i) {
        textures[i] = rhi.createTexture({
            .size = { 256, 256, 1 },
            .format = TextureFormat::RGBA8_UNORM, // actual texture would be srgb
            .aspect = TextureAspectFlagBits::ASPECT_COLOR_BIT,
            .initialData = textureData,
            .generateMipMaps = true,
        });
    }

    for (int i = 0; i < 40; ++i) {
        for (int j = 0; j < 40; ++j) {
            for (int k = 0; k < 400; ++k) {
                MeshData meshData;
                (void)resourceMapper.loadModel(rhi, NH3D_DIR "src/editor/assets/cube.glb", meshData); // Simulate 640'000 unique meshes
                scene.create(RenderComponent { meshData.mesh, { .albedoTexture = textures[k % std::size(textures)] } },
                    TransformComponent { { 8.0f * i - 160.0f, 8.0f * j - 160.0f, 8.0f * k - 1600.0f } });
            }
        }
    }

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

        // for (const auto& [e, _, transform] : scene.makeView<const RenderComponent&, TransformComponent&>()) {
        //     vec3 position = transform.position();
        //     ImGui::SliderFloat3("Position", &position.x, -10.0f, 10.0f);
        //     transform.setPosition(scene, e, position);
        // }

        ImGui::End();
        ImGui::Render();
    }

    return 0;
}
