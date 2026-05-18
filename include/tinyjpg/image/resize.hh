#pragma once

#include <optional>

#include "tinyjpg/core/config.hh"
#include "tinyjpg/image/image.hh"

namespace tinyjpg {

[[nodiscard]] Result<Image> resize_image(const Image& image, std::optional<PositiveInt> max_width,
                                         std::optional<PositiveInt> max_height, FitMode fit);

}  // namespace tinyjpg
