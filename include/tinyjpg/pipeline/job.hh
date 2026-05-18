#pragma once

#include <cstdint>
#include <filesystem>

#include "tinyjpg/core/config.hh"

namespace tinyjpg {

struct ProcessResult {
  std::filesystem::path input_path;
  std::filesystem::path output_path;
  Codec codec;
  std::uintmax_t bytes_before;
  std::uintmax_t bytes_after;
  bool written;
};

[[nodiscard]] Result<ProcessResult> process_file(const std::filesystem::path& input_path,
                                                 const AppConfig& config);

}  // namespace tinyjpg
