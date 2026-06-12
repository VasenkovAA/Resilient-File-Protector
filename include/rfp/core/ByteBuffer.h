#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace rfp::core {

using Byte = std::uint8_t;
using ByteBuffer = std::vector<Byte>;

ByteBuffer toBytes(std::string_view text);
std::string bytesToString(const ByteBuffer& bytes);

} // namespace rfp::core
