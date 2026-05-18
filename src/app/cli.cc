#include "tinyjpg/app/cli.hh"

#include <CLI/CLI.hpp>
#include <iostream>
#include <string>
#include <string_view>

#include "tinyjpg/core/config_io.hh"
#include "tinyjpg/core/version.hh"
#include "tinyjpg/pipeline/job.hh"

namespace tinyjpg::app {
namespace {

constexpr auto kUsage = "Usage: tinyjpg [--version] [--help] <command>\n";

[[noreturn]] void throw_usage(const Error& error) {
  throw CLI::RuntimeError(error.message, static_cast<int>(ExitCode::usage));
}

void print_default_config_command(bool& printed_defaults) {
  if (!printed_defaults) {
    printed_defaults = true;
  }

  const auto rendered = render_default_config();
  if (!rendered) {
    throw_usage(rendered.error());
  }
  std::cout << *rendered;
}

void validate_config_command(const std::string& config_path) {
  const auto result = validate_config_file(config_path);
  if (!result) {
    throw_usage(result.error());
  }
  std::cout << "config ok\n";
}

void migrate_config_command(const std::string& legacy_path) {
  const auto rendered = migrate_legacy_yaml(legacy_path);
  if (!rendered) {
    throw_usage(rendered.error());
  }
  std::cout << *rendered;
}

[[nodiscard]] Result<AppConfig> load_run_config(const std::string& config_path) {
  if (config_path.empty()) {
    return default_config();
  }
  return load_toml_config(config_path);
}

void run_file_command(const std::string& input_path, const std::string& config_path) {
  const auto config = load_run_config(config_path);
  if (!config) {
    throw_usage(config.error());
  }

  const auto result = process_file(input_path, *config);
  if (!result) {
    throw_usage(result.error());
  }

  if (!result->written) {
    std::cout << "nothing written; optimized output was not smaller\n";
    return;
  }

  std::cout << result->output_path.string() << " " << result->bytes_before - result->bytes_after
            << " bytes saved\n";
}

}  // namespace

std::string_view usage_text() noexcept { return kUsage; }

ExitCode run(std::span<char const* const> args) {
  auto app = CLI::App{"TinyJPG image optimizer"};
  app.set_version_flag("--version,-v", std::string{version_string()});
  app.require_subcommand(0, 1);

  auto printed_defaults = false;
  auto config_path = std::string{};
  auto legacy_path = std::string{};
  auto run_input_path = std::string{};
  auto run_config_path = std::string{};

  auto* config = app.add_subcommand("config", "Inspect and validate configuration");
  auto* print = config->add_subcommand("print", "Print a sample configuration");
  print->add_flag("--defaults", printed_defaults, "Print default values");
  print->callback([&printed_defaults] { print_default_config_command(printed_defaults); });

  auto* validate = config->add_subcommand("validate", "Validate a TOML configuration file");
  validate->add_option("file", config_path, "TOML configuration file")->required();
  validate->callback([&config_path] { validate_config_command(config_path); });

  auto* migrate = app.add_subcommand("migrate-config", "Convert a legacy YAML configuration");
  migrate->add_option("file", legacy_path, "Legacy YAML configuration file")->required();
  migrate->callback([&legacy_path] { migrate_config_command(legacy_path); });

  auto* run_command = app.add_subcommand("run", "Optimize one image file");
  run_command->add_option("file", run_input_path, "Image file to optimize")->required();
  run_command->add_option("--config,-c", run_config_path, "TOML configuration file");
  run_command->callback(
      [&run_input_path, &run_config_path] { run_file_command(run_input_path, run_config_path); });

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
