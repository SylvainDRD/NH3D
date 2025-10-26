#include <gtest/gtest.h>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <scene/scene.hpp>

namespace NH3D::Test {

TEST(ComponentViewTests, IterationTest)
{
    Scene scene;
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
}

}