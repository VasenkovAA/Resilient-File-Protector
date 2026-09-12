#include "rfp/crypto/Hash.h"
#include "rfp/crypto/CryptoRegistry.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>

namespace rfp::crypto {

namespace {
const EVP_MD* evpMd(HashId id) noexcept {
    switch (id) {
    case HashId::Sha256:   return EVP_sha256();
    case HashId::Sha512:   return EVP_sha512();
    case HashId::Sha3_256: return EVP_sha3_256();
    case HashId::Sha3_512: return EVP_sha3_512();
    case HashId::Blake2b:  return EVP_blake2b512();
    }
    return nullptr;
}
} // namespace

rfp::core::Result<ByteBuffer> Hash::digest(HashId id, std::span<const Byte> data)
{
    const auto spec = CryptoRegistry::hashSpec(id);
    if (!spec) return rfp::core::Error{ rfp::core::ErrorCode::InvalidArgument, "Unknown hash" };

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return rfp::core::Error{ rfp::core::ErrorCode::NotImplemented, "EVP_MD_CTX_new failed" };

    ByteBuffer out(spec->digestSize);
    unsigned int len = 0;
    const bool ok = EVP_DigestInit_ex(ctx, evpMd(id), nullptr) == 1
                 && EVP_DigestUpdate(ctx, data.data(), data.size()) == 1
                 && EVP_DigestFinal_ex(ctx, out.data(), &len) == 1;

    EVP_MD_CTX_free(ctx);
    if (!ok) return rfp::core::Error{ rfp::core::ErrorCode::DecodeError, "Hashing failed" };
    out.resize(len);
    return out;
}

rfp::core::Result<ByteBuffer> Hash::hmac(
    HashId id, std::span<const Byte> key, std::span<const Byte> data)
{
    const auto spec = CryptoRegistry::hashSpec(id);
    if (!spec) return rfp::core::Error{ rfp::core::ErrorCode::InvalidArgument, "Unknown hash" };

    // OpenSSL's one-shot HMAC() rejects nullptr for key/data even with len 0.
    // Feed it a valid dummy pointer whenever the span is empty.
    static const Byte kEmptySentinel = 0;
    const Byte* keyPtr  = key.empty()  ? &kEmptySentinel : key.data();
    const Byte* dataPtr = data.empty() ? &kEmptySentinel : data.data();

    ByteBuffer out(spec->digestSize);
    unsigned int len = 0;
        unsigned char* res = HMAC(evpMd(id),
                              keyPtr, static_cast<int>(key.size()),
                              dataPtr, data.size(),
                              out.data(), &len);
    // GCOVR_EXCL_START — HMAC() падает только при нехватке памяти
    if (!res) return rfp::core::Error{ rfp::core::ErrorCode::DecodeError, "HMAC failed" };
    // GCOVR_EXCL_STOP
    out.resize(len);
    return out;
}

} // namespace rfp::crypto