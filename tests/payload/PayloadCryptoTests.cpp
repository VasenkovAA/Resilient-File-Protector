
#include "TestHelpers.h"

#include "rfp/core/Error.h"
#include "rfp/crypto/CryptoRegistry.h"
#include "rfp/payload/PayloadCrypto.h"

#include <gtest/gtest.h>

#include <string>

using namespace rfp::payload;
using namespace rfp::crypto::test;
using rfp::core::ErrorCode;

// ===========================================================================
//  Basic round-trip
// ===========================================================================
TEST(PayloadCryptoTests, RoundTripDefaultParams) {
    auto pt = bytesOf("Secret message");
    EncryptParams ep; ep.password = "hunter2";

    auto enc = encrypt(pt, ep);
    ASSERT_TRUE(enc) << enc.error().message;
    EXPECT_TRUE(isPayload(enc.value()));

    // Header sanity
    const auto& buf = enc.value();
    EXPECT_EQ(buf[0], 'R');
    EXPECT_EQ(buf[1], 'F');
    EXPECT_EQ(buf[2], 'P');
    EXPECT_EQ(buf[3], '1');
    EXPECT_EQ(buf[4], 1);                                       // version
    EXPECT_EQ(buf[5], static_cast<rfp::core::Byte>(rfp::crypto::CipherId::Aes256Gcm));
    EXPECT_EQ(buf[6], static_cast<rfp::core::Byte>(rfp::crypto::KdfId::Pbkdf2HmacSha256));

    DecryptParams dp; dp.password = "hunter2";
    auto dec = decrypt(enc.value(), dp);
    ASSERT_TRUE(dec) << dec.error().message;
    EXPECT_EQ(dec.value(), pt);
}

TEST(PayloadCryptoTests, EmptyPlaintextRoundTrip) {
    EncryptParams ep; ep.password = "pw";
    auto enc = encrypt({}, ep);
    ASSERT_TRUE(enc);

    DecryptParams dp; dp.password = "pw";
    auto dec = decrypt(enc.value(), dp);
    ASSERT_TRUE(dec);
    EXPECT_TRUE(dec.value().empty());
}

TEST(PayloadCryptoTests, LongMessageRoundTrip) {
    std::string s(200000, 'A');
    auto pt = bytesOf(s);

    EncryptParams ep; ep.password = "pw"; ep.iterations = 1000;
    auto enc = encrypt(pt, ep);
    ASSERT_TRUE(enc);

    DecryptParams dp; dp.password = "pw";
    auto dec = decrypt(enc.value(), dp);
    ASSERT_TRUE(dec);
    EXPECT_EQ(dec.value(), pt);
}

TEST(PayloadCryptoTests, RandomSaltProducesDifferentCiphertext) {
    EncryptParams ep; ep.password = "pw";
    auto a = encrypt(bytesOf("same"), ep);
    auto b = encrypt(bytesOf("same"), ep);
    ASSERT_TRUE(a && b);
    EXPECT_NE(a.value(), b.value());
    EXPECT_EQ(a.value().size(), b.value().size());
}

// ===========================================================================
//  Wrong password / tampering
// ===========================================================================
TEST(PayloadCryptoTests, WrongPasswordFails) {
    EncryptParams ep; ep.password = "correct";
    auto enc = encrypt(bytesOf("data"), ep);
    ASSERT_TRUE(enc);

    DecryptParams dp; dp.password = "wrong";
    auto dec = decrypt(enc.value(), dp);
    ASSERT_FALSE(dec);
    EXPECT_EQ(dec.error().code, ErrorCode::DecodeError);
}

TEST(PayloadCryptoTests, TamperedMagicFails) {
    EncryptParams ep; ep.password = "pw";
    auto enc = encrypt(bytesOf("data"), ep);
    ASSERT_TRUE(enc);

    auto buf = enc.value();
    buf[0] = 'X';
    DecryptParams dp; dp.password = "pw";
    auto dec = decrypt(buf, dp);
    ASSERT_FALSE(dec);
    EXPECT_EQ(dec.error().code, ErrorCode::DecodeError);
}

