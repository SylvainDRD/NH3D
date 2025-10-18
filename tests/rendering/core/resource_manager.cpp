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
    };

    struct Cold {
        uint8_t coldValue;
    };

    [[nodiscard]] static inline bool valid(const Hot& hot, const Cold& cold) { return true; }

    static void release(const IRHI& rhi, Hot& hot, Cold& cold) { }
};

}

#include <rendering/core/resource_manager.hpp>

namespace NH3D::Test {

TEST(ResourceManagerTests, StorageTest)
{
    ResourceManager resourceManager;

    Handle<Texture> textureHandle = resourceManager.store<VulkanTexture>({ 1337 }, { "Hello there!" });
    Handle<Buffer> bufferHandle = resourceManager.store<VulkanBuffer>({ "Hello there too!" }, { 42 });

    EXPECT_EQ(resourceManager.getHotData<VulkanTexture>(textureHandle).hotValue, 1337);
    EXPECT_EQ(resourceManager.getColdData<VulkanTexture>(textureHandle).coldValue, "Hello there!");

    EXPECT_EQ(resourceManager.getHotData<VulkanBuffer>(bufferHandle).hotValue, "Hello there too!");
    EXPECT_EQ(resourceManager.getColdData<VulkanBuffer>(bufferHandle).coldValue, 42);
}

}
