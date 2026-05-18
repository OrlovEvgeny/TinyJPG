#include "tinyjpg/app/cli.hh"

#include <CLI/CLI.hpp>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "tinyjpg/core/config_io.hh"
#include "tinyjpg/core/version.hh"
#include "tinyjpg/pipeline/job.hh"
#include "tinyjpg/service/service.hh"

namespace tinyjpg::app {
namespace {

constexpr auto kUsage = "Usage: tinyjpg [--version] [--help] <command>\n";
volatile std::sig_atomic_t active_stop_requested = 0;

void request_stop(int /*signal*/) { active_stop_requested = 1; }

[[nodiscard]] bool stop_requested() { return active_stop_requested != 0; }

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

[[nodiscard]] Result<AppConfig> load_command_config(const std::string& config_path,
                                                    const std::string& preset_name) {
  auto config = load_run_config(config_path);
  if (!config) {
    return unexpected(config.error());
  }

  if (!preset_name.empty()) {
    auto variants = variant_preset(preset_name);
    if (!variants) {
      return unexpected(variants.error());
    }
    config->variants = *std::move(variants);
  }

  return config;
}

void print_process_result(const ProcessResult& result) {
  for (const auto& variant : result.variants) {
    if (!variant.written) {
      std::cout << result.input_path.string() << " " << variant.variant_name.value() << " skipped "
                << to_string(variant.skip_reason) << "\n";
      continue;
    }

    const auto saved = result.bytes_before > variant.bytes_after
                           ? result.bytes_before - variant.bytes_after
                           : std::uintmax_t{0};
    std::cout << variant.output_path.string() << " " << variant.variant_name.value() << " "
              << variant.width.value() << "x" << variant.height.value() << " " << saved
              << " bytes saved\n";
  }
}

void process_paths(const std::vector<std::filesystem::path>& paths, const AppConfig& config) {
  for (const auto& input_path : paths) {
    const auto result = process_file(input_path, config);
    if (!result) {
      throw_usage(result.error());
    }
    print_process_result(*result);
  }
}

[[nodiscard]] std::vector<std::filesystem::path> path_list(
    const std::vector<std::string>& input_paths) {
  auto paths = std::vector<std::filesystem::path>{};
  paths.reserve(input_paths.size());
  for (const auto& input_path : input_paths) {
    paths.emplace_back(input_path);
  }
  return paths;
}

void run_file_command(const std::vector<std::string>& input_paths, const std::string& config_path,
                      const std::string& preset_name) {
  auto config = load_command_config(config_path, preset_name);
  if (!config) {
    throw_usage(config.error());
  }

  auto paths = path_list(input_paths);
  process_paths(paths, *config);
}

void scan_command(const std::vector<std::string>& input_paths, const std::string& config_path,
                  const std::string& preset_name) {
  auto config = load_command_config(config_path, preset_name);
  if (!config) {
    throw_usage(config.error());
  }

  auto paths = path_list(input_paths);
  const auto result = scan_paths(paths, *config, std::cout);
  if (!result) {
    throw_usage(result.error());
  }
}

void watch_command(const std::vector<std::string>& input_paths, const std::string& config_path,
                   const std::string& preset_name) {
  auto config = load_command_config(config_path, preset_name);
  if (!config) {
    throw_usage(config.error());
  }

  auto paths = input_paths.empty() ? config->watch.paths : path_list(input_paths);
  if (paths.empty()) {
    throw_usage(Error::config("watch requires at least one path"));
  }

  active_stop_requested = 0;
  std::signal(SIGINT, request_stop);
  std::signal(SIGTERM, request_stop);

  const auto result = watch_paths(paths, *config, std::cout, stop_requested);
  if (!result) {
    throw_usage(result.error());
  }
}

void print_presets_command() {
  for (const auto name : preset_names()) {
    std::cout << name << "\n";
  }
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
  auto run_input_paths = std::vector<std::string>{};
  auto run_config_path = std::string{};
  auto run_preset_name = std::string{};
  auto scan_input_paths = std::vector<std::string>{};
  auto scan_config_path = std::string{};
  auto scan_preset_name = std::string{};
  auto watch_input_paths = std::vector<std::string>{};
  auto watch_config_path = std::string{};
  auto watch_preset_name = std::string{};

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

  auto* run_command = app.add_subcommand("run", "Optimize image files");
  run_command->add_option("file", run_input_paths, "Image files to optimize")->required();
  run_command->add_option("--config,-c", run_config_path, "TOML configuration file");
  run_command->add_option("--preset", run_preset_name, "Built-in variant preset");
  run_command->callback([&run_input_paths, &run_config_path, &run_preset_name] {
    run_file_command(run_input_paths, run_config_path, run_preset_name);
  });

  auto* scan = app.add_subcommand("scan", "Optimize images under files or directories");
  scan->add_option("path", scan_input_paths, "Files or directories to scan")->required();
  scan->add_option("--config,-c", scan_config_path, "TOML configuration file");
  scan->add_option("--preset", scan_preset_name, "Built-in variant preset");
  scan->callback([&scan_input_paths, &scan_config_path, &scan_preset_name] {
    scan_command(scan_input_paths, scan_config_path, scan_preset_name);
  });

  auto* watch = app.add_subcommand("watch", "Watch files or directories and optimize changes");
  watch->add_option("path", watch_input_paths, "Files or directories to watch");
  watch->add_option("--config,-c", watch_config_path, "TOML configuration file");
  watch->add_option("--preset", watch_preset_name, "Built-in variant preset");
  watch->callback([&watch_input_paths, &watch_config_path, &watch_preset_name] {
    watch_command(watch_input_paths, watch_config_path, watch_preset_name);
  });

  auto* presets = app.add_subcommand("presets", "Inspect built-in variant presets");
  auto* list_presets = presets->add_subcommand("list", "List built-in variant presets");
  list_presets->callback([] { print_presets_command(); });

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
