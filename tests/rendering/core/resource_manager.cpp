#include "mock_rhi.hpp"
#include <gtest/gtest.h>
#include <rendering/core/buffer.hpp>
#include <rendering/core/rhi.hpp>
#include <rendering/core/texture.hpp>

namespace NH3D {

// structure stubs
struct VulkanTexture {
    using ResourceType = Texture;

    struct Hot {
        int hotValue;
    };

    struct Cold {
        std::string coldValue;
    };

    [[nodiscard]] static inline bool valid(const Hot& hot, const Cold& cold) { return true; }

    static void release(const IRHI& rhi, Hot& hot, Cold& cold) { }
};

struct VulkanBuffer {
    using ResourceType = Buffer;

    struct Hot {
        std::string hotValue;
        bool valid = true;
    };

    struct Cold {
        uint8_t coldValue;
    };

    [[nodiscard]] static inline bool valid(const Hot& hot, const Cold& cold) { return hot.valid; }

    static void release(const IRHI& rhi, Hot& hot, Cold& cold) { hot.valid = false; }
};

}

#include <rendering/core/resource_manager.hpp>

namespace NH3D::Test {

TEST(ResourceManagerTests, GeneralTest)
{
    ResourceManager resourceManager;

    Handle<Texture> textureHandle = resourceManager.store<VulkanTexture>({ 1337 }, { "Hello there!" });
    Handle<Buffer> bufferHandle = resourceManager.store<VulkanBuffer>({ "Hello there too!", true }, { 42 });

    EXPECT_EQ(resourceManager.getHotData<VulkanTexture>(textureHandle).hotValue, 1337);
    EXPECT_EQ(resourceManager.getColdData<VulkanTexture>(textureHandle).coldValue, "Hello there!");

    EXPECT_EQ(resourceManager.getHotData<VulkanBuffer>(bufferHandle).hotValue, "Hello there too!");
    EXPECT_EQ(resourceManager.getColdData<VulkanBuffer>(bufferHandle).coldValue, 42);

    EXPECT_DEATH((void)resourceManager.getHotData<VulkanTexture>({ 4 }), ".*FATAL.*");
    EXPECT_DEATH((void)resourceManager.getColdData<VulkanTexture>({ 4 }), ".*FATAL.*");
}

TEST(ResourceManagerTests, ReleaseTest)
{
    ResourceManager resourceManager;

    Handle<Texture> textureHandle = resourceManager.store<VulkanTexture>({ 1337 }, { "Hello there!" });
    Handle<Buffer> bufferHandle = resourceManager.store<VulkanBuffer>({ "Hello there too!", true }, { 42 });

    MockRHI rhi;
    resourceManager.release<VulkanBuffer>(rhi, bufferHandle);
    // Assertion only present on cold getter for debug performance
    EXPECT_FALSE(VulkanBuffer::valid(resourceManager.getHotData<VulkanBuffer>(bufferHandle), { 42 }));
    EXPECT_DEATH((void)resourceManager.getColdData<VulkanBuffer>(bufferHandle), ".*FATAL.*");

    EXPECT_DEATH(resourceManager.release<VulkanTexture>(rhi, { 10 }), ".*FATAL.*");
}

TEST(ResourceManagerTests, ClearTest)
{
    ResourceManager resourceManager;

    Handle<Texture> textureHandle = resourceManager.store<VulkanTexture>({ 1337 }, { "Hello there!" });
    Handle<Buffer> bufferHandle = resourceManager.store<VulkanBuffer>({ "Hello there too!", true }, { 42 });

    MockRHI rhi;
    resourceManager.clear(rhi);

    EXPECT_DEATH((void)resourceManager.getHotData<VulkanBuffer>(bufferHandle), ".*FATAL.*");
    EXPECT_DEATH((void)resourceManager.getColdData<VulkanBuffer>(bufferHandle), ".*FATAL.*");
    // gtest is fucking weird sometimes
    EXPECT_DEATH(resourceManager.release<VulkanTexture>(rhi, { bufferHandle.index }), ".*FATAL.*");
}

}
