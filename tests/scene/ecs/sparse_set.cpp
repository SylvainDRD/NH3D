#include <gtest/gtest.h>
#include <scene/ecs/sparse_set.hpp>

namespace NH3D::Test {

TEST(SparseSetTests, AddTest)
{
    SparseSet<int> set;

    set.add(8, 17);
    set.add(0, 1337);
    set.add(1, 42);

    EXPECT_EQ(set.get(0), 1337);
    EXPECT_EQ(set.get(1), 42);
    EXPECT_EQ(set.get(8), 17);
    EXPECT_EQ(set.get(1), 42); // No weird side effect with get
}

TEST(SparseSetTests, AddInternalTest)
{
    SparseSet<int> set;

    for (int i = 0; i < 1025; ++i) {
        set.add(i, i + 10);
    }

    for (int i = 0; i < 1025; ++i) {
        EXPECT_EQ(set.get(i), i + 10);
    }

    EXPECT_DEATH((void)set.get(220'000), ".*FATAL.*Requested a component for an entity without storage");
    EXPECT_DEATH((void)set.get(2000), ".*FATAL.*Requested a non-existing component: Samir you're thrashing the cache");

    set.add(220'000, 1337);
    EXPECT_EQ(set.get(220'000), 1337);
}

TEST(SparseSetTests, GetRawTest)
{
    SparseSet<int> set;

    for (int i = 0; i < 1025; ++i) {
        set.add(1025 - i, i + 10);
    }

    for (int i = 0; i < 1025; ++i) {
        EXPECT_EQ(set.getRaw(i), i + 10);
    }

    set.add(220'000, 1337);
    EXPECT_EQ(set.getRaw(1025), 1337);

    EXPECT_DEATH((void)set.getRaw(2000), ".*FATAL.*Out of bound raw data SparseSet access");
}

TEST(SparseSetTests, EntitiesTest)
{
    SparseSet<int> set;

    for (int i = 1024; i >= 0; --i) {
        set.add(i, 0);
        EXPECT_EQ(set.size(), 1024 - i + 1);
    }

    set.add(220'000, 1337);
    EXPECT_EQ(set.size(), 1026);

    for (int i = 0; i < 1025; ++i) {
        EXPECT_EQ(set.entities()[i], 1024 - i);
    }

    EXPECT_EQ(set.entities().back(), 220'000);
    EXPECT_EQ(set.size(), 1026);
}

// Also testing internal mechanisms in some regards, but they are important for performance
TEST(SparseSetTests, RemoveTest)
{
    SparseSet<int> set;

    EXPECT_DEATH(set.remove(2000), ".*FATAL.*Trying to clear a component for an entity without storage");

    for (int i = 1024; i >= 0; --i) {
        set.add(i, 0);
        EXPECT_EQ(set.size(), 1024 - i + 1);
    }
    EXPECT_DEATH(set.remove(2000), ".*FATAL.*Trying to clear a non-existing component");

    set.add(220'000, 1337);
    EXPECT_EQ(set.size(), 1026);
    EXPECT_EQ(set.entities().back(), 220'000);

    set.remove(220'000);
    EXPECT_EQ(set.size(), 1025);
    EXPECT_EQ(set.entities().back(), 0);

    EXPECT_EQ(set.entities().front(), 1024);
    set.remove(1024);
    EXPECT_EQ(set.size(), 1024);
    EXPECT_EQ(set.entities().back(), 1);
    EXPECT_EQ(set.entities().front(), 0);
}

}