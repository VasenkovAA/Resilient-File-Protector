#include "rfp/crypto/CryptoPlaceholder.h"

namespace rfp::crypto {

rfp::core::Error moduleStatus()
{
    return rfp::core::Error{
        rfp::core::ErrorCode::NotImplemented,
        "Encryption module is reserved for stage 3"
    };
}

} // namespace rfp::crypto
