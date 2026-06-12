#include "rfp/core/Crc32.h"

#include <array>

namespace rfp::core {
namespace {

constexpr std::uint32_t polynomial = 0xEDB88320U;

consteval std::array<std::uint32_t, 256> makeTable()
{
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < table.size(); ++i) {
        std::uint32_t value = i;
        for (int bit = 0; bit < 8; ++bit) {
            if ((value & 1U) != 0U) {
                value = (value >> 1U) ^ polynomial;
            } else {
                value >>= 1U;
            }
        }
        table[i] = value;
    }
    return table;
}

constexpr auto table = makeTable();

} // namespace

std::uint32_t crc32(std::span<const Byte> data) noexcept
{
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const Byte byte : data) {
        const auto index = static_cast<std::uint8_t>((crc ^ byte) & 0xFFU);
        crc = (crc >> 8U) ^ table[index];
    }
    return crc ^ 0xFFFFFFFFU;
}

std::uint32_t crc32(std::string_view text) noexcept
{
    const auto* ptr = reinterpret_cast<const Byte*>(text.data());
    return crc32(std::span<const Byte>(ptr, text.size()));
}

} // namespace rfp::core
