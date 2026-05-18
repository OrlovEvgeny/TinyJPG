#include "tinyjpg/image/resize.hh"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string_view>

namespace tinyjpg {
namespace {

[[nodiscard]] Result<PositiveInt> checked_dimension(double value, std::string_view name) {
  return PositiveInt::from(std::max(1, static_cast<int>(std::lround(value))), name);
}

[[nodiscard]] Result<PositiveInt> checked_dimension(int value, std::string_view name) {
  return PositiveInt::from(std::max(1, value), name);
}

}  // namespace

Result<ResizePlan> plan_resize(PositiveInt source_width, PositiveInt source_height,
                               std::optional<PositiveInt> max_width,
                               std::optional<PositiveInt> max_height, FitMode fit) {
  if (!max_width && !max_height) {
    return ResizePlan{.width = source_width, .height = source_height, .resized = false};
  }

  const auto source_width_value = source_width.value();
  const auto source_height_value = source_height.value();
  auto target_width = max_width ? max_width->value() : source_width_value;
  auto target_height = max_height ? max_height->value() : source_height_value;

  if (fit == FitMode::fill) {
    target_width = std::min(target_width, source_width_value);
    target_height = std::min(target_height, source_height_value);

    const auto width = checked_dimension(target_width, "resize.width");
    const auto height = checked_dimension(target_height, "resize.height");
    if (!width) {
      return unexpected(width.error());
    }
    if (!height) {
      return unexpected(height.error());
    }

    return ResizePlan{
        .width = *width,
        .height = *height,
        .resized = width->value() != source_width_value || height->value() != source_height_value,
    };
  }

  const auto scale_x = static_cast<double>(target_width) / static_cast<double>(source_width_value);
  const auto scale_y =
      static_cast<double>(target_height) / static_cast<double>(source_height_value);
  auto scale = fit == FitMode::cover ? std::max(scale_x, scale_y) : std::min(scale_x, scale_y);
  scale = std::min(scale, 1.0);

  const auto width =
      checked_dimension(static_cast<double>(source_width_value) * scale, "resize.width");
  const auto height =
      checked_dimension(static_cast<double>(source_height_value) * scale, "resize.height");
  if (!width) {
    return unexpected(width.error());
  }
  if (!height) {
    return unexpected(height.error());
  }

  return ResizePlan{
      .width = *width,
      .height = *height,
      .resized = width->value() != source_width_value || height->value() != source_height_value,
  };
}

Result<Image> resize_image(const Image& image, std::optional<PositiveInt> max_width,
                           std::optional<PositiveInt> max_height, FitMode fit) {
  const auto resize = plan_resize(image.width, image.height, max_width, max_height, fit);
  if (!resize) {
    return unexpected(resize.error());
  }

  if (!resize->resized) {
    return image;
  }

  const auto source_width = image.width.value();
  const auto source_height = image.height.value();
  const auto out_width = resize->width.value();
  const auto out_height = resize->height.value();

  auto output = Image{.width = resize->width, .height = resize->height, .pixels = {}};
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
