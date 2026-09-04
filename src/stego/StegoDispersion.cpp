#include "rfp/stego/StegoDispersion.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <numeric>
#include <shared_mutex>
#include <stdexcept>
#include <unordered_map>

namespace rfp::stego {

namespace {
// Luminance coefficients (ITU-R BT.601)
constexpr double LUMINANCE_R = 0.299;
constexpr double LUMINANCE_G = 0.587;
constexpr double LUMINANCE_B = 0.114;

// Minimum window size (must be odd)
constexpr int MIN_WINDOW_SIZE = 3;

// Maximum supported channels
constexpr std::uint8_t MAX_CHANNELS = 4;

// Helper for linear pixel indexing
static inline std::size_t getPixelIndex(std::uint32_t x, std::uint32_t y,
                                        std::uint32_t width,
                                        std::uint8_t channels) noexcept {
  return (static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)) *
         channels;
}

// Round double to nearest uint64_t with proper rounding
static inline std::uint64_t roundToUint64(double value) noexcept {
  return static_cast<std::uint64_t>(std::llround(value));
}

} // anonymous namespace

int DispersionCalculator::validateWindowSize(int windowSize) noexcept {
  if (windowSize < MIN_WINDOW_SIZE) {
    return MIN_WINDOW_SIZE;
  }
  // Ensure odd window size for symmetric neighborhood
  if (windowSize % 2 == 0) {
    return windowSize + 1;
  }
  return windowSize;
}

DispersionCalculator::DispersionCalculator(const ImageBuffer &image,
                                           const StegoParams &params)
    : windowSize_(validateWindowSize(params.windowSize)),
      bitsPerChannel_(params.bitsPerChannel), width_(image.width),
      height_(image.height), metric_(params.metric) {

  if (!image.isValid()) {
    throw std::invalid_argument("Invalid image buffer");
  }

  if (bitsPerChannel_ == 0 || bitsPerChannel_ > 4) {
    throw std::invalid_argument("bitsPerChannel must be in range [1, 4]");
  }

  windowHalf_ = windowSize_ / 2;

  // Build integral images based on metric type
  switch (metric_) {
  case DispersionMetric::Luminance:
    buildLuminanceIntegral(image);
    break;
  case DispersionMetric::PerChannel:
  case DispersionMetric::Sum:
    buildChannelIntegrals(image);
    break;
  default:
    throw std::invalid_argument("Unknown dispersion metric");
  }

  // Reserve cache space for performance
  cache_.reserve(1024);
}

void DispersionCalculator::buildLuminanceIntegral(const ImageBuffer &image) {
  const std::uint32_t w = image.width;
  const std::uint32_t h = image.height;
  const std::uint8_t channels = image.channels;

  luminanceIntegral_.width = w;
  luminanceIntegral_.height = h;

  const std::size_t rows = static_cast<std::size_t>(h) + 1;
  const std::size_t cols = static_cast<std::size_t>(w) + 1;
  const std::size_t totalSize = rows * cols;

  luminanceIntegral_.sum.assign(totalSize, 0);
  luminanceIntegral_.sumSq.assign(totalSize, 0);

  const auto shift = static_cast<unsigned>(bitsPerChannel_);
  const std::uint8_t bitMask = static_cast<std::uint8_t>(~((1U << shift) - 1U));

  for (std::uint32_t y = 0; y < h; ++y) {
    for (std::uint32_t x = 0; x < w; ++x) {
      const auto idx = getPixelIndex(x, y, w, channels);

      // Quantize pixel values to stable bits
      const std::uint8_t r_stable = image.pixels[idx] & bitMask;
      const std::uint8_t g_stable = image.pixels[idx + 1] & bitMask;
      const std::uint8_t b_stable = image.pixels[idx + 2] & bitMask;

      // Calculate luminance (alpha channel is ignored for RGB images)
      const double luminance = LUMINANCE_R * r_stable + LUMINANCE_G * g_stable +
                               LUMINANCE_B * b_stable;

      const std::uint64_t Y_int = roundToUint64(luminance);
      const std::uint64_t Y_sq = Y_int * Y_int;

      // Update integral image using prefix sum
      const std::size_t pos = static_cast<std::size_t>(y + 1) * cols + (x + 1);
      const std::size_t top = static_cast<std::size_t>(y) * cols + (x + 1);
      const std::size_t left = static_cast<std::size_t>(y + 1) * cols + x;
      const std::size_t diag = static_cast<std::size_t>(y) * cols + x;

      luminanceIntegral_.sum[pos] = luminanceIntegral_.sum[top] +
                                    luminanceIntegral_.sum[left] -
                                    luminanceIntegral_.sum[diag] + Y_int;

      luminanceIntegral_.sumSq[pos] = luminanceIntegral_.sumSq[top] +
                                      luminanceIntegral_.sumSq[left] -
                                      luminanceIntegral_.sumSq[diag] + Y_sq;
    }
  }
}

