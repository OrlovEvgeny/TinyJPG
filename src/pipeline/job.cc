#include "tinyjpg/pipeline/job.hh"

#include <filesystem>
#include <string_view>

#include "tinyjpg/image/image.hh"
#include "tinyjpg/image/resize.hh"
#include "tinyjpg/pipeline/planner.hh"
#include "tinyjpg/platform/atomic_file.hh"

namespace tinyjpg {
namespace {

[[nodiscard]] ProcessVariantResult skipped_result(const PlannedVariant& planned,
                                                  ProcessSkipReason reason) {
  return ProcessVariantResult{
      .variant_name = planned.variant.name,
      .output_path = planned.output_path,
      .codec = planned.codec,
      .bytes_after = 0,
      .width = planned.width,
      .height = planned.height,
      .written = false,
      .skip_reason = reason,
  };
}

}  // namespace

std::string_view to_string(ProcessSkipReason value) noexcept {
  switch (value) {
    case ProcessSkipReason::none:
      return "none";
    case ProcessSkipReason::not_smaller:
      return "not_smaller";
    case ProcessSkipReason::exists:
      return "exists";
    case ProcessSkipReason::dry_run:
      return "dry_run";
  }
  return "unknown";
}

Result<ProcessResult> process_file(const std::filesystem::path& input_path,
                                   const AppConfig& config) {
  const auto source_codec = codec_from_path(input_path);
  if (!source_codec) {
    return unexpected(source_codec.error());
  }

  auto image = decode_image(input_path);
  if (!image) {
    return unexpected(image.error());
  }

  const auto bytes_before = std::filesystem::file_size(input_path);
  auto planned = plan_variants(
      input_path,
      SourceImageInfo{.width = image->width, .height = image->height, .codec = *source_codec},
      config);
  if (!planned) {
    return unexpected(planned.error());
  }

  auto result = ProcessResult{
      .input_path = input_path,
      .bytes_before = bytes_before,
      .variants = {},
  };
  result.variants.reserve(planned->size());

  for (const auto& variant : *planned) {
    if (config.output.on_exist == OnExist::skip && std::filesystem::exists(variant.output_path) &&
        variant.output_path.lexically_normal() != input_path.lexically_normal()) {
      result.variants.push_back(skipped_result(variant, ProcessSkipReason::exists));
      continue;
    }

    const auto mode = variant.variant.mode.value_or(config.compress.mode);
    const auto can_repack_jpeg_losslessly = *source_codec == Codec::jpeg &&
                                            variant.codec == Codec::jpeg &&
                                            mode == FidelityMode::lossless && !variant.resized;
    auto encode_variant = [&]() -> Result<EncodedImage> {
      if (can_repack_jpeg_losslessly) {
        return optimize_jpeg_lossless(input_path);
      }

      auto resized = resize_image(*image, variant.variant.max_width, variant.variant.max_height,
                                  variant.variant.fit);
      if (!resized) {
        return unexpected(resized.error());
      }
      return encode_image(*resized, variant.codec, variant.quality, mode, config.compress.effort);
    };
    auto encoded = encode_variant();
    if (!encoded) {
      return unexpected(encoded.error());
    }

    const auto bytes_after = static_cast<std::uintmax_t>(encoded->bytes.size());
    if (config.compress.skip_if_not_smaller && bytes_after >= bytes_before) {
      result.variants.push_back(ProcessVariantResult{
          .variant_name = variant.variant.name,
          .output_path = variant.output_path,
          .codec = variant.codec,
          .bytes_after = bytes_after,
          .width = variant.width,
          .height = variant.height,
          .written = false,
          .skip_reason = ProcessSkipReason::not_smaller,
      });
      continue;
    }

    if (config.general.dry_run) {
      result.variants.push_back(ProcessVariantResult{
          .variant_name = variant.variant.name,
          .output_path = variant.output_path,
          .codec = variant.codec,
          .bytes_after = bytes_after,
          .width = variant.width,
          .height = variant.height,
          .written = false,
          .skip_reason = ProcessSkipReason::dry_run,
      });
      continue;
    }

    auto written = write_file_atomic(variant.output_path, encoded->bytes);
    if (!written) {
      return unexpected(written.error());
    }

    result.variants.push_back(ProcessVariantResult{
        .variant_name = variant.variant.name,
        .output_path = variant.output_path,
        .codec = variant.codec,
        .bytes_after = bytes_after,
        .width = variant.width,
        .height = variant.height,
        .written = true,
        .skip_reason = ProcessSkipReason::none,
    });
  }

  return result;
}

}  // namespace tinyjpg
