#pragma once

#include "rfp/core/Result.h"
#include "rfp/stego/ImageBuffer.h"

#include <QString>

#include <cstddef>

namespace rfp::gui {

/// Load a raster image from disk into an ImageBuffer.
///
/// Supported formats are whatever the current Qt build provides, but only
/// 8-bit-per-channel RGB/RGBA inputs are meaningful for LSB steganography.
/// Images without an alpha channel are loaded as 3-channel RGB; images with
/// an alpha channel are loaded as 4-channel RGBA (even if fully opaque —
/// transparency is preserved verbatim).
///
/// Error codes:
///   - InvalidArgument  : path is empty
///   - IoError          : file missing / unreadable / not a known image
///   - InvalidImageBuffer : image has zero size or could not be converted
///                          to a suitable pixel format
[[nodiscard]] rfp::core::Result<rfp::stego::ImageBuffer>
loadImageBuffer(const QString &path);

/// Save an ImageBuffer to disk using the format inferred from the file
/// extension (PNG / BMP / TIFF / PPM / PGM). Lossy formats (JPEG) are
/// rejected because they destroy LSB payloads.
///
/// Error codes:
///   - InvalidArgument    : empty path or a lossy/unsupported extension
///   - InvalidImageBuffer : buffer is not valid
///   - IoError            : the file could not be written
[[nodiscard]] rfp::core::Result<void>
saveImageBuffer(const rfp::stego::ImageBuffer &buffer, const QString &path);

} // namespace rfp::gui