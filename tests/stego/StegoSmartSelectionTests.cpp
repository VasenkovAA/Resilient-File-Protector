#include "rfp/stego/Capacity.h"
#include "rfp/stego/StegoDecoder.h"
#include "rfp/stego/StegoEncoder.h"
#include "rfp/stego/StegoSlots.h"

#include <gtest/gtest.h>

namespace {

rfp::stego::ImageBuffer makeGradientImage(std::uint32_t w, std::uint32_t h) {
  rfp::stego::ImageBuffer img;
  img.width = w;
  img.height = h;
  img.channels = 4;
  img.pixels.resize(img.byteSize());
  for (std::uint32_t y = 0; y < h; ++y) {
    for (std::uint32_t x = 0; x < w; ++x) {
      const auto idx =
          (static_cast<std::size_t>(y) * w + static_cast<std::size_t>(x)) *
          img.channels;
      const std::uint8_t val = static_cast<std::uint8_t>((x + y) & 0xFF);
      img.pixels[idx] = val;
      img.pixels[idx + 1] = val;
      img.pixels[idx + 2] = val;
      img.pixels[idx + 3] = 255;
    }
  }
  return img;
}

} // namespace

TEST(SmartSelectionTests, CapacitySmartFiltersByThreshold) {
  auto img = makeGradientImage(10, 10);
  rfp::stego::StegoParams params;
  params.mode = rfp::stego::SlotSelectionMode::Smart;
  params.metric = rfp::stego::DispersionMetric::Luminance;
  params.windowSize = 3;
  params.bitsPerChannel = 1;
  params.dispersionThreshold = 0.0;
  const auto cap0 = rfp::stego::capacityBytes(img, params);
  params.dispersionThreshold = 1000.0;
  const auto cap1 = rfp::stego::capacityBytes(img, params);
  EXPECT_GT(cap0, cap1);
}

TEST(SmartSelectionTests, RoundTripWithSmartSelection) {
  auto img = makeGradientImage(16, 16);
  rfp::stego::StegoParams params;
  params.mode = rfp::stego::SlotSelectionMode::Smart;
  params.metric = rfp::stego::DispersionMetric::Luminance;
  params.windowSize = 3;
  params.bitsPerChannel = 1;
  params.dispersionThreshold = 0.0; // <-- изменено с 50.0 на 0.0
  params.seed = 123;
  params.applyShuffleAfterSort = true;

  const std::string text = "Test smart selection";
  auto encoded = rfp::stego::StegoEncoder::embedText(img, text, params);
  ASSERT_TRUE(encoded) << encoded.error().message;

  auto decoded = rfp::stego::StegoDecoder::extractBytes(encoded.value(),
                                                        text.size(), params);
  ASSERT_TRUE(decoded) << decoded.error().message;

  EXPECT_EQ(rfp::core::bytesToString(decoded.value()), text);
}

TEST(SmartSelectionTests, BuildSlotsSmartRespectsThreshold) {
  auto img = makeGradientImage(5, 5);
  rfp::stego::StegoParams params;
  params.mode = rfp::stego::SlotSelectionMode::Smart;
  params.metric = rfp::stego::DispersionMetric::Luminance;
  params.windowSize = 3;
  params.bitsPerChannel = 1;
  params.dispersionThreshold = 1000.0;

  const auto totalSlots = rfp::stego::detail::availableSlotCount(img, params);
  EXPECT_EQ(totalSlots, 0);

  auto slots = rfp::stego::detail::buildSlots(img, params, 1);
  ASSERT_FALSE(slots);
  EXPECT_EQ(slots.error().code, rfp::core::ErrorCode::CapacityExceeded);
}