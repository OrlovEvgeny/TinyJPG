#include <algorithm>
#include <cctype>
#include <csetjmp>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <vector>

// jpeglib.h depends on size_t and FILE declarations being visible first.
#include <jpeglib.h>
#include <png.h>
#if defined(TINYJPG_HAS_WEBP)
#include <webp/decode.h>
#include <webp/encode.h>
#endif

#include "tinyjpg/image/image.hh"

namespace tinyjpg {
namespace {

struct FileCloser {
  void operator()(std::FILE* file) const noexcept {
    if (file != nullptr) {
      std::fclose(file);
    }
  }
};

using FilePtr = std::unique_ptr<std::FILE, FileCloser>;

[[nodiscard]] FilePtr open_file(const std::filesystem::path& path, const char* mode) {
#if defined(_WIN32)
  auto* file = static_cast<std::FILE*>(nullptr);
  if (fopen_s(&file, path.string().c_str(), mode) != 0) {
    return FilePtr{};
  }
  return FilePtr{file};
#else
  return FilePtr{std::fopen(path.string().c_str(), mode)};
#endif
}

[[nodiscard]] bool has_extension(const std::filesystem::path& path, std::string_view extension) {
  auto actual = path.extension().string();
  std::ranges::transform(actual, actual.begin(),
                         [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return actual == extension;
}

struct PngReadGuard {
  png_structp png = nullptr;
  png_infop info = nullptr;

  ~PngReadGuard() {
    if (png != nullptr) {
      png_destroy_read_struct(&png, &info, nullptr);
    }
  }
};

struct PngWriteGuard {
  png_structp png = nullptr;
  png_infop info = nullptr;

  ~PngWriteGuard() {
    if (png != nullptr) {
      png_destroy_write_struct(&png, &info);
    }
  }
};

void png_write_to_vector(png_structp png, png_bytep data, png_size_t length) {
  auto* bytes = static_cast<std::vector<std::uint8_t>*>(png_get_io_ptr(png));
  bytes->insert(bytes->end(), data, data + length);
}

void png_flush_vector(png_structp) {}

[[nodiscard]] Result<Image> decode_png(const std::filesystem::path& path) {
  auto file = open_file(path, "rb");
  if (!file) {
    return unexpected(Error::filesystem(path, "failed to open PNG file"));
  }

  auto guard = PngReadGuard{
      .png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr),
      .info = nullptr,
  };
  if (guard.png == nullptr) {
    return unexpected(Error::internal("failed to create PNG reader"));
  }
  guard.info = png_create_info_struct(guard.png);
  if (guard.info == nullptr) {
    return unexpected(Error::internal("failed to create PNG info"));
  }

  if (setjmp(png_jmpbuf(guard.png)) != 0) {
    return unexpected(Error::filesystem(path, "failed to decode PNG file"));
  }

  png_init_io(guard.png, file.get());
  png_read_info(guard.png, guard.info);

  auto width = png_get_image_width(guard.png, guard.info);
  auto height = png_get_image_height(guard.png, guard.info);
  const auto bit_depth = png_get_bit_depth(guard.png, guard.info);
  const auto color_type = png_get_color_type(guard.png, guard.info);

  if (bit_depth == 16) {
    png_set_strip_16(guard.png);
  }
  if (color_type == PNG_COLOR_TYPE_PALETTE) {
    png_set_palette_to_rgb(guard.png);
  }
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
    png_set_expand_gray_1_2_4_to_8(guard.png);
  }
  if (png_get_valid(guard.png, guard.info, PNG_INFO_tRNS) != 0) {
    png_set_tRNS_to_alpha(guard.png);
  }
  if ((color_type & PNG_COLOR_MASK_ALPHA) != 0) {
    png_set_strip_alpha(guard.png);
  }
  if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
    png_set_gray_to_rgb(guard.png);
  }

  png_read_update_info(guard.png, guard.info);

  auto checked_width = PositiveInt::from(static_cast<int>(width), "image.width");
  auto checked_height = PositiveInt::from(static_cast<int>(height), "image.height");
  if (!checked_width) {
    return unexpected(checked_width.error());
  }
  if (!checked_height) {
    return unexpected(checked_height.error());
  }

