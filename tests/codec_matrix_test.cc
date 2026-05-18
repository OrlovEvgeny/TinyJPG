#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "tinyjpg/codec/registry.hh"
#include "tinyjpg/core/types.hh"
#include "tinyjpg/image/image.hh"

namespace {

std::filesystem::path temp_path(std::string_view name) {
  return std::filesystem::temp_directory_path() / name;
}

tinyjpg::Image test_image() {
  auto width = tinyjpg::PositiveInt::from(32, "width").value();
  auto height = tinyjpg::PositiveInt::from(32, "height").value();
  auto pixels = std::vector<std::uint8_t>{};
  pixels.resize(32U * 32U * 3U);
  for (auto y = 0; y < 32; ++y) {
    for (auto x = 0; x < 32; ++x) {
      const auto index = (static_cast<std::size_t>(y) * 32U + static_cast<std::size_t>(x)) * 3U;
      pixels[index] = static_cast<std::uint8_t>(x * 8);
      pixels[index + 1U] = static_cast<std::uint8_t>(y * 8);
      pixels[index + 2U] = static_cast<std::uint8_t>((x + y) * 4);
    }
  }
  return tinyjpg::Image{.width = width, .height = height, .pixels = std::move(pixels)};
}

void write_bytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
  auto out = std::ofstream{path, std::ios::binary | std::ios::trunc};
  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}

struct MatrixCase {
  tinyjpg::Codec codec;
  tinyjpg::FidelityMode mode;
  bool exact_pixels;
};

}  // namespace

int main() {
#if !defined(TINYJPG_HAS_WEBP) || !defined(TINYJPG_HAS_AVIF) || !defined(TINYJPG_HAS_JXL)
  std::cerr << "extra codec dependencies are not enabled\n";
  return 1;
#else
  const auto image = test_image();
  const auto quality = tinyjpg::Quality::from_percent(90).value();
  constexpr auto cases = std::array{
      MatrixCase{tinyjpg::Codec::webp, tinyjpg::FidelityMode::lossless, true},
      MatrixCase{tinyjpg::Codec::webp, tinyjpg::FidelityMode::lossy, false},
      MatrixCase{tinyjpg::Codec::avif, tinyjpg::FidelityMode::lossless, true},
      MatrixCase{tinyjpg::Codec::avif, tinyjpg::FidelityMode::lossy, false},
      MatrixCase{tinyjpg::Codec::jxl, tinyjpg::FidelityMode::lossless, true},
      MatrixCase{tinyjpg::Codec::jxl, tinyjpg::FidelityMode::visually_lossless, false},
      MatrixCase{tinyjpg::Codec::jxl, tinyjpg::FidelityMode::lossy, false},
  };

  auto failures = 0;
  for (const auto& entry : cases) {
    const auto encoded =
        tinyjpg::encode_image(image, entry.codec, quality, entry.mode, tinyjpg::EffortLevel::fast);
    if (!encoded) {
      std::cerr << tinyjpg::to_string(entry.codec) << ' ' << tinyjpg::to_string(entry.mode) << ": "
                << encoded.error().message << '\n';
      ++failures;
      continue;
    }

    const auto path =
        temp_path(std::string{"tinyjpg-matrix-"} + std::string{tinyjpg::to_string(entry.codec)} +
                  "-" + std::string{tinyjpg::to_string(entry.mode)} +
                  std::string{tinyjpg::codec_extension(entry.codec)});
    write_bytes(path, encoded->bytes);
    const auto decoded = tinyjpg::decode_image(path);
    if (!decoded) {
      std::cerr << path.string() << ": " << decoded.error().message << '\n';
      ++failures;
      continue;
    }
    if (decoded->width != image.width || decoded->height != image.height ||
        decoded->pixels.size() != image.pixels.size()) {
      std::cerr << tinyjpg::to_string(entry.codec) << ' ' << tinyjpg::to_string(entry.mode)
                << ": decoded dimensions or pixel count changed\n";
      ++failures;
      continue;
    }
    if (entry.exact_pixels && decoded->pixels != image.pixels) {
      std::cerr << tinyjpg::to_string(entry.codec) << ' ' << tinyjpg::to_string(entry.mode)
                << ": lossless pixels changed\n";
      ++failures;
    }
  }

  return failures == 0 ? 0 : 1;
#endif
}
