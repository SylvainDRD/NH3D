#include <gtest/gtest.h>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <mock_rhi.hpp>
#include <scene/scene.hpp>

namespace NH3D::Test {

TEST(ComponentViewTests, IterationTest)
{
    MockRHI rhi;
    Scene scene { rhi };
    scene.create(1, 'a');
    scene.create(2);
    scene.create(3, 'b', 42u);
    scene.create(4, 'c', 43u);
    scene.create(5, 44u);

    constexpr int ExpectedInt[] = { 1, 2, 3, 4, 5 };
    for (auto [e, i, c] : scene.makeView<int&, const char>()) {
        NH3D_STATIC_ASSERT(std::is_same_v<decltype(i), int&>);
        NH3D_STATIC_ASSERT(std::is_same_v<decltype(c), const char>);

        EXPECT_NE(e, 1);
        EXPECT_NE(e, 4);
        EXPECT_EQ(ExpectedInt[e], i);

        i++;
    }

    int intCharCount = 0;
    for (auto [e, i, c] : scene.makeView<const int&, const char>()) {
        NH3D_STATIC_ASSERT(std::is_same_v<decltype(i), const int&>);

        EXPECT_EQ(ExpectedInt[e] + 1, i);
        ++intCharCount;
    }
    EXPECT_EQ(intCharCount, 3);

    int intCount = 0;
    for (auto [e, i] : scene.makeView<int>()) {
        NH3D_STATIC_ASSERT(std::is_same_v<decltype(i), int>);

        // Internal mechanism
        EXPECT_EQ(e, intCount);

        ++intCount;
    }
    EXPECT_EQ(intCount, 5);

    const char* ExpectedChar = "bc";
    int charCount = 0;
    for (auto [e, c, u] : scene.makeView<const char, const uint32>()) {

        NH3D_STATIC_ASSERT(std::is_same_v<decltype(c), const char>);
        NH3D_STATIC_ASSERT(std::is_same_v<decltype(u), const uint32>);

        // Internal mechanism
        EXPECT_EQ(ExpectedChar[charCount], c);

        ++charCount;
    }
    EXPECT_EQ(charCount, 2);

    EXPECT_EQ(scene.get<int>(0), 2);
    EXPECT_EQ(scene.get<int>(1), 2);
    EXPECT_EQ(scene.get<int>(2), 4);
    EXPECT_EQ(scene.get<int>(3), 5);
    EXPECT_EQ(scene.get<int>(4), 5);
}

TEST(ComponentViewTests, EmptyViewTest)
{
    MockRHI rhi;
    Scene scene { rhi };
    scene.create(1);
    scene.create(2);

    int count = 0;
    for (auto [e, c] : scene.makeView<const char>()) {
        ++count;
    }
    EXPECT_EQ(count, 0);
}

TEST(ComponentViewTests, NonExistentComponentAccess)
{
    MockRHI rhi;
    Scene scene { rhi };
    Entity e = scene.create(1, 'a');

    EXPECT_DEATH((void)scene.get<uint32>(e), ".*FATAL.*");
}

TEST(ComponentViewTests, ViewSkipsRemovedEntities)
{
    MockRHI rhi;
    Scene scene { rhi };
    const Entity e1 = scene.create(1);
    const Entity e2 = scene.create(2);

    scene.remove(e1);

    int count = 0;
    for (auto [entity, value] : scene.makeView<int>()) {
        EXPECT_EQ(entity, e2);
        EXPECT_EQ(value, 2);
        ++count;
    }
    EXPECT_EQ(count, 1);
}

} // namespace NH3D::Test