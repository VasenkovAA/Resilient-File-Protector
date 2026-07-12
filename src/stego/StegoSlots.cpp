#include "rfp/stego/StegoSlots.h"
#include "rfp/stego/StegoDispersion.h"

#include <algorithm>
#include <numeric>
#include <random>
#include <vector>

namespace rfp::stego::detail {

bool channelEnabled(std::uint8_t channelIndex,
                    const StegoParams &params) noexcept {
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

std::size_t enabledChannelCount(const ImageBuffer &image,
                                const StegoParams &params) noexcept {
  std::size_t count = 0;
  for (std::uint8_t channel = 0; channel < image.channels; ++channel) {
    if (channelEnabled(channel, params)) {
      ++count;
    }
  }
  return count;
}

static std::vector<BitSlot> generateAllSlots(const ImageBuffer &image,
                                             const StegoParams &params) {
  const auto pixelCount = static_cast<std::size_t>(image.width) *
                          static_cast<std::size_t>(image.height);
  std::vector<BitSlot> slots;
  slots.reserve(pixelCount * enabledChannelCount(image, params) *
                params.bitsPerChannel);

  for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
    const std::size_t pixelOffset =
        pixel * static_cast<std::size_t>(image.channels);
    for (std::uint8_t channel = 0; channel < image.channels; ++channel) {
      if (!channelEnabled(channel, params)) {
        continue;
      }
      for (std::uint8_t bit = 0; bit < params.bitsPerChannel; ++bit) {
        slots.push_back(
            BitSlot{pixelOffset + static_cast<std::size_t>(channel), bit});
      }
    }
  }
  return slots;
}

std::size_t availableSlotCount(const ImageBuffer &image,
                               const StegoParams &params) noexcept {
  if (!image.isValid() || params.bitsPerChannel == 0 ||
      params.bitsPerChannel > 4) {
    return 0;
  }

  if (params.mode == SlotSelectionMode::Uniform) {
    const auto pixelCount = static_cast<std::size_t>(image.width) *
                            static_cast<std::size_t>(image.height);
    return pixelCount * enabledChannelCount(image, params) *
           static_cast<std::size_t>(params.bitsPerChannel);
  } else { // Smart
    DispersionCalculator calc(image, params);
    const auto pixelCount = static_cast<std::size_t>(image.width) *
                            static_cast<std::size_t>(image.height);
    std::size_t count = 0;
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
      for (std::uint8_t channel = 0; channel < image.channels; ++channel) {
        if (!channelEnabled(channel, params))
          continue;
        const double disp = calc.getDispersion(pixel, channel);
        if (disp >= params.dispersionThreshold) {
          count += params.bitsPerChannel;
        }
      }
    }
    return count;
  }
}

rfp::core::Result<std::vector<BitSlot>> buildSlots(const ImageBuffer &image,
                                                   const StegoParams &params,
                                                   std::size_t requiredBits) {
  if (!image.isValid()) {
    return rfp::core::Error{rfp::core::ErrorCode::InvalidImageBuffer,
                            "Invalid image buffer"};
  }

  if (params.bitsPerChannel == 0 || params.bitsPerChannel > 4) {
    return rfp::core::Error{rfp::core::ErrorCode::InvalidArgument,
                            "bitsPerChannel must be in range [1, 4]"};
  }

  if (enabledChannelCount(image, params) == 0) {
    return rfp::core::Error{rfp::core::ErrorCode::InvalidArgument,
                            "At least one color channel must be enabled"};
  }

  const std::size_t totalSlots = availableSlotCount(image, params);
  if (requiredBits > totalSlots) {
    return rfp::core::Error{rfp::core::ErrorCode::CapacityExceeded,
                            "Payload does not fit into the image"};
  }

  if (params.mode == SlotSelectionMode::Uniform) {
    auto slots = generateAllSlots(image, params);

    if (params.seed != 0U) {
      std::mt19937 generator(params.seed);
      std::shuffle(slots.begin(), slots.end(), generator);
    }

    slots.resize(requiredBits);
    return slots;
  } else { // Smart
    auto allSlots = generateAllSlots(image, params);
    DispersionCalculator calc(image, params);

    struct SlotWithDisp {
      double disp;
      BitSlot slot;
    };
    std::vector<SlotWithDisp> candidates;
    candidates.reserve(allSlots.size());

    std::size_t slotIndex = 0;
    const auto pixelCount = static_cast<std::size_t>(image.width) *
                            static_cast<std::size_t>(image.height);
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
      for (std::uint8_t channel = 0; channel < image.channels; ++channel) {
        if (!channelEnabled(channel, params))
          continue;
        const double disp = calc.getDispersion(pixel, channel);
        if (disp >= params.dispersionThreshold) {
          for (std::uint8_t bit = 0; bit < params.bitsPerChannel; ++bit) {
            candidates.push_back({disp, allSlots[slotIndex]});
            ++slotIndex;
          }
        } else {
          slotIndex += params.bitsPerChannel;
        }
      }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const SlotWithDisp &a, const SlotWithDisp &b) {
                return a.disp > b.disp;
              });

    std::vector<BitSlot> slots;
    slots.reserve(requiredBits);
    for (const auto &item : candidates) {
      slots.push_back(item.slot);
    }

    if (params.seed != 0U && params.applyShuffleAfterSort) {
      std::mt19937 generator(params.seed);
      std::shuffle(slots.begin(), slots.end(), generator);
    }

    slots.resize(requiredBits);
    return slots;
  }
}

} // namespace rfp::stego::detail