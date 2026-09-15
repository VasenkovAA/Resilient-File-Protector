#pragma once

#include "rfp/core/Result.h"
#include "rfp/stego/ImageBuffer.h"

#include <string>

namespace rfp::cli::png {

[[nodiscard]] rfp::core::Result<rfp::stego::ImageBuffer>
load(const std::string& path);

[[nodiscard]] rfp::core::Result<void>
save(const rfp::stego::ImageBuffer& image, const std::string& path);

} // namespace rfp::cli::png
