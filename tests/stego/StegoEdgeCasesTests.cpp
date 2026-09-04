#include "rfp/core/ByteBuffer.h"
#include "rfp/core/Error.h"
#include "rfp/stego/Capacity.h"
#include "rfp/stego/ImageBuffer.h"
#include "rfp/stego/StegoDecoder.h"
#include "rfp/stego/StegoDispersion.h"
#include "rfp/stego/StegoEncoder.h"
#include "rfp/stego/StegoParams.h"
#include "rfp/stego/StegoSlots.h"

#include <gtest/gtest.h>
#include <stdexcept>

namespace {

rfp::stego::ImageBuffer makeImage(std::uint32_t width, std::uint32_t height,
                                  std::uint8_t channels = 4) {
  rfp::stego::ImageBuffer img;
  img.width = width;
  img.height = height;
  img.channels = channels;
  img.pixels.assign(img.byteSize(), 0xAA);
  return img;
}

rfp::stego::ImageBuffer makeInvalidImage() {
  rfp::stego::ImageBuffer img;
  img.width = 0;
  img.height = 0;
  img.channels = 4;
  img.pixels.clear();
  return img;
}

} // namespace

TEST(ImageBufferTests, IsValidDetectsInvalidState) {
  auto img = makeImage(10, 10, 3);
  EXPECT_TRUE(img.isValid());

  img.width = 0;
  EXPECT_FALSE(img.isValid());
  img.width = 10;
  img.height = 0;
  EXPECT_FALSE(img.isValid());
  img.height = 10;
  img.channels = 2;
  EXPECT_FALSE(img.isValid());
  img.channels = 5;
  EXPECT_FALSE(img.isValid());

  img = makeImage(10, 10, 4);
  img.pixels.resize(img.byteSize() - 1);
  EXPECT_FALSE(img.isValid());
}

TEST(ImageBufferTests, ByteSizeCalculation) {
  auto img = makeImage(10, 20, 3);
  EXPECT_EQ(img.byteSize(), 10U * 20U * 3U);
  img = makeImage(5, 5, 4);
  EXPECT_EQ(img.byteSize(), 5U * 5U * 4U);
}

TEST(CapacityTests, UniformModeWithDifferentBitsPerChannel) {
  auto img = makeImage(10, 10, 4);
  rfp::stego::StegoParams params;
  params.mode = rfp::stego::SlotSelectionMode::Uniform;
  params.useAlphaChannel = true;

  // Теперь все 4 канала включены
  params.bitsPerChannel = 1;
  EXPECT_EQ(rfp::stego::capacityBits(img, params), 400U);
  EXPECT_EQ(rfp::stego::capacityBytes(img, params), 50U);

  params.bitsPerChannel = 2;
  EXPECT_EQ(rfp::stego::capacityBits(img, params), 800U);
  EXPECT_EQ(rfp::stego::capacityBytes(img, params), 100U);
}

TEST(CapacityTests, DispersionModeVariesWithThreshold) {
  auto img = makeImage(10, 10, 4);
  rfp::stego::StegoParams params;
  params.mode = rfp::stego::SlotSelectionMode::Dispersion;
  params.metric = rfp::stego::DispersionMetric::Luminance;
  params.windowSize = 3;
  params.bitsPerChannel = 1;
  params.useAlphaChannel = false;

  params.dispersionThreshold = 0.0;
  auto cap0 = rfp::stego::capacityBytes(img, params);
  params.dispersionThreshold = 10000.0;
  auto cap1 = rfp::stego::capacityBytes(img, params);
  EXPECT_GE(cap0, cap1);
}

TEST(CapacityTests, InvalidParamsReturnZero) {
  auto img = makeImage(10, 10, 4);
  rfp::stego::StegoParams params;
  params.bitsPerChannel = 0;
  EXPECT_EQ(rfp::stego::capacityBits(img, params), 0U);

  params.bitsPerChannel = 5;
  EXPECT_EQ(rfp::stego::capacityBits(img, params), 0U);

  params.bitsPerChannel = 1;
  params.useRedChannel = false;
  params.useGreenChannel = false;
  params.useBlueChannel = false;
  params.useAlphaChannel = false;
  EXPECT_EQ(rfp::stego::capacityBits(img, params), 0U);

  auto invalid = makeInvalidImage();
  EXPECT_EQ(rfp::stego::capacityBits(invalid, params), 0U);
}

TEST(StegoSlotsTests, ChannelEnabled) {
  rfp::stego::StegoParams params;
  params.useRedChannel = true;
  params.useGreenChannel = false;
  params.useBlueChannel = true;
  params.useAlphaChannel = false;

  EXPECT_TRUE(rfp::stego::detail::channelEnabled(0, params));
  EXPECT_FALSE(rfp::stego::detail::channelEnabled(1, params));
  EXPECT_TRUE(rfp::stego::detail::channelEnabled(2, params));
  EXPECT_FALSE(rfp::stego::detail::channelEnabled(3, params));
}

