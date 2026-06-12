#include "rfp/core/ByteBuffer.h"

#include <string>

namespace rfp::core {

ByteBuffer toBytes(std::string_view text)
{
    return ByteBuffer(text.begin(), text.end());
}

std::string bytesToString(const ByteBuffer& bytes)
{
    return std::string(bytes.begin(), bytes.end());
}

} // namespace rfp::core
