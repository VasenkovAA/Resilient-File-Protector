#pragma once

#include "rfp/core/Result.h"
#include "rfp/stego/ImageBuffer.h"
#include "rfp/stego/StegoParams.h"

#include <cstddef>
#include <vector>

namespace rfp::stego::detail {

struct BitSlot {
  std::size_t byteIndex = 0;
  std::uint8_t bitIndex = 0;
};

[[nodiscard]] bool channelEnabled(std::uint8_t channelIndex,
                                  const StegoParams &params) noexcept;
[[nodiscard]] std::size_t
enabledChannelCount(const ImageBuffer &image,
                    const StegoParams &params) noexcept;
[[nodiscard]] std::size_t
availableSlotCount(const ImageBuffer &image,
                   const StegoParams &params) noexcept;
[[nodiscard]] rfp::core::Result<std::vector<BitSlot>>
buildSlots(const ImageBuffer &image, const StegoParams &params,
           std::size_t requiredBits);

} // namespace rfp::stego::detail
namespace rfp::stego::detail {

/**
 * @brief Check if a specific channel is enabled in parameters
 */
bool channelEnabled(std::uint8_t channelIndex,
                    const StegoParams &params) noexcept;

} // namespace rfp::stego::detail