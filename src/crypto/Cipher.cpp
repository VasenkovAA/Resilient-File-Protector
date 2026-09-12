#include "rfp/crypto/Cipher.h"
#include "rfp/crypto/CryptoRegistry.h"
#include "rfp/crypto/Random.h"
#include "rfp/crypto/SecureMemory.h"

#include <openssl/evp.h>

#include <algorithm>

namespace rfp::crypto {

namespace {

const EVP_CIPHER* evpCipher(CipherId id) noexcept
{
    switch (id) {
    case CipherId::Aes128Gcm:        return EVP_aes_128_gcm();
    case CipherId::Aes256Gcm:        return EVP_aes_256_gcm();
    case CipherId::ChaCha20Poly1305: return EVP_chacha20_poly1305();
    case CipherId::Aes256Cbc:        return EVP_aes_256_cbc();
    case CipherId::Aes256Ctr:        return EVP_aes_256_ctr();
    }
    return nullptr;
}

inline rfp::core::Error err(rfp::core::ErrorCode c, const char* m)
{ return { c, m }; }

} // namespace

rfp::core::Result<EncryptionResult> Cipher::encrypt(
    CipherId id, const CipherParams& params, std::span<const Byte> plaintext)
{
    const auto specOpt = CryptoRegistry::cipherSpec(id);
    if (!specOpt) return err(rfp::core::ErrorCode::InvalidArgument, "Unknown cipher");
    const auto& spec = *specOpt;

    if (params.key.size() != spec.keySize)
        return err(rfp::core::ErrorCode::InvalidArgument, "Invalid key size");

    ByteBuffer iv = params.iv;
    if (iv.empty()) {
        auto r = Random::bytes(spec.ivSize);
        if (!r) return r.error();
        iv = std::move(r.value());
    } else if (iv.size() != spec.ivSize) {
        return err(rfp::core::ErrorCode::InvalidArgument, "Invalid IV size");
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return err(rfp::core::ErrorCode::NotImplemented, "EVP_CIPHER_CTX_new failed");

    EncryptionResult out;
    out.iv = iv;
    out.ciphertext.resize(plaintext.size() + static_cast<std::size_t>(EVP_CIPHER_block_size(evpCipher(id))));

    int outLen = 0, totalLen = 0;
    bool ok = EVP_EncryptInit_ex(ctx, evpCipher(id), nullptr, nullptr, nullptr) == 1;

    if (ok && spec.kind == CipherKind::Aead)
        ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN,
                                 static_cast<int>(iv.size()), nullptr) == 1;

    if (ok)
        ok = EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                                params.key.data(), iv.data()) == 1;

    if (ok && spec.supportsAad && !params.aad.empty())
        ok = EVP_EncryptUpdate(ctx, nullptr, &outLen,
                               params.aad.data(),
                               static_cast<int>(params.aad.size())) == 1;

    if (ok && !plaintext.empty()) {
        ok = EVP_EncryptUpdate(ctx, out.ciphertext.data(), &outLen,
                               plaintext.data(),
                               static_cast<int>(plaintext.size())) == 1;
        totalLen = outLen;
    }

    if (ok) {
        ok = EVP_EncryptFinal_ex(ctx, out.ciphertext.data() + totalLen, &outLen) == 1;
        totalLen += outLen;
    }

    if (ok && spec.kind == CipherKind::Aead) {
        out.tag.resize(spec.tagSize);
        ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG,
                                 static_cast<int>(spec.tagSize),
                                 out.tag.data()) == 1;
    }

    EVP_CIPHER_CTX_free(ctx);

    if (!ok) {
        secureWipe(out.ciphertext);
        secureWipe(out.tag);
        return err(rfp::core::ErrorCode::DecodeError, "Encryption failed");
    }

    out.ciphertext.resize(static_cast<std::size_t>(totalLen));
    return out;
}

rfp::core::Result<ByteBuffer> Cipher::decrypt(
    CipherId id, const CipherParams& params, std::span<const Byte> ciphertext)
{
    const auto specOpt = CryptoRegistry::cipherSpec(id);
    if (!specOpt) return err(rfp::core::ErrorCode::InvalidArgument, "Unknown cipher");
    const auto& spec = *specOpt;

    if (params.key.size() != spec.keySize)
        return err(rfp::core::ErrorCode::InvalidArgument, "Invalid key size");
    if (params.iv.size() != spec.ivSize)
        return err(rfp::core::ErrorCode::InvalidArgument, "Invalid IV size");
    if (spec.kind == CipherKind::Aead && params.tag.size() != spec.tagSize)
        return err(rfp::core::ErrorCode::InvalidArgument, "Invalid tag size");

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return err(rfp::core::ErrorCode::NotImplemented, "EVP_CIPHER_CTX_new failed");

    ByteBuffer out(ciphertext.size() + static_cast<std::size_t>(EVP_CIPHER_block_size(evpCipher(id))));
    int outLen = 0, totalLen = 0;

    bool ok = EVP_DecryptInit_ex(ctx, evpCipher(id), nullptr, nullptr, nullptr) == 1;

    if (ok && spec.kind == CipherKind::Aead)
        ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN,
                                 static_cast<int>(params.iv.size()), nullptr) == 1;

    if (ok)
        ok = EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                                params.key.data(), params.iv.data()) == 1;

    if (ok && spec.supportsAad && !params.aad.empty())
        ok = EVP_DecryptUpdate(ctx, nullptr, &outLen,
                               params.aad.data(),
                               static_cast<int>(params.aad.size())) == 1;

    if (ok && !ciphertext.empty()) {
        ok = EVP_DecryptUpdate(ctx, out.data(), &outLen,
                               ciphertext.data(),
                               static_cast<int>(ciphertext.size())) == 1;
        totalLen = outLen;
    }

    if (ok && spec.kind == CipherKind::Aead) {
        ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG,
                                 static_cast<int>(params.tag.size()),
                                 const_cast<Byte*>(params.tag.data())) == 1;
    }

    if (ok) {
        ok = EVP_DecryptFinal_ex(ctx, out.data() + totalLen, &outLen) == 1;
        totalLen += outLen;
    }

    EVP_CIPHER_CTX_free(ctx);

    if (!ok) {
        secureWipe(out);
        return err(rfp::core::ErrorCode::DecodeError,
                   "Decryption failed (tag mismatch or corrupted data)");
    }
    out.resize(static_cast<std::size_t>(totalLen));
    return out;
}

} // namespace rfp::crypto