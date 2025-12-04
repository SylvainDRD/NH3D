#pragma once

#include <general/window.hpp>
#include <rendering/core/buffer.hpp>
#include <rendering/core/handle.hpp>
#include <rendering/core/rhi.hpp>
#include <rendering/core/texture.hpp>

namespace NH3D {

class MockRHI : public IRHI {
public:
    MockRHI() = default;

    [[nodiscard]] virtual Handle<Texture> createTexture(const Texture::CreateInfo&) override { return InvalidHandle<Texture>; }

    virtual void destroyTexture(const Handle<Texture>) override { }

    [[nodiscard]] virtual Handle<Buffer> createBuffer(const Buffer::CreateInfo&) override { return InvalidHandle<Buffer>; };

    virtual void destroyBuffer(const Handle<Buffer>) override { }

    virtual void render(Scene&) override { }
};

}