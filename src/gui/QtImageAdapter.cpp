#include "QtImageAdapter.h"

#include <cstring>

namespace rfp::gui {

rfp::core::Result<rfp::stego::ImageBuffer>
loadImageBuffer(const QString &path) {
  const QImage loaded(path);
  if (loaded.isNull()) {
    return rfp::core::Error{rfp::core::ErrorCode::IoError,
                            "Failed to load image"};
  }




  const bool hasAlpha = loaded.hasAlphaChannel();
  const QImage image = loaded.convertToFormat(hasAlpha ? QImage::Format_RGBA8888
                                                       : QImage::Format_RGB888);

  rfp::stego::ImageBuffer buffer;
  buffer.width = static_cast<std::uint32_t>(image.width());
  buffer.height = static_cast<std::uint32_t>(image.height());
  buffer.channels = static_cast<std::uint8_t>(hasAlpha ? 4U : 3U);
  buffer.pixels.resize(buffer.byteSize());

  const auto rowBytes = static_cast<std::size_t>(image.width()) *
                        static_cast<std::size_t>(buffer.channels);
  for (int y = 0; y < image.height(); ++y) {
    const auto *src = image.constScanLine(y);
    auto *dst = buffer.pixels.data() + static_cast<std::size_t>(y) * rowBytes;
    std::memcpy(dst, src, rowBytes);
  }

  return buffer;
}

rfp::core::Result<void> saveImageBuffer(const rfp::stego::ImageBuffer &buffer,
                                        const QString &path) {
  if (!buffer.isValid()) {
    return rfp::core::Error{rfp::core::ErrorCode::InvalidImageBuffer,
                            "Invalid image buffer"};
  }

  const QImage::Format format =
      buffer.channels == 4 ? QImage::Format_RGBA8888 : QImage::Format_RGB888;
  QImage image(static_cast<int>(buffer.width), static_cast<int>(buffer.height),
               format);
  const auto rowBytes = static_cast<std::size_t>(buffer.width) *
                        static_cast<std::size_t>(buffer.channels);

  for (int y = 0; y < image.height(); ++y) {
    const auto *src =
        buffer.pixels.data() + static_cast<std::size_t>(y) * rowBytes;
    auto *dst = image.scanLine(y);
    std::memcpy(dst, src, rowBytes);
  }

  if (!image.save(path)) {
    return rfp::core::Error{rfp::core::ErrorCode::IoError,
                            "Failed to save image"};
  }

  return {};
}

}
