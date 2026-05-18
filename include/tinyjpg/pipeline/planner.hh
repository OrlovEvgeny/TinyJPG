#pragma once

#include <filesystem>
#include <vector>

#include "tinyjpg/core/config.hh"

namespace tinyjpg {

struct SourceImageInfo {
  PositiveInt width;
  PositiveInt height;
  Codec codec;
};

struct PlannedVariant {
  VariantConfig variant;
  std::filesystem::path output_path;
  Codec codec;
  Quality quality;
  PositiveInt width;
  PositiveInt height;
  bool resized;
};

[[nodiscard]] Result<std::vector<PlannedVariant>> plan_variants(
    const std::filesystem::path& input_path, SourceImageInfo source, const AppConfig& config);

}  // namespace tinyjpg
