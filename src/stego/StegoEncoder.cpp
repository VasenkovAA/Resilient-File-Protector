#include "rfp/stego/StegoEncoder.h"

#include "StegoSlots.h"
#include "rfp/core/ByteBuffer.h"

namespace rfp::stego {

rfp::core::Result<ImageBuffer> StegoEncoder::embedBytes(
    const ImageBuffer& source,
    const rfp::core::ByteBuffer& payload,
    const StegoParams& params)
{
    const std::size_t requiredBits = payload.size() * 8U;
    auto slotsResult = detail::buildSlots(source, params, requiredBits);
    if (!slotsResult) {
        return slotsResult.error();
    }

    ImageBuffer result = source;
    const auto& slots = slotsResult.value();

    for (std::size_t bitIndex = 0; bitIndex < requiredBits; ++bitIndex) {
        const std::uint8_t payloadByte = payload[bitIndex / 8U];
        const std::uint8_t payloadBit = static_cast<std::uint8_t>((payloadByte >> (7U - (bitIndex % 8U))) & 1U);
        const auto slot = slots[bitIndex];
        const std::uint8_t mask = static_cast<std::uint8_t>(1U << slot.bitIndex);

        result.pixels[slot.byteIndex] = static_cast<rfp::core::Byte>(
            (result.pixels[slot.byteIndex] & static_cast<std::uint8_t>(~mask)) |
            static_cast<std::uint8_t>(payloadBit << slot.bitIndex));
    }

    return result;
}

rfp::core::Result<ImageBuffer> StegoEncoder::embedText(
    const ImageBuffer& source,
    std::string_view text,
    const StegoParams& params)
{
    return embedBytes(source, rfp::core::toBytes(text), params);
}

} // namespace rfp::stego
