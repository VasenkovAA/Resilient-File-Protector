#pragma once

#include "rfp/core/ByteBuffer.h"
#include "rfp/core/Error.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace rfp::crypto {

using rfp::core::Byte;
using rfp::core::ByteBuffer;

// ---------------------------------------------------------------------------
//  Ciphers
// ---------------------------------------------------------------------------
enum class CipherId {
    Aes128Gcm,
    Aes256Gcm,
    ChaCha20Poly1305,
    Aes256Cbc,
    Aes256Ctr,
};

enum class CipherKind {
    Aead,       // AEAD: tag authenticates ciphertext + AAD
    BlockCbc,   // CBC + PKCS7 (no built-in auth)
    StreamCtr,  // CTR (no built-in auth)
};

struct CipherSpec {
    CipherId    id;
    const char* name;         // canonical, stable identifier: "AES-256-GCM"
    const char* displayName;  // human-readable: "AES-256-GCM (AEAD)"
    CipherKind  kind;
    std::size_t keySize;      // bytes
    std::size_t ivSize;       // recommended nonce size, bytes
    std::size_t tagSize;      // bytes; 0 for non-AEAD
    bool        supportsAad;
};

// ---------------------------------------------------------------------------
//  KDF
// ---------------------------------------------------------------------------
enum class KdfId {
    Pbkdf2HmacSha256,
    Pbkdf2HmacSha512,
    Scrypt,
    Argon2id,       // OpenSSL >= 3.2
};

struct KdfSpec {
    KdfId       id;
    const char* name;
    const char* displayName;
    bool        supportsMemoryCost;
    bool        supportsParallelism;
};

struct KdfParams {
    ByteBuffer    salt;         // required
    std::uint32_t iterations = 100000;
    std::uint32_t memoryKb   = 0;   // scrypt/argon2
    std::uint32_t parallelism = 1;  // argon2
};

// ---------------------------------------------------------------------------
//  Hash
// ---------------------------------------------------------------------------
enum class HashId { Sha256, Sha512, Sha3_256, Sha3_512, Blake2b };

struct HashSpec {
    HashId      id;
    const char* name;
    const char* displayName;
    std::size_t digestSize;
};

// ---------------------------------------------------------------------------
//  Cipher invocation parameters
// ---------------------------------------------------------------------------
struct CipherParams {
    ByteBuffer key;   // exactly spec.keySize bytes
    ByteBuffer iv;    // exactly spec.ivSize bytes; empty -> auto-generate
    ByteBuffer aad;   // optional (AEAD only)
    ByteBuffer tag;   // required for decryption of AEAD
};

} // namespace rfp::crypto