#include "rfp/crypto/CryptoPlaceholder.h"
#include <openssl/opensslv.h>
#include <string>

namespace rfp::crypto {

rfp::core::Error moduleStatus()
{
    // OPENSSL_VERSION_TEXT already starts with "OpenSSL" — don't duplicate it.
    return rfp::core::Error{
        rfp::core::ErrorCode::None,
        std::string(OPENSSL_VERSION_TEXT)
    };
}

} // namespace rfp::crypto