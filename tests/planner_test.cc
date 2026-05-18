#include "tinyjpg/pipeline/planner.hh"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace {

tinyjpg::PositiveInt positive(int value) {
  return tinyjpg::PositiveInt::from(value, "test").value();
}

}  // namespace

#if defined(TINYJPG_HAS_DOCTEST)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("planner computes contain dimensions without upscaling") {
  auto config = tinyjpg::default_config().value();
  config.variants = {tinyjpg::VariantConfig{
      .name = tinyjpg::VariantName::from("large").value(),
      .codec = tinyjpg::Codec::auto_select,
      .mode = std::nullopt,
      .quality = std::nullopt,
      .max_width = positive(1920),
      .max_height = std::nullopt,
      .fit = tinyjpg::FitMode::contain,
      .suffix = "-large",
  }};

  const auto planned = tinyjpg::plan_variants("photo.jpg",
                                              tinyjpg::SourceImageInfo{
                                                  .width = positive(4000),
                                                  .height = positive(3000),
                                                  .codec = tinyjpg::Codec::jpeg,
                                              },
                                              config);
  REQUIRE(planned.has_value());
  REQUIRE(planned->size() == 1);
  CHECK(planned->front().width.value() == 1920);
  CHECK(planned->front().height.value() == 1440);
  CHECK(planned->front().resized);

  const auto not_upscaled = tinyjpg::plan_variants("small.jpg",
                                                   tinyjpg::SourceImageInfo{
                                                       .width = positive(640),
                                                       .height = positive(480),
                                                       .codec = tinyjpg::Codec::jpeg,
                                                   },
                                                   config);
  REQUIRE(not_upscaled.has_value());
  CHECK(not_upscaled->front().width.value() == 640);
  CHECK(not_upscaled->front().height.value() == 480);
  CHECK_FALSE(not_upscaled->front().resized);
}

TEST_CASE("planner expands output pattern tokens") {
  auto config = tinyjpg::default_config().value();
  config.output.pattern = "{dir}/out/{stem}-{name}{suffix}-{width}x{height}.{ext}";
  config.variants = {tinyjpg::VariantConfig{
      .name = tinyjpg::VariantName::from("medium").value(),
      .codec = tinyjpg::Codec::auto_select,
      .mode = std::nullopt,
      .quality = std::nullopt,
      .max_width = positive(100),
      .max_height = std::nullopt,
      .fit = tinyjpg::FitMode::contain,
      .suffix = "-m",
  }};

  const auto input = std::filesystem::temp_directory_path() / "photo.png";
  const auto planned = tinyjpg::plan_variants(input,
                                              tinyjpg::SourceImageInfo{
                                                  .width = positive(200),
                                                  .height = positive(100),
                                                  .codec = tinyjpg::Codec::png,
                                              },
                                              config);

  REQUIRE(planned.has_value());
  CHECK(planned->front().output_path ==
        (std::filesystem::temp_directory_path() / "out/photo-medium-m-100x50.png")
            .lexically_normal());
}

TEST_CASE("planner versions existing output paths") {
  auto config = tinyjpg::default_config().value();
  config.output.on_exist = tinyjpg::OnExist::version;
  config.variants = {tinyjpg::VariantConfig{
      .name = tinyjpg::VariantName::from("original").value(),
      .codec = tinyjpg::Codec::auto_select,
      .mode = std::nullopt,
      .quality = std::nullopt,
      .max_width = std::nullopt,
      .max_height = std::nullopt,
      .fit = tinyjpg::FitMode::contain,
      .suffix = "-optimized",
  }};

  const auto input = std::filesystem::temp_directory_path() / "tinyjpg-versioned.png";
  const auto existing = std::filesystem::temp_directory_path() / "tinyjpg-versioned-optimized.png";
  {
    auto file = std::ofstream{existing};
    file << "existing";
  }

  const auto planned = tinyjpg::plan_variants(input,
                                              tinyjpg::SourceImageInfo{
                                                  .width = positive(10),
                                                  .height = positive(10),
                                                  .codec = tinyjpg::Codec::png,
                                              },
                                              config);

  REQUIRE(planned.has_value());
  CHECK(planned->front().output_path ==
        (std::filesystem::temp_directory_path() / "tinyjpg-versioned-optimized-1.png"));
  std::filesystem::remove(existing);
}

#else
int main() {
  auto config = tinyjpg::default_config();
  if (!config) {
    return 1;
  }
  auto planned = tinyjpg::plan_variants("photo.jpg",
                                        tinyjpg::SourceImageInfo{
                                            .width = positive(4000),
                                            .height = positive(3000),
                                            .codec = tinyjpg::Codec::jpeg,
                                        },
                                        *config);
  return planned.has_value() ? 0 : 1;
}
#endif