void DispersionCalculator::buildChannelIntegrals(const ImageBuffer &image) {
  const std::uint32_t w = image.width;
  const std::uint32_t h = image.height;
  const std::uint8_t channels = image.channels;

  // Only allocate for actual channels present
  channelIntegrals_.resize(channels);

  const std::size_t rows = static_cast<std::size_t>(h) + 1;
  const std::size_t cols = static_cast<std::size_t>(w) + 1;
  const std::size_t totalSize = rows * cols;

  for (std::uint8_t c = 0; c < channels; ++c) {
    channelIntegrals_[c].width = w;
    channelIntegrals_[c].height = h;
    channelIntegrals_[c].sum.assign(totalSize, 0);
    channelIntegrals_[c].sumSq.assign(totalSize, 0);
  }

  const auto shift = static_cast<unsigned>(bitsPerChannel_);
  const std::uint8_t bitMask = static_cast<std::uint8_t>(~((1U << shift) - 1U));

  for (std::uint32_t y = 0; y < h; ++y) {
    for (std::uint32_t x = 0; x < w; ++x) {
      const auto idx = getPixelIndex(x, y, w, channels);

      for (std::uint8_t c = 0; c < channels; ++c) {
        const std::uint8_t value = image.pixels[idx + c];
        const std::uint8_t stable = value & bitMask;
        const std::uint64_t s = static_cast<std::uint64_t>(stable);
        const std::uint64_t sq = s * s;

        const std::size_t pos =
            static_cast<std::size_t>(y + 1) * cols + (x + 1);
        const std::size_t top = static_cast<std::size_t>(y) * cols + (x + 1);
        const std::size_t left = static_cast<std::size_t>(y + 1) * cols + x;
        const std::size_t diag = static_cast<std::size_t>(y) * cols + x;

        auto &integral = channelIntegrals_[c];
        integral.sum[pos] =
            integral.sum[top] + integral.sum[left] - integral.sum[diag] + s;
        integral.sumSq[pos] = integral.sumSq[top] + integral.sumSq[left] -
                              integral.sumSq[diag] + sq;
      }
    }
  }
}

double DispersionCalculator::queryDispersion(const IntegralImage &integral,
                                             int x, int y, int half) const {
  // Clamp window boundaries to image edges
  const int x1 = std::max(0, x - half);
  const int x2 = std::min(static_cast<int>(integral.width) - 1, x + half);
  const int y1 = std::max(0, y - half);
  const int y2 = std::min(static_cast<int>(integral.height) - 1, y + half);

  // Early return for single pixel
  if (x1 == x2 && y1 == y2) {
    return 0.0;
  }

  const std::size_t cols = static_cast<std::size_t>(integral.width) + 1;

  // Integral image indices
  const std::size_t topLeft =
      static_cast<std::size_t>(y1) * cols + static_cast<std::size_t>(x1);
  const std::size_t topRight =
      static_cast<std::size_t>(y1) * cols + static_cast<std::size_t>(x2 + 1);
  const std::size_t bottomLeft =
      static_cast<std::size_t>(y2 + 1) * cols + static_cast<std::size_t>(x1);
  const std::size_t bottomRight = static_cast<std::size_t>(y2 + 1) * cols +
                                  static_cast<std::size_t>(x2 + 1);

  // Sum of values in window
  const std::uint64_t sum = integral.sum[bottomRight] -
                            integral.sum[bottomLeft] - integral.sum[topRight] +
                            integral.sum[topLeft];

  // Sum of squares in window
  const std::uint64_t sumSq =
      integral.sumSq[bottomRight] - integral.sumSq[bottomLeft] -
      integral.sumSq[topRight] + integral.sumSq[topLeft];

  const double n = static_cast<double>((x2 - x1 + 1) * (y2 - y1 + 1));

  if (n <= 1.0) {
    return 0.0;
  }

  // Variance = E[X^2] - E[X]^2
  const double mean = static_cast<double>(sum) / n;
  const double meanSq = static_cast<double>(sumSq) / n;

  return std::max(0.0, meanSq - mean * mean);
}

double DispersionCalculator::getDispersion(std::size_t pixelIndex,
                                           std::uint8_t channel) const {
  // Validate inputs
  if (pixelIndex >= static_cast<std::size_t>(width_) * height_) {
    return 0.0;
  }

  const std::uint32_t x = static_cast<std::uint32_t>(pixelIndex % width_);
  const std::uint32_t y = static_cast<std::uint32_t>(pixelIndex / width_);

  // Check cache first (thread-safe read)
  const std::uint64_t cacheKey = (static_cast<std::uint64_t>(channel) << 32) |
                                 static_cast<std::uint64_t>(pixelIndex);
  {
    std::shared_lock<std::shared_mutex> lock(cacheMutex_);
    auto it = cache_.find(cacheKey);
    if (it != cache_.end()) {
      return it->second;
    }
  }

  double result = 0.0;

  switch (metric_) {
  case DispersionMetric::Luminance:
    if (!luminanceIntegral_.isValid()) {
      result = 0.0;
    } else {
      result = queryDispersion(luminanceIntegral_, static_cast<int>(x),
                               static_cast<int>(y), windowHalf_);
    }
    break;

  case DispersionMetric::PerChannel:
    if (channel >= channelIntegrals_.size() || channel >= MAX_CHANNELS ||
        !channelIntegrals_[channel].isValid()) {
      result = 0.0;
    } else {
      result = queryDispersion(channelIntegrals_[channel], static_cast<int>(x),
                               static_cast<int>(y), windowHalf_);
    }
    break;

  case DispersionMetric::Sum: {
    double total = 0.0;
    const std::uint8_t maxChannel =
        std::min(static_cast<std::uint8_t>(channelIntegrals_.size()),
                 static_cast<std::uint8_t>(MAX_CHANNELS));
    for (std::uint8_t c = 0; c < maxChannel; ++c) {
      if (channelIntegrals_[c].isValid()) {
        total += queryDispersion(channelIntegrals_[c], static_cast<int>(x),
                                 static_cast<int>(y), windowHalf_);
      }
    }
    result = total;
    break;
  }

  default:
    result = 0.0;
    break;
  }

  // Cache the result (thread-safe write)
  {
    std::unique_lock<std::shared_mutex> lock(cacheMutex_);
    cache_[cacheKey] = result;
  }

  return result;
}

} // namespace rfp::stego