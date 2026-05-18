#include "tinyjpg/service/service.hh"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <ostream>
#include <span>
#include <sstream>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "tinyjpg/image/image.hh"
#include "tinyjpg/pipeline/job.hh"

namespace tinyjpg {
namespace {

using ClockDuration = std::filesystem::file_time_type::duration;
using LedgerTick = long long;

struct FileStamp {
  std::uintmax_t size;
  ClockDuration mtime;
};

struct QueueItem {
  std::filesystem::path path;
};

[[nodiscard]] bool ends_with(std::string_view value, std::string_view suffix) {
  return value.size() >= suffix.size() &&
         value.substr(value.size() - suffix.size(), suffix.size()) == suffix;
}

[[nodiscard]] bool is_supported_image_path(const std::filesystem::path& path) {
  return codec_from_path(path).has_value();
}

[[nodiscard]] bool is_generated_variant_path(const std::filesystem::path& path,
                                             const AppConfig& config) {
  const auto stem = path.stem().string();
  return std::ranges::any_of(config.variants, [&stem](const VariantConfig& variant) {
    const auto suffix = variant.suffix.empty() ? std::string{"-optimized"} : variant.suffix;
    return ends_with(stem, suffix);
  });
}

[[nodiscard]] std::string config_signature(const AppConfig& config) {
  auto out = std::ostringstream{};
  out << config.output.pattern << '|';
  for (const auto& variant : config.variants) {
    out << variant.name.value() << ':' << to_string(variant.codec) << ':' << variant.suffix << ':';
    if (variant.max_width) {
      out << variant.max_width->value();
    }
    out << 'x';
    if (variant.max_height) {
      out << variant.max_height->value();
    }
    out << ':' << to_string(variant.fit) << '|';
  }
  return out.str();
}

[[nodiscard]] Result<FileStamp> file_stamp(const std::filesystem::path& path) {
  auto error = std::error_code{};
  const auto size = std::filesystem::file_size(path, error);
  if (error) {
    return unexpected(Error::filesystem(path, error.message()));
  }
  const auto mtime = std::filesystem::last_write_time(path, error);
  if (error) {
    return unexpected(Error::filesystem(path, error.message()));
  }
  return FileStamp{.size = size, .mtime = mtime.time_since_epoch()};
}

[[nodiscard]] LedgerTick ledger_tick(ClockDuration duration) {
  return static_cast<LedgerTick>(duration.count());
}

class JobLedger {
 public:
  explicit JobLedger(std::filesystem::path path) : path_{std::move(path)} { load(); }

  [[nodiscard]] bool contains_current(const std::filesystem::path& file,
                                      const std::string& signature) const {
    auto stamp = file_stamp(file);
    if (!stamp) {
      return false;
    }
    const auto found = entries_.find(file.lexically_normal().string());
    return found != entries_.end() && found->second.size == stamp->size &&
           found->second.mtime == stamp->mtime && found->second.signature == signature;
  }

  void mark_current(const std::filesystem::path& file, const std::string& signature) {
    auto stamp = file_stamp(file);
    if (!stamp) {
      return;
    }
    entries_[file.lexically_normal().string()] =
        Entry{.size = stamp->size, .mtime = stamp->mtime, .signature = signature};
    flush();
  }

 private:
  struct Entry {
    std::uintmax_t size;
    ClockDuration mtime;
    std::string signature;
  };

  void load() {
    auto in = std::ifstream{path_};
    if (!in) {
      return;
    }

    auto line = std::string{};
    while (std::getline(in, line)) {
      auto path = std::string{};
      auto signature = std::string{};
      auto size = std::uintmax_t{};
      auto ticks = LedgerTick{};
      auto row = std::istringstream{line};
      if (std::getline(row, path, '\t') && row >> size && row.get() == '\t' && row >> ticks &&
          row.get() == '\t' && std::getline(row, signature)) {
        entries_[path] = Entry{.size = size, .mtime = ClockDuration{ticks}, .signature = signature};
      }
    }
  }

  void flush() const {
    if (!path_.parent_path().empty()) {
      std::filesystem::create_directories(path_.parent_path());
    }

    auto out = std::ofstream{path_, std::ios::trunc};
    for (const auto& [path, entry] : entries_) {
      out << path << '\t' << entry.size << '\t' << ledger_tick(entry.mtime) << '\t'
          << entry.signature << '\n';
    }
  }

  std::filesystem::path path_;
  std::unordered_map<std::string, Entry> entries_;
};

class WorkQueue {
 public:
  explicit WorkQueue(std::size_t capacity) : capacity_{std::max<std::size_t>(capacity, 1U)} {}

  [[nodiscard]] bool push(std::filesystem::path path, std::stop_token stop_token) {
    auto lock = std::unique_lock{mutex_};
    const auto ready =
        condition_.wait(lock, stop_token, [this] { return closed_ || queue_.size() < capacity_; });
    if (!ready || closed_) {
      return false;
    }

    queue_.push_back(QueueItem{.path = std::move(path)});
    condition_.notify_one();
    return true;
  }

  [[nodiscard]] std::optional<QueueItem> pop(std::stop_token stop_token) {
    auto lock = std::unique_lock{mutex_};
    const auto ready =
        condition_.wait(lock, stop_token, [this] { return closed_ || !queue_.empty(); });
    if (!ready || queue_.empty()) {
      return std::nullopt;
    }

    auto item = std::move(queue_.front());
    queue_.pop_front();
    condition_.notify_one();
    return item;
  }

  void close() {
    auto lock = std::lock_guard{mutex_};
    closed_ = true;
    condition_.notify_all();
  }

