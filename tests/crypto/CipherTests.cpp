#include "TestHelpers.h"
#include "rfp/crypto/Cipher.h"
#include "rfp/crypto/CryptoRegistry.h"
#include "rfp/crypto/Random.h"

#include <gtest/gtest.h>

using namespace rfp::crypto;
using namespace rfp::crypto::test;

namespace {

[[maybe_unused]] CipherParams makeParams(CipherId id,
                        const ByteBuffer& key,
                        const ByteBuffer& iv,
                        const ByteBuffer& aad = {},
                        const ByteBuffer& tag = {}) {
    CipherParams p;
    p.key = key;
    p.iv  = iv;
    p.aad = aad;
    p.tag = tag;
    return p;
}

const char* nameOf(CipherId id) {
    return CryptoRegistry::cipherSpec(id)->name;
}

} // namespace

// ===========================================================================
//  NIST / RFC test vectors
// ===========================================================================

// NIST SP 800-38D, Test Case 1 (AES-128-GCM)
TEST(CipherTests, Aes128Gcm_TestCase1_EmptyPlaintext) {
    auto key = fromHex("00000000000000000000000000000000");
    auto iv  = fromHex("000000000000000000000000");
    auto pt  = ByteBuffer{};

    auto r = Cipher::encrypt(CipherId::Aes128Gcm, makeParams(CipherId::Aes128Gcm, key, iv), pt);
    ASSERT_TRUE(r) << r.error().message;
    EXPECT_TRUE(r->ciphertext.empty());
    EXPECT_EQ(toHex(r->tag), "58e2fccefa7e3061367f1d57a4e7455a");
}

// NIST SP 800-38D, Test Case 2 (AES-128-GCM)
TEST(CipherTests, Aes128Gcm_TestCase2_16BytePlaintext) {
    auto key = fromHex("00000000000000000000000000000000");
    auto iv  = fromHex("000000000000000000000000");
    auto pt  = fromHex("00000000000000000000000000000000");

    auto r = Cipher::encrypt(CipherId::Aes128Gcm, makeParams(CipherId::Aes128Gcm, key, iv), pt);
    ASSERT_TRUE(r) << r.error().message;
    EXPECT_EQ(toHex(r->ciphertext), "0388dace60b6a392f328c2b971b2fe78");
    EXPECT_EQ(toHex(r->tag),        "ab6e47d42cec13bdf53a67b21257bddf");
}

// NIST SP 800-38D, Test Case 4 (AES-128-GCM, with AAD)
TEST(CipherTests, Aes128Gcm_TestCase4_WithAad) {
    auto key = fromHex("feffe9928665731c6d6a8f9467308308");
    auto iv  = fromHex("cafebabefacedbaddecaf888");
    auto aad = fromHex("feedfacedeadbeeffeedfacedeadbeefabaddad2");
    // 60 bytes — ВНИМАНИЕ: TC4, не TC5!
    auto pt  = fromHex("d9313225f88406e5a55909c5aff5269a"
                       "86a7a9531534f7da2e4c303d8a318a72"
                       "1c3c0c95956809532fcf0e2449a6b525"
                       "b16aedf5aa0de657ba637b39");

    auto params = makeParams(CipherId::Aes128Gcm, key, iv, aad);
    auto r = Cipher::encrypt(CipherId::Aes128Gcm, params, pt);
    ASSERT_TRUE(r) << r.error().message;

    EXPECT_EQ(toHex(r->ciphertext),
              "42831ec2217774244b7221b784d0d49c"
              "e3aa212f2c02a4e035c17e2329aca12e"
              "21d514b25466931c7d8f6a5aac84aa05"
              "1ba30b396a0aac973d58e091");
    EXPECT_EQ(toHex(r->tag), "5bc94fbc3221a5db94fae95ae7121a47");

    auto dp = makeParams(CipherId::Aes128Gcm, key, iv, aad, r->tag);
    auto d = Cipher::decrypt(CipherId::Aes128Gcm, dp, r->ciphertext);
    ASSERT_TRUE(d) << d.error().message;
    EXPECT_EQ(d.value(), pt);
}

