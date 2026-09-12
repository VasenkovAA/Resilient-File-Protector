#include "rfp/crypto/Encoding.h"
#include "rfp/core/ByteBuffer.h"

#include <gtest/gtest.h>

using namespace rfp::crypto;
using rfp::core::ByteBuffer;

// ===========================================================================
//  Base64 (RFC 4648 §10 test vectors)
// ===========================================================================
TEST(EncodingTests, Base64EncodeRfc4648Vectors) {
    EXPECT_EQ(base64Encode({}),                                     "");
    EXPECT_EQ(base64Encode(ByteBuffer{'f'}),                        "Zg==");
    EXPECT_EQ(base64Encode(ByteBuffer{'f','o'}),                    "Zm8=");
    EXPECT_EQ(base64Encode(ByteBuffer{'f','o','o'}),                "Zm9v");
    EXPECT_EQ(base64Encode(ByteBuffer{'f','o','o','b'}),            "Zm9vYg==");
    EXPECT_EQ(base64Encode(ByteBuffer{'f','o','o','b','a'}),        "Zm9vYmE=");
    EXPECT_EQ(base64Encode(ByteBuffer{'f','o','o','b','a','r'}),    "Zm9vYmFy");
}

TEST(EncodingTests, Base64DecodeRfc4648Vectors) {
    EXPECT_TRUE(base64Decode("").empty());
    EXPECT_EQ(base64Decode("Zg=="),       ByteBuffer({'f'}));
    EXPECT_EQ(base64Decode("Zm8="),       ByteBuffer({'f','o'}));
    EXPECT_EQ(base64Decode("Zm9v"),       ByteBuffer({'f','o','o'}));
    EXPECT_EQ(base64Decode("Zm9vYg=="),   ByteBuffer({'f','o','o','b'}));
    EXPECT_EQ(base64Decode("Zm9vYmE="),   ByteBuffer({'f','o','o','b','a'}));
    EXPECT_EQ(base64Decode("Zm9vYmFy"),   ByteBuffer({'f','o','o','b','a','r'}));
}

TEST(EncodingTests, Base64RoundTrip) {
    for (std::size_t n = 0; n < 32; ++n) {
        ByteBuffer src(n);
        for (std::size_t i = 0; i < n; ++i)
            src[i] = static_cast<rfp::core::Byte>((i * 37 + 11) & 0xFF);

        auto enc = base64Encode(src);
        auto dec = base64Decode(enc);
        EXPECT_EQ(dec, src) << "n=" << n;
    }
}

TEST(EncodingTests, Base64DecodeInvalidReturnsEmpty) {
    // Неверные символы
    EXPECT_TRUE(base64Decode("@@@@").empty());
    EXPECT_TRUE(base64Decode("Zg===").empty());
    EXPECT_TRUE(base64Decode("!!!!").empty());
}

// ===========================================================================
//  Hex
// ===========================================================================
TEST(EncodingTests, HexEncodeKnown) {
    EXPECT_EQ(hexEncode({}),                                  "");
    EXPECT_EQ(hexEncode(ByteBuffer{0x00}),                    "00");
    EXPECT_EQ(hexEncode(ByteBuffer{0xFF}),                    "ff");
    EXPECT_EQ(hexEncode(ByteBuffer{0xDE,0xAD,0xBE,0xEF}),     "deadbeef");
    EXPECT_EQ(hexEncode(ByteBuffer{0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF}),
              "0123456789abcdef");
}

TEST(EncodingTests, HexDecodeKnown) {
    EXPECT_TRUE(hexDecode("").empty());
    EXPECT_EQ(hexDecode("00"),             ByteBuffer{0x00});
    EXPECT_EQ(hexDecode("ff"),             ByteBuffer{0xFF});
    EXPECT_EQ(hexDecode("DEADBEEF"),       ByteBuffer({0xDE,0xAD,0xBE,0xEF}));
    EXPECT_EQ(hexDecode("deadbeef"),       ByteBuffer({0xDE,0xAD,0xBE,0xEF}));
    EXPECT_EQ(hexDecode("0123456789abcdef"),
              ByteBuffer({0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF}));
}

TEST(EncodingTests, HexRoundTrip) {
    for (std::size_t n = 0; n < 32; ++n) {
        ByteBuffer src(n);
        for (std::size_t i = 0; i < n; ++i)
            src[i] = static_cast<rfp::core::Byte>((i * 91 + 7) & 0xFF);
        EXPECT_EQ(hexDecode(hexEncode(src)), src);
    }
}

TEST(EncodingTests, HexDecodeOddLengthReturnsEmpty) {
    EXPECT_TRUE(hexDecode("a").empty());
    EXPECT_TRUE(hexDecode("abc").empty());
    EXPECT_TRUE(hexDecode("deadbee").empty());
}

TEST(EncodingTests, HexDecodeInvalidCharReturnsEmpty) {
    EXPECT_TRUE(hexDecode("zz").empty());
    EXPECT_TRUE(hexDecode("0g").empty());
    EXPECT_TRUE(hexDecode("g0").empty());
    EXPECT_TRUE(hexDecode("0x").empty());
}