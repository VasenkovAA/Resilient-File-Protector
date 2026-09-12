#include "rfp/crypto/Encoding.h"
#include <openssl/evp.h>

namespace rfp::crypto {

std::string base64Encode(std::span<const Byte> data)
{
    if (data.empty()) return {};
    std::string out(4 * ((data.size() + 2) / 3), '\0');
    const int written = EVP_EncodeBlock(
        reinterpret_cast<unsigned char*>(out.data()),
        data.data(), static_cast<int>(data.size()));
    // GCOVR_EXCL_START — EVP_EncodeBlock не возвращает <0 для валидного входа
    if (written < 0) return {};
    // GCOVR_EXCL_STOP
    out.resize(static_cast<std::size_t>(written));
    return out;
}

ByteBuffer base64Decode(std::string_view text)
{
    if (text.empty()) return {};
    ByteBuffer out(3 * (text.size() / 4));
    const int written = EVP_DecodeBlock(
        out.data(),
        reinterpret_cast<const unsigned char*>(text.data()),
        static_cast<int>(text.size()));
    if (written < 0) return {};
    std::size_t pad = 0;
    if (text.back() == '=') ++pad;
    if (text.size() > 1 && text[text.size()-2] == '=') ++pad;
    out.resize(static_cast<std::size_t>(written) - pad);
    return out;
}

std::string hexEncode(std::span<const Byte> data)
{
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(data.size() * 2);
    for (Byte b : data) { out.push_back(kHex[b >> 4]); out.push_back(kHex[b & 0x0F]); }
    return out;
}

ByteBuffer hexDecode(std::string_view text)
{
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    if (text.size() % 2) return {};
    ByteBuffer out(text.size() / 2);
    for (std::size_t i = 0; i < out.size(); ++i) {
        int hi = nib(text[2*i]), lo = nib(text[2*i+1]);
        if (hi < 0 || lo < 0) return {};
        out[i] = static_cast<Byte>((hi << 4) | lo);
    }
    return out;
}

} // namespace rfp::crypto