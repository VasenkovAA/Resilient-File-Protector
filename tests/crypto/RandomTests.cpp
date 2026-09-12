#include "rfp/crypto/Random.h"

#include <gtest/gtest.h>

using namespace rfp::crypto;

TEST(RandomTests, BytesSizeRespected) {
    for (std::size_t n : {0U, 1U, 16U, 32U, 1024U}) {
        auto b = Random::bytes(n);
        ASSERT_TRUE(b);
        EXPECT_EQ(b->size(), n);
    }
}

TEST(RandomTests, ConsecutiveCallsDiffer) {
    auto a = Random::bytes(32);
    auto b = Random::bytes(32);
    ASSERT_TRUE(a && b);
    // Столкновение 32-байтовых CSPRNG значений астрономически маловероятно.
    EXPECT_NE(*a, *b);
}

TEST(RandomTests, FillModifiesBuffer) {
    rfp::core::ByteBuffer buf(64, 0xAA);
    auto err = Random::fill(buf);
    EXPECT_TRUE(err.ok());

    // Не все байты остались 0xAA
    bool anyChanged = false;
    for (auto v : buf) if (v != 0xAA) { anyChanged = true; break; }
    EXPECT_TRUE(anyChanged);
}

TEST(RandomTests, FillEmptyIsSafe) {
    rfp::core::ByteBuffer empty;
    EXPECT_TRUE(Random::fill(empty).ok());
}