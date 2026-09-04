#include "rfp/core/Crc32.h"

#include <array>
#include <string_view>

namespace rfp::core {

namespace {
// CRC-32 polynomial (IEEE 802.3)
constexpr std::uint32_t CRC_POLYNOMIAL = 0xEDB88320U;

/**
 * @brief Generate CRC-32 lookup table at compile time
 */
consteval std::array<std::uint32_t, 256> makeCrcTable() {
  std::array<std::uint32_t, 256> table{};
  for (std::uint32_t i = 0; i < table.size(); ++i) {
    std::uint32_t value = i;
    for (int bit = 0; bit < 8; ++bit) {
      if ((value & 1U) != 0U) {
        value = (value >> 1U) ^ CRC_POLYNOMIAL;
      } else {
        value >>= 1U;
      }
    }
    table[i] = value;
  }
  return table;
}

// Compile-time generated lookup table
constexpr auto CRC_TABLE = makeCrcTable();

} // anonymous namespace

std::uint32_t crc32(std::span<const Byte> data) noexcept {
  std::uint32_t crc = 0xFFFFFFFFU;

  for (const Byte byte : data) {
    const std::uint8_t index = static_cast<std::uint8_t>((crc ^ byte) & 0xFFU);
    crc = (crc >> 8U) ^ CRC_TABLE[index];
  }

  return crc ^ 0xFFFFFFFFU;
}

std::uint32_t crc32(std::string_view text) noexcept {
  const auto *ptr = reinterpret_cast<const Byte *>(text.data());
  return crc32(std::span<const Byte>(ptr, text.size()));
}

} // namespace rfp::core