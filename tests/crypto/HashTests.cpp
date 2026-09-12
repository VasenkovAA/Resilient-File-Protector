#include "TestHelpers.h"
#include "rfp/crypto/Hash.h"
#include "rfp/crypto/CryptoRegistry.h"

#include <gtest/gtest.h>

using namespace rfp::crypto;
using namespace rfp::crypto::test;

// ===========================================================================
//  Digest (NIST FIPS 180-4 / FIPS 202, BLAKE2 spec)
// ===========================================================================

TEST(HashTests, Sha256KnownVectors) {
    auto empty = Hash::digest(HashId::Sha256, {});
    ASSERT_TRUE(empty);
    EXPECT_EQ(toHex(*empty),
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    auto abc = Hash::digest(HashId::Sha256, bytesOf("abc"));
    ASSERT_TRUE(abc);
    EXPECT_EQ(toHex(*abc),
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(HashTests, Sha512KnownVectors) {
    auto empty = Hash::digest(HashId::Sha512, {});
    ASSERT_TRUE(empty);
    EXPECT_EQ(toHex(*empty),
        "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce"
        "47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e");

    auto abc = Hash::digest(HashId::Sha512, bytesOf("abc"));
    ASSERT_TRUE(abc);
    EXPECT_EQ(toHex(*abc),
        "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
        "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
}

TEST(HashTests, Sha3_256KnownVectors) {
    auto empty = Hash::digest(HashId::Sha3_256, {});
    ASSERT_TRUE(empty);
    EXPECT_EQ(toHex(*empty),
              "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a");

    auto abc = Hash::digest(HashId::Sha3_256, bytesOf("abc"));
    ASSERT_TRUE(abc);
    EXPECT_EQ(toHex(*abc),
              "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532");
}

TEST(HashTests, Sha3_512KnownVectors) {
    auto empty = Hash::digest(HashId::Sha3_512, {});
    ASSERT_TRUE(empty);
    EXPECT_EQ(toHex(*empty),
        "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a6"
        "15b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26");

    auto abc = Hash::digest(HashId::Sha3_512, bytesOf("abc"));
    ASSERT_TRUE(abc);
    EXPECT_EQ(toHex(*abc),
        "b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e"
        "10e116e9192af3c91a7ec57647e3934057340b4cf408d5a56592f8274eec53f0");
}

TEST(HashTests, Blake2bKnownVectors) {
    auto empty = Hash::digest(HashId::Blake2b, {});
    ASSERT_TRUE(empty);
    EXPECT_EQ(toHex(*empty),
        "786a02f742015903c6c6fd852552d272912f4740e15847618a86e217f71f5419"
        "d25e1031afee585313896444934eb04b903a685b1448b755d56f701afe9be2ce");

    auto abc = Hash::digest(HashId::Blake2b, bytesOf("abc"));
    ASSERT_TRUE(abc);
    EXPECT_EQ(toHex(*abc),
        "ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d1"
        "7d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923");
}

TEST(HashTests, DigestSizesMatchRegistry) {
    for (const auto& h : CryptoRegistry::availableHashes()) {
        auto d = Hash::digest(h.id, bytesOf("x"));
        ASSERT_TRUE(d) << h.name;
        EXPECT_EQ(d->size(), h.digestSize) << h.name;
    }
}

TEST(HashTests, UnknownHashReturnsError) {
    auto d = Hash::digest(static_cast<HashId>(999), bytesOf("x"));
    ASSERT_FALSE(d);
    EXPECT_EQ(d.error().code, rfp::core::ErrorCode::InvalidArgument);

    auto h = Hash::hmac(static_cast<HashId>(999), zeros(16), bytesOf("x"));
    ASSERT_FALSE(h);
    EXPECT_EQ(h.error().code, rfp::core::ErrorCode::InvalidArgument);
}

// ===========================================================================
//  HMAC — RFC 4231
// ===========================================================================

TEST(HashTests, HmacSha256_Rfc4231_TestCase1) {
    auto key  = ByteBuffer(20, 0x0b);
    auto data = bytesOf("Hi There");

    auto m = Hash::hmac(HashId::Sha256, key, data);
    ASSERT_TRUE(m);
    EXPECT_EQ(toHex(*m),
              "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
}

TEST(HashTests, HmacSha256_Rfc4231_TestCase2) {
    auto key  = bytesOf("Jefe");
    auto data = bytesOf("what do ya want for nothing?");

    auto m = Hash::hmac(HashId::Sha256, key, data);
    ASSERT_TRUE(m);
    EXPECT_EQ(toHex(*m),
              "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
}

TEST(HashTests, HmacSha256_Rfc4231_TestCase3) {
    auto key  = ByteBuffer(20, 0xaa);
    auto data = ByteBuffer(50, 0xdd);

    auto m = Hash::hmac(HashId::Sha256, key, data);
    ASSERT_TRUE(m);
    EXPECT_EQ(toHex(*m),
              "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe");
}

TEST(HashTests, HmacSha512_Rfc4231_TestCase1) {
    auto key  = ByteBuffer(20, 0x0b);
    auto data = bytesOf("Hi There");

    auto m = Hash::hmac(HashId::Sha512, key, data);
    ASSERT_TRUE(m);
    EXPECT_EQ(toHex(*m),
        "87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cde"
        "daa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854");
}

TEST(HashTests, HmacSha512_Rfc4231_TestCase2) {
    auto key  = bytesOf("Jefe");
    auto data = bytesOf("what do ya want for nothing?");

    auto m = Hash::hmac(HashId::Sha512, key, data);
    ASSERT_TRUE(m);
    EXPECT_EQ(toHex(*m),
        "164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea250554"
        "9758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737");
}

TEST(HashTests, HmacWorksWithAllHashes) {
    for (const auto& h : CryptoRegistry::availableHashes()) {
        auto m = Hash::hmac(h.id, zeros(32), bytesOf("payload"));
        ASSERT_TRUE(m) << h.name;
        EXPECT_EQ(m->size(), h.digestSize) << h.name;
    }
}

TEST(HashTests, HmacEmptyKeyAndData) {
    auto m = Hash::hmac(HashId::Sha256, {}, {});
    ASSERT_TRUE(m) << m.error().message;
    EXPECT_EQ(m->size(), 32U);
    EXPECT_EQ(toHex(*m),
              "b613679a0814d9ec772f95d778c35fc5"
              "ff1697c493715653c6c712144292c5ad");
}

TEST(HashTests, HmacSha512EmptyKeyAndData) {
    auto m = Hash::hmac(HashId::Sha512, {}, {});
    ASSERT_TRUE(m) << m.error().message;
    EXPECT_EQ(m->size(), 64U);
    EXPECT_EQ(toHex(*m),
              "b936cee86c9f87aa5d3c6f2e84cb5a42"
              "39a5fe50480a6ec66b70ab5b1f4ac673"
              "0c6c515421b327ec1d69402e53dfb49a"
              "d7381eb067b338fd7b0cb22247225d47");
}

// ===========================================================================
//  Different inputs -> different digests
// ===========================================================================
TEST(HashTests, DifferentInputsDifferentDigests) {
    auto a = Hash::digest(HashId::Sha256, bytesOf("a"));
    auto b = Hash::digest(HashId::Sha256, bytesOf("b"));
    ASSERT_TRUE(a && b);
    EXPECT_NE(*a, *b);
}

TEST(HashTests, HmacDifferentKeyDifferentTag) {
    auto m1 = Hash::hmac(HashId::Sha256, bytesOf("key1"), bytesOf("data"));
    auto m2 = Hash::hmac(HashId::Sha256, bytesOf("key2"), bytesOf("data"));
    ASSERT_TRUE(m1 && m2);
    EXPECT_NE(*m1, *m2);
}