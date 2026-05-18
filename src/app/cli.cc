#include "tinyjpg/app/cli.hh"

#include <CLI/CLI.hpp>
#include <iostream>
#include <string_view>

#include "tinyjpg/core/config_io.hh"
#include "tinyjpg/core/version.hh"

namespace tinyjpg::app {
namespace {

constexpr auto kUsage = "Usage: tinyjpg [--version] [--help] <command>\n";

}  // namespace

std::string_view usage_text() noexcept { return kUsage; }

ExitCode run(std::span<char const* const> args) {
  auto app = CLI::App{"TinyJPG image optimizer"};
  app.set_version_flag("--version,-v", std::string{version_string()});
  app.require_subcommand(0, 1);

  auto printed_defaults = false;
  auto config_path = std::string{};
  auto legacy_path = std::string{};

  auto* config = app.add_subcommand("config", "Inspect and validate configuration");
  auto* print = config->add_subcommand("print", "Print a sample configuration");
  print->add_flag("--defaults", printed_defaults, "Print default values");
  print->callback([&printed_defaults] {
    if (!printed_defaults) {
      printed_defaults = true;
    }

    const auto rendered = render_default_config();
    if (!rendered) {
      throw CLI::RuntimeError(rendered.error().message, static_cast<int>(ExitCode::usage));
    }
    std::cout << *rendered;
  });

  auto* validate = config->add_subcommand("validate", "Validate a TOML configuration file");
  validate->add_option("file", config_path, "TOML configuration file")->required();
  validate->callback([&config_path] {
    const auto result = validate_config_file(config_path);
    if (!result) {
      throw CLI::RuntimeError(result.error().message, static_cast<int>(ExitCode::usage));
    }
    std::cout << "config ok\n";
  });

  auto* migrate = app.add_subcommand("migrate-config", "Convert a legacy YAML configuration");
  migrate->add_option("file", legacy_path, "Legacy YAML configuration file")->required();
  migrate->callback([&legacy_path] {
    const auto rendered = migrate_legacy_yaml(legacy_path);
    if (!rendered) {
      throw CLI::RuntimeError(rendered.error().message, static_cast<int>(ExitCode::usage));
    }
    std::cout << *rendered;
  });

  if (args.size() <= 1) {
    std::cout << app.help();
    return ExitCode::ok;
  }

  try {
    app.parse(static_cast<int>(args.size()), args.data());
  } catch (const CLI::RuntimeError& error) {
    std::cerr << error.what() << '\n';
    return ExitCode::usage;
  } catch (const CLI::ParseError& error) {
    const auto exit_code = app.exit(error);
    return exit_code == 0 ? ExitCode::ok : ExitCode::usage;
  }

  return ExitCode::ok;
}

}  // namespace tinyjpg::app
