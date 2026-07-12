#pragma once

#include <cstdint>

namespace rfp::stego {

enum class SlotSelectionMode : std::uint8_t { Uniform, Smart };

enum class DispersionMetric : std::uint8_t { Luminance, PerChannel, Sum };

struct StegoParams {
  std::uint8_t bitsPerChannel = 1;
  std::uint32_t seed = 0;
  bool useRedChannel = true;
  bool useGreenChannel = true;
  bool useBlueChannel = true;
  bool useAlphaChannel = false;

  SlotSelectionMode mode = SlotSelectionMode::Uniform;
  int windowSize = 3;
  DispersionMetric metric = DispersionMetric::Luminance;
  double dispersionThreshold = 0.0;
  bool applyShuffleAfterSort = true;
};

} // namespace rfp::stego