TEST(StegoSlotsTests, AvailableSlotCountHandlesEdgeCases) {
  auto img = makeImage(10, 10, 4);
  rfp::stego::StegoParams params;
  params.mode = rfp::stego::SlotSelectionMode::Uniform;
  params.bitsPerChannel = 1;
  params.useAlphaChannel = false;

  EXPECT_EQ(rfp::stego::detail::availableSlotCount(img, params), 300U);

  params.mode = rfp::stego::SlotSelectionMode::Dispersion;
  params.dispersionThreshold = 0.0;
  auto count = rfp::stego::detail::availableSlotCount(img, params);
  EXPECT_LE(count, 300U);

  params.bitsPerChannel = 0;
  EXPECT_EQ(rfp::stego::detail::availableSlotCount(img, params), 0U);
}

TEST(StegoSlotsTests, BuildSlotsSuccessWithShuffle) {
  auto img = makeImage(5, 5, 4);
  rfp::stego::StegoParams params;
  params.mode = rfp::stego::SlotSelectionMode::Uniform;
  params.bitsPerChannel = 1;
  params.seed = 12345;

  std::size_t required = 50;
  auto slotsResult = rfp::stego::detail::buildSlots(img, params, required);
  ASSERT_TRUE(slotsResult) << slotsResult.error().message;
  const auto &slots = slotsResult.value();
  EXPECT_EQ(slots.size(), required);
}

TEST(StegoSlotsTests, BuildSlotsErrors) {
  auto img = makeImage(5, 5, 4);
  rfp::stego::StegoParams params;
  params.mode = rfp::stego::SlotSelectionMode::Uniform;
  params.bitsPerChannel = 1;

  std::size_t required = 200;
  auto slotsResult = rfp::stego::detail::buildSlots(img, params, required);
  ASSERT_FALSE(slotsResult);
  EXPECT_EQ(slotsResult.error().code, rfp::core::ErrorCode::CapacityExceeded);

  params.bitsPerChannel = 0;
  slotsResult = rfp::stego::detail::buildSlots(img, params, 10);
  ASSERT_FALSE(slotsResult);
  EXPECT_EQ(slotsResult.error().code, rfp::core::ErrorCode::InvalidArgument);

  params.bitsPerChannel = 5;
  slotsResult = rfp::stego::detail::buildSlots(img, params, 10);
  ASSERT_FALSE(slotsResult);

  params.bitsPerChannel = 1;
  params.useRedChannel = false;
  params.useGreenChannel = false;
  params.useBlueChannel = false;
  params.useAlphaChannel = false;
  slotsResult = rfp::stego::detail::buildSlots(img, params, 10);
  ASSERT_FALSE(slotsResult);
}

TEST(StegoEncoderTests, EmbedBytesErrors) {
  auto img = makeImage(10, 10, 4);
  rfp::stego::StegoParams params;
  params.bitsPerChannel = 1;

  rfp::core::ByteBuffer empty;
  auto result = rfp::stego::StegoEncoder::embedBytes(img, empty, params);
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error().code, rfp::core::ErrorCode::InvalidArgument);

  auto invalid = makeInvalidImage();
  result = rfp::stego::StegoEncoder::embedBytes(
      invalid, rfp::core::toBytes("test"), params);
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error().code, rfp::core::ErrorCode::InvalidImageBuffer);

  std::string longText(1000, 'x');
  result = rfp::stego::StegoEncoder::embedBytes(
      img, rfp::core::toBytes(longText), params);
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error().code, rfp::core::ErrorCode::CapacityExceeded);

  params.bitsPerChannel = 0;
  result = rfp::stego::StegoEncoder::embedBytes(img, rfp::core::toBytes("test"),
                                                params);
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error().code, rfp::core::ErrorCode::InvalidArgument);

  params.bitsPerChannel = 1;
  params.useRedChannel = false;
  params.useGreenChannel = false;
  params.useBlueChannel = false;
  params.useAlphaChannel = false;
  result = rfp::stego::StegoEncoder::embedBytes(img, rfp::core::toBytes("test"),
                                                params);
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error().code, rfp::core::ErrorCode::InvalidArgument);
}

TEST(StegoEncoderTests, EmbedTextErrors) {
  auto img = makeImage(10, 10, 4);
  rfp::stego::StegoParams params;
  params.bitsPerChannel = 1;

  auto result = rfp::stego::StegoEncoder::embedText(img, "", params);
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error().code, rfp::core::ErrorCode::InvalidArgument);

  auto invalid = makeInvalidImage();
  result = rfp::stego::StegoEncoder::embedText(invalid, "test", params);
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error().code, rfp::core::ErrorCode::InvalidImageBuffer);
}

TEST(StegoDecoderTests, ExtractBytesEdgeCases) {
  auto img = makeImage(10, 10, 4);
  rfp::stego::StegoParams params;
  params.bitsPerChannel = 1;

  auto result = rfp::stego::StegoDecoder::extractBytes(img, 0, params);
  ASSERT_TRUE(result);
  EXPECT_TRUE(result.value().empty());

  auto invalid = makeInvalidImage();
  result = rfp::stego::StegoDecoder::extractBytes(invalid, 10, params);
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error().code, rfp::core::ErrorCode::InvalidImageBuffer);

  result = rfp::stego::StegoDecoder::extractBytes(img, 1000, params);
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error().code, rfp::core::ErrorCode::CapacityExceeded);
}