TEST(PayloadCryptoTests, TamperedCiphertextFails) {
    EncryptParams ep; ep.password = "pw";
    auto enc = encrypt(bytesOf("authentic message"), ep);
    ASSERT_TRUE(enc);

    auto buf = enc.value();
    // Flip a bit somewhere in the ciphertext region (skip header+salt+iv).
    // Header 16 + salt 16 + iv 12 = 44; flip in the middle of the payload.
    ASSERT_GT(buf.size(), 50U);
    buf[50] ^= 0x01;

    DecryptParams dp; dp.password = "pw";
    auto dec = decrypt(buf, dp);
    ASSERT_FALSE(dec);
    EXPECT_EQ(dec.error().code, ErrorCode::DecodeError);
}

TEST(PayloadCryptoTests, TruncatedPayloadFails) {
    EncryptParams ep; ep.password = "pw";
    auto enc = encrypt(bytesOf("data"), ep);
    ASSERT_TRUE(enc);

    auto buf = enc.value();
    buf.resize(10);

    DecryptParams dp; dp.password = "pw";
    auto dec = decrypt(buf, dp);
    ASSERT_FALSE(dec);
}

TEST(PayloadCryptoTests, TamperedIterationsStillFails) {
    // Меняем итерации в header — KDF выдаст другой ключ, GCM не аутентифицируется.
    EncryptParams ep; ep.password = "pw"; ep.iterations = 1000;
    auto enc = encrypt(bytesOf("data"), ep);
    ASSERT_TRUE(enc);

    auto buf = enc.value();
    // Header offset 8 = iterations BE
    buf[8] = 0x00;
    buf[9] = 0x00;
    buf[10] = 0x00;
    buf[11] = 0x10;   // 16 вместо 1000

    DecryptParams dp; dp.password = "pw";
    auto dec = decrypt(buf, dp);
    ASSERT_FALSE(dec);
}

// ===========================================================================
//  Input validation
// ===========================================================================
TEST(PayloadCryptoTests, EmptyPasswordRejectedOnEncrypt) {
    EncryptParams ep;   // password empty
    auto enc = encrypt(bytesOf("data"), ep);
    ASSERT_FALSE(enc);
    EXPECT_EQ(enc.error().code, ErrorCode::InvalidArgument);
}

TEST(PayloadCryptoTests, EmptyPasswordRejectedOnDecrypt) {
    EncryptParams ep; ep.password = "pw";
    auto enc = encrypt(bytesOf("data"), ep);
    ASSERT_TRUE(enc);

    DecryptParams dp;   // empty
    auto dec = decrypt(enc.value(), dp);
    ASSERT_FALSE(dec);
    EXPECT_EQ(dec.error().code, ErrorCode::InvalidArgument);
}

TEST(PayloadCryptoTests, UnknownCipherRejected) {
    EncryptParams ep; ep.password = "pw";
    ep.cipher = static_cast<rfp::crypto::CipherId>(42);
    auto enc = encrypt(bytesOf("data"), ep);
    ASSERT_FALSE(enc);
    EXPECT_EQ(enc.error().code, ErrorCode::InvalidArgument);
}

TEST(PayloadCryptoTests, UnknownKdfRejected) {
    EncryptParams ep; ep.password = "pw";
    ep.kdf = static_cast<rfp::crypto::KdfId>(42);
    auto enc = encrypt(bytesOf("data"), ep);
    ASSERT_FALSE(enc);
    EXPECT_EQ(enc.error().code, ErrorCode::InvalidArgument);
}

TEST(PayloadCryptoTests, TooShortInputRejected) {
    DecryptParams dp; dp.password = "pw";
    auto dec = decrypt(bytesOf("short"), dp);
    ASSERT_FALSE(dec);
    EXPECT_EQ(dec.error().code, ErrorCode::DecodeError);
}

// ===========================================================================
//  isPayload()
// ===========================================================================
TEST(PayloadCryptoTests, IsPayloadDetectsCorrectly) {
    EXPECT_FALSE(isPayload({}));
    EXPECT_FALSE(isPayload(bytesOf("hello world")));
    EXPECT_FALSE(isPayload(bytesOf("RFP")));
    EXPECT_FALSE(isPayload(bytesOf("RFP0____________")));   // wrong version

    EncryptParams ep; ep.password = "pw";
    auto enc = encrypt(bytesOf("data"), ep);
    ASSERT_TRUE(enc);
    EXPECT_TRUE(isPayload(enc.value()));
}

