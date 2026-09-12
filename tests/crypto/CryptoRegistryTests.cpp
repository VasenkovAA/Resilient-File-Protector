#include "TestHelpers.h"
#include "rfp/crypto/CryptoRegistry.h"

#include <gtest/gtest.h>

using namespace rfp::crypto;
using namespace rfp::crypto::test;

// ---------------------------------------------------------------------------
//  Listing
// ---------------------------------------------------------------------------
TEST(CryptoRegistryTests, AvailableCiphersNotEmpty) {
    const auto ciphers = CryptoRegistry::availableCiphers();
    ASSERT_GE(ciphers.size(), 3U);
    for (const auto& c : ciphers) {
        EXPECT_NE(c.name,        nullptr);
        EXPECT_NE(c.displayName, nullptr);
        EXPECT_GT(c.keySize, 0U);
        EXPECT_GT(c.ivSize,  0U);
        if (c.kind == CipherKind::Aead) EXPECT_GT(c.tagSize, 0U);
        else                            EXPECT_EQ(c.tagSize, 0U);
    }
}

TEST(CryptoRegistryTests, AvailableKdfsNotEmpty) {
    const auto kdfs = CryptoRegistry::availableKdfs();
    ASSERT_GE(kdfs.size(), 3U);
    for (const auto& k : kdfs) {
        EXPECT_NE(k.name,        nullptr);
        EXPECT_NE(k.displayName, nullptr);
    }
}

TEST(CryptoRegistryTests, AvailableHashesNotEmpty) {
    const auto hashes = CryptoRegistry::availableHashes();
    ASSERT_GE(hashes.size(), 3U);
    for (const auto& h : hashes) {
        EXPECT_NE(h.name, nullptr);
        EXPECT_GT(h.digestSize, 0U);
    }
}

// ---------------------------------------------------------------------------
//  Lookup by enum
// ---------------------------------------------------------------------------
TEST(CryptoRegistryTests, CipherSpecLookupValid) {
    for (const auto& c : CryptoRegistry::availableCiphers()) {
        auto s = CryptoRegistry::cipherSpec(c.id);
        ASSERT_TRUE(s.has_value());
        EXPECT_EQ(s->name, c.name);
    }
}

TEST(CryptoRegistryTests, CipherSpecLookupInvalid) {
    auto s = CryptoRegistry::cipherSpec(static_cast<CipherId>(999));
    EXPECT_FALSE(s.has_value());
}

TEST(CryptoRegistryTests, KdfSpecLookup) {
    for (const auto& k : CryptoRegistry::availableKdfs()) {
        auto s = CryptoRegistry::kdfSpec(k.id);
        ASSERT_TRUE(s.has_value());
        EXPECT_EQ(s->name, k.name);
    }
    EXPECT_FALSE(CryptoRegistry::kdfSpec(static_cast<KdfId>(999)).has_value());
}

TEST(CryptoRegistryTests, HashSpecLookup) {
    for (const auto& h : CryptoRegistry::availableHashes()) {
        auto s = CryptoRegistry::hashSpec(h.id);
        ASSERT_TRUE(s.has_value());
        EXPECT_EQ(s->name, h.name);
    }
    EXPECT_FALSE(CryptoRegistry::hashSpec(static_cast<HashId>(999)).has_value());
}

// ---------------------------------------------------------------------------
//  Lookup by name
// ---------------------------------------------------------------------------
TEST(CryptoRegistryTests, CipherByNameKnown) {
    auto id = CryptoRegistry::cipherByName("AES-256-GCM");
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(*id, CipherId::Aes256Gcm);

    EXPECT_EQ(*CryptoRegistry::cipherByName("ChaCha20-Poly1305"),
              CipherId::ChaCha20Poly1305);
    EXPECT_EQ(*CryptoRegistry::cipherByName("AES-128-GCM"), CipherId::Aes128Gcm);
    EXPECT_EQ(*CryptoRegistry::cipherByName("AES-256-CBC"), CipherId::Aes256Cbc);
    EXPECT_EQ(*CryptoRegistry::cipherByName("AES-256-CTR"), CipherId::Aes256Ctr);
}

TEST(CryptoRegistryTests, CipherByNameUnknown) {
    EXPECT_FALSE(CryptoRegistry::cipherByName("AES-999").has_value());
    EXPECT_FALSE(CryptoRegistry::cipherByName("").has_value());
}

TEST(CryptoRegistryTests, KdfByName) {
    EXPECT_EQ(*CryptoRegistry::kdfByName("PBKDF2-HMAC-SHA256"), KdfId::Pbkdf2HmacSha256);
    EXPECT_EQ(*CryptoRegistry::kdfByName("PBKDF2-HMAC-SHA512"), KdfId::Pbkdf2HmacSha512);
    EXPECT_EQ(*CryptoRegistry::kdfByName("scrypt"),             KdfId::Scrypt);
    EXPECT_EQ(*CryptoRegistry::kdfByName("argon2id"),           KdfId::Argon2id);
    EXPECT_FALSE(CryptoRegistry::kdfByName("bcrypt").has_value());
}

TEST(CryptoRegistryTests, HashByName) {
    EXPECT_EQ(*CryptoRegistry::hashByName("SHA-256"),  HashId::Sha256);
    EXPECT_EQ(*CryptoRegistry::hashByName("SHA-512"),  HashId::Sha512);
    EXPECT_EQ(*CryptoRegistry::hashByName("SHA3-256"), HashId::Sha3_256);
    EXPECT_EQ(*CryptoRegistry::hashByName("SHA3-512"), HashId::Sha3_512);
    EXPECT_EQ(*CryptoRegistry::hashByName("BLAKE2b"),  HashId::Blake2b);
    EXPECT_FALSE(CryptoRegistry::hashByName("MD5").has_value());
}

// ---------------------------------------------------------------------------
//  Backend identity
// ---------------------------------------------------------------------------
TEST(CryptoRegistryTests, BackendInfo) {
    EXPECT_EQ(CryptoRegistry::backendName(), "OpenSSL");
    EXPECT_FALSE(CryptoRegistry::backendVersion().empty());
    // OpenSSL version strings usually contain "OpenSSL"
    EXPECT_NE(CryptoRegistry::backendVersion().find("OpenSSL"), std::string::npos);
}