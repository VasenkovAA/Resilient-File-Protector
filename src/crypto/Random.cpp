#include "rfp/crypto/Random.h"
#include <openssl/rand.h>

namespace rfp::crypto {

rfp::core::Result<ByteBuffer> Random::bytes(std::size_t count)
{
    ByteBuffer out(count);
    // GCOVR_EXCL_START — только при поломке libcrypto (RAND_bytes != 1)
    if (count && RAND_bytes(out.data(), static_cast<int>(count)) != 1)
        return rfp::core::Error{ rfp::core::ErrorCode::IoError, "RAND_bytes failed" };
    // GCOVR_EXCL_STOP
    return out;
}

rfp::core::Error Random::fill(std::span<Byte> out) noexcept
{
    if (out.empty()) return {};
    // GCOVR_EXCL_START
    if (RAND_bytes(out.data(), static_cast<int>(out.size())) != 1)
        return rfp::core::Error{ rfp::core::ErrorCode::IoError, "RAND_bytes failed" };
    // GCOVR_EXCL_STOP
    return {};
}

} // namespace rfp::crypto