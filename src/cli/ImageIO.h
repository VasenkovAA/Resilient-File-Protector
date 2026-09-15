#pragma once

#include "rfp/core/Result.h"
#include "rfp/stego/ImageBuffer.h"

#include <string>

namespace rfp::cli {

/// Load image. Detects format by magic bytes:
///   - PNG  (89 50 4E 47)   -> libpng
///   - PNM  (P5 / P6)       -> built-in parser
[[nodiscard]] rfp::core::Result<rfp::stego::ImageBuffer>
loadImage(const std::string& path);

/// Save image. Format is chosen by file extension:
///   - ".png"             -> PNG
///   - ".ppm" / ".pgm"    -> PNM (P6)
///   - anything else      -> PNG
[[nodiscard]] rfp::core::Result<void>
saveImage(const rfp::stego::ImageBuffer& image, const std::string& path);

} // namespace rfp::cli
