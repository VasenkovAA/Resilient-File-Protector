#pragma once

#include "rfp/stego/ImageBuffer.h"
#include "rfp/stego/StegoParams.h"

#include <cstdint>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace rfp::stego {

/**
 * @brief Integral image for fast variance calculation in local neighborhoods
 *
 * Uses summed area table (SAT) for O(1) query of sum and sum of squares
 * in any rectangular region.
 */
struct IntegralImage {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::vector<std::uint64_t> sum;   ///< Sum of pixel values
  std::vector<std::uint64_t> sumSq; ///< Sum of squared pixel values

  [[nodiscard]] bool isValid() const noexcept {
    return width > 0 && height > 0 && !sum.empty() && !sumSq.empty() &&
           sum.size() == sumSq.size();
  }
};

/**
 * @brief Calculates local dispersion (variance) using integral images
 *
 * Thread-safe after construction. Supports luminance-based, per-channel,
 * and summed dispersion metrics.
 */
class DispersionCalculator {
public:
  /**
   * @brief Construct calculator for given image and parameters
   * @param image Source image (must be valid RGB/RGBA)
   * @param params Steganography parameters (window size, metric, etc.)
   * @throws std::invalid_argument if image is invalid or params invalid
   */
  explicit DispersionCalculator(const ImageBuffer &image,
                                const StegoParams &params);

  /**
   * @brief Get dispersion value for pixel at given index and channel
   * @param pixelIndex Linear pixel index (0-based)
   * @param channel Channel index (0=R, 1=G, 2=B, 3=A)
   * @return Variance in the local window [0, max possible value]
   */
  [[nodiscard]] double getDispersion(std::size_t pixelIndex,
                                     std::uint8_t channel) const;

  /**
   * @brief Get the window half-size used for calculations
   */
  [[nodiscard]] int getWindowHalf() const noexcept { return windowHalf_; }

  /**
   * @brief Get the metric type being used
   */
  [[nodiscard]] DispersionMetric getMetric() const noexcept { return metric_; }

private:
  // Build integral images for different metrics
  void buildLuminanceIntegral(const ImageBuffer &image);
  void buildChannelIntegrals(const ImageBuffer &image);

  // Query integral image for local variance
  [[nodiscard]] double queryDispersion(const IntegralImage &integral, int x,
                                       int y, int half) const;

  // Validate window size and adjust if needed
  static int validateWindowSize(int windowSize) noexcept;

private:
  const int windowSize_;
  int windowHalf_ = 0;
  const std::uint8_t bitsPerChannel_;
  const std::uint32_t width_;
  const std::uint32_t height_;
  const DispersionMetric metric_ = DispersionMetric::Luminance;

  // Only one of these will be valid based on metric_
  IntegralImage luminanceIntegral_;
  std::vector<IntegralImage> channelIntegrals_;

  // Cache for performance (thread-safe)
  mutable std::unordered_map<std::uint64_t, double> cache_;
  mutable std::shared_mutex cacheMutex_;
};

} // namespace rfp::stego