// ===========================================================================
//  All ciphers round-trip
// ===========================================================================
class PayloadAllCiphers
    : public ::testing::TestWithParam<rfp::crypto::CipherId> {};

TEST_P(PayloadAllCiphers, RoundTrip) {
    const auto id = GetParam();
    const auto spec = rfp::crypto::CryptoRegistry::cipherSpec(id);
    ASSERT_TRUE(spec.has_value());

    EncryptParams ep; ep.password = "pw"; ep.cipher = id;
    auto enc = encrypt(bytesOf("hello world"), ep);
    ASSERT_TRUE(enc) << spec->name << ": " << enc.error().message;

    DecryptParams dp; dp.password = "pw";
    auto dec = decrypt(enc.value(), dp);
    ASSERT_TRUE(dec) << spec->name << ": " << dec.error().message;
    EXPECT_EQ(dec.value(), bytesOf("hello world"));
}

INSTANTIATE_TEST_SUITE_P(AllCiphers, PayloadAllCiphers,
                         ::testing::Values(rfp::crypto::CipherId::Aes128Gcm,
                                           rfp::crypto::CipherId::Aes256Gcm,
                                           rfp::crypto::CipherId::ChaCha20Poly1305,
                                           rfp::crypto::CipherId::Aes256Cbc,
                                           rfp::crypto::CipherId::Aes256Ctr));

// ===========================================================================
//  All KDFs round-trip
// ===========================================================================
class PayloadAllKdfs
    : public ::testing::TestWithParam<rfp::crypto::KdfId> {};

TEST_P(PayloadAllKdfs, RoundTrip) {
    const auto id = GetParam();
    if (id == rfp::crypto::KdfId::Argon2id) {
        GTEST_SKIP() << "Argon2id not implemented yet";
    }

    EncryptParams ep; ep.password = "pw"; ep.kdf = id;
    ep.iterations = 1000;   // keep tests fast
    auto enc = encrypt(bytesOf("hello"), ep);
    ASSERT_TRUE(enc);

    DecryptParams dp; dp.password = "pw";
    auto dec = decrypt(enc.value(), dp);
    ASSERT_TRUE(dec);
    EXPECT_EQ(dec.value(), bytesOf("hello"));
}

INSTANTIATE_TEST_SUITE_P(AllKdfs, PayloadAllKdfs,
                         ::testing::Values(rfp::crypto::KdfId::Pbkdf2HmacSha256,
                                           rfp::crypto::KdfId::Pbkdf2HmacSha512,
                                           rfp::crypto::KdfId::Scrypt,
                                           rfp::crypto::KdfId::Argon2id));

// ===========================================================================
//  Wire-format details
// ===========================================================================
TEST(PayloadCryptoTests, SaltAndIvAreRandomised) {
    EncryptParams ep; ep.password = "pw";
    auto a = encrypt(bytesOf("x"), ep);
    auto b = encrypt(bytesOf("x"), ep);
    ASSERT_TRUE(a && b);

    // Header is 16 bytes; salt at [16,32), iv at [32,44).
    const auto& A = a.value();
    const auto& B = b.value();
    ASSERT_GE(A.size(), 44U);
    ASSERT_GE(B.size(), 44U);

    const bool saltDiffers = !std::equal(A.begin() + 16, A.begin() + 32, B.begin() + 16);
    const bool ivDiffers   = !std::equal(A.begin() + 32, A.begin() + 44, B.begin() + 32);
    EXPECT_TRUE(saltDiffers);
    EXPECT_TRUE(ivDiffers);
}

TEST(PayloadCryptoTests, IterationsAreStoredInHeader) {
    EncryptParams ep; ep.password = "pw"; ep.iterations = 12345;
    auto enc = encrypt(bytesOf("x"), ep);
    ASSERT_TRUE(enc);

    const auto& buf = enc.value();
    const std::uint32_t stored =
        (static_cast<std::uint32_t>(buf[8])  << 24) |
        (static_cast<std::uint32_t>(buf[9])  << 16) |
        (static_cast<std::uint32_t>(buf[10]) <<  8) |
        static_cast<std::uint32_t>(buf[11]);
    EXPECT_EQ(stored, 12345u);
}