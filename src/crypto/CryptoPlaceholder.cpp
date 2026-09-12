#include "rfp/crypto/CryptoPlaceholder.h"
#include <openssl/opensslv.h>
#include <string>

namespace rfp::crypto {

rfp::core::Error moduleStatus()
{
    return rfp::core::Error{
        rfp::core::ErrorCode::None,
        std::string("OpenSSL ") + OPENSSL_VERSION_TEXT
    };
}

} // namespace rfp::crypto