TEST(CipherTests, Aes128Gcm_TestCase5_WithoutAad) {
    auto key = fromHex("feffe9928665731c6d6a8f9467308308");
    auto iv  = fromHex("cafebabefacedbaddecaf888");
    auto pt  = fromHex("d9313225f88406e5a55909c5aff5269a"
                       "86a7a9531534f7da2e4c303d8a318a72"
                       "1c3c0c95956809532fcf0e2449a6b525"
                       "b16aedf5aa0de657ba637b391aafd255");

    auto r = Cipher::encrypt(CipherId::Aes128Gcm,
                             makeParams(CipherId::Aes128Gcm, key, iv), pt);
    ASSERT_TRUE(r) << r.error().message;

    EXPECT_EQ(toHex(r->ciphertext),
              "42831ec2217774244b7221b784d0d49c"
              "e3aa212f2c02a4e035c17e2329aca12e"
              "21d514b25466931c7d8f6a5aac84aa05"
              "1ba30b396a0aac973d58e091473f5985");
    EXPECT_EQ(toHex(r->tag), "4d5c2af327cd64a62cf35abd2ba6fab4");
}

// NIST SP 800-38D, Test Case 13 (AES-256-GCM)
TEST(CipherTests, Aes256Gcm_TestCase13_EmptyPlaintext) {
    auto key = fromHex("0000000000000000000000000000000000000000000000000000000000000000");
    auto iv  = fromHex("000000000000000000000000");

    auto r = Cipher::encrypt(CipherId::Aes256Gcm,
                             makeParams(CipherId::Aes256Gcm, key, iv), {});
    ASSERT_TRUE(r) << r.error().message;
    EXPECT_EQ(toHex(r->tag), "530f8afbc74536b9a963b4f1c4cb738b");
}

// NIST SP 800-38D, Test Case 14 (AES-256-GCM)
TEST(CipherTests, Aes256Gcm_TestCase14_16BytePlaintext) {
    auto key = fromHex("0000000000000000000000000000000000000000000000000000000000000000");
    auto iv  = fromHex("000000000000000000000000");
    auto pt  = fromHex("00000000000000000000000000000000");

    auto r = Cipher::encrypt(CipherId::Aes256Gcm,
                             makeParams(CipherId::Aes256Gcm, key, iv), pt);
    ASSERT_TRUE(r) << r.error().message;
    EXPECT_EQ(toHex(r->ciphertext), "cea7403d4d606b6e074ec5d3baf39d18");
    EXPECT_EQ(toHex(r->tag),        "d0d1c8a799996bf0265b98b5d48ab919");
}

// RFC 8439 §2.8.2 — ChaCha20-Poly1305 AEAD
TEST(CipherTests, ChaCha20Poly1305_Rfc8439) {
    auto key   = fromHex("808182838485868788898a8b8c8d8e8f"
                         "909192939495969798999a9b9c9d9e9f");
    auto nonce = fromHex("070000004041424344454647");
    auto aad   = fromHex("50515253c0c1c2c3c4c5c6c7");
    auto pt    = bytesOf("Ladies and Gentlemen of the class of '99: If I could "
                         "offer you only one tip for the future, sunscreen would be it.");

    auto params = makeParams(CipherId::ChaCha20Poly1305, key, nonce, aad);
    auto r = Cipher::encrypt(CipherId::ChaCha20Poly1305, params, pt);
    ASSERT_TRUE(r) << r.error().message;

    EXPECT_EQ(toHex(r->ciphertext),
              "d31a8d34648e60db7b86afbc53ef7ec2"
              "a4aded51296e08fea9e2b5a736ee62d6"
              "3dbea45e8ca9671282fafb69da92728b"
              "1a71de0a9e060b2905d6a5b67ecd3b36"
              "92ddbd7f2d778b8c9803aee328091b58"
              "fab324e4fad675945585808b4831d7bc"
              "3ff4def08e4b7a9de576d26586cec64b"
              "6116");
    EXPECT_EQ(toHex(r->tag), "1ae10b594f09e26a7e902ecbd0600691");
}

// ===========================================================================
//  Round-trip для всех пяти шифров
// ===========================================================================
class CipherRoundTrip : public ::testing::TestWithParam<CipherId> {};

