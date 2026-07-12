#include "rfp/stego/StegoDispersion.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>

namespace rfp::stego {

DispersionCalculator::DispersionCalculator(const ImageBuffer &image,
                                           const StegoParams &params)
    : windowSize_(params.windowSize), bitsPerChannel_(params.bitsPerChannel),
      width_(image.width), height_(image.height) {
  windowHalf_ = windowSize_ / 2;

  switch (params.metric) {
  case DispersionMetric::Luminance:
    useLuminance_ = true;
    buildLuminanceIntegral(image);
    break;
  case DispersionMetric::PerChannel:
    usePerChannel_ = true;
    buildChannelIntegrals(image);
    break;
  case DispersionMetric::Sum:
    useSum_ = true;
    buildChannelIntegrals(image);
    break;
  }
}

void DispersionCalculator::buildLuminanceIntegral(const ImageBuffer &image) {
  const std::uint32_t w = image.width;
  const std::uint32_t h = image.height;
  luminanceIntegral_.width = w;
  luminanceIntegral_.height = h;

  const std::size_t rows = h + 1;
  const std::size_t cols = w + 1;
  luminanceIntegral_.sum.assign(rows * cols, 0);
  luminanceIntegral_.sumSq.assign(rows * cols, 0);

  auto pixelIndex = [w, channels = image.channels](
                        std::uint32_t x, std::uint32_t y) -> std::size_t {
    return (static_cast<std::size_t>(y) * w + static_cast<std::size_t>(x)) *
           channels;
  };

  for (std::uint32_t y = 0; y < h; ++y) {
    for (std::uint32_t x = 0; x < w; ++x) {
      const auto idx = pixelIndex(x, y);
      const std::uint8_t r = image.pixels[idx];
      const std::uint8_t g = image.pixels[idx + 1];
      const std::uint8_t b = image.pixels[idx + 2];

      const auto shift = bitsPerChannel_;
      const std::uint8_t r_stable =
          static_cast<std::uint8_t>((r >> shift) << shift);
      const std::uint8_t g_stable =
          static_cast<std::uint8_t>((g >> shift) << shift);
      const std::uint8_t b_stable =
          static_cast<std::uint8_t>((b >> shift) << shift);

      const double Y = 0.299 * r_stable + 0.587 * g_stable + 0.114 * b_stable;
      const std::uint64_t Y_int = static_cast<std::uint64_t>(std::round(Y));
      const std::uint64_t Y_sq = Y_int * Y_int;

      const std::size_t pos =
          (static_cast<std::size_t>(y + 1) * cols + (x + 1));
      const std::size_t top = (static_cast<std::size_t>(y) * cols + (x + 1));
      const std::size_t left = (static_cast<std::size_t>(y + 1) * cols + x);
      const std::size_t diag = (static_cast<std::size_t>(y) * cols + x);

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
  channelIntegrals_.resize(4);

  const std::size_t rows = h + 1;
  const std::size_t cols = w + 1;

  for (std::uint8_t c = 0; c < 4; ++c) {
    channelIntegrals_[c].width = w;
    channelIntegrals_[c].height = h;
    channelIntegrals_[c].sum.assign(rows * cols, 0);
    channelIntegrals_[c].sumSq.assign(rows * cols, 0);
  }

  auto pixelIndex = [w, channels = image.channels](
                        std::uint32_t x, std::uint32_t y) -> std::size_t {
    return (static_cast<std::size_t>(y) * w + static_cast<std::size_t>(x)) *
           channels;
  };

  for (std::uint32_t y = 0; y < h; ++y) {
    for (std::uint32_t x = 0; x < w; ++x) {
      const auto idx = pixelIndex(x, y);
      for (std::uint8_t c = 0; c < 4 && c < image.channels; ++c) {
        const std::uint8_t val = image.pixels[idx + c];
        const auto shift = bitsPerChannel_;
        const std::uint8_t stable =
            static_cast<std::uint8_t>((val >> shift) << shift);
        const std::uint64_t s = static_cast<std::uint64_t>(stable);
        const std::uint64_t sq = s * s;

        const std::size_t pos =
            (static_cast<std::size_t>(y + 1) * cols + (x + 1));
        const std::size_t top = (static_cast<std::size_t>(y) * cols + (x + 1));
        const std::size_t left = (static_cast<std::size_t>(y + 1) * cols + x);
        const std::size_t diag = (static_cast<std::size_t>(y) * cols + x);

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
  const int x1 = std::max(0, x - half);
  const int x2 = std::min(static_cast<int>(integral.width) - 1, x + half);
  const int y1 = std::max(0, y - half);
  const int y2 = std::min(static_cast<int>(integral.height) - 1, y + half);

  const std::size_t cols = integral.width + 1;
  const std::size_t topLeft =
      static_cast<std::size_t>(y1) * cols + static_cast<std::size_t>(x1);
  const std::size_t topRight =
      static_cast<std::size_t>(y1) * cols + static_cast<std::size_t>(x2 + 1);
  const std::size_t bottomLeft =
      static_cast<std::size_t>(y2 + 1) * cols + static_cast<std::size_t>(x1);
  const std::size_t bottomRight = static_cast<std::size_t>(y2 + 1) * cols +
                                  static_cast<std::size_t>(x2 + 1);

  const std::uint64_t sum = integral.sum[bottomRight] -
                            integral.sum[bottomLeft] - integral.sum[topRight] +
                            integral.sum[topLeft];
  const std::uint64_t sumSq =
      integral.sumSq[bottomRight] - integral.sumSq[bottomLeft] -
      integral.sumSq[topRight] + integral.sumSq[topLeft];
  const double n = static_cast<double>((x2 - x1 + 1) * (y2 - y1 + 1));

  if (n <= 1.0)
    return 0.0;
  const double mean = static_cast<double>(sum) / n;
  const double meanSq = static_cast<double>(sumSq) / n;
  return meanSq - mean * mean;
}

double DispersionCalculator::getDispersion(std::size_t pixelIndex,
                                           std::uint8_t channel) const {
  const std::uint32_t x = static_cast<std::uint32_t>(pixelIndex % width_);
  const std::uint32_t y = static_cast<std::uint32_t>(pixelIndex / width_);

  if (useLuminance_) {
    return queryDispersion(luminanceIntegral_, static_cast<int>(x),
                           static_cast<int>(y), windowHalf_);
  }
  if (usePerChannel_) {
    if (channel >= channelIntegrals_.size())
      return 0.0;
    return queryDispersion(channelIntegrals_[channel], static_cast<int>(x),
                           static_cast<int>(y), windowHalf_);
  }
  if (useSum_) {
    double total = 0.0;
    for (std::uint8_t c = 0; c < 4; ++c) {
      if (c < channelIntegrals_.size()) {
        total += queryDispersion(channelIntegrals_[c], static_cast<int>(x),
                                 static_cast<int>(y), windowHalf_);
      }
    }
    return total;
  }
  return 0.0;
}

} // namespace rfp::stego