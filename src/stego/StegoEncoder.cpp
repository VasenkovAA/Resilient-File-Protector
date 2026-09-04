#include "rfp/stego/StegoEncoder.h"
#include "rfp/core/ByteBuffer.h"
#include "rfp/stego/StegoSlots.h"

#include <cstdint>

namespace rfp::stego {

namespace {
/**
 * @brief Embed a single bit into the image at specified slot
 */
static inline void embedBit(ImageBuffer &image, const detail::BitSlot &slot,
                            std::uint8_t bit) noexcept {
  const std::uint8_t mask = static_cast<std::uint8_t>(1U << slot.bitIndex);

  // Clear bit then set to desired value
  image.pixels[slot.byteIndex] = static_cast<rfp::core::Byte>(
      (image.pixels[slot.byteIndex] & static_cast<std::uint8_t>(~mask)) |
      static_cast<std::uint8_t>((bit & 1U) << slot.bitIndex));
}
} // anonymous namespace

rfp::core::Result<ImageBuffer>
StegoEncoder::embedBytes(const ImageBuffer &source,
                         const rfp::core::ByteBuffer &payload,
                         const StegoParams &params) {
  // Validate payload
  if (payload.empty()) {
    return rfp::core::Error{rfp::core::ErrorCode::InvalidArgument,
                            "Empty payload"};
  }

  const std::size_t requiredBits = payload.size() * 8U;

  // Build slots for embedding
  auto slotsResult = detail::buildSlots(source, params, requiredBits);
  if (!slotsResult) {
    return slotsResult.error();
  }

  ImageBuffer result = source;
  const auto &slots = slotsResult.value();

  // Embed bits MSB-first
  for (std::size_t bitIndex = 0; bitIndex < requiredBits; ++bitIndex) {
    const std::uint8_t payloadByte = payload[bitIndex / 8U];
    const std::uint8_t payloadBit =
        static_cast<std::uint8_t>((payloadByte >> (7U - (bitIndex % 8U))) & 1U);

    embedBit(result, slots[bitIndex],
             payloadBit); // slots[bitIndex] - тип detail::BitSlot
  }

  return result;
}

rfp::core::Result<ImageBuffer>
StegoEncoder::embedText(const ImageBuffer &source, std::string_view text,
                        const StegoParams &params) {
  if (text.empty()) {
    return rfp::core::Error{rfp::core::ErrorCode::InvalidArgument,
                            "Empty text"};
  }

  return embedBytes(source, rfp::core::toBytes(text), params);
}

} // namespace rfp::stego