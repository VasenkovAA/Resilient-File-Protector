#pragma once

#include "rfp/stego/ImageBuffer.h"
#include "rfp/stego/StegoParams.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rfp::stego {

class DispersionCalculator {
public:
  DispersionCalculator(const ImageBuffer &image, const StegoParams &params);

  [[nodiscard]] double getDispersion(std::size_t pixelIndex,
                                     std::uint8_t channel) const;

private:
  struct IntegralImage {
    std::vector<std::uint64_t> sum;
    std::vector<std::uint64_t> sumSq;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
  };

  void buildLuminanceIntegral(const ImageBuffer &image);
  void buildChannelIntegrals(const ImageBuffer &image);

  [[nodiscard]] double queryDispersion(const IntegralImage &integral, int x,
                                       int y, int half) const;

  std::vector<IntegralImage> channelIntegrals_;
  IntegralImage luminanceIntegral_;
  bool useLuminance_ = false;
  bool usePerChannel_ = false;
  bool useSum_ = false;
  int windowHalf_ = 1;
  int windowSize_ = 3;
  std::uint8_t bitsPerChannel_ = 1;
  std::uint32_t width_ = 0;
  std::uint32_t height_ = 0;
};

} // namespace rfp::stego
