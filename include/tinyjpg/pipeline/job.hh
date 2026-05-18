#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

#include "tinyjpg/core/config.hh"

namespace tinyjpg {

enum class ProcessSkipReason {
  none,
  not_smaller,
  exists,
  dry_run,
};

struct ProcessVariantResult {
  VariantName variant_name;
  std::filesystem::path output_path;
  Codec codec;
  std::uintmax_t bytes_after;
  PositiveInt width;
  PositiveInt height;
  bool written;
  ProcessSkipReason skip_reason;
};

struct ProcessResult {
  std::filesystem::path input_path;
  std::uintmax_t bytes_before;
  std::vector<ProcessVariantResult> variants;
};

[[nodiscard]] Result<ProcessResult> process_file(const std::filesystem::path& input_path,
                                                 const AppConfig& config);
[[nodiscard]] std::string_view to_string(ProcessSkipReason value) noexcept;

}  // namespace tinyjpg
