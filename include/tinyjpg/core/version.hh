#pragma once

#include <string_view>

namespace tinyjpg {

struct VersionInfo {
  std::string_view project_name;
  std::string_view version;
};

[[nodiscard]] VersionInfo version_info() noexcept;
[[nodiscard]] std::string_view version_string() noexcept;

}  // namespace tinyjpg
