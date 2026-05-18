#include "tinyjpg/image/resize.hh"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace tinyjpg {
namespace {

[[nodiscard]] Result<PositiveInt> checked_dimension(double value, std::string_view name) {
  return PositiveInt::from(std::max(1, static_cast<int>(std::lround(value))), name);
}

}  // namespace

Result<Image> resize_image(const Image& image, std::optional<PositiveInt> max_width,
                           std::optional<PositiveInt> max_height, FitMode fit) {
  if (!max_width && !max_height) {
    return image;
  }

  const auto source_width = image.width.value();
  const auto source_height = image.height.value();
  const auto target_width = max_width ? max_width->value() : source_width;
  const auto target_height = max_height ? max_height->value() : source_height;

  auto scale_x = static_cast<double>(target_width) / static_cast<double>(source_width);
  auto scale_y = static_cast<double>(target_height) / static_cast<double>(source_height);
  auto scale = std::min(scale_x, scale_y);
  if (fit == FitMode::cover) {
    scale = std::max(scale_x, scale_y);
  } else if (fit == FitMode::fill) {
    const auto width = PositiveInt::from(target_width, "resize.width");
    const auto height = PositiveInt::from(target_height, "resize.height");
    if (!width) {
      return unexpected(width.error());
    }
    if (!height) {
      return unexpected(height.error());
    }

    auto output = Image{.width = *width, .height = *height, .pixels = {}};
    output.pixels.resize(static_cast<std::size_t>(target_width) *
                         static_cast<std::size_t>(target_height) * 3U);
    for (auto y = 0; y < target_height; ++y) {
      const auto src_y = std::min(source_height - 1, y * source_height / target_height);
      for (auto x = 0; x < target_width; ++x) {
        const auto src_x = std::min(source_width - 1, x * source_width / target_width);
        const auto src_index =
            (static_cast<std::size_t>(src_y) * static_cast<std::size_t>(source_width) +
             static_cast<std::size_t>(src_x)) *
            3U;
        const auto dst_index =
            (static_cast<std::size_t>(y) * static_cast<std::size_t>(target_width) +
             static_cast<std::size_t>(x)) *
            3U;
        std::ranges::copy_n(image.pixels.begin() + static_cast<std::ptrdiff_t>(src_index), 3,
                            output.pixels.begin() + static_cast<std::ptrdiff_t>(dst_index));
      }
    }
    return output;
  }

  scale = std::min(scale, 1.0);
  const auto width = checked_dimension(static_cast<double>(source_width) * scale, "resize.width");
  const auto height =
      checked_dimension(static_cast<double>(source_height) * scale, "resize.height");
  if (!width) {
    return unexpected(width.error());
  }
  if (!height) {
    return unexpected(height.error());
  }

  const auto out_width = width->value();
  const auto out_height = height->value();
  if (out_width == source_width && out_height == source_height) {
    return image;
  }

  auto output = Image{.width = *width, .height = *height, .pixels = {}};
  output.pixels.resize(static_cast<std::size_t>(out_width) * static_cast<std::size_t>(out_height) *
                       3U);
  for (auto y = 0; y < out_height; ++y) {
    const auto src_y = std::min(source_height - 1, y * source_height / out_height);
    for (auto x = 0; x < out_width; ++x) {
      const auto src_x = std::min(source_width - 1, x * source_width / out_width);
      const auto src_index =
          (static_cast<std::size_t>(src_y) * static_cast<std::size_t>(source_width) +
           static_cast<std::size_t>(src_x)) *
          3U;
      const auto dst_index = (static_cast<std::size_t>(y) * static_cast<std::size_t>(out_width) +
                              static_cast<std::size_t>(x)) *
                             3U;
      std::ranges::copy_n(image.pixels.begin() + static_cast<std::ptrdiff_t>(src_index), 3,
                          output.pixels.begin() + static_cast<std::ptrdiff_t>(dst_index));
    }
  }

  return output;
}

}  // namespace tinyjpg
