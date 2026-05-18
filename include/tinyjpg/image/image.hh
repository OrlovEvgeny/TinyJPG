#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "tinyjpg/core/error.hh"
#include "tinyjpg/core/types.hh"

namespace tinyjpg {

struct Image {
  PositiveInt width;
  PositiveInt height;
  std::vector<std::uint8_t> pixels;
};

struct EncodedImage {
  Codec codec;
  std::vector<std::uint8_t> bytes;
};

[[nodiscard]] Result<Codec> codec_from_path(const std::filesystem::path& path);
[[nodiscard]] std::filesystem::path replace_extension_for_codec(std::filesystem::path path,
                                                                Codec codec);
[[nodiscard]] Result<Image> decode_image(const std::filesystem::path& path);
[[nodiscard]] Result<EncodedImage> encode_image(const Image& image, Codec codec, Quality quality,
                                                EffortLevel effort);

}  // namespace tinyjpg
