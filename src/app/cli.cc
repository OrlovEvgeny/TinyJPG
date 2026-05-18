#include "tinyjpg/app/cli.hh"

#include <iostream>
#include <string_view>

#include "tinyjpg/core/version.hh"

namespace tinyjpg::app {
namespace {

constexpr auto kUsage =
    "Usage: tinyjpg [--version] [--help]\n"
    "\n"
    "Commands will be added as the C++ rewrite reaches each feature area.\n";

void print_version() {
  const auto info = version_info();
  std::cout << info.project_name << ' ' << info.version << '\n';
}

}  // namespace

std::string_view usage_text() noexcept { return kUsage; }

ExitCode run(std::span<char const* const> args) {
  if (args.size() <= 1) {
    std::cout << usage_text();
    return ExitCode::ok;
  }

  const std::string_view command{args[1]};
  if (command == "--version" || command == "-v") {
    print_version();
    return ExitCode::ok;
  }

  if (command == "--help" || command == "-h") {
    std::cout << usage_text();
    return ExitCode::ok;
  }

  std::cerr << "unknown option: " << command << '\n' << usage_text();
  return ExitCode::usage;
}

}  // namespace tinyjpg::app
