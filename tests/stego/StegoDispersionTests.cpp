#include "rfp/stego/StegoDispersion.h"
#include <gtest/gtest.h>

namespace {

rfp::stego::ImageBuffer makeSolidImage(std::uint32_t w, std::uint32_t h,
                                       std::uint8_t value) {
  rfp::stego::ImageBuffer img;
  img.width = w;
  img.height = h;
  img.channels = 4;
  img.pixels.assign(img.byteSize(), value);
  return img;
}

} // namespace

TEST(StegoDispersionTests, SolidImageDispersionZero) {
  auto img = makeSolidImage(10, 10, 0x80);
  rfp::stego::StegoParams params;
  params.metric = rfp::stego::DispersionMetric::Luminance;
  params.windowSize = 3;
  params.bitsPerChannel = 1;

  rfp::stego::DispersionCalculator calc(img, params);
  for (std::size_t i = 0; i < img.pixels.size() / img.channels; ++i) {
    EXPECT_DOUBLE_EQ(calc.getDispersion(i, 0), 0.0);
  }
}

TEST(StegoDispersionTests, StableDispersionAfterBitZeroing) {
  rfp::stego::ImageBuffer img;
  img.width = 3;
  img.height = 3;
  img.channels = 4;
  img.pixels.assign(img.byteSize(), 0x00);
  const auto center =
      static_cast<std::size_t>(1) * img.width * img.channels + 1 * img.channels;
  img.pixels[center] = 0xFF;
  img.pixels[center + 1] = 0xFF;
  img.pixels[center + 2] = 0xFF;

  rfp::stego::StegoParams params;
  params.metric = rfp::stego::DispersionMetric::Luminance;
  params.windowSize = 3;
  params.bitsPerChannel = 1;

  rfp::stego::DispersionCalculator calc(img, params);
  const auto centerIdx = static_cast<std::size_t>(1) * img.width + 1;
  const double d1 = calc.getDispersion(centerIdx, 0);
  EXPECT_GT(d1, 0.0);

  rfp::stego::ImageBuffer img2 = img;
  img2.pixels[center] = 0xFE;
  img2.pixels[center + 1] = 0xFE;
  img2.pixels[center + 2] = 0xFE;

  rfp::stego::DispersionCalculator calc2(img2, params);
  const double d2 = calc2.getDispersion(centerIdx, 0);
  EXPECT_DOUBLE_EQ(d1, d2);
}

TEST(StegoDispersionTests, WindowSizeAffectsDispersion) {
  auto img = makeSolidImage(5, 5, 0x80);
  img.pixels[0] = 0xFF;
  img.pixels[1] = 0xFF;
  img.pixels[2] = 0xFF;

  rfp::stego::StegoParams params;
  params.metric = rfp::stego::DispersionMetric::Luminance;
  params.bitsPerChannel = 1;

  params.windowSize = 3;
  rfp::stego::DispersionCalculator calc3(img, params);
  params.windowSize = 5;
  rfp::stego::DispersionCalculator calc5(img, params);
  const double d3 = calc3.getDispersion(0, 0);
  const double d5 = calc5.getDispersion(0, 0);
  EXPECT_GT(d3, d5);
}