TEST_P(CipherRoundTrip, EncryptDecrypt) {
    const auto id   = GetParam();
    const auto spec = *CryptoRegistry::cipherSpec(id);

    auto key = Random::bytes(spec.keySize).value();
    auto iv  = Random::bytes(spec.ivSize).value();
    auto pt  = bytesOf("The quick brown fox jumps over the lazy dog. 1234567890");

    auto params = makeParams(id, key, iv);
    auto enc = Cipher::encrypt(id, params, pt);
    ASSERT_TRUE(enc) << nameOf(id) << ": " << enc.error().message;

    auto dp = makeParams(id, key, enc->iv, {}, enc->tag);
    auto dec = Cipher::decrypt(id, dp, enc->ciphertext);
    ASSERT_TRUE(dec) << nameOf(id) << ": " << dec.error().message;
    EXPECT_EQ(dec.value(), pt);
}

INSTANTIATE_TEST_SUITE_P(AllCiphers, CipherRoundTrip,
    ::testing::Values(CipherId::Aes128Gcm,
                      CipherId::Aes256Gcm,
                      CipherId::ChaCha20Poly1305,
                      CipherId::Aes256Cbc,
                      CipherId::Aes256Ctr));

// ===========================================================================
//  Отличия ключа / IV / AAD влияют на результат (защита от «пустого» шифра)
// ===========================================================================
TEST(CipherTests, DifferentKeyProducesDifferentCiphertext) {
    auto iv = zeros(12);
    auto pt = bytesOf("deterministic");

    auto k1 = zeros(32); auto k2 = zeros(32); k2[0] = 0xFF;

    auto e1 = Cipher::encrypt(CipherId::Aes256Gcm, makeParams(CipherId::Aes256Gcm, k1, iv), pt);
    auto e2 = Cipher::encrypt(CipherId::Aes256Gcm, makeParams(CipherId::Aes256Gcm, k2, iv), pt);
    ASSERT_TRUE(e1 && e2);
    EXPECT_NE(e1->ciphertext, e2->ciphertext);
    EXPECT_NE(e1->tag,        e2->tag);
}

TEST(CipherTests, DifferentIvProducesDifferentCiphertext) {
    auto k  = zeros(32);
    auto i1 = zeros(12); auto i2 = zeros(12); i2[0] = 1;
    auto pt = bytesOf("deterministic");

    auto e1 = Cipher::encrypt(CipherId::Aes256Gcm, makeParams(CipherId::Aes256Gcm, k, i1), pt);
    auto e2 = Cipher::encrypt(CipherId::Aes256Gcm, makeParams(CipherId::Aes256Gcm, k, i2), pt);
    ASSERT_TRUE(e1 && e2);
    EXPECT_NE(e1->ciphertext, e2->ciphertext);
}

TEST(CipherTests, DifferentAadProducesDifferentTag) {
    auto k   = zeros(32);
    auto iv  = zeros(12);
    auto pt  = bytesOf("deterministic");
    auto aad1 = bytesOf("AAD-1");
    auto aad2 = bytesOf("AAD-2");

    auto e1 = Cipher::encrypt(CipherId::Aes256Gcm,
                              makeParams(CipherId::Aes256Gcm, k, iv, aad1), pt);
    auto e2 = Cipher::encrypt(CipherId::Aes256Gcm,
                              makeParams(CipherId::Aes256Gcm, k, iv, aad2), pt);
    ASSERT_TRUE(e1 && e2);
    EXPECT_EQ(e1->ciphertext, e2->ciphertext);   // AAD не влияет на CT для GCM
    EXPECT_NE(e1->tag,        e2->tag);          // но влияет на tag
}

