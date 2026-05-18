#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "tinyjpg/core/error.hh"
#include "tinyjpg/core/types.hh"

namespace tinyjpg {

enum class LogLevel {
  trace,
  debug,
  info,
  warn,
  error,
};

enum class OnExist {
  skip,
  overwrite,
  version,
};

struct GeneralConfig {
  NonNegativeInt workers;
  LogLevel log_level;
  std::optional<std::filesystem::path> log_file;
  PositiveInt queue_capacity;
  NonNegativeInt stable_wait_ms;
  bool dry_run;
};

struct WatchConfig {
  std::vector<std::filesystem::path> paths;
  bool recursive;
  std::vector<std::string> include;
  std::vector<std::string> exclude;
  std::vector<std::string> prefix;
};

struct CompressionConfig {
  FidelityMode mode;
  EffortLevel effort;
  bool keep_metadata;
  bool skip_if_not_smaller;
  bool preserve_original;
};

struct VariantConfig {
  VariantName name;
  Codec codec;
  std::optional<FidelityMode> mode;
  std::optional<Quality> quality;
  std::optional<PositiveInt> max_width;
  std::optional<PositiveInt> max_height;
  FitMode fit;
  std::string suffix;
};

struct OutputConfig {
  std::string pattern;
  std::optional<std::filesystem::path> directory;
  OnExist on_exist;
};

struct AppConfig {
  GeneralConfig general;
  WatchConfig watch;
  CompressionConfig compress;
  std::vector<VariantConfig> variants;
  OutputConfig output;
};

[[nodiscard]] Result<LogLevel> parse_log_level(std::string_view value);
[[nodiscard]] Result<OnExist> parse_on_exist(std::string_view value);
[[nodiscard]] std::string_view to_string(LogLevel value) noexcept;
[[nodiscard]] std::string_view to_string(OnExist value) noexcept;

[[nodiscard]] Result<AppConfig> default_config();
[[nodiscard]] Result<void> validate_config(const AppConfig& config);
[[nodiscard]] std::vector<std::string_view> preset_names();
[[nodiscard]] Result<std::vector<VariantConfig>> variant_preset(std::string_view name);

}  // namespace tinyjpg
