#include <gtest/gtest.h>
#include <scene/ecs/dynamic_bitset.hpp>

using namespace NH3D;

TEST(DynamicBitset, InitialStateIsFalse)
{
    DynamicBitset bits { 64 };
    for (size_t i = 0; i < 64; ++i) {
        EXPECT_FALSE(bits[i]);
    }
}

TEST(DynamicBitset, SetAndGetSingleBits)
{
    DynamicBitset bits { 32 };

    bits.setFlag(0, true);
    bits.setFlag(5, true);
    bits.setFlag(31, true);

    EXPECT_TRUE(bits[0]);
    EXPECT_TRUE(bits[5]);
    EXPECT_TRUE(bits[31]);

    EXPECT_FALSE(bits[1]);
    EXPECT_FALSE(bits[30]);

    bits.setFlag(5, false);
    EXPECT_FALSE(bits[5]);

    EXPECT_DEATH((void)bits[32], ".*FATAL.*");
}

TEST(DynamicBitset, GrowingCapacityKeepsPreviousBits)
{
    DynamicBitset bits { 8 };

    bits.setFlag(0, true);
    bits.setFlag(7, true);

    // Force growth
    bits.setFlag(63, true);
    bits.setFlag(64, true);

    EXPECT_TRUE(bits[0]);
    EXPECT_TRUE(bits[7]);
    EXPECT_TRUE(bits[63]);
    EXPECT_TRUE(bits[64]);
}

TEST(DynamicBitset, DataPointerIsNotNullForNonEmptyBitset)
{
    DynamicBitset bits { 16 };
    bits.setFlag(3, true);

    const void* raw = bits.data();
    EXPECT_NE(raw, nullptr);
    EXPECT_EQ(*reinterpret_cast<const uint32*>(raw), 1U << 3);
}
