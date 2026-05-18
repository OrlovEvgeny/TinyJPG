#include "tinyjpg/core/config.hh"

#include <string>
#include <string_view>

#if defined(TINYJPG_HAS_DOCTEST)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("default config validates") {
  const auto config = tinyjpg::default_config();
  REQUIRE(config.has_value());
  CHECK(tinyjpg::validate_config(*config).has_value());
  CHECK(config->general.workers.value() == 0);
  CHECK(config->general.queue_capacity.value() == 512);
  CHECK(config->variants.size() == 4);
}

TEST_CASE("variant validation rejects duplicate names") {
  auto config = tinyjpg::default_config().value();
  config.variants.push_back(config.variants.front());

  const auto result = tinyjpg::validate_config(config);
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().message.find("duplicate variant name") != std::string_view::npos);
}

TEST_CASE("variant validation requires a size for non-original variants") {
  auto config = tinyjpg::default_config().value();
  auto variant = config.variants.front();
  variant.name = tinyjpg::VariantName::from("bad").value();
  config.variants.push_back(std::move(variant));

  const auto result = tinyjpg::validate_config(config);
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().message.find("max_width or max_height") != std::string_view::npos);
}

TEST_CASE("output pattern is required") {
  auto config = tinyjpg::default_config().value();
  config.output.pattern.clear();

  const auto result = tinyjpg::validate_config(config);
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().message.find("output.pattern") != std::string_view::npos);
}
#else
int main() {
  auto config = tinyjpg::default_config();
  if (!config.has_value() || !tinyjpg::validate_config(*config).has_value()) {
    return 1;
  }
  if (config->general.workers.value() != 0 || config->general.queue_capacity.value() != 512 ||
      config->variants.size() != 4) {
    return 1;
  }

  auto duplicate = *config;
  duplicate.variants.push_back(duplicate.variants.front());
  if (tinyjpg::validate_config(duplicate).has_value()) {
    return 1;
  }

  auto missing_size = *config;
  auto variant = missing_size.variants.front();
  variant.name = tinyjpg::VariantName::from("bad").value();
  missing_size.variants.push_back(std::move(variant));
  if (tinyjpg::validate_config(missing_size).has_value()) {
    return 1;
  }

  auto missing_pattern = *config;
  missing_pattern.output.pattern.clear();
  if (tinyjpg::validate_config(missing_pattern).has_value()) {
    return 1;
  }

  return 0;
}
#endif
