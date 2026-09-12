#include "rfp/crypto/CryptoRegistry.h"

#include <openssl/opensslv.h>

#include <array>
#include <string>

namespace rfp::crypto {

namespace {

constexpr std::array<CipherSpec, 5> kCiphers = {{
  { CipherId::Aes128Gcm,       "AES-128-GCM",       "AES-128-GCM (AEAD)",       CipherKind::Aead,      16, 12, 16, true  },
  { CipherId::Aes256Gcm,       "AES-256-GCM",       "AES-256-GCM (AEAD)",       CipherKind::Aead,      32, 12, 16, true  },
  { CipherId::ChaCha20Poly1305,"ChaCha20-Poly1305", "ChaCha20-Poly1305 (AEAD)", CipherKind::Aead,      32, 12, 16, true  },
  { CipherId::Aes256Cbc,       "AES-256-CBC",       "AES-256-CBC (PKCS7)",      CipherKind::BlockCbc,  32, 16,  0, false },
  { CipherId::Aes256Ctr,       "AES-256-CTR",       "AES-256-CTR",              CipherKind::StreamCtr, 32, 16,  0, false },
}};

constexpr std::array<KdfSpec, 4> kKdfs = {{
  { KdfId::Pbkdf2HmacSha256, "PBKDF2-HMAC-SHA256", "PBKDF2-HMAC-SHA256", false, false },
  { KdfId::Pbkdf2HmacSha512, "PBKDF2-HMAC-SHA512", "PBKDF2-HMAC-SHA512", false, false },
  { KdfId::Scrypt,           "scrypt",             "scrypt",             true,  true  },
  { KdfId::Argon2id,         "argon2id",           "Argon2id (OpenSSL 3.2+)", true, true },
}};

constexpr std::array<HashSpec, 5> kHashes = {{
  { HashId::Sha256,   "SHA-256",  "SHA-256",    32 },
  { HashId::Sha512,   "SHA-512",  "SHA-512",    64 },
  { HashId::Sha3_256, "SHA3-256", "SHA3-256",   32 },
  { HashId::Sha3_512, "SHA3-512", "SHA3-512",   64 },
  { HashId::Blake2b,  "BLAKE2b",  "BLAKE2b-512",64 },
}};

} // namespace

std::span<const CipherSpec> CryptoRegistry::availableCiphers() noexcept
{ return { kCiphers.data(), kCiphers.size() }; }

std::span<const KdfSpec> CryptoRegistry::availableKdfs() noexcept
{ return { kKdfs.data(), kKdfs.size() }; }

std::span<const HashSpec> CryptoRegistry::availableHashes() noexcept
{ return { kHashes.data(), kHashes.size() }; }

std::optional<CipherSpec> CryptoRegistry::cipherSpec(CipherId id) noexcept
{
    for (const auto& s : kCiphers) if (s.id == id) return s;
    return std::nullopt;
}
std::optional<KdfSpec> CryptoRegistry::kdfSpec(KdfId id) noexcept
{
    for (const auto& s : kKdfs) if (s.id == id) return s;
    return std::nullopt;
}
std::optional<HashSpec> CryptoRegistry::hashSpec(HashId id) noexcept
{
    for (const auto& s : kHashes) if (s.id == id) return s;
    return std::nullopt;
}

std::optional<CipherId> CryptoRegistry::cipherByName(std::string_view n) noexcept
{
    for (const auto& s : kCiphers) if (n == s.name) return s.id;
    return std::nullopt;
}
std::optional<KdfId> CryptoRegistry::kdfByName(std::string_view n) noexcept
{
    for (const auto& s : kKdfs) if (n == s.name) return s.id;
    return std::nullopt;
}
std::optional<HashId> CryptoRegistry::hashByName(std::string_view n) noexcept
{
    for (const auto& s : kHashes) if (n == s.name) return s.id;
    return std::nullopt;
}

std::string CryptoRegistry::backendName()    noexcept { return "OpenSSL"; }
std::string CryptoRegistry::backendVersion() noexcept { return OPENSSL_VERSION_TEXT; }

} // namespace rfp::crypto