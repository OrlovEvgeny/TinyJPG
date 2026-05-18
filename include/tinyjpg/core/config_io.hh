#pragma once

#include <filesystem>
#include <string>

#include "tinyjpg/core/config.hh"
#include "tinyjpg/core/error.hh"

namespace tinyjpg {

[[nodiscard]] Result<AppConfig> load_toml_config(const std::filesystem::path& path);
[[nodiscard]] Result<void> validate_config_file(const std::filesystem::path& path);
[[nodiscard]] Result<std::string> render_default_config();

}  // namespace tinyjpg
