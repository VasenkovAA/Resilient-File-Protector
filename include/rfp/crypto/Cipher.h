#pragma once

#include "rfp/core/Result.h"
#include "rfp/crypto/CryptoTypes.h"

#include <span>

namespace rfp::crypto {

struct EncryptionResult {
    ByteBuffer ciphertext;
    ByteBuffer tag;   // empty for non-AEAD
    ByteBuffer iv;    // generated if params.iv was empty
};

class Cipher {
public:
    /// Encrypt. If params.iv is empty, a fresh random IV is generated
    /// and returned. For AEAD ciphers the auth tag is returned separately.
    [[nodiscard]] static rfp::core::Result<EncryptionResult>
    encrypt(CipherId id,
            const CipherParams& params,
            std::span<const Byte> plaintext);

    /// Decrypt. For AEAD, params.tag must contain the authentication tag.
    /// On tag mismatch the function returns ErrorCode::DecodeError.
    [[nodiscard]] static rfp::core::Result<ByteBuffer>
    decrypt(CipherId id,
            const CipherParams& params,
            std::span<const Byte> ciphertext);
};

} // namespace rfp::crypto