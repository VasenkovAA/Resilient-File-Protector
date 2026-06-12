#pragma once

#include <string>

namespace rfp::core {

enum class ErrorCode {
    None = 0,
    InvalidArgument,
    CapacityExceeded,
    InvalidImageBuffer,
    UnsupportedFormat,
    IoError,
    DecodeError,
    NotImplemented
};

struct Error {
    ErrorCode code = ErrorCode::None;
    std::string message;

    [[nodiscard]] bool ok() const noexcept { return code == ErrorCode::None; }
};

} // namespace rfp::core
