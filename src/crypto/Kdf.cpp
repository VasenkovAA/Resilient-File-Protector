#include "rfp/crypto/Kdf.h"
#include "rfp/crypto/Random.h"
#include "rfp/crypto/CryptoRegistry.h"

#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/core_names.h>

#include <cstdint>

namespace rfp::crypto {

namespace {

const EVP_MD* prfMd(KdfId id) noexcept
{
    switch (id) {
    case KdfId::Pbkdf2HmacSha256: return EVP_sha256();
    case KdfId::Pbkdf2HmacSha512: return EVP_sha512();
    default: return nullptr;
    }
}

} // namespace

rfp::core::Result<ByteBuffer> Kdf::deriveKey(
    KdfId id, std::string_view password, const KdfParams& params, std::size_t keyLength)
{
    if (keyLength == 0)
        return rfp::core::Error{ rfp::core::ErrorCode::InvalidArgument, "keyLength must be > 0" };
    if (params.salt.empty())
        return rfp::core::Error{ rfp::core::ErrorCode::InvalidArgument, "Salt must not be empty" };

    ByteBuffer out(keyLength);

    switch (id) {
    case KdfId::Pbkdf2HmacSha256:
    case KdfId::Pbkdf2HmacSha512: {
        if (PKCS5_PBKDF2_HMAC(
                password.data(), static_cast<int>(password.size()),
                params.salt.data(), static_cast<int>(params.salt.size()),
                static_cast<int>(params.iterations), prfMd(id),
                static_cast<int>(keyLength), out.data()) != 1)
            return rfp::core::Error{ rfp::core::ErrorCode::DecodeError, "PBKDF2 failed" };
        return out;
    }
    case KdfId::Scrypt: {
        EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "SCRYPT", nullptr);
        if (!kdf) return rfp::core::Error{ rfp::core::ErrorCode::NotImplemented, "scrypt unavailable" };
        EVP_KDF_CTX* ctx = EVP_KDF_CTX_new(kdf);
        EVP_KDF_free(kdf);
        if (!ctx) return rfp::core::Error{ rfp::core::ErrorCode::NotImplemented, "scrypt ctx failed" };

        std::uint64_t n = 1ULL << 14;
        std::uint32_t r = 8;
        std::uint32_t p = params.parallelism ? params.parallelism : 1;

        OSSL_PARAM osp[6];
        osp[0] = OSSL_PARAM_construct_octet_string(
                    OSSL_KDF_PARAM_PASSWORD,
                    const_cast<char*>(password.data()), password.size());
        osp[1] = OSSL_PARAM_construct_octet_string(
                    OSSL_KDF_PARAM_SALT,
                    const_cast<Byte*>(params.salt.data()), params.salt.size());
        osp[2] = OSSL_PARAM_construct_uint64(OSSL_KDF_PARAM_SCRYPT_N, &n);
        osp[3] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_SCRYPT_R, &r);
        osp[4] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_SCRYPT_P, &p);
        osp[5] = OSSL_PARAM_construct_end();

        int rc = EVP_KDF_derive(ctx, out.data(), keyLength, osp);
        EVP_KDF_CTX_free(ctx);
        if (rc != 1) return rfp::core::Error{ rfp::core::ErrorCode::DecodeError, "scrypt failed" };
        return out;
    }
    case KdfId::Argon2id:
        return rfp::core::Error{
            rfp::core::ErrorCode::NotImplemented,
            "Argon2id requires OpenSSL >= 3.2 and the corresponding EVP_KDF parameters"};
    }
    return rfp::core::Error{ rfp::core::ErrorCode::InvalidArgument, "Unknown KDF" };
}

ByteBuffer Kdf::generateSalt(std::size_t length)
{
    auto r = Random::bytes(length);
    return r ? std::move(r.value()) : ByteBuffer{};
}

} // namespace rfp::crypto