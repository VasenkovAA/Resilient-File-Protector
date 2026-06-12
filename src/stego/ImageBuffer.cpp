#include "rfp/stego/ImageBuffer.h"

namespace rfp::stego {

std::size_t ImageBuffer::byteSize() const noexcept
{
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * static_cast<std::size_t>(channels);
}

bool ImageBuffer::isValid() const noexcept
{
    if (width == 0 || height == 0) {
        return false;
    }
    if (channels < 3 || channels > 4) {
        return false;
    }
    return pixels.size() == byteSize();
}

} // namespace rfp::stego
