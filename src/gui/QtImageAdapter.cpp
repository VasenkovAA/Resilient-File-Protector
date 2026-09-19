#include "QtImageAdapter.h"

#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QStringList>

#include <cstring>
#include <limits>

namespace rfp::gui {
namespace {

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

// Extensions we accept for writing. Everything here is lossless.
const QStringList kLosslessWriteSuffixes = {
    QStringLiteral("png"),  QStringLiteral("bmp"), QStringLiteral("tif"),
    QStringLiteral("tiff"), QStringLiteral("ppm"), QStringLiteral("pgm"),
    QStringLiteral("pnm"),
};

// Extensions we explicitly refuse to write, with a user-friendly reason.
bool isLossySuffix(const QString &suffix) {
  return suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg") ||
         suffix == QLatin1String("webp") || suffix == QLatin1String("heic") ||
         suffix == QLatin1String("avif");
}

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

// Copy a QImage (already in RGB888 / RGBA8888) into an ImageBuffer.
rfp::stego::ImageBuffer qImageToBuffer(const QImage &image) {
  const bool hasAlpha = (image.format() == QImage::Format_RGBA8888);
  const std::size_t channels = hasAlpha ? 4u : 3u;

  rfp::stego::ImageBuffer buffer;
  buffer.width = static_cast<std::uint32_t>(image.width());
  buffer.height = static_cast<std::uint32_t>(image.height());
  buffer.channels = static_cast<std::uint8_t>(channels);
  buffer.pixels.resize(buffer.byteSize());

  const std::size_t rowBytes =
      static_cast<std::size_t>(image.width()) * channels;
  const auto *base = image.constBits();

  for (int y = 0; y < image.height(); ++y) {
    const auto *src =
        base + static_cast<std::ptrdiff_t>(y) * image.bytesPerLine();
    auto *dst = buffer.pixels.data() + static_cast<std::size_t>(y) * rowBytes;
    std::memcpy(dst, src, rowBytes);
  }
  return buffer;
}

// Copy an ImageBuffer into a QImage in the corresponding format.
QImage bufferToQImage(const rfp::stego::ImageBuffer &buffer) {
  const QImage::Format format =
      (buffer.channels == 4) ? QImage::Format_RGBA8888 : QImage::Format_RGB888;

  QImage image(static_cast<int>(buffer.width), static_cast<int>(buffer.height),
               format);
  if (image.isNull()) {
    return QImage();
  }

  const std::size_t rowBytes =
      static_cast<std::size_t>(buffer.width) * buffer.channels;

  for (int y = 0; y < image.height(); ++y) {
    const auto *src =
        buffer.pixels.data() + static_cast<std::size_t>(y) * rowBytes;
    auto *dst = image.scanLine(y);
    std::memcpy(dst, src, rowBytes);
  }
  return image;
}

} // namespace

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------

rfp::core::Result<rfp::stego::ImageBuffer>
loadImageBuffer(const QString &path) {
  if (path.isEmpty()) {
    return rfp::core::Error{rfp::core::ErrorCode::InvalidArgument,
                            "Image path is empty"};
  }

  QImageReader reader(path);
  reader.setDecideFormatFromContent(true);
  const QImage loaded = reader.read();

  if (loaded.isNull()) {
    return rfp::core::Error{rfp::core::ErrorCode::IoError,
                            QStringLiteral("Failed to load image '%1': %2")
                                .arg(path, reader.errorString())
                                .toStdString()};
  }
  if (loaded.width() <= 0 || loaded.height() <= 0) {
    return rfp::core::Error{rfp::core::ErrorCode::InvalidImageBuffer,
                            "Image has zero width or height"};
  }

  // Guard against absurdly large images that would overflow size_t on
  // 32-bit builds.
  constexpr std::size_t kMaxPixels =
      std::numeric_limits<std::size_t>::max() / 4; // 4 = max channels
  const std::size_t pixelCount = static_cast<std::size_t>(loaded.width()) *
                                 static_cast<std::size_t>(loaded.height());
  if (pixelCount > kMaxPixels) {
    return rfp::core::Error{rfp::core::ErrorCode::InvalidImageBuffer,
                            "Image is too large to process"};
  }

  const bool hasAlpha = loaded.hasAlphaChannel();
  const QImage::Format targetFormat =
      hasAlpha ? QImage::Format_RGBA8888 : QImage::Format_RGB888;

  const QImage converted = loaded.convertToFormat(targetFormat);
  if (converted.isNull()) {
    return rfp::core::Error{rfp::core::ErrorCode::InvalidImageBuffer,
                            "Failed to convert image to RGB888/RGBA8888"};
  }

  return qImageToBuffer(converted);
}

rfp::core::Result<void> saveImageBuffer(const rfp::stego::ImageBuffer &buffer,
                                        const QString &path) {
  if (!buffer.isValid()) {
    return rfp::core::Error{rfp::core::ErrorCode::InvalidImageBuffer,
                            "Invalid image buffer"};
  }
  if (path.isEmpty()) {
    return rfp::core::Error{rfp::core::ErrorCode::InvalidArgument,
                            "Output path is empty"};
  }

  const QString suffix = QFileInfo(path).suffix().toLower();
  if (suffix.isEmpty()) {
    return rfp::core::Error{rfp::core::ErrorCode::InvalidArgument,
                            "Output path has no extension"};
  }
  if (isLossySuffix(suffix)) {
    return rfp::core::Error{
        rfp::core::ErrorCode::InvalidArgument,
        QStringLiteral("'%1' is a lossy format and cannot preserve LSB "
                       "payloads. Use PNG instead.")
            .arg(suffix)
            .toStdString()};
  }
  if (!kLosslessWriteSuffixes.contains(suffix)) {
    return rfp::core::Error{rfp::core::ErrorCode::InvalidArgument,
                            QStringLiteral("Unsupported output format: '%1'")
                                .arg(suffix)
                                .toStdString()};
  }

  const QImage image = bufferToQImage(buffer);
  if (image.isNull()) {
    return rfp::core::Error{rfp::core::ErrorCode::InvalidImageBuffer,
                            "Failed to convert buffer to QImage"};
  }

  QImageWriter writer(path);
  writer.setFormat(suffix.toLatin1());
  if (!writer.write(image)) {
    return rfp::core::Error{rfp::core::ErrorCode::IoError,
                            QStringLiteral("Failed to save image '%1': %2")
                                .arg(path, writer.errorString())
                                .toStdString()};
  }
  return {};
}

} // namespace rfp::gui