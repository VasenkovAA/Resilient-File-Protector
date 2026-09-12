#include "rfp/crypto/CryptoPlaceholder.h"
#include "rfp/crypto/CryptoRegistry.h"
#include <gtest/gtest.h>

TEST(CryptoPlaceholderTests, ReportsOpenSSLVersion) {
    auto e = rfp::crypto::moduleStatus();
    EXPECT_TRUE(e.ok());
    EXPECT_FALSE(e.message.empty());
    EXPECT_NE(e.message.find("OpenSSL"), std::string::npos);
}

TEST(CryptoPlaceholderTests, MatchesRegistryBackend) {
    auto e = rfp::crypto::moduleStatus();
    EXPECT_EQ(e.message, rfp::crypto::CryptoRegistry::backendVersion());
}