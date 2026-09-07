#pragma once

#include <optional>
#include <string>
#include <utility>

namespace home {

enum class ErrorCode {
    None = 0,
    InvalidArgument,
    NotFound,
    AlreadyExists,
    RevisionConflict,
    ValidationFailed,
    Overflow,
    SerializationError,
    InternalError
};

struct Error final {
    ErrorCode code{ErrorCode::None};
    std::string message{};

    [[nodiscard]] bool ok() const noexcept { return code == ErrorCode::None; }
};

template <typename T>
class Result final {
public:
    static Result success(T value) { return Result{std::move(value), Error{}}; }
    static Result failure(ErrorCode code, std::string message) {
        return Result{std::nullopt, Error{code, std::move(message)}};
    }

    [[nodiscard]] bool ok() const noexcept { return value_.has_value(); }
    explicit operator bool() const noexcept { return ok(); }
    [[nodiscard]] const T& value() const& { return value_.value(); }
    [[nodiscard]] T&& value() && { return std::move(value_.value()); }
    [[nodiscard]] const Error& error() const noexcept { return error_; }

private:
    Result(T value, Error error) : value_(std::move(value)), error_(std::move(error)) {}
    Result(std::nullopt_t, Error error) : value_(std::nullopt), error_(std::move(error)) {}

    std::optional<T> value_;
    Error error_;
};

template <>
class Result<void> final {
public:
    static Result success() { return Result{Error{}}; }
    static Result failure(ErrorCode code, std::string message) {
        return Result{Error{code, std::move(message)}};
    }

    [[nodiscard]] bool ok() const noexcept { return error_.ok(); }
    explicit operator bool() const noexcept { return ok(); }
    [[nodiscard]] const Error& error() const noexcept { return error_; }

private:
    explicit Result(Error error) : error_(std::move(error)) {}
    Error error_;
};

} // namespace home
