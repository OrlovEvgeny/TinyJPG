#include "tinyjpg/service/service.hh"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include "tinyjpg/image/image.hh"

namespace {

std::filesystem::path temp_dir(std::string_view name) {
  auto path = std::filesystem::temp_directory_path() / name;
  std::filesystem::remove_all(path);
  std::filesystem::create_directories(path);
  return path;
}

[[maybe_unused]] tinyjpg::Image test_image() {
  auto width = tinyjpg::PositiveInt::from(24, "width").value();
  auto height = tinyjpg::PositiveInt::from(24, "height").value();
  auto pixels = std::vector<std::uint8_t>{};
  pixels.resize(24U * 24U * 3U);
  for (auto y = 0; y < 24; ++y) {
    for (auto x = 0; x < 24; ++x) {
      const auto index = (static_cast<std::size_t>(y) * 24U + static_cast<std::size_t>(x)) * 3U;
      pixels[index] = static_cast<std::uint8_t>(x * 10);
      pixels[index + 1U] = static_cast<std::uint8_t>(y * 10);
      pixels[index + 2U] = static_cast<std::uint8_t>((x + y) * 5);
    }
  }
  return tinyjpg::Image{.width = width, .height = height, .pixels = std::move(pixels)};
}

[[maybe_unused]] void write_bytes(const std::filesystem::path& path,
                                  const std::vector<std::uint8_t>& bytes) {
  auto out = std::ofstream{path, std::ios::binary | std::ios::trunc};
  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}

}  // namespace

#if defined(TINYJPG_HAS_DOCTEST)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("collect image files skips generated variants") {
  const auto root = temp_dir("tinyjpg-service-collect");
  write_bytes(root / "input.png", {1, 2, 3});
  write_bytes(root / "input-optimized.png", {1, 2, 3});
  write_bytes(root / "note.txt", {1, 2, 3});

  const auto config = tinyjpg::default_config().value();
  const auto roots = std::vector<std::filesystem::path>{root};
  const auto files = tinyjpg::collect_image_files(roots, config);
  REQUIRE(files.has_value());
  REQUIRE(files->size() == 1);
  CHECK(files->front().filename() == "input.png");
}

TEST_CASE("scan uses ledger to skip unchanged files") {
  const auto root = temp_dir("tinyjpg-service-scan");
  const auto image = test_image();
  const auto encoded =
      tinyjpg::encode_image(image, tinyjpg::Codec::png, tinyjpg::Quality::from_percent(82).value(),
                            tinyjpg::EffortLevel::fast);
  REQUIRE(encoded.has_value());
  write_bytes(root / "input.png", encoded->bytes);

  auto config = tinyjpg::default_config().value();
  config.compress.effort = tinyjpg::EffortLevel::max;
  config.general.workers = tinyjpg::NonNegativeInt::from(1, "workers").value();
  const auto roots = std::vector<std::filesystem::path>{root};

  auto first_out = std::ostringstream{};
  const auto first = tinyjpg::scan_paths(roots, config, first_out);
  REQUIRE(first.has_value());
  CHECK(first->files_processed == 1);
  CHECK(std::filesystem::exists(root / ".tinyjpg-ledger"));

  auto second_out = std::ostringstream{};
  const auto second = tinyjpg::scan_paths(roots, config, second_out);
  REQUIRE(second.has_value());
  CHECK(second->files_processed == 0);
  CHECK(second->files_skipped == 1);
}

#else
int main() {
  const auto root = temp_dir("tinyjpg-service-fallback");
  const auto config = tinyjpg::default_config();
  if (!config) {
    return 1;
  }
  const auto roots = std::vector<std::filesystem::path>{root};
  const auto files = tinyjpg::collect_image_files(roots, *config);
  return files.has_value() ? 0 : 1;
}
#endif
