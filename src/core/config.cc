#include "tinyjpg/core/config.hh"

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <string_view>
#include <utility>

namespace tinyjpg {
namespace {

template <typename EnumValue, std::size_t Size>
Result<EnumValue> parse_enum(std::string_view input,
                             const std::array<std::pair<std::string_view, EnumValue>, Size>& values,
                             std::string_view field_name) {
  for (const auto& [name, value] : values) {
    if (name == input) {
      return value;
    }
  }

  return unexpected(
      Error::config(std::string{"unknown "} + std::string{field_name} + ": " + std::string{input}));
}

[[nodiscard]] Result<VariantConfig> original_variant() {
  auto name = VariantName::from("original");
  if (!name) {
    return unexpected(name.error());
  }

  return VariantConfig{
      .name = *std::move(name),
      .codec = Codec::auto_select,
      .mode = std::nullopt,
      .quality = std::nullopt,
      .max_width = std::nullopt,
      .max_height = std::nullopt,
      .fit = FitMode::contain,
      .suffix = "",
  };
}

[[nodiscard]] Result<VariantConfig> sized_variant(std::string name_value, int max_width,
                                                  Codec codec, std::optional<Quality> quality,
                                                  std::string suffix) {
  auto name = VariantName::from(std::move(name_value));
  if (!name) {
    return unexpected(name.error());
  }

  auto width = PositiveInt::from(max_width, "variant.max_width");
  if (!width) {
    return unexpected(width.error());
  }

  return VariantConfig{
      .name = *std::move(name),
      .codec = codec,
      .mode = std::nullopt,
      .quality = quality,
      .max_width = *width,
      .max_height = std::nullopt,
      .fit = FitMode::contain,
      .suffix = std::move(suffix),
  };
}

[[nodiscard]] Result<VariantConfig> thumb_variant() {
  auto name = VariantName::from("thumb");
  if (!name) {
    return unexpected(name.error());
  }

  auto width = PositiveInt::from(320, "variant.max_width");
  if (!width) {
    return unexpected(width.error());
  }

  auto height = PositiveInt::from(320, "variant.max_height");
  if (!height) {
    return unexpected(height.error());
  }

  auto quality = Quality::from_percent(75);
  if (!quality) {
    return unexpected(quality.error());
  }

  return VariantConfig{
      .name = *std::move(name),
      .codec = Codec::auto_select,
      .mode = std::nullopt,
      .quality = *quality,
      .max_width = *width,
      .max_height = *height,
      .fit = FitMode::cover,
      .suffix = "-thumb",
  };
}

[[nodiscard]] bool is_original_variant(const VariantConfig& variant) {
  return variant.name.value() == std::string_view{"original"};
}

[[nodiscard]] bool has_size_constraint(const VariantConfig& variant) {
  return variant.max_width.has_value() || variant.max_height.has_value();
}

[[nodiscard]] bool codec_supports(Codec codec, FidelityMode mode) {
  static_cast<void>(codec);
  static_cast<void>(mode);
  return true;
}

}  // namespace

Result<LogLevel> parse_log_level(std::string_view value) {
  static constexpr auto kValues = std::array{
      std::pair{std::string_view{"trace"}, LogLevel::trace},
      std::pair{std::string_view{"debug"}, LogLevel::debug},
      std::pair{std::string_view{"info"}, LogLevel::info},
      std::pair{std::string_view{"warn"}, LogLevel::warn},
      std::pair{std::string_view{"error"}, LogLevel::error},
  };
  return parse_enum(value, kValues, "log level");
}

Result<OnExist> parse_on_exist(std::string_view value) {
  static constexpr auto kValues = std::array{
      std::pair{std::string_view{"skip"}, OnExist::skip},
      std::pair{std::string_view{"overwrite"}, OnExist::overwrite},
      std::pair{std::string_view{"version"}, OnExist::version},
  };
  return parse_enum(value, kValues, "on-exist policy");
}

std::string_view to_string(LogLevel value) noexcept {
  switch (value) {
    case LogLevel::trace:
      return "trace";
    case LogLevel::debug:
      return "debug";
    case LogLevel::info:
      return "info";
    case LogLevel::warn:
      return "warn";
    case LogLevel::error:
      return "error";
  }
  return "unknown";
}

std::string_view to_string(OnExist value) noexcept {
  switch (value) {
    case OnExist::skip:
      return "skip";
    case OnExist::overwrite:
      return "overwrite";
    case OnExist::version:
      return "version";
  }
  return "unknown";
}

Result<AppConfig> default_config() {
  auto workers = NonNegativeInt::from(0, "general.workers");
  auto queue_capacity = PositiveInt::from(512, "general.queue_capacity");
  auto stable_wait = NonNegativeInt::from(400, "general.stable_wait_ms");
  auto medium_quality = Quality::from_percent(82);

  if (!workers) {
    return unexpected(workers.error());
  }
  if (!queue_capacity) {
    return unexpected(queue_capacity.error());
  }
  if (!stable_wait) {
    return unexpected(stable_wait.error());
  }
  if (!medium_quality) {
    return unexpected(medium_quality.error());
  }

  auto thumb = thumb_variant();
  auto original = original_variant();
  auto large = sized_variant("large", 1920, Codec::auto_select, std::nullopt, "-large");
  auto medium = sized_variant("medium", 1024, Codec::auto_select, *medium_quality, "-medium");

  if (!thumb) {
    return unexpected(thumb.error());
  }
  if (!original) {
    return unexpected(original.error());
  }
  if (!large) {
    return unexpected(large.error());
  }
  if (!medium) {
    return unexpected(medium.error());
  }

  return AppConfig{
      .general =
          GeneralConfig{
              .workers = *workers,
              .log_level = LogLevel::info,
              .log_file = std::nullopt,
              .queue_capacity = *queue_capacity,
              .stable_wait_ms = *stable_wait,
              .dry_run = false,
          },
      .watch =
          WatchConfig{
              .paths = {},
              .recursive = true,
              .include = {"*.jpg", "*.jpeg", "*.png", "*.webp"},
              .exclude = {"**/.cache/**", "*.tmp"},
              .prefix = {},
          },
      .compress =
          CompressionConfig{
              .mode = FidelityMode::lossless,
              .effort = EffortLevel::max,
              .keep_metadata = false,
              .skip_if_not_smaller = true,
              .preserve_original = true,
          },
      .variants = {*std::move(original), *std::move(large), *std::move(medium), *std::move(thumb)},
      .output =
          OutputConfig{
              .pattern = "{dir}/{stem}{suffix}.{ext}",
              .directory = std::nullopt,
              .on_exist = OnExist::skip,
          },
  };
}

Result<void> validate_config(const AppConfig& config) {
  if (config.variants.empty()) {
    return unexpected(Error::config("at least one variant is required"));
  }

  auto names = std::set<std::string>{};
  for (const auto& variant : config.variants) {
    const auto [_, inserted] = names.insert(std::string{variant.name.value()});
    if (!inserted) {
      return unexpected(
          Error::config("duplicate variant name: " + std::string{variant.name.value()}));
    }

    if (!is_original_variant(variant) && !has_size_constraint(variant)) {
      return unexpected(Error::config("variant " + std::string{variant.name.value()} +
                                      " must set max_width or max_height"));
    }

    const auto mode = variant.mode.value_or(config.compress.mode);
    if (!codec_supports(variant.codec, mode)) {
      return unexpected(Error::config("variant " + std::string{variant.name.value()} +
                                      " uses an unsupported codec/mode pair"));
    }
  }

  if (config.output.pattern.empty()) {
    return unexpected(Error::config("output.pattern must not be empty"));
  }

  return {};
}

std::vector<std::string_view> preset_names() { return {"web", "ecommerce", "avatar"}; }

Result<std::vector<VariantConfig>> variant_preset(std::string_view name) {
  auto original = original_variant();
  if (!original) {
    return unexpected(original.error());
  }

  if (name == "web") {
    auto large = sized_variant("large", 1920, Codec::auto_select, std::nullopt, "-large");
    auto medium_quality = Quality::from_percent(82);
    if (!medium_quality) {
      return unexpected(medium_quality.error());
    }
    auto medium = sized_variant("medium", 1024, Codec::auto_select, *medium_quality, "-medium");
    auto thumb = thumb_variant();
    if (!large) {
      return unexpected(large.error());
    }
    if (!medium) {
      return unexpected(medium.error());
    }
    if (!thumb) {
      return unexpected(thumb.error());
    }
    thumb->codec = Codec::auto_select;
    return std::vector<VariantConfig>{*std::move(original), *std::move(large), *std::move(medium),
                                      *std::move(thumb)};
  }

  if (name == "ecommerce") {
    auto hero = sized_variant("hero", 1600, Codec::auto_select, std::nullopt, "-hero");
    auto listing = sized_variant("listing", 900, Codec::auto_select, std::nullopt, "-listing");
    auto thumb = sized_variant("thumb", 320, Codec::auto_select, std::nullopt, "-thumb");
    if (!hero) {
      return unexpected(hero.error());
    }
    if (!listing) {
      return unexpected(listing.error());
    }
    if (!thumb) {
      return unexpected(thumb.error());
    }
    return std::vector<VariantConfig>{*std::move(original), *std::move(hero), *std::move(listing),
                                      *std::move(thumb)};
  }

  if (name == "avatar") {
    auto full = sized_variant("full", 512, Codec::auto_select, std::nullopt, "-full");
    auto thumb = sized_variant("thumb", 128, Codec::auto_select, std::nullopt, "-thumb");
    if (!full) {
      return unexpected(full.error());
    }
    if (!thumb) {
      return unexpected(thumb.error());
    }
    full->max_height = full->max_width;
    full->fit = FitMode::cover;
    thumb->max_height = thumb->max_width;
    thumb->fit = FitMode::cover;
    return std::vector<VariantConfig>{*std::move(original), *std::move(full), *std::move(thumb)};
  }

  return unexpected(Error::config("unknown preset: " + std::string{name}));
}

}  // namespace tinyjpg
