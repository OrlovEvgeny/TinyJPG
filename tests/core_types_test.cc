#include <string>
#include <string_view>

#include "tinyjpg/core/types.hh"

#if defined(TINYJPG_HAS_DOCTEST)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("quality validates its range") {
  const auto quality = tinyjpg::Quality::from_percent(82);
  REQUIRE(quality.has_value());
  CHECK(quality->percent() == 82);
  CHECK_FALSE(tinyjpg::Quality::from_percent(0).has_value());
  CHECK_FALSE(tinyjpg::Quality::from_percent(101).has_value());
}

TEST_CASE("variant names are constrained") {
  const auto name = tinyjpg::VariantName::from("thumb-320");
  REQUIRE(name.has_value());
  CHECK(name->value() == std::string_view{"thumb-320"});
  CHECK_FALSE(tinyjpg::VariantName::from("").has_value());
  CHECK_FALSE(tinyjpg::VariantName::from("thumb/320").has_value());
}

TEST_CASE("config enums parse and stringify") {
  CHECK(tinyjpg::parse_codec("webp").value() == tinyjpg::Codec::webp);
  CHECK(tinyjpg::parse_fidelity_mode("lossless").value() == tinyjpg::FidelityMode::lossless);
  CHECK(tinyjpg::parse_effort_level("max").value() == tinyjpg::EffortLevel::max);
  CHECK(tinyjpg::parse_fit_mode("cover").value() == tinyjpg::FitMode::cover);
  CHECK(tinyjpg::to_string(tinyjpg::Codec::jxl) == std::string_view{"jxl"});
  CHECK_FALSE(tinyjpg::parse_codec("gif").has_value());
}
#else
int main() {
  const auto quality = tinyjpg::Quality::from_percent(82);
  if (!quality.has_value() || quality->percent() != 82) {
    return 1;
  }
  if (tinyjpg::Quality::from_percent(0).has_value() ||
      tinyjpg::Quality::from_percent(101).has_value()) {
    return 1;
  }

  const auto name = tinyjpg::VariantName::from("thumb-320");
  if (!name.has_value() || name->value() != std::string_view{"thumb-320"}) {
    return 1;
  }
  if (tinyjpg::VariantName::from("").has_value() ||
      tinyjpg::VariantName::from("thumb/320").has_value()) {
    return 1;
  }

  if (tinyjpg::parse_codec("webp").value() != tinyjpg::Codec::webp ||
      tinyjpg::parse_fidelity_mode("lossless").value() != tinyjpg::FidelityMode::lossless ||
      tinyjpg::parse_effort_level("max").value() != tinyjpg::EffortLevel::max ||
      tinyjpg::parse_fit_mode("cover").value() != tinyjpg::FitMode::cover ||
      tinyjpg::to_string(tinyjpg::Codec::jxl) != std::string_view{"jxl"} ||
      tinyjpg::parse_codec("gif").has_value()) {
    return 1;
  }

  return 0;
}
#endif