// ===========================================================================
//  Автогенерация IV
// ===========================================================================
TEST(CipherTests, EmptyIvIsAutoGenerated) {
    auto k  = zeros(32);
    auto pt = bytesOf("auto-iv");
    CipherParams p; p.key = k;   // IV пустой

    auto e1 = Cipher::encrypt(CipherId::Aes256Gcm, p, pt);
    auto e2 = Cipher::encrypt(CipherId::Aes256Gcm, p, pt);
    ASSERT_TRUE(e1 && e2);

    EXPECT_EQ(e1->iv.size(), 12U);
    EXPECT_EQ(e2->iv.size(), 12U);
    EXPECT_NE(e1->iv, e2->iv);   // CSPRNG должен дать разные IV

    // Оба варианта должны корректно расшифровываться
    auto d1 = Cipher::decrypt(CipherId::Aes256Gcm,
                              makeParams(CipherId::Aes256Gcm, k, e1->iv, {}, e1->tag),
                              e1->ciphertext);
    auto d2 = Cipher::decrypt(CipherId::Aes256Gcm,
                              makeParams(CipherId::Aes256Gcm, k, e2->iv, {}, e2->tag),
                              e2->ciphertext);
    EXPECT_EQ(d1.value(), pt);
    EXPECT_EQ(d2.value(), pt);
}

// ===========================================================================
//  Ошибки параметров
// ===========================================================================
TEST(CipherTests, UnknownCipherRejected) {
    CipherParams p;
    p.key = zeros(32);
    p.iv  = zeros(12);
    auto r = Cipher::encrypt(static_cast<CipherId>(42), p, bytesOf("x"));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code, rfp::core::ErrorCode::InvalidArgument);

    auto d = Cipher::decrypt(static_cast<CipherId>(42), p, bytesOf("x"));
    ASSERT_FALSE(d);
    EXPECT_EQ(d.error().code, rfp::core::ErrorCode::InvalidArgument);
}

TEST(CipherTests, WrongKeySizeRejected) {
    CipherParams p;
    p.key = zeros(16);   // должно быть 32 для Aes256Gcm
    p.iv  = zeros(12);
    auto r = Cipher::encrypt(CipherId::Aes256Gcm, p, bytesOf("x"));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code, rfp::core::ErrorCode::InvalidArgument);
}

TEST(CipherTests, WrongIvSizeRejected) {
    CipherParams p;
    p.key = zeros(32);
    p.iv  = zeros(10);   // должно быть 12
    auto r = Cipher::encrypt(CipherId::Aes256Gcm, p, bytesOf("x"));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code, rfp::core::ErrorCode::InvalidArgument);
}

TEST(CipherTests, WrongTagSizeRejectedOnDecrypt) {
    auto k = zeros(32);
    auto iv = zeros(12);
    auto e = Cipher::encrypt(CipherId::Aes256Gcm,
                             makeParams(CipherId::Aes256Gcm, k, iv),
                             bytesOf("test"));
    ASSERT_TRUE(e);

    CipherParams p;
    p.key = k;
    p.iv  = iv;
    p.tag = zeros(8);   // должно быть 16
    auto d = Cipher::decrypt(CipherId::Aes256Gcm, p, e->ciphertext);
    ASSERT_FALSE(d);
    EXPECT_EQ(d.error().code, rfp::core::ErrorCode::InvalidArgument);
}

// ===========================================================================
//  Аутентификация: подделка ciphertext / tag / AAD / ключа
// ===========================================================================
TEST(CipherTests, TamperedCiphertextFailsAead) {
    auto k = zeros(32);
    auto iv = zeros(12);
    auto pt = bytesOf("authentic");

    auto e = Cipher::encrypt(CipherId::Aes256Gcm,
                             makeParams(CipherId::Aes256Gcm, k, iv), pt);
    ASSERT_TRUE(e);

    auto ct = e->ciphertext; ct[0] ^= 0x01;

    auto d = Cipher::decrypt(CipherId::Aes256Gcm,
                             makeParams(CipherId::Aes256Gcm, k, iv, {}, e->tag), ct);
    ASSERT_FALSE(d);
    EXPECT_EQ(d.error().code, rfp::core::ErrorCode::DecodeError);
}

TEST(CipherTests, TamperedTagFailsAead) {
    auto k = zeros(32);
    auto iv = zeros(12);

    auto e = Cipher::encrypt(CipherId::Aes256Gcm,
                             makeParams(CipherId::Aes256Gcm, k, iv),
                             bytesOf("authentic"));
    ASSERT_TRUE(e);

    auto tag = e->tag; tag[0] ^= 0x01;

    auto d = Cipher::decrypt(CipherId::Aes256Gcm,
                             makeParams(CipherId::Aes256Gcm, k, iv, {}, tag),
                             e->ciphertext);
    ASSERT_FALSE(d);
    EXPECT_EQ(d.error().code, rfp::core::ErrorCode::DecodeError);
}

