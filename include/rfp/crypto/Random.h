#pragma once

#include "rfp/core/Result.h"
#include "rfp/crypto/CryptoTypes.h"

#include <cstddef>
#include <span>

namespace rfp::crypto {

class Random {
public:
    [[nodiscard]] static rfp::core::Result<ByteBuffer> bytes(std::size_t count);
    [[nodiscard]] static rfp::core::Error fill(std::span<Byte> out) noexcept;
};

} // namespace rfp::crypto