#pragma once

#include <compare>
#include <cstdint>
#include <string>
#include <string_view>

#include "tinyjpg/core/error.hh"

namespace tinyjpg {

class Quality {
 public:
  [[nodiscard]] static Result<Quality> from_percent(int value);

  [[nodiscard]] constexpr int percent() const noexcept { return value_; }
  [[nodiscard]] constexpr auto operator<=>(const Quality&) const = default;

 private:
  explicit constexpr Quality(int value) : value_{value} {}

  int value_;
};

class PositiveInt {
 public:
  [[nodiscard]] static Result<PositiveInt> from(int value, std::string_view field_name);

  [[nodiscard]] constexpr int value() const noexcept { return value_; }
  [[nodiscard]] constexpr auto operator<=>(const PositiveInt&) const = default;

 private:
  explicit constexpr PositiveInt(int value) : value_{value} {}

  int value_;
};

class NonNegativeInt {
 public:
  [[nodiscard]] static Result<NonNegativeInt> from(int value, std::string_view field_name);

  [[nodiscard]] constexpr int value() const noexcept { return value_; }
  [[nodiscard]] constexpr auto operator<=>(const NonNegativeInt&) const = default;

 private:
  explicit constexpr NonNegativeInt(int value) : value_{value} {}

  int value_;
};

class VariantName {
 public:
  [[nodiscard]] static Result<VariantName> from(std::string value);

  [[nodiscard]] std::string_view value() const noexcept { return value_; }
  [[nodiscard]] auto operator<=>(const VariantName&) const = default;

 private:
  explicit VariantName(std::string value) : value_{std::move(value)} {}

  std::string value_;
};

enum class Codec {
  auto_select,
  jpeg,
  png,
  webp,
  avif,
  jxl,
};

enum class FidelityMode {
  lossless,
  visually_lossless,
  lossy,
};

enum class EffortLevel {
  fast,
  balanced,
  max,
};

enum class FitMode {
  contain,
  cover,
  fill,
};

[[nodiscard]] Result<Codec> parse_codec(std::string_view value);
[[nodiscard]] Result<FidelityMode> parse_fidelity_mode(std::string_view value);
[[nodiscard]] Result<EffortLevel> parse_effort_level(std::string_view value);
[[nodiscard]] Result<FitMode> parse_fit_mode(std::string_view value);

[[nodiscard]] std::string_view to_string(Codec value) noexcept;
[[nodiscard]] std::string_view to_string(FidelityMode value) noexcept;
[[nodiscard]] std::string_view to_string(EffortLevel value) noexcept;
[[nodiscard]] std::string_view to_string(FitMode value) noexcept;

}  // namespace tinyjpg
