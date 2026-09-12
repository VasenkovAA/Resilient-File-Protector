#pragma once

#include "rfp/crypto/CryptoTypes.h"
#include "rfp/crypto/Encoding.h"
#include "rfp/core/ByteBuffer.h"

#include <cassert>
#include <string>
#include <string_view>

namespace rfp::crypto::test {

/// Hex string -> ByteBuffer. Crashes on invalid hex — only for use in tests
/// where the input is a hard-coded, known-good literal.
inline rfp::core::ByteBuffer fromHex(std::string_view hex) {
    auto out = hexDecode(hex);
    assert(out.size() == hex.size() / 2 && "Bad hex literal in test");
    return out;
}

inline std::string toHex(const rfp::core::ByteBuffer& b) {
    return hexEncode(b);
}

inline rfp::core::ByteBuffer bytesOf(std::string_view s) {
    return rfp::core::ByteBuffer(s.begin(), s.end());
}

/// Convenience: zero-filled buffer of n bytes.
inline rfp::core::ByteBuffer zeros(std::size_t n) {
    return rfp::core::ByteBuffer(n, 0);
}

} // namespace rfp::crypto::test