#include "mock_rhi.hpp"
#include <gtest/gtest.h>
#include <rendering/core/split_pool.hpp>

namespace NH3D {

struct B { };

struct A {
    using ResourceType = B;

    struct Hot {
        int hotValue;
        bool valid;
    };

    struct Cold {
        std::string coldValue;
    };

    [[nodiscard]] static inline bool valid(const Hot& hot, const Cold& cold) { return hot.valid; }

    static void release(const IRHI& rhi, Hot& hot, Cold& cold) { hot.valid = false; }
};

}

namespace NH3D::Test {

TEST(SplitPoolTests, SizeTest)
{
    SplitPool<A> pool { 10, 10 };

    auto _ = pool.store({ 1337 }, { "Hello there!" });

    EXPECT_EQ(pool.size(), 1);
}

TEST(SplitPoolTests, StorageTest)
{
    SplitPool<A> pool { 10, 10 };

    Handle<B> handle = pool.store({ 1337 }, { "Hello there!" });

    EXPECT_EQ(pool.getHotData(handle).hotValue, 1337);
    EXPECT_EQ(pool.getColdData(handle).coldValue, "Hello there!");
}

TEST(SplitPoolTests, HandleRecyclingTest)
{
    SplitPool<A> pool { 10, 10 };

    Handle<B> handle1 = pool.store({ 1337, true }, { "Hello there!" });

    pool.release(MockRHI {}, handle1);
    EXPECT_EQ(pool.size(), 0);

    Handle<B> handle2 = pool.store({ 1337 }, { "Hello there!" });

    EXPECT_EQ(pool.size(), 1);
    EXPECT_EQ(handle1.index, handle2.index);
}

}