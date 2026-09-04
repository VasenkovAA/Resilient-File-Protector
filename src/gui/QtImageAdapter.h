#pragma once

#include "rfp/core/Result.h"
#include "rfp/stego/ImageBuffer.h"
#include <QImage>
#include <QString>

namespace rfp::gui {

[[nodiscard]] rfp::core::Result<rfp::stego::ImageBuffer>
loadImageBuffer(const QString &path);
[[nodiscard]] rfp::core::Result<void>
saveImageBuffer(const rfp::stego::ImageBuffer &buffer, const QString &path);

}