TEST(DispersionCalculatorTests, ConstructorThrowsOnInvalidImage) {
  rfp::stego::StegoParams params;
  params.bitsPerChannel = 1;

  auto invalid = makeInvalidImage();
  EXPECT_THROW(rfp::stego::DispersionCalculator calc(invalid, params),
               std::invalid_argument);
}

TEST(DispersionCalculatorTests, ConstructorThrowsOnInvalidBitsPerChannel) {
  auto img = makeImage(10, 10, 4);
  rfp::stego::StegoParams params;
  params.bitsPerChannel = 0;
  EXPECT_THROW(rfp::stego::DispersionCalculator calc(img, params),
               std::invalid_argument);

  params.bitsPerChannel = 5;
  EXPECT_THROW(rfp::stego::DispersionCalculator calc(img, params),
               std::invalid_argument);
}

TEST(DispersionCalculatorTests, WindowSizeIsCorrected) {
  auto img = makeImage(10, 10, 4);
  rfp::stego::StegoParams params;
  params.bitsPerChannel = 1;
  params.metric = rfp::stego::DispersionMetric::Luminance;

  params.windowSize = 2;
  rfp::stego::DispersionCalculator calc1(img, params);
  EXPECT_EQ(calc1.getWindowHalf(), 1);

  params.windowSize = 4;
  rfp::stego::DispersionCalculator calc2(img, params);
  EXPECT_EQ(calc2.getWindowHalf(), 2);

  params.windowSize = 1;
  rfp::stego::DispersionCalculator calc3(img, params);
  EXPECT_EQ(calc3.getWindowHalf(), 1);
}

TEST(DispersionCalculatorTests, PerChannelMetricWorks) {
  auto img = makeImage(5, 5, 3);
  img.pixels[0] = 255;
  img.pixels[1] = 255;
  img.pixels[2] = 255;

  rfp::stego::StegoParams params;
  params.metric = rfp::stego::DispersionMetric::PerChannel;
  params.windowSize = 3;
  params.bitsPerChannel = 1;

  rfp::stego::DispersionCalculator calc(img, params);
  double d0 = calc.getDispersion(0, 0);
  double d1 = calc.getDispersion(0, 1);
  double d2 = calc.getDispersion(0, 2);
  EXPECT_GT(d0, 0.0);
  EXPECT_GT(d1, 0.0);
  EXPECT_GT(d2, 0.0);
}

TEST(DispersionCalculatorTests, SumMetricWorks) {
  auto img = makeImage(5, 5, 3);
  img.pixels[0] = 255;
  img.pixels[1] = 0;
  img.pixels[2] = 0;

  rfp::stego::StegoParams params;
  params.metric = rfp::stego::DispersionMetric::Sum;
  params.windowSize = 3;
  params.bitsPerChannel = 1;

  rfp::stego::DispersionCalculator calc(img, params);
  double dSum = calc.getDispersion(0, 0);
  EXPECT_GT(dSum, 0.0);
}

TEST(DispersionCalculatorTests, InvalidChannelIndexReturnsZero) {
  auto img = makeImage(5, 5, 4);
  rfp::stego::StegoParams params;
  params.metric = rfp::stego::DispersionMetric::Luminance;
  params.windowSize = 3;
  params.bitsPerChannel = 1;

  rfp::stego::DispersionCalculator calc(img, params);
  EXPECT_DOUBLE_EQ(calc.getDispersion(0, 10), 0.0);
}

TEST(DispersionCalculatorTests, CacheBehavior) {
  auto img = makeImage(10, 10, 4);
  rfp::stego::StegoParams params;
  params.metric = rfp::stego::DispersionMetric::Luminance;
  params.windowSize = 3;
  params.bitsPerChannel = 1;

  rfp::stego::DispersionCalculator calc(img, params);
  double d1 = calc.getDispersion(0, 0);
  double d2 = calc.getDispersion(0, 0);
  EXPECT_DOUBLE_EQ(d1, d2);
}

TEST(DispersionCalculatorTests, LuminanceMetricWithAlphaIgnored) {
  auto img = makeImage(3, 3, 4);
  const std::size_t center = (1 * 3 + 1) * 4;
  img.pixels[center] = 255;
  img.pixels[center + 1] = 255;
  img.pixels[center + 2] = 255;
  img.pixels[center + 3] = 255;

  rfp::stego::StegoParams params;
  params.metric = rfp::stego::DispersionMetric::Luminance;
  params.windowSize = 3;
  params.bitsPerChannel = 1;

  rfp::stego::DispersionCalculator calc(img, params);
  double d = calc.getDispersion(center / 4, 0);
  EXPECT_GT(d, 0.0);
}
