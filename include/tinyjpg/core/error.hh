#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

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

template <typename T>
using Result = std::expected<T, Error>;

}  // namespace tinyjpg
