#pragma once

#include "rfp/crypto/CryptoTypes.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace rfp::crypto {

/// Read-only registry of algorithms known to the current backend.
/// Thread-safe: all methods are pure (no mutation).
class CryptoRegistry {
public:
    // Lists (hard-coded specs; backend is queried at runtime for support).
    [[nodiscard]] static std::span<const CipherSpec> availableCiphers() noexcept;
    [[nodiscard]] static std::span<const KdfSpec>    availableKdfs()    noexcept;
    [[nodiscard]] static std::span<const HashSpec>   availableHashes()  noexcept;

    // Lookup by enum.
    [[nodiscard]] static std::optional<CipherSpec> cipherSpec(CipherId id) noexcept;
    [[nodiscard]] static std::optional<KdfSpec>    kdfSpec(KdfId id)       noexcept;
    [[nodiscard]] static std::optional<HashSpec>   hashSpec(HashId id)     noexcept;

    // Lookup by canonical name (stable string, safe for CLI/JSON).
    [[nodiscard]] static std::optional<CipherId> cipherByName(std::string_view name) noexcept;
    [[nodiscard]] static std::optional<KdfId>    kdfByName(std::string_view name)    noexcept;
    [[nodiscard]] static std::optional<HashId>   hashByName(std::string_view name)   noexcept;

    // Backend identity — used by UI and logs.
    [[nodiscard]] static std::string backendName()    noexcept;  // "OpenSSL"
    [[nodiscard]] static std::string backendVersion() noexcept;  // "OpenSSL 3.0.13 ..."
};

} // namespace rfp::crypto