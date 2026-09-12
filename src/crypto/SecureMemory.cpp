#include "rfp/crypto/SecureMemory.h"
#include <openssl/crypto.h>

namespace rfp::crypto {

void secureWipe(std::span<Byte> buffer) noexcept
{
    if (!buffer.empty()) OPENSSL_cleanse(buffer.data(), buffer.size());
}

bool constantTimeEquals(std::span<const Byte> a, std::span<const Byte> b) noexcept
{
    if (a.size() != b.size()) return false;
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

} // namespace rfp::crypto