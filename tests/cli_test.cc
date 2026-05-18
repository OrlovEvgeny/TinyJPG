#include "tinyjpg/app/cli.hh"

#include <array>
#include <iostream>
#include <sstream>
#include <string>

namespace {

template <std::size_t Size>
[[nodiscard]] std::string run_and_capture(const std::array<const char*, Size>& args,
                                          tinyjpg::app::ExitCode* exit_code = nullptr) {
  auto out = std::ostringstream{};
  auto* original = std::cout.rdbuf(out.rdbuf());
  const auto result = tinyjpg::app::run(args);
  std::cout.rdbuf(original);
  if (exit_code != nullptr) {
    *exit_code = result;
  }
  return out.str();
}

}  // namespace

#if defined(TINYJPG_HAS_DOCTEST)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("doctor supports json output") {
  constexpr std::array args{"tinyjpg", "doctor", "--format", "json"};
  auto exit_code = tinyjpg::app::ExitCode::usage;
  const auto output = run_and_capture(args, &exit_code);

  CHECK(exit_code == tinyjpg::app::ExitCode::ok);
  CHECK(output.find("\"checks\"") != std::string::npos);
  CHECK(output.find("\"default_config\"") != std::string::npos);
}

TEST_CASE("completion prints shell script") {
  constexpr std::array args{"tinyjpg", "completion", "bash"};
  auto exit_code = tinyjpg::app::ExitCode::usage;
  const auto output = run_and_capture(args, &exit_code);

  CHECK(exit_code == tinyjpg::app::ExitCode::ok);
  CHECK(output.find("complete -F _tinyjpg_complete tinyjpg") != std::string::npos);
}

TEST_CASE("presets list supports json output") {
  constexpr std::array args{"tinyjpg", "presets", "list", "--format", "json"};
  auto exit_code = tinyjpg::app::ExitCode::usage;
  const auto output = run_and_capture(args, &exit_code);

  CHECK(exit_code == tinyjpg::app::ExitCode::ok);
  CHECK(output == "[\"web\",\"ecommerce\",\"avatar\"]\n");
}
#else
int main() {
  constexpr std::array doctor_args{"tinyjpg", "doctor", "--format", "json"};
  auto exit_code = tinyjpg::app::ExitCode::usage;
  auto output = run_and_capture(doctor_args, &exit_code);
  if (exit_code != tinyjpg::app::ExitCode::ok ||
      output.find("\"default_config\"") == std::string::npos) {
    return 1;
  }

  constexpr std::array completion_args{"tinyjpg", "completion", "bash"};
  output = run_and_capture(completion_args, &exit_code);
  if (exit_code != tinyjpg::app::ExitCode::ok ||
      output.find("complete -F _tinyjpg_complete tinyjpg") == std::string::npos) {
    return 1;
  }

  constexpr std::array presets_args{"tinyjpg", "presets", "list", "--format", "json"};
  output = run_and_capture(presets_args, &exit_code);
  if (exit_code != tinyjpg::app::ExitCode::ok || output != "[\"web\",\"ecommerce\",\"avatar\"]\n") {
    return 1;
  }

  return 0;
}
#endif
