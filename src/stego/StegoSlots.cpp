#include "StegoSlots.h"

#include <algorithm>
#include <numeric>
#include <random>

namespace rfp::stego::detail {

bool channelEnabled(std::uint8_t channelIndex, const StegoParams& params) noexcept
{
    switch (channelIndex) {
    case 0:
        return params.useRedChannel;
    case 1:
        return params.useGreenChannel;
    case 2:
        return params.useBlueChannel;
    case 3:
        return params.useAlphaChannel;
    default:
        return false;
    }
}

std::size_t enabledChannelCount(const ImageBuffer& image, const StegoParams& params) noexcept
{
    std::size_t count = 0;
    for (std::uint8_t channel = 0; channel < image.channels; ++channel) {
        if (channelEnabled(channel, params)) {
            ++count;
        }
    }
    return count;
}

std::size_t availableSlotCount(const ImageBuffer& image, const StegoParams& params) noexcept
{
    if (!image.isValid() || params.bitsPerChannel == 0 || params.bitsPerChannel > 4) {
        return 0;
    }

    const auto pixelCount = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height);
    return pixelCount * enabledChannelCount(image, params) * static_cast<std::size_t>(params.bitsPerChannel);
}

rfp::core::Result<std::vector<BitSlot>> buildSlots(
    const ImageBuffer& image,
    const StegoParams& params,
    std::size_t requiredBits)
{
    if (!image.isValid()) {
        return rfp::core::Error{rfp::core::ErrorCode::InvalidImageBuffer, "Invalid image buffer"};
    }

    if (params.bitsPerChannel == 0 || params.bitsPerChannel > 4) {
        return rfp::core::Error{rfp::core::ErrorCode::InvalidArgument, "bitsPerChannel must be in range [1, 4]"};
    }

    if (enabledChannelCount(image, params) == 0) {
        return rfp::core::Error{rfp::core::ErrorCode::InvalidArgument, "At least one color channel must be enabled"};
    }

    const std::size_t totalSlots = availableSlotCount(image, params);
    if (requiredBits > totalSlots) {
        return rfp::core::Error{rfp::core::ErrorCode::CapacityExceeded, "Payload does not fit into the image"};
    }

    std::vector<BitSlot> slots;
    slots.reserve(totalSlots);

    const auto pixelCount = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height);
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        const std::size_t pixelOffset = pixel * static_cast<std::size_t>(image.channels);
        for (std::uint8_t channel = 0; channel < image.channels; ++channel) {
            if (!channelEnabled(channel, params)) {
                continue;
            }
            for (std::uint8_t bit = 0; bit < params.bitsPerChannel; ++bit) {
                slots.push_back(BitSlot{pixelOffset + static_cast<std::size_t>(channel), bit});
            }
        }
    }

    if (params.seed != 0U) {
        std::mt19937 generator(params.seed);
        std::shuffle(slots.begin(), slots.end(), generator);
    }

    slots.resize(requiredBits);
    return slots;
}

} // namespace rfp::stego::detail