  const auto row_bytes = png_get_rowbytes(guard.png, guard.info);
  auto storage = std::vector<std::uint8_t>{};
  storage.resize(row_bytes * height);
  auto rows = std::vector<png_bytep>{};
  rows.reserve(height);
  for (auto y = png_uint_32{0}; y < height; ++y) {
    rows.push_back(storage.data() + (static_cast<std::size_t>(y) * row_bytes));
  }

  png_read_image(guard.png, rows.data());
  png_read_end(guard.png, nullptr);

  return Image{
      .width = *checked_width,
      .height = *checked_height,
      .pixels = std::move(storage),
  };
}

struct JpegErrorManager {
  jpeg_error_mgr base;
  std::jmp_buf jump;
  char message[JMSG_LENGTH_MAX]{};
};

void jpeg_error_exit(j_common_ptr info) {
  auto* error = reinterpret_cast<JpegErrorManager*>(info->err);
  (*info->err->format_message)(info, error->message);
  longjmp(error->jump, 1);
}

[[nodiscard]] Result<Image> decode_jpeg(const std::filesystem::path& path) {
  auto file = open_file(path, "rb");
  if (!file) {
    return unexpected(Error::filesystem(path, "failed to open JPEG file"));
  }

  auto info = jpeg_decompress_struct{};
  auto error = JpegErrorManager{};
  info.err = jpeg_std_error(&error.base);
  error.base.error_exit = jpeg_error_exit;

  if (setjmp(error.jump) != 0) {
    jpeg_destroy_decompress(&info);
    return unexpected(Error::filesystem(path, error.message));
  }

  jpeg_create_decompress(&info);
  jpeg_stdio_src(&info, file.get());
  jpeg_read_header(&info, TRUE);
  info.out_color_space = JCS_RGB;
  jpeg_start_decompress(&info);

  auto checked_width = PositiveInt::from(static_cast<int>(info.output_width), "image.width");
  auto checked_height = PositiveInt::from(static_cast<int>(info.output_height), "image.height");
  if (!checked_width) {
    jpeg_destroy_decompress(&info);
    return unexpected(checked_width.error());
  }
  if (!checked_height) {
    jpeg_destroy_decompress(&info);
    return unexpected(checked_height.error());
  }

  const auto row_stride = static_cast<std::size_t>(info.output_width) * 3U;
  auto pixels = std::vector<std::uint8_t>{};
  pixels.resize(row_stride * info.output_height);

  while (info.output_scanline < info.output_height) {
    auto* row = pixels.data() + (static_cast<std::size_t>(info.output_scanline) * row_stride);
    jpeg_read_scanlines(&info, &row, 1);
  }

  jpeg_finish_decompress(&info);
  jpeg_destroy_decompress(&info);

  return Image{
      .width = *checked_width,
      .height = *checked_height,
      .pixels = std::move(pixels),
  };
}

#if defined(TINYJPG_HAS_WEBP)
[[nodiscard]] Result<std::vector<std::uint8_t>> read_binary_file(
    const std::filesystem::path& path) {
  auto in = std::ifstream{path, std::ios::binary};
  if (!in) {
    return unexpected(Error::filesystem(path, "failed to open image file"));
  }
  return std::vector<std::uint8_t>{std::istreambuf_iterator<char>{in},
                                   std::istreambuf_iterator<char>{}};
}

struct WebPBufferDeleter {
  void operator()(std::uint8_t* buffer) const noexcept {
    if (buffer != nullptr) {
      WebPFree(buffer);
    }
  }
};

using WebPBufferPtr = std::unique_ptr<std::uint8_t, WebPBufferDeleter>;

[[nodiscard]] Result<Image> decode_webp(const std::filesystem::path& path) {
  auto bytes = read_binary_file(path);
  if (!bytes) {
    return unexpected(bytes.error());
  }

  auto width = 0;
  auto height = 0;
  if (WebPGetInfo(bytes->data(), bytes->size(), &width, &height) == 0) {
    return unexpected(Error::filesystem(path, "failed to read WebP metadata"));
  }

  auto* decoded = WebPDecodeRGB(bytes->data(), bytes->size(), &width, &height);
  if (decoded == nullptr) {
    return unexpected(Error::filesystem(path, "failed to decode WebP file"));
  }
  auto owned = WebPBufferPtr{decoded};

  auto checked_width = PositiveInt::from(width, "image.width");
  auto checked_height = PositiveInt::from(height, "image.height");
  if (!checked_width) {
    return unexpected(checked_width.error());
  }
  if (!checked_height) {
    return unexpected(checked_height.error());
  }

  const auto byte_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3U;
  return Image{
      .width = *checked_width,
      .height = *checked_height,
      .pixels = std::vector<std::uint8_t>{owned.get(), owned.get() + byte_count},
  };
}
#endif

[[nodiscard]] Result<EncodedImage> encode_png(const Image& image, EffortLevel effort) {
  auto guard = PngWriteGuard{
      .png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr),
      .info = nullptr,
  };
  if (guard.png == nullptr) {
    return unexpected(Error::internal("failed to create PNG writer"));
  }
  guard.info = png_create_info_struct(guard.png);
  if (guard.info == nullptr) {
    return unexpected(Error::internal("failed to create PNG info"));
  }

