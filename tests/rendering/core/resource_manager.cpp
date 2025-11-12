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
    ResourceManager<VulkanTexture> resourceManager { 100, 10 };

    Handle<Texture> textureHandle = resourceManager.store({ 1337 }, { "Hello there!" });

    EXPECT_EQ(resourceManager.get<VulkanTexture::Hot>(textureHandle).hotValue, 1337);
    EXPECT_EQ(resourceManager.get<VulkanTexture::Cold>(textureHandle).coldValue, "Hello there!");

    EXPECT_DEATH((void)resourceManager.get<VulkanTexture::Hot>({ 4 }), ".*FATAL.*");
    EXPECT_DEATH((void)resourceManager.get<VulkanTexture::Cold>({ 4 }), ".*FATAL.*");
}

TEST(ResourceManagerTests, ReleaseTest)
{
    ResourceManager<VulkanBuffer> resourceManager { 100, 10 };

    Handle<Buffer> bufferHandle = resourceManager.store({ "Hello there too!", true }, { 42 });

    MockRHI rhi;
    resourceManager.release(rhi, bufferHandle);
    // Assertion only present on cold getter for debug performance
    EXPECT_FALSE(VulkanBuffer::valid(resourceManager.get<VulkanBuffer::Hot>(bufferHandle), { 42 }));
    EXPECT_DEATH((void)resourceManager.get<VulkanBuffer::Cold>(bufferHandle), ".*FATAL.*");
}

TEST(ResourceManagerTests, ClearTest)
{
    ResourceManager<VulkanBuffer> resourceManager { 100, 10 };

    Handle<Buffer> bufferHandle = resourceManager.store({ "Hello there too!", true }, { 42 });

    MockRHI rhi;
    resourceManager.clear(rhi);

    EXPECT_DEATH((void)resourceManager.get<VulkanBuffer::Hot>(bufferHandle), ".*FATAL.*");
    EXPECT_DEATH((void)resourceManager.get<VulkanBuffer::Cold>(bufferHandle), ".*FATAL.*");
    EXPECT_DEATH(resourceManager.release(rhi, { bufferHandle.index }), ".*FATAL.*");
}

TEST(ResourceManagerTests, ReleaseInvalidHandleTest)
{
    ResourceManager<VulkanBuffer> resourceManager { 100, 10 };
    MockRHI rhi;

    const Handle<Buffer> handle = resourceManager.store({ "Hello there!", true }, { 7 });
    resourceManager.release(rhi, handle);
    EXPECT_DEATH(resourceManager.release(rhi, handle), ".*FATAL.*");
}

TEST(ResourceManagerTests, StoreReusesReleasedSlot)
{
    ResourceManager<VulkanBuffer> resourceManager { 100, 10 };
    MockRHI rhi;

    const Handle<Buffer> first = resourceManager.store({ "A", true }, { 1 });
    const Handle<Buffer> second = resourceManager.store({ "B", true }, { 2 });

    resourceManager.release(rhi, first);
    const Handle<Buffer> reused = resourceManager.store({ "C", true }, { 3 });

    EXPECT_EQ(reused.index, first.index);
    EXPECT_EQ(resourceManager.get<VulkanBuffer::Hot>(reused).hotValue, "C");

    EXPECT_EQ(resourceManager.get<VulkanBuffer::Hot>(second).hotValue, "B");
}

}
