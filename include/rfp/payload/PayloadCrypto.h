#pragma once

#include "rfp/core/ByteBuffer.h"
#include "rfp/core/Result.h"
#include "rfp/crypto/CryptoTypes.h"

#include <cstdint>
#include <span>
#include <string>

namespace rfp::payload {

/// Parameters for encrypting a payload.
struct EncryptParams {
    std::string   password;                                // required
    rfp::crypto::CipherId cipher = rfp::crypto::CipherId::Aes256Gcm;
    rfp::crypto::KdfId    kdf    = rfp::crypto::KdfId::Pbkdf2HmacSha256;
    std::uint32_t iterations = 100000;
};

/// Parameters for decrypting a payload (all crypto params come from the
/// self-describing header, only the password is needed).
struct DecryptParams {
    std::string password;                                  // required
};

/// Wire format (version 1):
///
///   offset  size  field
///   ------  ----  -----------------------------------------
///        0     4  magic 'R','F','P','1'
///        4     1  version (=1)
///        5     1  cipherId (rfp::crypto::CipherId)
///        6     1  kdfId    (rfp::crypto::KdfId)
///        7     1  flags    (reserved, =0)
///        8     4  iterations (big-endian uint32)
///       12     1  saltLen
///       13     1  ivLen
///       14     2  reserved (=0)
///       16  saltLen  salt
///   16+N  ivLen   iv
///   ...    ...    ciphertext
///   ...  tagLen   auth tag (0 for non-AEAD)
[[nodiscard]] rfp::core::Result<rfp::core::ByteBuffer>
encrypt(std::span<const rfp::core::Byte> plaintext,
        const EncryptParams& params);

[[nodiscard]] rfp::core::Result<rfp::core::ByteBuffer>
decrypt(std::span<const rfp::core::Byte> encrypted,
        const DecryptParams& params);

/// Cheap check: does this buffer look like an RFP1 payload?
[[nodiscard]] bool isPayload(std::span<const rfp::core::Byte> data) noexcept;

} // namespace rfp::payload
