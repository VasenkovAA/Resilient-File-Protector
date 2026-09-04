#pragma once

#include <cstdint>

namespace rfp::stego {

/**
 * @brief Dispersion calculation methods
 */
enum class DispersionMetric {
  Luminance,  ///< Use weighted luminance (RGB → Y)
  PerChannel, ///< Calculate per-color channel separately
  Sum         ///< Sum of per-channel dispersions
};

/**
 * @brief Slot selection strategies
 */
enum class SlotSelectionMode {
  Uniform,   ///< Use all available slots uniformly
  Dispersion ///< Select slots with high local dispersion
};

/**
 * @brief Steganography parameters
 */
struct StegoParams {
  // Slot selection
  SlotSelectionMode mode = SlotSelectionMode::Uniform;

  // Dispersion-based selection parameters
  DispersionMetric metric = DispersionMetric::Luminance;
  double dispersionThreshold = 100.0;
  int windowSize = 5; // Must be odd, >= 3

  // Bit embedding parameters
  std::uint8_t bitsPerChannel = 1; // [1, 4]

  // Channel enable flags
  bool useRedChannel = true;
  bool useGreenChannel = true;
  bool useBlueChannel = true;
  bool useAlphaChannel = false; // Usually disabled by default

  // Randomization
  std::uint32_t seed = 0; // 0 = no randomization
  bool applyShuffleAfterSort = false;
};

} // namespace rfp::stego