  if (setjmp(png_jmpbuf(guard.png)) != 0) {
    return unexpected(Error::internal("failed to encode PNG image"));
  }

  auto bytes = std::vector<std::uint8_t>{};
  png_set_write_fn(guard.png, &bytes, png_write_to_vector, png_flush_vector);

  png_set_IHDR(guard.png, guard.info, static_cast<png_uint_32>(image.width.value()),
               static_cast<png_uint_32>(image.height.value()), 8, PNG_COLOR_TYPE_RGB,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
  const auto compression_level = effort == EffortLevel::fast ? 3 : 9;
  png_set_compression_level(guard.png, compression_level);
  if (effort == EffortLevel::max) {
    png_set_filter(guard.png, PNG_FILTER_TYPE_BASE, PNG_ALL_FILTERS);
  }

  png_write_info(guard.png, guard.info);

  auto rows = std::vector<png_const_bytep>{};
  rows.reserve(static_cast<std::size_t>(image.height.value()));
  const auto row_bytes = static_cast<std::size_t>(image.width.value()) * 3U;
  for (auto y = 0; y < image.height.value(); ++y) {
    rows.push_back(image.pixels.data() + (static_cast<std::size_t>(y) * row_bytes));
  }

  png_write_image(guard.png, const_cast<png_bytepp>(rows.data()));
  png_write_end(guard.png, nullptr);

  return EncodedImage{.codec = Codec::png, .bytes = std::move(bytes)};
}

[[nodiscard]] Result<EncodedImage> encode_jpeg(const Image& image, Quality quality) {
  auto info = jpeg_compress_struct{};
  auto error = JpegErrorManager{};
  info.err = jpeg_std_error(&error.base);
  error.base.error_exit = jpeg_error_exit;

  auto* raw_bytes = static_cast<unsigned char*>(nullptr);
  auto raw_size = static_cast<unsigned long>(0);

  if (setjmp(error.jump) != 0) {
    jpeg_destroy_compress(&info);
    std::free(raw_bytes);
    return unexpected(Error::internal(error.message));
  }

  jpeg_create_compress(&info);
  jpeg_mem_dest(&info, &raw_bytes, &raw_size);

  info.image_width = static_cast<JDIMENSION>(image.width.value());
  info.image_height = static_cast<JDIMENSION>(image.height.value());
  info.input_components = 3;
  info.in_color_space = JCS_RGB;

  jpeg_set_defaults(&info);
  jpeg_set_quality(&info, quality.percent(), TRUE);
  info.optimize_coding = TRUE;
  jpeg_simple_progression(&info);
  jpeg_start_compress(&info, TRUE);

  const auto row_stride = static_cast<std::size_t>(image.width.value()) * 3U;
  while (info.next_scanline < info.image_height) {
    auto* row = const_cast<std::uint8_t*>(
        image.pixels.data() + (static_cast<std::size_t>(info.next_scanline) * row_stride));
    jpeg_write_scanlines(&info, &row, 1);
  }

  jpeg_finish_compress(&info);
  auto bytes = std::vector<std::uint8_t>{raw_bytes, raw_bytes + raw_size};
  jpeg_destroy_compress(&info);
  std::free(raw_bytes);

  return EncodedImage{.codec = Codec::jpeg, .bytes = std::move(bytes)};
}

#if defined(TINYJPG_HAS_WEBP)
[[nodiscard]] Result<EncodedImage> encode_webp(const Image& image, Quality quality,
                                               FidelityMode mode) {
  auto* raw_bytes = static_cast<std::uint8_t*>(nullptr);
  const auto width = image.width.value();
  const auto height = image.height.value();
  const auto stride = width * 3;
  const auto byte_count =
      mode == FidelityMode::lossless
          ? WebPEncodeLosslessRGB(image.pixels.data(), width, height, stride, &raw_bytes)
          : WebPEncodeRGB(image.pixels.data(), width, height, stride,
                          static_cast<float>(quality.percent()), &raw_bytes);
  if (byte_count == 0 || raw_bytes == nullptr) {
    return unexpected(Error::internal("failed to encode WebP image"));
  }
  auto owned = WebPBufferPtr{raw_bytes};
  return EncodedImage{
      .codec = Codec::webp,
      .bytes = std::vector<std::uint8_t>{owned.get(), owned.get() + byte_count},
  };
}
#endif

}  // namespace