TEST(CipherTests, TamperedAadFailsAead) {
    auto k = zeros(32);
    auto iv = zeros(12);
    auto aad = bytesOf("original-aad");

    auto e = Cipher::encrypt(CipherId::Aes256Gcm,
                             makeParams(CipherId::Aes256Gcm, k, iv, aad),
                             bytesOf("authentic"));
    ASSERT_TRUE(e);

    auto badAad = bytesOf("tampered-aad");
    auto d = Cipher::decrypt(CipherId::Aes256Gcm,
                             makeParams(CipherId::Aes256Gcm, k, iv, badAad, e->tag),
                             e->ciphertext);
    ASSERT_FALSE(d);
    EXPECT_EQ(d.error().code, rfp::core::ErrorCode::DecodeError);
}

TEST(CipherTests, WrongKeyFailsAeadDecryption) {
    auto k1 = zeros(32);
    auto k2 = zeros(32); k2[0] = 0xFF;
    auto iv = zeros(12);

    auto e = Cipher::encrypt(CipherId::Aes256Gcm,
                             makeParams(CipherId::Aes256Gcm, k1, iv),
                             bytesOf("secret"));
    ASSERT_TRUE(e);

    auto d = Cipher::decrypt(CipherId::Aes256Gcm,
                             makeParams(CipherId::Aes256Gcm, k2, iv, {}, e->tag),
                             e->ciphertext);
    ASSERT_FALSE(d);
    EXPECT_EQ(d.error().code, rfp::core::ErrorCode::DecodeError);
}

// ===========================================================================
//  Не-AEAD: CBC / CTR — просто round-trip с длиной больше блока
// ===========================================================================
TEST(CipherTests, Aes256CbcLongerThanBlock) {
    auto k  = Random::bytes(32).value();
    auto iv = Random::bytes(16).value();
    auto pt = bytesOf(std::string(100, 'A'));

    auto e = Cipher::encrypt(CipherId::Aes256Cbc,
                             makeParams(CipherId::Aes256Cbc, k, iv), pt);
    ASSERT_TRUE(e);
    EXPECT_TRUE(e->tag.empty());

    auto d = Cipher::decrypt(CipherId::Aes256Cbc,
                             makeParams(CipherId::Aes256Cbc, k, iv),
                             e->ciphertext);
    ASSERT_TRUE(d);
    EXPECT_EQ(d.value(), pt);
}

TEST(CipherTests, Aes256CtrEmptyPlaintext) {
    auto k  = zeros(32);
    auto iv = zeros(16);

    auto e = Cipher::encrypt(CipherId::Aes256Ctr,
                             makeParams(CipherId::Aes256Ctr, k, iv), {});
    ASSERT_TRUE(e);
    EXPECT_TRUE(e->ciphertext.empty());

    auto d = Cipher::decrypt(CipherId::Aes256Ctr,
                             makeParams(CipherId::Aes256Ctr, k, iv),
                             e->ciphertext);
    ASSERT_TRUE(d);
    EXPECT_TRUE(d->empty());
}

// ===========================================================================
//  ChaCha20-Poly1305 без AAD — round-trip
// ===========================================================================
TEST(CipherTests, ChaCha20RoundTripNoAad) {
    auto k  = Random::bytes(32).value();
    auto iv = Random::bytes(12).value();
    auto pt = bytesOf("no aad here");

    auto e = Cipher::encrypt(CipherId::ChaCha20Poly1305,
                             makeParams(CipherId::ChaCha20Poly1305, k, iv), pt);
    ASSERT_TRUE(e);

    auto d = Cipher::decrypt(CipherId::ChaCha20Poly1305,
                             makeParams(CipherId::ChaCha20Poly1305, k, iv, {}, e->tag),
                             e->ciphertext);
    ASSERT_TRUE(d);
    EXPECT_EQ(d.value(), pt);
}