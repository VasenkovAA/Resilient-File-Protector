#pragma once

#include "rfp/crypto/CryptoTypes.h"
#include <span>

namespace rfp::crypto {

/// Zero a buffer in a way the compiler must not optimise out.
void secureWipe(std::span<Byte> buffer) noexcept;

/// Constant-time comparison; true iff same size and equal content.
[[nodiscard]] bool constantTimeEquals(std::span<const Byte> a,
                                      std::span<const Byte> b) noexcept;

} // namespace rfp::crypto