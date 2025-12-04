#include <general/engine.hpp>
#include <general/resource_mapper.hpp>
#include <general/window.hpp>
#include <imgui.h>
#include <misc/math.hpp>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/enums.hpp>
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

    Entity cameraEntity = scene.create(CameraComponent {}, TransformComponent {});

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

    for (int i = 0; i < 14; ++i) {
        for (int j = 0; j < 14; ++j) {
            for (int k = 0; k < 200; ++k) {
                MeshData meshData;
                (void)resourceMapper.loadModel(rhi, NH3D_DIR "src/editor/assets/cube.glb", meshData);
                scene.create(RenderComponent { meshData.mesh, { .albedoTexture = textures[k % std::size(textures)] } },
                    TransformComponent { { 8.0f * i - 56.0f, 8.0f * j - 56.0f, 8.0f * k - 200.0f } });
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

    vec2 lastMousePos;
    vec2 angle { 0.0f, 0.0f }; // x yaw, y pitch
    while (engine.update()) {
        const vec2 mousePos = window.getMousePosition();
        const float dt = engine.deltaTime();

        /// ImGui input
        io.DeltaTime = dt;
        io.MousePos = { mousePos.x, mousePos.y };
        io.MouseDown[ImGuiMouseButton_Left] = window.isMouseButtonPressed(MouseButton::Left);
        io.MouseDown[ImGuiMouseButton_Right] = window.isMouseButtonPressed(MouseButton::Right);

        /// FPS display
        accumulatedTime += dt;
        maxDeltaTime = std::max(maxDeltaTime, dt);
        if (accumulatedTime > 0.125f) {
            accumulatedTime = 0.0f;

            lastFPS = 1.0f / maxDeltaTime;

            frameTimes[frameTimesIndex] = maxDeltaTime * 1000.0f; // in ms
            frameTimesIndex = (frameTimesIndex + 1) & (frameTimes.size() - 1);

            maxDeltaTime = 0.0f;
        }

        /// Input handling
        const vec2 mouseDelta = mousePos - lastMousePos;
        lastMousePos = mousePos;

        auto& cameraTransform = scene.get<TransformComponent>(cameraEntity);
        const vec3 forward = cameraTransform.rotation() * vec3 { 0.0f, 0.0f, 1.0f };
        const vec3 right = cameraTransform.rotation() * vec3 { 1.0f, 0.0f, 0.0f };
        const vec3 up = cameraTransform.rotation() * vec3 { 0.0f, 1.0f, 0.0f };

        if (window.isMouseButtonPressed(MouseButton::Right)) {
            const float sensitivity = 2.0f;

            const auto& cameraComponent = scene.get<CameraComponent>(cameraEntity);
            const float fovY = cameraComponent.getFovY();
            const float aspectRatio = window.getWidth() / static_cast<float>(window.getHeight());
            const float fovX = 2.0f * std::atan(std::tan(fovY * 0.5f) * aspectRatio);

            const vec2 screenPercentage = mouseDelta / vec2 { window.getWidth(), window.getHeight() };
            angle.x += screenPercentage.x * fovX * sensitivity;
            angle.y += screenPercentage.y * fovY * sensitivity;

            cameraTransform.setRotation(scene, cameraEntity, quat { vec3 { angle.y, angle.x, 0.0f } });
        } else if (window.isMouseButtonPressed(MouseButton::Middle)) {
            const float moveSensitivity = 0.01f;

            const vec3 translation = (right * mouseDelta.x + up * -mouseDelta.y) * moveSensitivity;
            cameraTransform.translate(scene, cameraEntity, translation);
        }

        const float moveSpeed = 5.0f;
        vec3 direction = {};
        if (window.isKeyPressed(SDL_SCANCODE_W)) {
            direction += forward;
        }
        if (window.isKeyPressed(SDL_SCANCODE_S)) {
            direction -= forward;
        }
        if (window.isKeyPressed(SDL_SCANCODE_A)) {
            direction -= right;
        }
        if (window.isKeyPressed(SDL_SCANCODE_D)) {
            direction += right;
        }
        if (window.isKeyPressed(SDL_SCANCODE_E)) {
            direction += up;
        }
        if (window.isKeyPressed(SDL_SCANCODE_Q)) {
            direction -= up;
        }

        if (length2(direction) > 0.001f) {
            direction = normalize(direction);
            cameraTransform.translate(scene, cameraEntity, moveSpeed * dt * direction);
        }

        ImGui::NewFrame();

        ImGui::Begin("Debug Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("FPS: %.2f", lastFPS);
        ImGui::PlotLines("Overall Frame Time (ms)", frameTimes.data(), frameTimes.size(), (frameTimesIndex + 1) & (frameTimes.size() - 1),
            nullptr, 0.0f, 16.0f, ImVec2(0, 80));

        ImGui::End();
        ImGui::Render();
    }

    return 0;
}
