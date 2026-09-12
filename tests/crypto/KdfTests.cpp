#include "TestHelpers.h"
#include "rfp/crypto/Kdf.h"

#include <gtest/gtest.h>

using namespace rfp::crypto;
using namespace rfp::crypto::test;

namespace {

KdfParams makeParams(const ByteBuffer& salt, std::uint32_t iters = 1) {
    KdfParams p;
    p.salt = salt;
    p.iterations = iters;
    return p;
}

} // namespace

// ===========================================================================
//  PBKDF2-HMAC-SHA256 (RFC 6070-style vectors)
// ===========================================================================
TEST(KdfTests, Pbkdf2Sha256_Iter1) {
    auto salt = bytesOf("salt");
    auto k = Kdf::deriveKey(KdfId::Pbkdf2HmacSha256, "password", makeParams(salt, 1), 32);
    ASSERT_TRUE(k) << k.error().message;
    EXPECT_EQ(toHex(*k),
              "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
}

TEST(KdfTests, Pbkdf2Sha256_Iter2) {
    auto salt = bytesOf("salt");
    auto k = Kdf::deriveKey(KdfId::Pbkdf2HmacSha256, "password", makeParams(salt, 2), 32);
    ASSERT_TRUE(k);
    EXPECT_EQ(toHex(*k),
              "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43");
}

TEST(KdfTests, Pbkdf2Sha256_Iter4096) {
    auto salt = bytesOf("salt");
    auto k = Kdf::deriveKey(KdfId::Pbkdf2HmacSha256, "password", makeParams(salt, 4096), 32);
    ASSERT_TRUE(k);
    EXPECT_EQ(toHex(*k),
              "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");
}

// ===========================================================================
//  PBKDF2-HMAC-SHA512
// ===========================================================================
TEST(KdfTests, Pbkdf2Sha512_Iter1) {
    auto salt = bytesOf("salt");
    auto k = Kdf::deriveKey(KdfId::Pbkdf2HmacSha512, "password", makeParams(salt, 1), 64);
    ASSERT_TRUE(k);
    EXPECT_EQ(toHex(*k),
        "867f70cf1ade02cff3752599a3a53dc4af34c7a669815ae5d513554e1c8cf252"
        "c02d470a285a0501bad999bfe943c08f050235d7d68b1da55e63f73b60a57fce");
}

TEST(KdfTests, Pbkdf2Sha512_Iter2) {
    auto salt = bytesOf("salt");
    auto k = Kdf::deriveKey(KdfId::Pbkdf2HmacSha512, "password", makeParams(salt, 2), 64);
    ASSERT_TRUE(k);
    EXPECT_EQ(toHex(*k),
        "e1d9c16aa681708a45f5c7c4e215ceb66e011a2e9f0040713f18aefdb866d53c"
        "f76cab2868a39b9f7840edce4fef5a82be67335c77a6068e04112754f27ccf4e");
}

TEST(KdfTests, Pbkdf2Sha512_Iter4096) {
    auto salt = bytesOf("salt");
    auto k = Kdf::deriveKey(KdfId::Pbkdf2HmacSha512, "password", makeParams(salt, 4096), 64);
    ASSERT_TRUE(k);
    EXPECT_EQ(toHex(*k),
        "d197b1b33db0143e018b12f3d1d1479e6cdebdcc97c5c0f87f6902e072f457b5"
        "143f30602641b3d55cd335988cb36b84376060ecd532e039b742a239434af2d5");
}

// ===========================================================================
//  PBKDF2 — длина вывода
// ===========================================================================
TEST(KdfTests, Pbkdf2OutputLengthRespected) {
    for (std::size_t len : {16U, 32U, 48U, 64U, 100U}) {
        auto k = Kdf::deriveKey(KdfId::Pbkdf2HmacSha256, "pw",
                                makeParams(bytesOf("salt"), 1), len);
        ASSERT_TRUE(k);
        EXPECT_EQ(k->size(), len);
    }
}

// ===========================================================================
//  scrypt — consistency, разные соли -> разные ключи
// ===========================================================================
TEST(KdfTests, ScryptIsDeterministic) {
    auto salt = bytesOf("some salt 1234");
    auto p = makeParams(salt, 0);
    p.parallelism = 1;

    auto k1 = Kdf::deriveKey(KdfId::Scrypt, "password", p, 32);
    auto k2 = Kdf::deriveKey(KdfId::Scrypt, "password", p, 32);
    ASSERT_TRUE(k1 && k2);
    EXPECT_EQ(*k1, *k2);
    EXPECT_EQ(k1->size(), 32U);
}

TEST(KdfTests, ScryptDifferentSalt) {
    auto p1 = makeParams(bytesOf("salt-A"), 0);
    auto p2 = makeParams(bytesOf("salt-B"), 0);

    auto k1 = Kdf::deriveKey(KdfId::Scrypt, "password", p1, 32);
    auto k2 = Kdf::deriveKey(KdfId::Scrypt, "password", p2, 32);
    ASSERT_TRUE(k1 && k2);
    EXPECT_NE(*k1, *k2);
}

TEST(KdfTests, ScryptDifferentPassword) {
    auto p = makeParams(bytesOf("same salt"), 0);
    auto k1 = Kdf::deriveKey(KdfId::Scrypt, "pw1", p, 32);
    auto k2 = Kdf::deriveKey(KdfId::Scrypt, "pw2", p, 32);
    ASSERT_TRUE(k1 && k2);
    EXPECT_NE(*k1, *k2);
}

// ===========================================================================
//  Argon2id — пока NotImplemented
// ===========================================================================
TEST(KdfTests, Argon2idNotImplemented) {
    auto p = makeParams(bytesOf("salt"), 3);
    p.parallelism = 1;
    auto k = Kdf::deriveKey(KdfId::Argon2id, "password", p, 32);
    ASSERT_FALSE(k);
    EXPECT_EQ(k.error().code, rfp::core::ErrorCode::NotImplemented);
}

// ===========================================================================
//  Ошибки
// ===========================================================================
TEST(KdfTests, EmptySaltRejected) {
    KdfParams p; p.iterations = 1;
    auto k = Kdf::deriveKey(KdfId::Pbkdf2HmacSha256, "pw", p, 32);
    ASSERT_FALSE(k);
    EXPECT_EQ(k.error().code, rfp::core::ErrorCode::InvalidArgument);
}

TEST(KdfTests, ZeroKeyLengthRejected) {
    auto k = Kdf::deriveKey(KdfId::Pbkdf2HmacSha256, "pw",
                            makeParams(bytesOf("salt"), 1), 0);
    ASSERT_FALSE(k);
    EXPECT_EQ(k.error().code, rfp::core::ErrorCode::InvalidArgument);
}

TEST(KdfTests, UnknownKdfRejected) {
    auto k = Kdf::deriveKey(static_cast<KdfId>(999), "pw",
                            makeParams(bytesOf("salt"), 1), 32);
    ASSERT_FALSE(k);
    EXPECT_EQ(k.error().code, rfp::core::ErrorCode::InvalidArgument);
}

TEST(KdfTests, EmptyPasswordStillWorks) {
    auto k = Kdf::deriveKey(KdfId::Pbkdf2HmacSha256, "",
                            makeParams(bytesOf("salt"), 1), 32);
    ASSERT_TRUE(k);
    EXPECT_EQ(k->size(), 32U);
}

// ===========================================================================
//  Salt generation
// ===========================================================================
TEST(KdfTests, GenerateSalt) {
    auto s1 = Kdf::generateSalt(16);
    auto s2 = Kdf::generateSalt(16);

    EXPECT_EQ(s1.size(), 16U);
    EXPECT_EQ(s2.size(), 16U);
    EXPECT_NE(s1, s2);   // CSPRNG

    EXPECT_EQ(Kdf::generateSalt(0).size(), 0U);
    EXPECT_EQ(Kdf::generateSalt(64).size(), 64U);
}