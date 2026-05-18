#pragma once

#include <span>
#include <string_view>

namespace tinyjpg::app {

enum class ExitCode : int {
  ok = 0,
  usage = 2,
};

[[nodiscard]] ExitCode run(std::span<char const* const> args);
[[nodiscard]] std::string_view usage_text() noexcept;

}  // namespace tinyjpg::app
