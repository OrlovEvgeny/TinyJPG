#include "tinyjpg/app/cli.hh"

#include <CLI/CLI.hpp>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
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

enum class OutputFormat {
  text,
  table,
  json,
};

struct CommandOptions {
  std::string config_path;
  std::string preset_name;
  std::string format_name{"text"};
  bool dry_run{false};
  bool quiet{false};
};

void request_stop(int /*signal*/) { active_stop_requested = 1; }

[[nodiscard]] bool stop_requested() { return active_stop_requested != 0; }

[[noreturn]] void throw_usage(const Error& error) {
  throw CLI::RuntimeError(error.message, static_cast<int>(ExitCode::usage));
}

[[nodiscard]] Result<OutputFormat> parse_output_format(std::string_view value) {
  if (value == "text") {
    return OutputFormat::text;
  }
  if (value == "table") {
    return OutputFormat::table;
  }
  if (value == "json") {
    return OutputFormat::json;
  }
  return unexpected(Error::config("unknown output format: " + std::string{value}));
}

[[nodiscard]] std::string json_escape(std::string_view value) {
  auto escaped = std::string{};
  escaped.reserve(value.size());
  for (const auto character : value) {
    switch (character) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped += character;
        break;
    }
  }
  return escaped;
}

void print_json_string(std::ostream& out, std::string_view value) {
  out << '"' << json_escape(value) << '"';
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

[[nodiscard]] Result<AppConfig> load_command_config(const CommandOptions& options) {
  auto config = load_command_config(options.config_path, options.preset_name);
  if (!config) {
    return unexpected(config.error());
  }
  if (options.dry_run) {
    config->general.dry_run = true;
  }
  return config;
}

void print_process_result_text(const ProcessResult& result, std::ostream& out) {
  for (const auto& variant : result.variants) {
    if (!variant.written) {
      out << result.input_path.string() << " " << variant.variant_name.value() << " skipped "
          << to_string(variant.skip_reason) << "\n";
      continue;
    }

    const auto saved = result.bytes_before > variant.bytes_after
                           ? result.bytes_before - variant.bytes_after
                           : std::uintmax_t{0};
    out << variant.output_path.string() << " " << variant.variant_name.value() << " "
        << variant.width.value() << "x" << variant.height.value() << " " << saved
        << " bytes saved\n";
  }
}

void print_process_result_table_header(std::ostream& out) {
  out << std::left << std::setw(28) << "input" << std::setw(14) << "variant" << std::setw(12)
      << "status" << std::setw(11) << "size" << "output\n";
}

void print_process_result_table(const ProcessResult& result, std::ostream& out) {
  for (const auto& variant : result.variants) {
    const auto status =
        variant.written ? std::string{"written"} : std::string{to_string(variant.skip_reason)};
    auto size = std::ostringstream{};
    size << variant.width.value() << "x" << variant.height.value();
    out << std::left << std::setw(28) << result.input_path.filename().string() << std::setw(14)
        << variant.variant_name.value() << std::setw(12) << status << std::setw(11) << size.str()
        << variant.output_path.string() << "\n";
  }
}

void print_process_result_json(const ProcessResult& result, std::ostream& out, bool first_result) {
  if (!first_result) {
    out << ",\n";
  }
  out << "  {\"input\":";
  print_json_string(out, result.input_path.string());
  out << ",\"bytes_before\":" << result.bytes_before << ",\"variants\":[";
  for (std::size_t index = 0; index < result.variants.size(); ++index) {
    const auto& variant = result.variants[index];
    if (index > 0) {
      out << ',';
    }
    out << "{\"name\":";
    print_json_string(out, variant.variant_name.value());
    out << ",\"output\":";
    print_json_string(out, variant.output_path.string());
    out << ",\"codec\":";
    print_json_string(out, to_string(variant.codec));
    out << ",\"width\":" << variant.width.value() << ",\"height\":" << variant.height.value()
        << ",\"written\":" << (variant.written ? "true" : "false") << ",\"skip_reason\":";
    print_json_string(out, to_string(variant.skip_reason));
    out << ",\"bytes_after\":" << variant.bytes_after << '}';
  }
  out << "]}";
}

void print_summary_json(const ServiceSummary& summary, std::ostream& out) {
  out << "{\"files_seen\":" << summary.files_seen
      << ",\"files_processed\":" << summary.files_processed
      << ",\"files_skipped\":" << summary.files_skipped << ",\"errors\":" << summary.errors
      << "}\n";
}

[[nodiscard]] std::ostream& command_output_stream(bool quiet, OutputFormat format,
                                                  std::ostringstream& quiet_stream) {
  if (quiet || format == OutputFormat::json) {
    return quiet_stream;
  }
  return std::cout;
}

void process_paths(const std::vector<std::filesystem::path>& paths, const AppConfig& config,
                   OutputFormat format, bool quiet) {
  if (quiet && format != OutputFormat::json) {
    for (const auto& input_path : paths) {
      const auto result = process_file(input_path, config);
      if (!result) {
        throw_usage(result.error());
      }
    }
    return;
  }

  if (format == OutputFormat::json) {
    std::cout << "[\n";
  } else if (format == OutputFormat::table) {
    print_process_result_table_header(std::cout);
  }

  auto first_json_result = true;
  for (const auto& input_path : paths) {
    const auto result = process_file(input_path, config);
    if (!result) {
      throw_usage(result.error());
    }
    switch (format) {
      case OutputFormat::text:
        print_process_result_text(*result, std::cout);
        break;
      case OutputFormat::table:
        print_process_result_table(*result, std::cout);
        break;
      case OutputFormat::json:
        print_process_result_json(*result, std::cout, first_json_result);
        first_json_result = false;
        break;
    }
  }

  if (format == OutputFormat::json) {
    std::cout << "\n]\n";
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

void run_file_command(const std::vector<std::string>& input_paths, const CommandOptions& options) {
  auto format = parse_output_format(options.format_name);
  if (!format) {
    throw_usage(format.error());
  }

  auto config = load_command_config(options);
  if (!config) {
    throw_usage(config.error());
  }

  auto paths = path_list(input_paths);
  process_paths(paths, *config, *format, options.quiet);
}

void scan_command(const std::vector<std::string>& input_paths, const CommandOptions& options) {
  auto format = parse_output_format(options.format_name);
  if (!format) {
    throw_usage(format.error());
  }

  auto config = load_command_config(options);
  if (!config) {
    throw_usage(config.error());
  }

  auto paths = path_list(input_paths);
  auto quiet_stream = std::ostringstream{};
  auto& out = command_output_stream(options.quiet, *format, quiet_stream);
  const auto result = scan_paths(paths, *config, out);
  if (!result) {
    throw_usage(result.error());
  }
  if (*format == OutputFormat::json) {
    print_summary_json(*result, std::cout);
  }
}

void watch_command(const std::vector<std::string>& input_paths, const CommandOptions& options) {
  auto format = parse_output_format(options.format_name);
  if (!format) {
    throw_usage(format.error());
  }

  auto config = load_command_config(options);
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

  auto quiet_stream = std::ostringstream{};
  auto& out = command_output_stream(options.quiet, *format, quiet_stream);
  const auto result = watch_paths(paths, *config, out, stop_requested);
  if (!result) {
    throw_usage(result.error());
  }
  if (*format == OutputFormat::json) {
    print_summary_json(*result, std::cout);
  }
}

void print_presets_command(const std::string& format_name) {
  auto format = parse_output_format(format_name);
  if (!format) {
    throw_usage(format.error());
  }

  const auto names = preset_names();
  if (*format == OutputFormat::json) {
    std::cout << '[';
    for (std::size_t index = 0; index < names.size(); ++index) {
      if (index > 0) {
        std::cout << ',';
      }
      print_json_string(std::cout, names[index]);
    }
    std::cout << "]\n";
    return;
  }

  if (*format == OutputFormat::table) {
    std::cout << "preset\n";
  }

  for (const auto name : names) {
    std::cout << name << "\n";
  }
}

void doctor_command(const std::string& format_name) {
  auto format = parse_output_format(format_name);
  if (!format) {
    throw_usage(format.error());
  }

  const auto config = default_config();
  const auto config_ok = config.has_value();
  const auto cwd = std::filesystem::current_path();
  auto error = std::error_code{};
  const auto cwd_ok = std::filesystem::exists(cwd, error) && !error;

  if (*format == OutputFormat::json) {
    std::cout << "{\"version\":";
    print_json_string(std::cout, version_string());
    std::cout << ",\"checks\":[{\"name\":\"default_config\",\"ok\":"
              << (config_ok ? "true" : "false")
              << "},{\"name\":\"current_directory\",\"ok\":" << (cwd_ok ? "true" : "false")
              << "}]}\n";
    return;
  }

  if (*format == OutputFormat::table) {
    std::cout << std::left << std::setw(22) << "check" << "status\n";
    std::cout << std::left << std::setw(22) << "default_config" << (config_ok ? "ok" : "failed")
              << "\n";
    std::cout << std::left << std::setw(22) << "current_directory" << (cwd_ok ? "ok" : "failed")
              << "\n";
    return;
  }

  std::cout << "version " << version_string() << "\n";
  std::cout << "default_config " << (config_ok ? "ok" : "failed") << "\n";
  std::cout << "current_directory " << (cwd_ok ? "ok" : "failed") << "\n";
}

void completion_command(const std::string& shell) {
  static constexpr auto kCommands = "config run scan watch presets doctor completion";
  if (shell == "bash") {
    std::cout << "_tinyjpg_complete() {\n"
              << "  COMPREPLY=( $(compgen -W \"" << kCommands
              << "\" -- \"${COMP_WORDS[COMP_CWORD]}\") )\n"
              << "}\ncomplete -F _tinyjpg_complete tinyjpg\n";
    return;
  }
  if (shell == "zsh") {
    std::cout << "#compdef tinyjpg\n_arguments '1:command:(" << kCommands << ")'\n";
    return;
  }
  if (shell == "fish") {
    std::cout << "complete -c tinyjpg -f -a \"" << kCommands << "\"\n";
    return;
  }

  throw_usage(Error::config("completion shell must be bash, zsh, or fish"));
}

void add_common_run_options(CLI::App& command, CommandOptions& options) {
  command.add_option("--config,-c", options.config_path, "TOML configuration file");
  command.add_option("--preset", options.preset_name, "Built-in variant preset");
  command.add_flag("--dry-run", options.dry_run, "Plan work without writing output files");
  command.add_flag("--quiet,-q", options.quiet, "Suppress per-file output");
  command.add_option("--format", options.format_name, "Output format: text, table, json")
      ->check(CLI::IsMember({"text", "table", "json"}));
}

}  // namespace

std::string_view usage_text() noexcept { return kUsage; }

ExitCode run(std::span<char const* const> args) {
  auto app = CLI::App{"TinyJPG image optimizer"};
  app.set_version_flag("--version,-v", std::string{version_string()});
  app.require_subcommand(0, 1);

  auto printed_defaults = false;
  auto config_path = std::string{};
  auto run_input_paths = std::vector<std::string>{};
  auto run_options = CommandOptions{};
  auto scan_input_paths = std::vector<std::string>{};
  auto scan_options = CommandOptions{};
  auto watch_input_paths = std::vector<std::string>{};
  auto watch_options = CommandOptions{};
  auto presets_format = std::string{"text"};
  auto doctor_format = std::string{"text"};
  auto completion_shell = std::string{};

  auto* config = app.add_subcommand("config", "Inspect and validate configuration");
  auto* print = config->add_subcommand("print", "Print a sample configuration");
  print->add_flag("--defaults", printed_defaults, "Print default values");
  print->callback([&printed_defaults] { print_default_config_command(printed_defaults); });

  auto* validate = config->add_subcommand("validate", "Validate a TOML configuration file");
  validate->add_option("file", config_path, "TOML configuration file")->required();
  validate->callback([&config_path] { validate_config_command(config_path); });

  auto* run_command = app.add_subcommand("run", "Optimize image files");
  run_command->add_option("file", run_input_paths, "Image files to optimize")->required();
  add_common_run_options(*run_command, run_options);
  run_command->callback(
      [&run_input_paths, &run_options] { run_file_command(run_input_paths, run_options); });

  auto* scan = app.add_subcommand("scan", "Optimize images under files or directories");
  scan->add_option("path", scan_input_paths, "Files or directories to scan")->required();
  add_common_run_options(*scan, scan_options);
  scan->callback(
      [&scan_input_paths, &scan_options] { scan_command(scan_input_paths, scan_options); });

  auto* watch = app.add_subcommand("watch", "Watch files or directories and optimize changes");
  watch->add_option("path", watch_input_paths, "Files or directories to watch");
  add_common_run_options(*watch, watch_options);
  watch->callback(
      [&watch_input_paths, &watch_options] { watch_command(watch_input_paths, watch_options); });

  auto* presets = app.add_subcommand("presets", "Inspect built-in variant presets");
  auto* list_presets = presets->add_subcommand("list", "List built-in variant presets");
  list_presets->add_option("--format", presets_format, "Output format: text, table, json")
      ->check(CLI::IsMember({"text", "table", "json"}));
  list_presets->callback([&presets_format] { print_presets_command(presets_format); });

  auto* doctor = app.add_subcommand("doctor", "Check the local TinyJPG runtime");
  doctor->add_option("--format", doctor_format, "Output format: text, table, json")
      ->check(CLI::IsMember({"text", "table", "json"}));
  doctor->callback([&doctor_format] { doctor_command(doctor_format); });

  auto* completion = app.add_subcommand("completion", "Print shell completion script");
  completion->add_option("shell", completion_shell, "Shell: bash, zsh, fish")
      ->required()
      ->check(CLI::IsMember({"bash", "zsh", "fish"}));
  completion->callback([&completion_shell] { completion_command(completion_shell); });

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
