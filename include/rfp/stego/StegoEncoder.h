#pragma once

#include "rfp/core/ByteBuffer.h"
#include "rfp/core/Result.h"
#include "rfp/stego/ImageBuffer.h"
#include "rfp/stego/StegoParams.h"

#include <string_view>

namespace rfp::stego {

class StegoEncoder {
public:
    [[nodiscard]] static rfp::core::Result<ImageBuffer> embedBytes(
        const ImageBuffer& source,
        const rfp::core::ByteBuffer& payload,
        const StegoParams& params);

    [[nodiscard]] static rfp::core::Result<ImageBuffer> embedText(
        const ImageBuffer& source,
        std::string_view text,
        const StegoParams& params);
};

} // namespace rfp::stego
