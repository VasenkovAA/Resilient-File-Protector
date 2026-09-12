#pragma once

#include "rfp/core/Result.h"
#include "rfp/crypto/CryptoTypes.h"

#include <cstddef>
#include <string_view>

namespace rfp::crypto {

class Kdf {
public:
    /// Derive a key of keyLength bytes from a password.
    [[nodiscard]] static rfp::core::Result<ByteBuffer>
    deriveKey(KdfId id,
              std::string_view password,
              const KdfParams& params,
              std::size_t keyLength);

    /// Convenience wrapper around Random::bytes.
    [[nodiscard]] static ByteBuffer generateSalt(std::size_t length = 16);
};

} // namespace rfp::crypto