#include "rfp/core/Crc32.h"

#include <gtest/gtest.h>

TEST(Crc32Tests, KnownValueForAsciiText)
{
    EXPECT_EQ(rfp::core::crc32("123456789"), 0xCBF43926U);
}
