#include "tinyjpg/pipeline/job.hh"

#include <algorithm>
#include <filesystem>
#include <iterator>

#include "tinyjpg/codec/registry.hh"
#include "tinyjpg/image/image.hh"
#include "tinyjpg/image/resize.hh"
#include "tinyjpg/platform/atomic_file.hh"

namespace tinyjpg {
namespace {

[[nodiscard]] const VariantConfig* select_original_variant(const AppConfig& config) {
  const auto found = std::ranges::find_if(config.variants, [](const VariantConfig& variant) {
    return variant.name.value() == "original";
  });
  if (found == config.variants.end()) {
    return nullptr;
  }
  return std::to_address(found);
}

[[nodiscard]] Result<Quality> effective_quality(const VariantConfig* variant) {
  if (variant != nullptr && variant->quality) {
    return *variant->quality;
  }
  return Quality::from_percent(82);
}

[[nodiscard]] std::filesystem::path output_path_for(const std::filesystem::path& input_path,
                                                    const AppConfig& config,
                                                    const VariantConfig* variant, Codec codec) {
  const auto directory = config.output.directory.value_or(input_path.parent_path());
  const auto suffix =
      variant == nullptr || variant->suffix.empty() ? "-optimized" : variant->suffix;
  auto output = directory / (input_path.stem().string() + suffix + input_path.extension().string());
  return replace_extension_for_codec(std::move(output), codec);
}

}  // namespace

Result<ProcessResult> process_file(const std::filesystem::path& input_path,
                                   const AppConfig& config) {
  const auto source_codec = codec_from_path(input_path);
  if (!source_codec) {
    return unexpected(source_codec.error());
  }

  const auto registry = default_codec_registry();
  const auto* variant = select_original_variant(config);
  const auto output_codec =
      variant != nullptr && variant->codec != Codec::auto_select ? variant->codec : *source_codec;
  if (!registry.supports(output_codec)) {
    return unexpected(Error::unsupported("selected codec is not available: " +
                                         std::string{to_string(output_codec)}));
  }

  const auto quality = effective_quality(variant);
  if (!quality) {
    return unexpected(quality.error());
  }

  auto image = decode_image(input_path);
  if (!image) {
    return unexpected(image.error());
  }

  auto resized = resize_image(*image, variant == nullptr ? std::nullopt : variant->max_width,
                              variant == nullptr ? std::nullopt : variant->max_height,
                              variant == nullptr ? FitMode::contain : variant->fit);
  if (!resized) {
    return unexpected(resized.error());
  }

  auto encoded = encode_image(*resized, output_codec, *quality, config.compress.effort);
  if (!encoded) {
    return unexpected(encoded.error());
  }

  const auto bytes_before = std::filesystem::file_size(input_path);
  const auto output_path = output_path_for(input_path, config, variant, output_codec);
  const auto bytes_after = static_cast<std::uintmax_t>(encoded->bytes.size());
  if (config.compress.skip_if_not_smaller && bytes_after >= bytes_before) {
    return ProcessResult{
        .input_path = input_path,
        .output_path = output_path,
        .codec = output_codec,
        .bytes_before = bytes_before,
        .bytes_after = bytes_after,
        .written = false,
    };
  }

  auto written = write_file_atomic(output_path, encoded->bytes);
  if (!written) {
    return unexpected(written.error());
  }

  return ProcessResult{
      .input_path = input_path,
      .output_path = output_path,
      .codec = output_codec,
      .bytes_before = bytes_before,
      .bytes_after = bytes_after,
      .written = true,
  };
}

}  // namespace tinyjpg
