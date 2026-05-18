#include "tinyjpg/core/types.hh"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>

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

  return unexpected(Error::invalid_argument(std::string{"unknown "} + std::string{field_name} +
                                            ": " + std::string{input}));
}

[[nodiscard]] bool is_variant_char(unsigned char value) {
  return std::isalnum(value) != 0 || value == '_' || value == '-';
}

}  // namespace

Result<Quality> Quality::from_percent(int value) {
  if (value < 1 || value > 100) {
    return unexpected(Error::invalid_argument("quality must be between 1 and 100"));
  }

  return Quality{value};
}

Result<PositiveInt> PositiveInt::from(int value, std::string_view field_name) {
  if (value <= 0) {
    return unexpected(Error::invalid_argument(std::string{field_name} + " must be positive"));
  }

  return PositiveInt{value};
}

Result<NonNegativeInt> NonNegativeInt::from(int value, std::string_view field_name) {
  if (value < 0) {
    return unexpected(
        Error::invalid_argument(std::string{field_name} + " must be zero or greater"));
  }

  return NonNegativeInt{value};
}

Result<VariantName> VariantName::from(std::string value) {
  if (value.empty()) {
    return unexpected(Error::invalid_argument("variant name must not be empty"));
  }

  const auto valid = std::ranges::all_of(
      value, [](const char ch) { return is_variant_char(static_cast<unsigned char>(ch)); });
  if (!valid) {
    return unexpected(
        Error::invalid_argument("variant name may contain only letters, digits, '_' and '-'"));
  }

  return VariantName{std::move(value)};
}

Result<Codec> parse_codec(std::string_view value) {
  static constexpr auto kValues = std::array{
      std::pair{std::string_view{"auto"}, Codec::auto_select},
      std::pair{std::string_view{"jpeg"}, Codec::jpeg},
      std::pair{std::string_view{"png"}, Codec::png},
      std::pair{std::string_view{"webp"}, Codec::webp},
      std::pair{std::string_view{"avif"}, Codec::avif},
      std::pair{std::string_view{"jxl"}, Codec::jxl},
  };
  return parse_enum(value, kValues, "codec");
}

Result<FidelityMode> parse_fidelity_mode(std::string_view value) {
  static constexpr auto kValues = std::array{
      std::pair{std::string_view{"lossless"}, FidelityMode::lossless},
      std::pair{std::string_view{"visually_lossless"}, FidelityMode::visually_lossless},
      std::pair{std::string_view{"lossy"}, FidelityMode::lossy},
  };
  return parse_enum(value, kValues, "fidelity mode");
}

Result<EffortLevel> parse_effort_level(std::string_view value) {
  static constexpr auto kValues = std::array{
      std::pair{std::string_view{"fast"}, EffortLevel::fast},
      std::pair{std::string_view{"balanced"}, EffortLevel::balanced},
      std::pair{std::string_view{"max"}, EffortLevel::max},
  };
  return parse_enum(value, kValues, "effort level");
}

Result<FitMode> parse_fit_mode(std::string_view value) {
  static constexpr auto kValues = std::array{
      std::pair{std::string_view{"contain"}, FitMode::contain},
      std::pair{std::string_view{"cover"}, FitMode::cover},
      std::pair{std::string_view{"fill"}, FitMode::fill},
  };
  return parse_enum(value, kValues, "fit mode");
}

std::string_view to_string(Codec value) noexcept {
  switch (value) {
    case Codec::auto_select:
      return "auto";
    case Codec::jpeg:
      return "jpeg";
    case Codec::png:
      return "png";
    case Codec::webp:
      return "webp";
    case Codec::avif:
      return "avif";
    case Codec::jxl:
      return "jxl";
  }
  return "unknown";
}

std::string_view to_string(FidelityMode value) noexcept {
  switch (value) {
    case FidelityMode::lossless:
      return "lossless";
    case FidelityMode::visually_lossless:
      return "visually_lossless";
    case FidelityMode::lossy:
      return "lossy";
  }
  return "unknown";
}

std::string_view to_string(EffortLevel value) noexcept {
  switch (value) {
    case EffortLevel::fast:
      return "fast";
    case EffortLevel::balanced:
      return "balanced";
    case EffortLevel::max:
      return "max";
  }
  return "unknown";
}

std::string_view to_string(FitMode value) noexcept {
  switch (value) {
    case FitMode::contain:
      return "contain";
    case FitMode::cover:
      return "cover";
    case FitMode::fill:
      return "fill";
  }
  return "unknown";
}

}  // namespace tinyjpg
