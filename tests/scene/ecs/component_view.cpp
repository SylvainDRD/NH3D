#include "misc/utils.hpp"
#include <gtest/gtest.h>
#include <scene/ecs/entity_manager.hpp>

namespace NH3D::Test {

TEST(ComponentViewTests, IterationTest)
{
    constexpr int Expected[] = { 1, 2, 3, 4, 5 };
    
    EntityManager::create(1, 'a');
    EntityManager::create(2);
    EntityManager::create(3, 'b');
    EntityManager::create(4, 'b');
    EntityManager::create(5);

    for (auto [e, i, c] : EntityManager::makeView<int&, const char>()) {
        NH3D_STATIC_ASSERT(std::is_same_v<decltype(i), int&>);
        NH3D_STATIC_ASSERT(std::is_same_v<decltype(c), const char>);
        
        EXPECT_NE(e, 1);
        EXPECT_NE(e, 4);
        EXPECT_EQ(Expected[e], i);

        i++;
    }

    int intCharCount = 0;
    for (auto [e, i, c] : EntityManager::makeView<const int&, const char>()) {
        NH3D_STATIC_ASSERT(std::is_same_v<decltype(i), const int&>);

        EXPECT_EQ(Expected[e] + 1, i);
        ++intCharCount;
    }

    EXPECT_EQ(intCharCount, 3);

    int intCount = 0;
    for (auto [e,  i] : EntityManager::makeView<int>()) {
        NH3D_STATIC_ASSERT(std::is_same_v<decltype(i), int>);

        // Internal mechanism
        EXPECT_EQ(e, intCount);

        ++intCount;
    }

    EXPECT_EQ(intCount, 5);
}

}