 private:
  std::size_t capacity_;
  std::deque<QueueItem> queue_;
  std::mutex mutex_;
  std::condition_variable_any condition_;
  bool closed_{false};
};

[[nodiscard]] std::filesystem::path ledger_path_for(std::span<const std::filesystem::path> roots,
                                                    const AppConfig& config) {
  if (config.output.directory) {
    return *config.output.directory / ".tinyjpg-ledger";
  }
  if (!roots.empty()) {
    const auto root = roots.front();
    if (std::filesystem::is_directory(root)) {
      return root / ".tinyjpg-ledger";
    }
    if (!root.parent_path().empty()) {
      return root.parent_path() / ".tinyjpg-ledger";
    }
  }
  return ".tinyjpg-ledger";
}

[[nodiscard]] std::size_t worker_count(const AppConfig& config) {
  if (config.general.workers.value() > 0) {
    return static_cast<std::size_t>(config.general.workers.value());
  }
  const auto hardware = std::thread::hardware_concurrency();
  return std::max<std::size_t>(hardware == 0 ? 1U : hardware, 1U);
}

void print_process_result(const ProcessResult& result, std::ostream& out) {
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

[[nodiscard]] bool is_stable(const std::filesystem::path& path, const AppConfig& config) {
  const auto first = file_stamp(path);
  if (!first) {
    return false;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds{config.general.stable_wait_ms.value()});
  const auto second = file_stamp(path);
  return second && first->size == second->size && first->mtime == second->mtime;
}

[[nodiscard]] Result<ServiceSummary> process_with_workers(
    std::span<const std::filesystem::path> files, std::span<const std::filesystem::path> roots,
    const AppConfig& config, std::ostream& out, bool use_stability_check,
    std::stop_token stop_token) {
  auto summary = ServiceSummary{
      .files_seen = files.size(),
      .files_processed = 0,
      .files_skipped = 0,
      .errors = 0,
  };
  auto signature = config_signature(config);
  auto ledger = JobLedger{ledger_path_for(roots, config)};
  auto queue = WorkQueue{static_cast<std::size_t>(config.general.queue_capacity.value())};
  auto mutex = std::mutex{};
  auto workers = std::vector<std::jthread>{};
  const auto count = worker_count(config);
  workers.reserve(count);

  for (auto remaining = count; remaining > 0; --remaining) {
    workers.emplace_back([&](std::stop_token worker_stop) {
      while (auto item = queue.pop(worker_stop)) {
        if (use_stability_check && !is_stable(item->path, config)) {
          auto lock = std::lock_guard{mutex};
          ++summary.files_skipped;
          continue;
        }

        auto result = process_file(item->path, config);
        auto lock = std::lock_guard{mutex};
        if (!result) {
          ++summary.errors;
          out << item->path.string() << " error " << result.error().message << "\n";
          continue;
        }

        ++summary.files_processed;
        ledger.mark_current(item->path, signature);
        print_process_result(*result, out);
      }
    });
  }

  for (const auto& file : files) {
    if (stop_token.stop_requested()) {
      break;
    }
    auto already_processed = false;
    {
      auto lock = std::lock_guard{mutex};
      already_processed = ledger.contains_current(file, signature);
      if (already_processed) {
        ++summary.files_skipped;
      }
    }
    if (already_processed) {
      continue;
    }
    if (!queue.push(file, stop_token)) {
      break;
    }
  }

  queue.close();
  workers.clear();
  return summary;
}

}  // namespace

Result<std::vector<std::filesystem::path>> collect_image_files(
    std::span<const std::filesystem::path> input_paths, const AppConfig& config) {
  auto files = std::vector<std::filesystem::path>{};
  for (const auto& root : input_paths) {
    auto error = std::error_code{};
    if (std::filesystem::is_regular_file(root, error)) {
      if (is_supported_image_path(root) && !is_generated_variant_path(root, config)) {
        files.push_back(root);
      }
      continue;
    }

    if (!std::filesystem::is_directory(root, error)) {
      return unexpected(Error::filesystem(root, "path is not a file or directory"));
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator{root}) {
      if (entry.is_regular_file() && is_supported_image_path(entry.path()) &&
          !is_generated_variant_path(entry.path(), config)) {
        files.push_back(entry.path());
      }
    }
  }
  return files;
}

Result<ServiceSummary> scan_paths(std::span<const std::filesystem::path> input_paths,
                                  const AppConfig& config, std::ostream& out) {
  auto files = collect_image_files(input_paths, config);
  if (!files) {
    return unexpected(files.error());
  }
  return process_with_workers(*files, input_paths, config, out, false, std::stop_token{});
}

Result<ServiceSummary> watch_paths(std::span<const std::filesystem::path> input_paths,
                                   const AppConfig& config, std::ostream& out,
                                   std::stop_token stop_token) {
  auto summary = ServiceSummary{
      .files_seen = 0,
      .files_processed = 0,
      .files_skipped = 0,
      .errors = 0,
  };

  while (!stop_token.stop_requested()) {
    auto files = collect_image_files(input_paths, config);
    if (!files) {
      return unexpected(files.error());
    }

    auto pass = process_with_workers(*files, input_paths, config, out, true, stop_token);
    if (!pass) {
      return unexpected(pass.error());
    }

    summary.files_seen += pass->files_seen;
    summary.files_processed += pass->files_processed;
    summary.files_skipped += pass->files_skipped;
    summary.errors += pass->errors;
    std::this_thread::sleep_for(std::chrono::milliseconds{250});
  }

  return summary;
}

}  // namespace tinyjpg
