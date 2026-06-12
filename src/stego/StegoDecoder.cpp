#include "rfp/stego/StegoDecoder.h"

#include "StegoSlots.h"

namespace rfp::stego {

rfp::core::Result<rfp::core::ByteBuffer> StegoDecoder::extractBytes(
    const ImageBuffer& source,
    std::size_t payloadSizeBytes,
    const StegoParams& params)
{
    const std::size_t requiredBits = payloadSizeBytes * 8U;
    auto slotsResult = detail::buildSlots(source, params, requiredBits);
    if (!slotsResult) {
        return slotsResult.error();
    }

    rfp::core::ByteBuffer payload(payloadSizeBytes, 0);
    const auto& slots = slotsResult.value();

    for (std::size_t bitIndex = 0; bitIndex < requiredBits; ++bitIndex) {
        const auto slot = slots[bitIndex];
        const std::uint8_t bit = static_cast<std::uint8_t>((source.pixels[slot.byteIndex] >> slot.bitIndex) & 1U);
        payload[bitIndex / 8U] = static_cast<rfp::core::Byte>(
            payload[bitIndex / 8U] | static_cast<std::uint8_t>(bit << (7U - (bitIndex % 8U))));
    }

    return payload;
}

} // namespace rfp::stego
