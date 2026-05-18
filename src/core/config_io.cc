#include "tinyjpg/core/config_io.hh"

#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <toml++/toml.hpp>
#include <utility>
#include <vector>

namespace tinyjpg {
namespace {

[[nodiscard]] Result<int> checked_int(std::int64_t value, std::string_view field_name) {
  if (value < 0 || value > 1'000'000) {
    return unexpected(Error::config(std::string{field_name} + " is outside the valid range"));
  }
  return static_cast<int>(value);
}

template <typename Node>
[[nodiscard]] std::vector<std::string> string_array_or_empty(toml::node_view<Node> node) {
  auto values = std::vector<std::string>{};
  const auto* array = node.as_array();
  if (array == nullptr) {
    return values;
  }

  for (const auto& item : *array) {
    if (auto value = item.template value<std::string>()) {
      values.push_back(*std::move(value));
    }
  }
  return values;
}

template <typename Node>
[[nodiscard]] std::vector<std::filesystem::path> path_array_or_empty(toml::node_view<Node> node) {
  auto values = std::vector<std::filesystem::path>{};
  for (const auto& value : string_array_or_empty(node)) {
    values.emplace_back(value);
  }
  return values;
}

Result<VariantConfig> parse_variant(const toml::table& table, const CompressionConfig& global) {
  static_cast<void>(global);
  const auto name_text = table["name"].value_or(std::string{});
  auto name = VariantName::from(name_text);
  if (!name) {
    return unexpected(name.error());
  }

  auto codec = Codec::auto_select;
  if (auto text = table["codec"].value<std::string>()) {
    auto parsed = parse_codec(*text);
    if (!parsed) {
      return unexpected(parsed.error());
    }
    codec = *parsed;
  }

  auto mode = std::optional<FidelityMode>{};
  if (auto text = table["mode"].value<std::string>()) {
    auto parsed = parse_fidelity_mode(*text);
    if (!parsed) {
      return unexpected(parsed.error());
    }
    mode = *parsed;
  }

  auto quality = std::optional<Quality>{};
  if (auto value = table["quality"].value<std::int64_t>()) {
    auto checked = checked_int(*value, "variant.quality");
    if (!checked) {
      return unexpected(checked.error());
    }
    auto parsed = Quality::from_percent(*checked);
    if (!parsed) {
      return unexpected(parsed.error());
    }
    quality = *parsed;
  }

  auto max_width = std::optional<PositiveInt>{};
  if (auto value = table["max_width"].value<std::int64_t>()) {
    auto checked = checked_int(*value, "variant.max_width");
    if (!checked) {
      return unexpected(checked.error());
    }
    auto parsed = PositiveInt::from(*checked, "variant.max_width");
    if (!parsed) {
      return unexpected(parsed.error());
    }
    max_width = *parsed;
  }

  auto max_height = std::optional<PositiveInt>{};
  if (auto value = table["max_height"].value<std::int64_t>()) {
    auto checked = checked_int(*value, "variant.max_height");
    if (!checked) {
      return unexpected(checked.error());
    }
    auto parsed = PositiveInt::from(*checked, "variant.max_height");
    if (!parsed) {
      return unexpected(parsed.error());
    }
    max_height = *parsed;
  }

  auto fit = FitMode::contain;
  if (auto text = table["fit"].value<std::string>()) {
    auto parsed = parse_fit_mode(*text);
    if (!parsed) {
      return unexpected(parsed.error());
    }
    fit = *parsed;
  }

  return VariantConfig{
      .name = *std::move(name),
      .codec = codec,
      .mode = mode,
      .quality = quality,
      .max_width = max_width,
      .max_height = max_height,
      .fit = fit,
      .suffix = table["suffix"].value_or(std::string{}),
  };
}

void append_string_array(std::ostringstream& out, std::string_view key,
                         const std::vector<std::string>& values) {
  out << key << " = [";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      out << ", ";
    }
    out << '"' << values[index] << '"';
  }
  out << "]\n";
}

}  // namespace

