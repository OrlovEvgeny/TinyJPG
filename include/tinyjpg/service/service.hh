#pragma once

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <span>
#include <stop_token>
#include <vector>

#include "tinyjpg/core/config.hh"

namespace tinyjpg {

struct ServiceSummary {
  std::size_t files_seen;
  std::size_t files_processed;
  std::size_t files_skipped;
  std::size_t errors;
};

[[nodiscard]] Result<std::vector<std::filesystem::path>> collect_image_files(
    std::span<const std::filesystem::path> input_paths, const AppConfig& config);

[[nodiscard]] Result<ServiceSummary> scan_paths(std::span<const std::filesystem::path> input_paths,
                                                const AppConfig& config, std::ostream& out);

[[nodiscard]] Result<ServiceSummary> watch_paths(std::span<const std::filesystem::path> input_paths,
                                                 const AppConfig& config, std::ostream& out,
                                                 std::stop_token stop_token);

}  // namespace tinyjpg
