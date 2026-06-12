#pragma once

#include "rfp/core/ByteBuffer.h"
#include "rfp/core/Result.h"

#include <cstddef>
#include <cstdint>

namespace rfp::stego {

struct ImageBuffer {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint8_t channels = 4;
    rfp::core::ByteBuffer pixels;

    [[nodiscard]] std::size_t byteSize() const noexcept;
    [[nodiscard]] bool isValid() const noexcept;
};

} // namespace rfp::stego
