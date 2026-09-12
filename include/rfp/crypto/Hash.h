#pragma once

#include "rfp/core/Result.h"
#include "rfp/crypto/CryptoTypes.h"

#include <span>

namespace rfp::crypto {

class Hash {
public:
    [[nodiscard]] static rfp::core::Result<ByteBuffer>
    digest(HashId id, std::span<const Byte> data);

    [[nodiscard]] static rfp::core::Result<ByteBuffer>
    hmac(HashId id,
         std::span<const Byte> key,
         std::span<const Byte> data);
};

} // namespace rfp::crypto