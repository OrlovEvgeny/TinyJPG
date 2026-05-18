#include "tinyjpg/core/config_io.hh"

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "tinyjpg/app/cli.hh"

#if defined(TINYJPG_HAS_DOCTEST)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

namespace {

std::filesystem::path write_temp_file(std::string_view name, std::string_view content) {
  auto path = std::filesystem::temp_directory_path() / name;
  auto out = std::ofstream{path};
  out << content;
  return path;
}

}  // namespace

TEST_CASE("default config renders as TOML and loads") {
  const auto rendered = tinyjpg::render_default_config();
  REQUIRE(rendered.has_value());

  const auto path = write_temp_file("tinyjpg-default-test.toml", *rendered);
  const auto loaded = tinyjpg::load_toml_config(path);
  REQUIRE(loaded.has_value());
  CHECK(loaded->variants.size() == 4);
  CHECK(tinyjpg::validate_config(*loaded).has_value());
}

TEST_CASE("toml validation rejects bad variants") {
  const auto path = write_temp_file("tinyjpg-bad-variant.toml",
                                    "[[variant]]\n"
                                    "name = \"bad\"\n"
                                    "codec = \"webp\"\n"
                                    "suffix = \"-bad\"\n");

  const auto loaded = tinyjpg::load_toml_config(path);
  REQUIRE_FALSE(loaded.has_value());
  CHECK(loaded.error().message.find("max_width or max_height") != std::string::npos);
}

TEST_CASE("config CLI commands return stable exit codes") {
  const auto rendered = tinyjpg::render_default_config().value();
  const auto path = write_temp_file("tinyjpg-cli-config.toml", rendered);

  const auto path_text = path.string();
  const std::array<char const*, 4> validate_args{
      "tinyjpg",
      "config",
      "validate",
      path_text.c_str(),
  };
  CHECK(tinyjpg::app::run(validate_args) == tinyjpg::app::ExitCode::ok);

  constexpr std::array<char const*, 4> print_args{"tinyjpg", "config", "print", "--defaults"};
  CHECK(tinyjpg::app::run(print_args) == tinyjpg::app::ExitCode::ok);
}
#else
namespace {

std::filesystem::path write_temp_file(std::string_view name, std::string_view content) {
  auto path = std::filesystem::temp_directory_path() / name;
  auto out = std::ofstream{path};
  out << content;
  return path;
}

}  // namespace

int main() {
  const auto rendered = tinyjpg::render_default_config();
  if (!rendered.has_value()) {
    return 1;
  }

  const auto path = write_temp_file("tinyjpg-default-test.toml", *rendered);
  const auto loaded = tinyjpg::load_toml_config(path);
  if (!loaded.has_value() || !tinyjpg::validate_config(*loaded).has_value()) {
    return 1;
  }

  const auto path_text = path.string();
  const std::array<char const*, 4> validate_args{
      "tinyjpg",
      "config",
      "validate",
      path_text.c_str(),
  };
  if (tinyjpg::app::run(validate_args) != tinyjpg::app::ExitCode::ok) {
    return 1;
  }

  return 0;
}
#endif
