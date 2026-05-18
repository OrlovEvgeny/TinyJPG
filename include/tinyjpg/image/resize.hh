#pragma once

#include <optional>

#include "tinyjpg/core/config.hh"
#include "tinyjpg/image/image.hh"

namespace tinyjpg {

struct ResizePlan {
  PositiveInt width;
  PositiveInt height;
  bool resized;
};

[[nodiscard]] Result<ResizePlan> plan_resize(PositiveInt source_width, PositiveInt source_height,
                                             std::optional<PositiveInt> max_width,
                                             std::optional<PositiveInt> max_height, FitMode fit);
[[nodiscard]] Result<Image> resize_image(const Image& image, std::optional<PositiveInt> max_width,
                                         std::optional<PositiveInt> max_height, FitMode fit);

}  // namespace tinyjpg
