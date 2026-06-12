#pragma once

#include <cstdint>

namespace rfp::stego {

struct StegoParams {
    std::uint8_t bitsPerChannel = 1;
    std::uint32_t seed = 0;
    bool useRedChannel = true;
    bool useGreenChannel = true;
    bool useBlueChannel = true;
    bool useAlphaChannel = false;
};

} // namespace rfp::stego
