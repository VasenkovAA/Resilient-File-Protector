#pragma once

#include "rfp/core/Error.h"

#include <utility>
#include <variant>

namespace rfp::core {

template <typename T>
class Result {
public:
    Result(T value) : data_(std::move(value)) {}
    Result(Error error) : data_(std::move(error)) {}

    [[nodiscard]] bool hasValue() const noexcept
    {
        return std::holds_alternative<T>(data_);
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return hasValue();
    }

    [[nodiscard]] const T& value() const&
    {
        return std::get<T>(data_);
    }

    [[nodiscard]] T& value() &
    {
        return std::get<T>(data_);
    }

    [[nodiscard]] T&& value() &&
    {
        return std::move(std::get<T>(data_));
    }

    [[nodiscard]] const Error& error() const&
    {
        return std::get<Error>(data_);
    }

private:
    std::variant<T, Error> data_;
};

template <>
class Result<void> {
public:
    Result() = default;
    Result(Error error) : error_(std::move(error)) {}

    [[nodiscard]] bool hasValue() const noexcept
    {
        return error_.ok();
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return hasValue();
    }

    [[nodiscard]] const Error& error() const noexcept
    {
        return error_;
    }

private:
    Error error_{};
};

} // namespace rfp::core