Result<Codec> codec_from_path(const std::filesystem::path& path) {
  if (has_extension(path, ".png")) {
    return Codec::png;
  }
  if (has_extension(path, ".jpg") || has_extension(path, ".jpeg")) {
    return Codec::jpeg;
  }
#if defined(TINYJPG_HAS_WEBP)
  if (has_extension(path, ".webp")) {
    return Codec::webp;
  }
#endif
  return unexpected(
      Error::unsupported("unsupported image extension: " + path.extension().string()));
}

std::filesystem::path replace_extension_for_codec(std::filesystem::path path, Codec codec) {
  switch (codec) {
    case Codec::jpeg:
      path.replace_extension(".jpg");
      break;
    case Codec::png:
      path.replace_extension(".png");
      break;
    case Codec::webp:
      path.replace_extension(".webp");
      break;
    case Codec::auto_select:
    case Codec::avif:
    case Codec::jxl:
      break;
  }
  return path;
}

Result<Image> decode_image(const std::filesystem::path& path) {
  auto codec = codec_from_path(path);
  if (!codec) {
    return unexpected(codec.error());
  }
  if (*codec == Codec::png) {
    return decode_png(path);
  }
  if (*codec == Codec::jpeg) {
    return decode_jpeg(path);
  }
#if defined(TINYJPG_HAS_WEBP)
  if (*codec == Codec::webp) {
    return decode_webp(path);
  }
#endif
  return unexpected(Error::unsupported("unsupported image codec"));
}

Result<EncodedImage> encode_image(const Image& image, Codec codec, Quality quality,
                                  FidelityMode mode, EffortLevel effort) {
  if (image.pixels.size() != static_cast<std::size_t>(image.width.value()) *
                                 static_cast<std::size_t>(image.height.value()) * 3U) {
    return unexpected(Error::invalid_argument("image pixels must be packed RGB"));
  }
  if (codec == Codec::png) {
    return encode_png(image, effort);
  }
  if (codec == Codec::jpeg) {
    return encode_jpeg(image, quality);
  }
#if defined(TINYJPG_HAS_WEBP)
  if (codec == Codec::webp) {
    return encode_webp(image, quality, mode);
  }
#else
  static_cast<void>(mode);
#endif
  return unexpected(Error::unsupported("unsupported output codec"));
}

}  // namespace tinyjpg
