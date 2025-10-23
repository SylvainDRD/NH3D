#include "mock_rhi.hpp"
#include "gtest/gtest.h"
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

TEST(SplitPoolTests, StorageTest)
{
    SplitPool<A> pool { 10, 10 };

    Handle<B> handle = pool.store({ 1337, true }, { "Hello there!" });

    EXPECT_EQ(pool.getHotData(handle).hotValue, 1337);
    EXPECT_EQ(pool.getColdData(handle).coldValue, "Hello there!");

    EXPECT_DEATH((void)pool.getHotData({ 4 }), ".*FATAL.*Invalid handle index");
    EXPECT_DEATH((void)pool.getColdData({ 4 }), ".*FATAL.*Invalid handle index");
}

TEST(SplitPoolTests, ReleaseTest)
{
    SplitPool<A> pool { 10, 10 };

    Handle<B> handle = pool.store({ 1337, true }, { "Hello there!" });

    EXPECT_DEATH((void)pool.getHotData({ 4 }), ".*FATAL.*Invalid handle index");
    EXPECT_DEATH((void)pool.getColdData({ 4 }), ".*FATAL.*Invalid handle index");
    EXPECT_TRUE(A::valid(pool.getHotData(handle), pool.getColdData(handle)));

    MockRHI rhi;
    pool.release(rhi, handle);

    EXPECT_FALSE(A::valid(pool.getHotData(handle), { "Hello there!" }));
    EXPECT_DEATH((void)pool.getColdData({ handle }), ".*FATAL.*Attempting to access cold data of an invalid handle");

    EXPECT_DEATH(pool.release(rhi, { 4 }), ".*FATAL.*Invalid handle index");
    // Cold data is fetched to check the data validity
    EXPECT_DEATH(pool.release(rhi, handle), ".*FATAL.*Attempting to access cold data of an invalid handle");
}

TEST(SplitPoolTests, ClearTest)
{
    SplitPool<A> pool { 10, 10 };

    Handle<B> handle = pool.store({ 1337, true }, { "Hello there!" });

    pool.clear(MockRHI {});

    EXPECT_DEATH((void)pool.getHotData(handle), ".*FATAL.*Invalid handle index");
    EXPECT_DEATH((void)pool.getColdData(handle), ".*FATAL.*Invalid handle index");
}

TEST(SplitPoolTests, HandleRecyclingTest)
{
    SplitPool<A> pool { 10, 10 };

    const Handle<B> handle1 = pool.store({ 1337, true }, { "Hello there!" });

    pool.release(MockRHI {}, handle1);
    EXPECT_DEATH((void)pool.getColdData({ handle1 }), ".*FATAL.*Attempting to access cold data of an invalid handle");

    const Handle<B> handle2 = pool.store({ 1337, true }, { "Hello there!" });

    EXPECT_EQ(handle1.index, handle2.index);
    EXPECT_EQ(pool.getColdData(handle2).coldValue, "Hello there!");
}

}