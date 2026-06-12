#pragma once

#include "rfp/core/ByteBuffer.h"
#include "rfp/core/Result.h"
#include "rfp/stego/ImageBuffer.h"
#include "rfp/stego/StegoParams.h"

#include <cstddef>

namespace rfp::stego {

class StegoDecoder {
public:
    [[nodiscard]] static rfp::core::Result<rfp::core::ByteBuffer> extractBytes(
        const ImageBuffer& source,
        std::size_t payloadSizeBytes,
        const StegoParams& params);
};

} // namespace rfp::stego
