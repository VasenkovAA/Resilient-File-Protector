#include "rfp/stego/Capacity.h"

#include "StegoSlots.h"

namespace rfp::stego {

std::size_t capacityBits(const ImageBuffer& image, const StegoParams& params) noexcept
{
    return detail::availableSlotCount(image, params);
}

std::size_t capacityBytes(const ImageBuffer& image, const StegoParams& params) noexcept
{
    return capacityBits(image, params) / 8U;
}

} // namespace rfp::stego
