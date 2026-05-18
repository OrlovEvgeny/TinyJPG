#include "tinyjpg/pipeline/planner.hh"

#include <array>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "tinyjpg/codec/registry.hh"
#include "tinyjpg/image/resize.hh"

namespace tinyjpg {
namespace {

struct PatternValues {
  std::filesystem::path directory;
  std::string stem;
  std::string suffix;
  std::string extension;
  std::string name;
  std::string codec;
  std::string width;
  std::string height;
};

[[nodiscard]] Result<Quality> effective_quality(const VariantConfig& variant) {
  if (variant.quality) {
    return *variant.quality;
  }
  return Quality::from_percent(82);
}

[[nodiscard]] std::string extension_token(Codec codec) {
  auto extension = std::string{codec_extension(codec)};
  if (!extension.empty() && extension.front() == '.') {
    extension.erase(extension.begin());
  }
  return extension;
}

[[nodiscard]] bool same_path(const std::filesystem::path& left,
                             const std::filesystem::path& right) {
  return left.lexically_normal() == right.lexically_normal();
}

[[nodiscard]] Result<std::string> expand_pattern(std::string_view pattern,
                                                 const PatternValues& values) {
  const auto replacements = std::array{
      std::pair{std::string_view{"dir"}, values.directory.string()},
      std::pair{std::string_view{"stem"}, values.stem},
      std::pair{std::string_view{"suffix"}, values.suffix},
      std::pair{std::string_view{"ext"}, values.extension},
      std::pair{std::string_view{"name"}, values.name},
      std::pair{std::string_view{"codec"}, values.codec},
      std::pair{std::string_view{"width"}, values.width},
      std::pair{std::string_view{"height"}, values.height},
  };

  auto output = std::ostringstream{};
  for (auto index = std::size_t{0}; index < pattern.size();) {
    if (pattern[index] != '{') {
      output << pattern[index];
      ++index;
      continue;
    }

    const auto end = pattern.find('}', index + 1U);
    if (end == std::string_view::npos) {
      return unexpected(Error::config("output.pattern has an unclosed token"));
    }

    const auto token = pattern.substr(index + 1U, end - index - 1U);
    auto replaced = false;
    for (const auto& [name, value] : replacements) {
      if (name == token) {
        output << value;
        replaced = true;
        break;
      }
    }
    if (!replaced) {
      return unexpected(Error::config("unknown output.pattern token: " + std::string{token}));
    }
    index = end + 1U;
  }

  return output.str();
}

[[nodiscard]] Result<std::filesystem::path> render_output_path(
    const std::filesystem::path& input_path, const AppConfig& config, const VariantConfig& variant,
    Codec codec, const ResizePlan& resize, std::string suffix) {
  const auto base_directory = config.output.directory.value_or(input_path.parent_path());
  const auto directory = base_directory.empty() ? std::filesystem::path{"."} : base_directory;

  auto values = PatternValues{
      .directory = directory,
      .stem = input_path.stem().string(),
      .suffix = std::move(suffix),
      .extension = extension_token(codec),
      .name = std::string{variant.name.value()},
      .codec = std::string{to_string(codec)},
      .width = std::to_string(resize.width.value()),
      .height = std::to_string(resize.height.value()),
  };

  auto rendered = expand_pattern(config.output.pattern, values);
  if (!rendered) {
    return unexpected(rendered.error());
  }

  auto output_path = std::filesystem::path{*rendered};
  if (output_path.is_relative() && config.output.directory &&
      config.output.pattern.find("{dir}") == std::string::npos) {
    output_path = *config.output.directory / output_path;
  }

  if (same_path(output_path, input_path) && config.compress.preserve_original &&
      values.suffix.empty()) {
    values.suffix = "-optimized";
    rendered = expand_pattern(config.output.pattern, values);
    if (!rendered) {
      return unexpected(rendered.error());
    }
    output_path = std::filesystem::path{*rendered};
    if (output_path.is_relative() && config.output.directory &&
        config.output.pattern.find("{dir}") == std::string::npos) {
      output_path = *config.output.directory / output_path;
    }
  }

  return output_path.lexically_normal();
}

[[nodiscard]] std::filesystem::path versioned_path(std::filesystem::path output_path) {
  if (!std::filesystem::exists(output_path)) {
    return output_path;
  }

  const auto parent = output_path.parent_path();
  const auto stem = output_path.stem().string();
  const auto extension = output_path.extension().string();
  for (auto index = 1; index < 10'000; ++index) {
    auto candidate = parent / (stem + "-" + std::to_string(index) + extension);
    if (!std::filesystem::exists(candidate)) {
      return candidate;
    }
  }
  return output_path;
}

}  // namespace

Result<std::vector<PlannedVariant>> plan_variants(const std::filesystem::path& input_path,
                                                  SourceImageInfo source, const AppConfig& config) {
  const auto registry = default_codec_registry();
  auto planned = std::vector<PlannedVariant>{};
  planned.reserve(config.variants.size());

  for (const auto& variant : config.variants) {
    const auto codec = variant.codec == Codec::auto_select ? source.codec : variant.codec;
    if (!registry.supports(codec)) {
      return unexpected(
          Error::unsupported("selected codec is not available: " + std::string{to_string(codec)}));
    }

    auto quality = effective_quality(variant);
    if (!quality) {
      return unexpected(quality.error());
    }

    auto resize = plan_resize(source.width, source.height, variant.max_width, variant.max_height,
                              variant.fit);
    if (!resize) {
      return unexpected(resize.error());
    }

    auto output_path =
        render_output_path(input_path, config, variant, codec, *resize, variant.suffix);
    if (!output_path) {
      return unexpected(output_path.error());
    }
    if (config.output.on_exist == OnExist::version) {
      output_path = versioned_path(*output_path);
    }

    planned.push_back(PlannedVariant{
        .variant = variant,
        .output_path = *std::move(output_path),
        .codec = codec,
        .quality = *quality,
        .width = resize->width,
        .height = resize->height,
        .resized = resize->resized,
    });
  }

  return planned;
}

}  // namespace tinyjpg
