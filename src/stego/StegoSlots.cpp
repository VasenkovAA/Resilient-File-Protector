#include "rfp/stego/StegoSlots.h"
#include "rfp/stego/StegoDispersion.h"

#include <algorithm>
#include <cassert>
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
    assert(false && "Invalid channel index");
    return false;
  }
}

namespace {
constexpr std::uint8_t MAX_BITS_PER_CHANNEL = 4;
constexpr std::uint8_t MIN_BITS_PER_CHANNEL = 1;

/**
 * @brief Count enabled channels in the image
 */
std::size_t countEnabledChannels(const ImageBuffer &image,
                                 const StegoParams &params) noexcept {
  std::size_t count = 0;
  for (std::uint8_t channel = 0; channel < image.channels; ++channel) {
    if (channelEnabled(channel, params)) {
      ++count;
    }
  }
  return count;
}

/**
 * @brief Generate all possible bit slots in the image
 */
std::vector<BitSlot> generateAllSlots(const ImageBuffer &image,
                                      const StegoParams &params) {
  const auto pixelCount = static_cast<std::size_t>(image.width) *
                          static_cast<std::size_t>(image.height);

  const std::size_t enabledChannels = countEnabledChannels(image, params);
  const std::size_t totalSlots =
      pixelCount * enabledChannels * params.bitsPerChannel;

  std::vector<BitSlot> slots;
  slots.reserve(totalSlots);

  for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
    const std::size_t pixelOffset = pixel * image.channels;

    for (std::uint8_t channel = 0; channel < image.channels; ++channel) {
      if (!channelEnabled(channel, params)) {
        continue;
      }

      for (std::uint8_t bit = 0; bit < params.bitsPerChannel; ++bit) {
        slots.emplace_back(
            BitSlot{pixelOffset + static_cast<std::size_t>(channel), bit});
      }
    }
  }

  return slots;
}

/**
 * @brief Validate steganography parameters
 */
rfp::core::Result<void> validateParams(const ImageBuffer &image,
                                       const StegoParams &params) {
  if (!image.isValid()) {
    return rfp::core::Error{rfp::core::ErrorCode::InvalidImageBuffer,
                            "Invalid image buffer"};
  }

  if (params.bitsPerChannel < MIN_BITS_PER_CHANNEL ||
      params.bitsPerChannel > MAX_BITS_PER_CHANNEL) {
    return rfp::core::Error{rfp::core::ErrorCode::InvalidArgument,
                            "bitsPerChannel must be in range [1, 4]"};
  }

  if (countEnabledChannels(image, params) == 0) {
    return rfp::core::Error{rfp::core::ErrorCode::InvalidArgument,
                            "At least one color channel must be enabled"};
  }

  return {};
}

} // anonymous namespace

std::size_t availableSlotCount(const ImageBuffer &image,
                               const StegoParams &params) noexcept {
  if (!image.isValid() || params.bitsPerChannel < MIN_BITS_PER_CHANNEL ||
      params.bitsPerChannel > MAX_BITS_PER_CHANNEL) {
    return 0;
  }

  const auto pixelCount = static_cast<std::size_t>(image.width) *
                          static_cast<std::size_t>(image.height);
  const std::size_t enabledChannels = countEnabledChannels(image, params);

  if (params.mode == SlotSelectionMode::Uniform) {
    return pixelCount * enabledChannels *
           static_cast<std::size_t>(params.bitsPerChannel);
  }

  // Dispersion-based selection
  try {
    DispersionCalculator calc(image, params);
    std::size_t count = 0;

    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
      for (std::uint8_t channel = 0; channel < image.channels; ++channel) {
        if (!channelEnabled(channel, params)) {
          continue;
        }

        const double disp = calc.getDispersion(pixel, channel);
        if (disp >= params.dispersionThreshold) {
          count += params.bitsPerChannel;
        }
      }
    }
    return count;
  } catch (const std::exception &) {
    // On any exception, fall back to uniform counting
    return pixelCount * enabledChannels *
           static_cast<std::size_t>(params.bitsPerChannel);
  }
}

rfp::core::Result<std::vector<BitSlot>> buildSlots(const ImageBuffer &image,
                                                   const StegoParams &params,
                                                   std::size_t requiredBits) {
  // Validate parameters
  auto validation = validateParams(image, params);
  if (!validation) {
    return validation.error();
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
  }

  // Dispersion-based selection
  auto allSlots = generateAllSlots(image, params);

  try {
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
        if (!channelEnabled(channel, params)) {
          continue;
        }

        const double disp = calc.getDispersion(pixel, channel);
        if (disp >= params.dispersionThreshold) {
          // Add all bits for this channel
          for (std::uint8_t bit = 0; bit < params.bitsPerChannel; ++bit) {
            if (slotIndex < allSlots.size()) {
              candidates.emplace_back(SlotWithDisp{disp, allSlots[slotIndex]});
            }
            ++slotIndex;
          }
        } else {
          slotIndex += params.bitsPerChannel;
        }
      }
    }

    // Sort by dispersion descending (higher dispersion = better for hiding)
    std::sort(candidates.begin(), candidates.end(),
              [](const SlotWithDisp &a, const SlotWithDisp &b) {
                return a.disp > b.disp;
              });

    std::vector<BitSlot> slots;
    slots.reserve(requiredBits);

    for (const auto &item : candidates) {
      slots.push_back(item.slot);
    }

    // Optional shuffling after sorting
    if (params.seed != 0U && params.applyShuffleAfterSort) {
      std::mt19937 generator(params.seed);
      std::shuffle(slots.begin(), slots.end(), generator);
    }

    slots.resize(requiredBits);
    return slots;

  } catch (const std::exception &e) {
    return rfp::core::Error{
        rfp::core::ErrorCode::InvalidArgument,
        std::string("Failed to build dispersion-based slots: ") + e.what()};
  }
}

} // namespace rfp::stego::detail