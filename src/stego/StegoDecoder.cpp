#include "rfp/stego/StegoDecoder.h"
#include "rfp/stego/StegoSlots.h"

namespace rfp::stego {

namespace {
/**
 * @brief Extract a single bit from image at specified slot
 */
static inline std::uint8_t extractBit(const ImageBuffer &image,
                                      const detail::BitSlot &slot) noexcept {
  return static_cast<std::uint8_t>(
      (image.pixels[slot.byteIndex] >> slot.bitIndex) & 1U);
}
} // anonymous namespace

rfp::core::Result<rfp::core::ByteBuffer>
StegoDecoder::extractBytes(const ImageBuffer &source,
                           std::size_t payloadSizeBytes,
                           const StegoParams &params) {
  if (payloadSizeBytes == 0) {
    return rfp::core::ByteBuffer{};
  }

  const std::size_t requiredBits = payloadSizeBytes * 8U;

  // Build slots for extraction (must match encoder's slot selection)
  auto slotsResult = detail::buildSlots(source, params, requiredBits);
  if (!slotsResult) {
    return slotsResult.error();
  }

  rfp::core::ByteBuffer payload(payloadSizeBytes, 0);
  const auto &slots = slotsResult.value();

  // Extract bits and assemble bytes (MSB-first)
  for (std::size_t bitIndex = 0; bitIndex < requiredBits; ++bitIndex) {
    const std::uint8_t bit = extractBit(source, slots[bitIndex]);
    const std::size_t byteIndex = bitIndex / 8U;
    const std::uint8_t bitPosition =
        static_cast<std::uint8_t>(7U - (bitIndex % 8U));

    payload[byteIndex] = static_cast<rfp::core::Byte>(
        payload[byteIndex] | static_cast<std::uint8_t>(bit << bitPosition));
  }

  return payload;
}

} // namespace rfp::stego