#pragma once

#include <rendering/core/rhi.hpp>

namespace NH3D {

class MockRHI : public IRHI {
public:
    MockRHI() = default;

    [[nodiscard]] virtual Handle<Texture> createTexture(const Texture::CreateInfo& info) override
    {
        return { 0 };
    }

    virtual void destroyTexture(const Handle<Texture> handle) override
    {
    }

    [[nodiscard]] virtual Handle<Buffer> createBuffer(const Buffer::CreateInfo& info) override { return { 0 }; };

    virtual void destroyBuffer(const Handle<Buffer> handle) override { }

    virtual void render(Scene& scene) const override { }
};

}