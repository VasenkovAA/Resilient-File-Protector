#pragma once

#include "rfp/stego/ImageBuffer.h"
#include "rfp/stego/StegoParams.h"

#include <cstddef>

namespace rfp::stego {

[[nodiscard]] std::size_t capacityBits(const ImageBuffer& image, const StegoParams& params) noexcept;
[[nodiscard]] std::size_t capacityBytes(const ImageBuffer& image, const StegoParams& params) noexcept;

} // namespace rfp::stego
