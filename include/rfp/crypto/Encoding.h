#pragma once

#include "rfp/crypto/CryptoTypes.h"
#include <span>
#include <string>
#include <string_view>

namespace rfp::crypto {

[[nodiscard]] std::string base64Encode(std::span<const Byte> data);
[[nodiscard]] ByteBuffer  base64Decode(std::string_view text);
[[nodiscard]] std::string hexEncode(std::span<const Byte> data);
[[nodiscard]] ByteBuffer  hexDecode(std::string_view text);

} // namespace rfp::crypto