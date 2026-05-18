#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <vector>

#include "tinyjpg/core/types.hh"
#include "tinyjpg/image/image.hh"

namespace {

tinyjpg::Image benchmark_image() {
  auto width = tinyjpg::PositiveInt::from(128, "width").value();
  auto height = tinyjpg::PositiveInt::from(128, "height").value();
  auto pixels = std::vector<std::uint8_t>{};
  pixels.resize(128U * 128U * 3U);
  for (auto y = 0; y < 128; ++y) {
    for (auto x = 0; x < 128; ++x) {
      const auto index = (static_cast<std::size_t>(y) * 128U + static_cast<std::size_t>(x)) * 3U;
      const auto pattern = (x * 19) ^ (y * 23) ^ ((x * y) % 241);
      pixels[index] = static_cast<std::uint8_t>((x * 2 + pattern) % 256);
      pixels[index + 1U] = static_cast<std::uint8_t>((y * 3 + pattern / 2) % 256);
      pixels[index + 2U] = static_cast<std::uint8_t>(((x + y) * 2 + pattern / 5) % 256);
    }
  }
  return tinyjpg::Image{.width = width, .height = height, .pixels = std::move(pixels)};
}

struct BenchmarkCase {
  tinyjpg::Codec codec;
  tinyjpg::FidelityMode mode;
  int quality;
  std::string_view label;
};

}  // namespace

int main() {
#if !defined(TINYJPG_HAS_WEBP) || !defined(TINYJPG_HAS_AVIF) || !defined(TINYJPG_HAS_JXL)
  std::cerr << "extra codec dependencies are not enabled\n";
  return 1;
#else
  const auto image = benchmark_image();
  const auto baseline =
      tinyjpg::encode_image(image, tinyjpg::Codec::png, tinyjpg::Quality::from_percent(90).value(),
                            tinyjpg::FidelityMode::lossless, tinyjpg::EffortLevel::fast);
  if (!baseline) {
    std::cerr << baseline.error().message << '\n';
    return 1;
  }

  constexpr auto cases = std::array{
      BenchmarkCase{tinyjpg::Codec::webp, tinyjpg::FidelityMode::lossy, 82, "webp lossy"},
      BenchmarkCase{tinyjpg::Codec::avif, tinyjpg::FidelityMode::lossy, 60, "avif lossy"},
      BenchmarkCase{tinyjpg::Codec::jxl, tinyjpg::FidelityMode::visually_lossless, 90,
                    "jxl visually_lossless"},
  };

  auto failures = 0;
  std::cout << "codec,bytes,baseline_bytes,ratio,elapsed_ms\n";
  for (const auto& entry : cases) {
    const auto started = std::chrono::steady_clock::now();
    const auto encoded = tinyjpg::encode_image(
        image, entry.codec, tinyjpg::Quality::from_percent(entry.quality).value(), entry.mode,
        tinyjpg::EffortLevel::fast);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);
    if (!encoded) {
      std::cerr << entry.label << ": " << encoded.error().message << '\n';
      ++failures;
      continue;
    }

    const auto ratio =
        static_cast<double>(encoded->bytes.size()) / static_cast<double>(baseline->bytes.size());
    std::cout << entry.label << ',' << encoded->bytes.size() << ',' << baseline->bytes.size() << ','
              << std::fixed << std::setprecision(3) << ratio << ',' << elapsed.count() << '\n';
    if (encoded->bytes.size() >= baseline->bytes.size()) {
      ++failures;
    }
  }

  return failures == 0 ? 0 : 1;
#endif
}
