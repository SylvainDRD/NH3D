#pragma once

#include <misc/utils.hpp>
#include <rendering/core/buffer.hpp>
#include <rendering/core/handle.hpp>
#include <rendering/core/texture.hpp>

namespace NH3D {

class Scene;
class RenderGraph;
class Window;

// TODO: singleton
class IRHI {
    NH3D_NO_COPY_MOVE(IRHI)
public:
    static constexpr uint32_t MaxFramesInFlight = 2;

public:
    IRHI() = default;

    virtual ~IRHI() = default;

    [[nodiscard]] virtual Handle<Texture> createTexture(const Texture::CreateInfo& info) = 0;

    virtual void destroyTexture(const Handle<Texture> handle) = 0;

    [[nodiscard]] virtual Handle<Buffer> createBuffer(const Buffer::CreateInfo& info) = 0;

    virtual void destroyBuffer(const Handle<Buffer> handle) = 0;

    virtual void render(Scene& scene) const = 0;
};

}