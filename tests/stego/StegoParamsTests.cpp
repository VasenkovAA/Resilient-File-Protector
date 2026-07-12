
#include "rfp/stego/StegoParams.h"
#include <gtest/gtest.h>

TEST(StegoParamsTests, DefaultValues) {
  rfp::stego::StegoParams params;
  EXPECT_EQ(params.bitsPerChannel, 1);
  EXPECT_EQ(params.seed, 0);
  EXPECT_TRUE(params.useRedChannel);
  EXPECT_TRUE(params.useGreenChannel);
  EXPECT_TRUE(params.useBlueChannel);
  EXPECT_FALSE(params.useAlphaChannel);
  EXPECT_EQ(params.mode, rfp::stego::SlotSelectionMode::Uniform);
  EXPECT_EQ(params.windowSize, 3);
  EXPECT_EQ(params.metric, rfp::stego::DispersionMetric::Luminance);
  EXPECT_DOUBLE_EQ(params.dispersionThreshold, 0.0);
  EXPECT_TRUE(params.applyShuffleAfterSort);
}