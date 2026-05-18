#include "tinyjpg/app/cli.hh"
#include "tinyjpg/core/version.hh"

#include <array>
#include <string_view>

#if defined(TINYJPG_HAS_DOCTEST)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("version metadata is available") {
  CHECK(tinyjpg::version_string() == std::string_view{"0.1.0"});

  const auto info = tinyjpg::version_info();
  CHECK(info.project_name == std::string_view{"TinyJPG"});
  CHECK(info.version == std::string_view{"0.1.0"});
}

TEST_CASE("help exits successfully") {
  constexpr std::array args{"tinyjpg", "--help"};
  CHECK(tinyjpg::app::run(args) == tinyjpg::app::ExitCode::ok);
}
#else
int main() {
  if (tinyjpg::version_string() != std::string_view{"0.1.0"}) {
    return 1;
  }

  const auto info = tinyjpg::version_info();
  if (info.project_name != std::string_view{"TinyJPG"} ||
      info.version != std::string_view{"0.1.0"}) {
    return 1;
  }

  constexpr std::array args{"tinyjpg", "--help"};
  if (tinyjpg::app::run(args) != tinyjpg::app::ExitCode::ok) {
    return 1;
  }

  return 0;
}
#endif
