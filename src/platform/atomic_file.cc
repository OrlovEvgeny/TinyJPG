#include "tinyjpg/platform/atomic_file.hh"

#include <chrono>
#include <fstream>
#include <string>

namespace tinyjpg {
namespace {

[[nodiscard]] std::filesystem::path temp_path_for(const std::filesystem::path& path) {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  auto temp = path;
  temp += ".tmp-" + std::to_string(stamp);
  return temp;
}

}  // namespace

Result<void> write_file_atomic(const std::filesystem::path& path,
                               std::span<const std::uint8_t> bytes) {
  const auto parent = path.parent_path();
  if (!parent.empty()) {
    auto error = std::error_code{};
    std::filesystem::create_directories(parent, error);
    if (error) {
      return unexpected(Error::filesystem(parent, error.message()));
    }
  }

  const auto temp = temp_path_for(path);
  {
    auto out = std::ofstream{temp, std::ios::binary | std::ios::trunc};
    if (!out) {
      return unexpected(Error::filesystem(temp, "failed to open temporary file for writing"));
    }

    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.flush();
    if (!out) {
      return unexpected(Error::filesystem(temp, "failed to write temporary file"));
    }
  }

  auto error = std::error_code{};
  std::filesystem::rename(temp, path, error);
  if (error) {
    std::filesystem::remove(temp);
    return unexpected(Error::filesystem(path, error.message()));
  }

  return {};
}

}  // namespace tinyjpg
