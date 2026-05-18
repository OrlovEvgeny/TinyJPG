#pragma once

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace tinyjpg {

enum class ErrorCode {
  invalid_argument,
  config,
  filesystem,
  unsupported,
  internal,
};

struct Error {
  ErrorCode code;
  std::string message;
  std::optional<std::filesystem::path> path;

  [[nodiscard]] static Error invalid_argument(std::string message) {
    return Error{
        .code = ErrorCode::invalid_argument,
        .message = std::move(message),
        .path = std::nullopt,
    };
  }

  [[nodiscard]] static Error config(std::string message) {
    return Error{
        .code = ErrorCode::config,
        .message = std::move(message),
        .path = std::nullopt,
    };
  }

  [[nodiscard]] static Error filesystem(std::filesystem::path path, std::string message) {
    return Error{
        .code = ErrorCode::filesystem,
        .message = std::move(message),
        .path = std::move(path),
    };
  }
};

struct Unexpected {
  Error error;
};

[[nodiscard]] inline Unexpected unexpected(Error error) {
  return Unexpected{.error = std::move(error)};
}

template <typename T>
class Result {
 public:
  Result(const T& value) : storage_{value} {}
  Result(T&& value) : storage_{std::move(value)} {}
  Result(Unexpected error) : storage_{std::move(error.error)} {}

  [[nodiscard]] bool has_value() const noexcept { return std::holds_alternative<T>(storage_); }
  [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }

  [[nodiscard]] T& value() & {
    if (!has_value()) {
      throw std::logic_error{"accessed error Result value"};
    }
    return std::get<T>(storage_);
  }

  [[nodiscard]] const T& value() const& {
    if (!has_value()) {
      throw std::logic_error{"accessed error Result value"};
    }
    return std::get<T>(storage_);
  }

  [[nodiscard]] T&& value() && {
    if (!has_value()) {
      throw std::logic_error{"accessed error Result value"};
    }
    return std::get<T>(std::move(storage_));
  }

  [[nodiscard]] Error& error() & { return std::get<Error>(storage_); }
  [[nodiscard]] const Error& error() const& { return std::get<Error>(storage_); }

  [[nodiscard]] T& operator*() & { return value(); }
  [[nodiscard]] const T& operator*() const& { return value(); }
  [[nodiscard]] T&& operator*() && { return std::move(*this).value(); }

  [[nodiscard]] T* operator->() { return &value(); }
  [[nodiscard]] const T* operator->() const { return &value(); }

 private:
  std::variant<T, Error> storage_;
};

template <>
class Result<void> {
 public:
  Result() = default;
  Result(Unexpected error) : storage_{std::move(error.error)} {}

  [[nodiscard]] bool has_value() const noexcept {
    return std::holds_alternative<std::monostate>(storage_);
  }

  [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }

  void value() const {
    if (!has_value()) {
      throw std::logic_error{"accessed error Result value"};
    }
  }

  [[nodiscard]] Error& error() & { return std::get<Error>(storage_); }
  [[nodiscard]] const Error& error() const& { return std::get<Error>(storage_); }

 private:
  std::variant<std::monostate, Error> storage_{std::monostate{}};
};

}  // namespace tinyjpg