Result<AppConfig> load_toml_config(const std::filesystem::path& path) {
  auto config = default_config();
  if (!config) {
    return unexpected(config.error());
  }

  auto table = toml::table{};
  try {
    table = toml::parse_file(path.string());
  } catch (const toml::parse_error& error) {
    return unexpected(Error::filesystem(path, std::string{error.description()}));
  }

  if (const auto* general = table["general"].as_table()) {
    if (auto value = (*general)["workers"].value<std::int64_t>()) {
      auto checked = checked_int(*value, "general.workers");
      if (!checked) {
        return unexpected(checked.error());
      }
      auto workers = NonNegativeInt::from(*checked, "general.workers");
      if (!workers) {
        return unexpected(workers.error());
      }
      config->general.workers = *workers;
    }

    if (auto value = (*general)["queue_capacity"].value<std::int64_t>()) {
      auto checked = checked_int(*value, "general.queue_capacity");
      if (!checked) {
        return unexpected(checked.error());
      }
      auto capacity = PositiveInt::from(*checked, "general.queue_capacity");
      if (!capacity) {
        return unexpected(capacity.error());
      }
      config->general.queue_capacity = *capacity;
    }

    if (auto value = (*general)["stable_wait_ms"].value<std::int64_t>()) {
      auto checked = checked_int(*value, "general.stable_wait_ms");
      if (!checked) {
        return unexpected(checked.error());
      }
      auto stable_wait = NonNegativeInt::from(*checked, "general.stable_wait_ms");
      if (!stable_wait) {
        return unexpected(stable_wait.error());
      }
      config->general.stable_wait_ms = *stable_wait;
    }

    if (auto value = (*general)["log_level"].value<std::string>()) {
      auto log_level = parse_log_level(*value);
      if (!log_level) {
        return unexpected(log_level.error());
      }
      config->general.log_level = *log_level;
    }

    config->general.dry_run = (*general)["dry_run"].value_or(config->general.dry_run);
  }

  if (const auto* watch = table["watch"].as_table()) {
    config->watch.paths = path_array_or_empty((*watch)["paths"]);
    config->watch.include = string_array_or_empty((*watch)["include"]);
    config->watch.exclude = string_array_or_empty((*watch)["exclude"]);
    config->watch.prefix = string_array_or_empty((*watch)["prefix"]);
    config->watch.recursive = (*watch)["recursive"].value_or(config->watch.recursive);
  }

  if (const auto* compress = table["compress"].as_table()) {
    if (auto value = (*compress)["mode"].value<std::string>()) {
      auto mode = parse_fidelity_mode(*value);
      if (!mode) {
        return unexpected(mode.error());
      }
      config->compress.mode = *mode;
    }
    if (auto value = (*compress)["effort"].value<std::string>()) {
      auto effort = parse_effort_level(*value);
      if (!effort) {
        return unexpected(effort.error());
      }
      config->compress.effort = *effort;
    }

    config->compress.keep_metadata =
        (*compress)["keep_metadata"].value_or(config->compress.keep_metadata);
    config->compress.skip_if_not_smaller =
        (*compress)["skip_if_not_smaller"].value_or(config->compress.skip_if_not_smaller);
    config->compress.preserve_original =
        (*compress)["preserve_original"].value_or(config->compress.preserve_original);
  }

  if (const auto* variants = table["variant"].as_array()) {
    config->variants.clear();
    for (const auto& node : *variants) {
      const auto* variant_table = node.as_table();
      if (variant_table == nullptr) {
        return unexpected(Error::config("variant entries must be tables"));
      }
      auto variant = parse_variant(*variant_table, config->compress);
      if (!variant) {
        return unexpected(variant.error());
      }
      config->variants.push_back(*std::move(variant));
    }
  }

  if (const auto* output = table["output"].as_table()) {
    config->output.pattern = (*output)["pattern"].value_or(config->output.pattern);
    if (auto value = (*output)["directory"].value<std::string>(); value && !value->empty()) {
      config->output.directory = std::filesystem::path{*value};
    }
    if (auto value = (*output)["on_exist"].value<std::string>()) {
      auto on_exist = parse_on_exist(*value);
      if (!on_exist) {
        return unexpected(on_exist.error());
      }
      config->output.on_exist = *on_exist;
    }
  }

  auto valid = validate_config(*config);
  if (!valid) {
    return unexpected(valid.error());
  }

  return config;
}

Result<void> validate_config_file(const std::filesystem::path& path) {
  auto config = load_toml_config(path);
  if (!config) {
    return unexpected(config.error());
  }
  return {};
}

Result<std::string> render_default_config() {
  auto config = default_config();
  if (!config) {
    return unexpected(config.error());
  }

  auto out = std::ostringstream{};
  out << "[general]\n";
  out << "workers = " << config->general.workers.value() << "\n";
  out << "log_level = \"" << to_string(config->general.log_level) << "\"\n";
  out << "queue_capacity = " << config->general.queue_capacity.value() << "\n";
  out << "stable_wait_ms = " << config->general.stable_wait_ms.value() << "\n";
  out << "dry_run = " << (config->general.dry_run ? "true" : "false") << "\n\n";

  out << "[watch]\n";
  out << "paths = []\n";
  out << "recursive = " << (config->watch.recursive ? "true" : "false") << "\n";
  append_string_array(out, "include", config->watch.include);
  append_string_array(out, "exclude", config->watch.exclude);
  append_string_array(out, "prefix", config->watch.prefix);
  out << "\n";

  out << "[compress]\n";
  out << "mode = \"" << to_string(config->compress.mode) << "\"\n";
  out << "effort = \"" << to_string(config->compress.effort) << "\"\n";
  out << "keep_metadata = " << (config->compress.keep_metadata ? "true" : "false") << "\n";
  out << "skip_if_not_smaller = " << (config->compress.skip_if_not_smaller ? "true" : "false")
      << "\n";
  out << "preserve_original = " << (config->compress.preserve_original ? "true" : "false")
      << "\n\n";

  for (const auto& variant : config->variants) {
    out << "[[variant]]\n";
    out << "name = \"" << variant.name.value() << "\"\n";
    out << "codec = \"" << to_string(variant.codec) << "\"\n";
    if (variant.max_width) {
      out << "max_width = " << variant.max_width->value() << "\n";
    }
    if (variant.max_height) {
      out << "max_height = " << variant.max_height->value() << "\n";
    }
    if (variant.quality) {
      out << "quality = " << variant.quality->percent() << "\n";
    }
    if (variant.fit != FitMode::contain) {
      out << "fit = \"" << to_string(variant.fit) << "\"\n";
    }
    out << "suffix = \"" << variant.suffix << "\"\n\n";
  }

  out << "[output]\n";
  out << "pattern = \"" << config->output.pattern << "\"\n";
  out << "directory = \"\"\n";
  out << "on_exist = \"" << to_string(config->output.on_exist) << "\"\n";

  return out.str();
}

}  // namespace tinyjpg
