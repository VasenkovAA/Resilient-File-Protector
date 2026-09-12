#include "rfp/crypto/SecureMemory.h"
#include "rfp/core/ByteBuffer.h"

#include <gtest/gtest.h>

using namespace rfp::crypto;
using rfp::core::ByteBuffer;

TEST(SecureMemoryTests, WipeZeroesBuffer) {
    ByteBuffer buf(64, 0xAB);
    secureWipe(buf);
    for (auto b : buf) EXPECT_EQ(b, 0);
}

TEST(SecureMemoryTests, WipeEmptyIsSafe) {
    ByteBuffer empty;
    secureWipe(empty);
    EXPECT_TRUE(empty.empty());
}

TEST(SecureMemoryTests, WipeSingleByte) {
    ByteBuffer buf(1, 0xFF);
    secureWipe(buf);
    EXPECT_EQ(buf[0], 0);
}

TEST(SecureMemoryTests, ConstantTimeEqualsSame) {
    ByteBuffer a(16, 0x42);
    ByteBuffer b(16, 0x42);
    EXPECT_TRUE(constantTimeEquals(a, b));
}

TEST(SecureMemoryTests, ConstantTimeEqualsDifferent) {
    ByteBuffer a(16, 0x42);
    ByteBuffer b(16, 0x42); b[15] = 0x43;
    EXPECT_FALSE(constantTimeEquals(a, b));
}

TEST(SecureMemoryTests, ConstantTimeEqualsDifferentSizes) {
    ByteBuffer a(16, 0x00);
    ByteBuffer b(15, 0x00);
    EXPECT_FALSE(constantTimeEquals(a, b));
}

TEST(SecureMemoryTests, ConstantTimeEqualsEmpty) {
    ByteBuffer a, b;
    EXPECT_TRUE(constantTimeEquals(a, b));
}

TEST(SecureMemoryTests, ConstantTimeEqualsEmptyVsNonEmpty) {
    ByteBuffer a;
    ByteBuffer b(1, 0);
    EXPECT_FALSE(constantTimeEquals(a, b));
}

TEST(SecureMemoryTests, ConstantTimeEqualsHighBitDifference) {
    ByteBuffer a(8, 0x00);
    ByteBuffer b(8, 0x00); b[0] = 0x80;
    EXPECT_FALSE(constantTimeEquals(a, b));
}