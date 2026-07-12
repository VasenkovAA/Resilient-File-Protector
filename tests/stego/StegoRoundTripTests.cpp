#include "rfp/core/ByteBuffer.h"
#include "rfp/stego/Capacity.h"
#include "rfp/stego/ImageBuffer.h"
#include "rfp/stego/StegoDecoder.h"
#include "rfp/stego/StegoEncoder.h"
#include "rfp/stego/StegoParams.h"

#include <gtest/gtest.h>

namespace {

rfp::stego::ImageBuffer makeImage(std::uint32_t width, std::uint32_t height) {
  rfp::stego::ImageBuffer image;
  image.width = width;
  image.height = height;
  image.channels = 4;
  image.pixels.assign(image.byteSize(), 0xAAU);
  return image;
}

} // namespace

TEST(StegoRoundTripTests, EmbedsAndExtractsTextSequentially) {
  auto image = makeImage(32, 32);
  rfp::stego::StegoParams params;
  params.bitsPerChannel = 1;

  const std::string text = "Resilient File Protector";
  auto encoded = rfp::stego::StegoEncoder::embedText(image, text, params);
  ASSERT_TRUE(encoded) << encoded.error().message;

  auto decoded = rfp::stego::StegoDecoder::extractBytes(encoded.value(),
                                                        text.size(), params);
  ASSERT_TRUE(decoded) << decoded.error().message;

  EXPECT_EQ(rfp::core::bytesToString(decoded.value()), text);
}

TEST(StegoRoundTripTests, EmbedsAndExtractsTextWithSeededDistribution) {
  auto image = makeImage(32, 32);
  rfp::stego::StegoParams params;
  params.bitsPerChannel = 1;
  params.seed = 1337;

  const std::string text = "seeded payload";
  auto encoded = rfp::stego::StegoEncoder::embedText(image, text, params);
  ASSERT_TRUE(encoded) << encoded.error().message;

  auto decoded = rfp::stego::StegoDecoder::extractBytes(encoded.value(),
                                                        text.size(), params);
  ASSERT_TRUE(decoded) << decoded.error().message;

  EXPECT_EQ(rfp::core::bytesToString(decoded.value()), text);
}

TEST(StegoRoundTripTests, ReportsCapacityError) {
  auto image = makeImage(1, 1);
  rfp::stego::StegoParams params;
  params.bitsPerChannel = 1;

  const std::string text = "too large";
  auto encoded = rfp::stego::StegoEncoder::embedText(image, text, params);

  ASSERT_FALSE(encoded);
  EXPECT_EQ(encoded.error().code, rfp::core::ErrorCode::CapacityExceeded);
}

TEST(StegoRoundTripTests, CalculatesCapacity) {
  auto image = makeImage(10, 10);
  rfp::stego::StegoParams params;
  params.bitsPerChannel = 1;

  EXPECT_EQ(rfp::stego::capacityBytes(image, params), 37U);
}
