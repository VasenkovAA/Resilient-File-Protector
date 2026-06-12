#pragma once

#include "rfp/core/ByteBuffer.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace rfp::core {

std::uint32_t crc32(std::span<const Byte> data) noexcept;
std::uint32_t crc32(std::string_view text) noexcept;

} // namespace rfp::core
