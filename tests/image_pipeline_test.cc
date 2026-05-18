#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "tinyjpg/image/image.hh"
#include "tinyjpg/pipeline/job.hh"

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

}  // namespace

#if defined(TINYJPG_HAS_DOCTEST)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("png round trip preserves pixels") {
  const auto image = test_image();
  const auto encoded =
      tinyjpg::encode_image(image, tinyjpg::Codec::png, tinyjpg::Quality::from_percent(82).value(),
                            tinyjpg::EffortLevel::max);
  REQUIRE(encoded.has_value());

  const auto path = temp_path("tinyjpg-round-trip.png");
  write_bytes(path, encoded->bytes);

  const auto decoded = tinyjpg::decode_image(path);
  REQUIRE(decoded.has_value());
  CHECK(decoded->width == image.width);
  CHECK(decoded->height == image.height);
  CHECK(decoded->pixels == image.pixels);
}

TEST_CASE("pipeline writes a smaller optimized png") {
  const auto image = test_image();
  const auto input =
      tinyjpg::encode_image(image, tinyjpg::Codec::png, tinyjpg::Quality::from_percent(82).value(),
                            tinyjpg::EffortLevel::fast);
  REQUIRE(input.has_value());

  const auto input_path = temp_path("tinyjpg-pipeline-input.png");
  const auto output_path = temp_path("tinyjpg-pipeline-input-optimized.png");
  std::filesystem::remove(output_path);
  write_bytes(input_path, input->bytes);

  auto config = tinyjpg::default_config().value();
  config.compress.effort = tinyjpg::EffortLevel::max;
  const auto result = tinyjpg::process_file(input_path, config);
  REQUIRE(result.has_value());
  REQUIRE_FALSE(result->variants.empty());
  const auto& original = result->variants.front();
  REQUIRE(original.written);
  CHECK(original.output_path == output_path);
  CHECK(original.bytes_after < result->bytes_before);

  const auto decoded = tinyjpg::decode_image(output_path);
  REQUIRE(decoded.has_value());
  CHECK(decoded->pixels == image.pixels);
}

TEST_CASE("pipeline writes the configured variant set") {
  const auto image = test_image();
  const auto input =
      tinyjpg::encode_image(image, tinyjpg::Codec::png, tinyjpg::Quality::from_percent(82).value(),
                            tinyjpg::EffortLevel::fast);
  REQUIRE(input.has_value());

  const auto input_path = temp_path("tinyjpg-pipeline-variants.png");
  for (const auto suffix : {"-optimized", "-large", "-medium", "-thumb"}) {
    std::filesystem::remove(temp_path(std::string{"tinyjpg-pipeline-variants"} + suffix + ".png"));
  }
  write_bytes(input_path, input->bytes);

  auto config = tinyjpg::default_config().value();
  config.compress.effort = tinyjpg::EffortLevel::max;
  const auto result = tinyjpg::process_file(input_path, config);
  REQUIRE(result.has_value());
  REQUIRE(result->variants.size() == config.variants.size());

  for (const auto& variant : result->variants) {
    CHECK(variant.codec == tinyjpg::Codec::png);
    CHECK(variant.output_path.extension() == ".png");
  }
}

TEST_CASE("jpeg codec decodes encoded output") {
  const auto image = test_image();
  const auto encoded =
      tinyjpg::encode_image(image, tinyjpg::Codec::jpeg, tinyjpg::Quality::from_percent(90).value(),
                            tinyjpg::EffortLevel::balanced);
  REQUIRE(encoded.has_value());

  const auto path = temp_path("tinyjpg-round-trip.jpg");
  write_bytes(path, encoded->bytes);

  const auto decoded = tinyjpg::decode_image(path);
  REQUIRE(decoded.has_value());
  CHECK(decoded->width == image.width);
  CHECK(decoded->height == image.height);
  CHECK(decoded->pixels.size() == image.pixels.size());
}
#else
int main() {
  auto image = test_image();
  auto encoded =
      tinyjpg::encode_image(image, tinyjpg::Codec::png, tinyjpg::Quality::from_percent(82).value(),
                            tinyjpg::EffortLevel::max);
  if (!encoded) {
    return 1;
  }

  const auto path = temp_path("tinyjpg-round-trip.png");
  write_bytes(path, encoded->bytes);
  auto decoded = tinyjpg::decode_image(path);
  if (!decoded || decoded->pixels != image.pixels) {
    return 1;
  }
  return 0;
}
#endif
