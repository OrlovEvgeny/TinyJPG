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
#include <optional>
#include <span>
#include <string>
#include <vector>

// jpeglib.h depends on size_t and FILE declarations being visible first.
#include <jpeglib.h>
#include <png.h>
#if defined(TINYJPG_HAS_AVIF)
#include <avif/avif.h>
#endif
#if defined(TINYJPG_HAS_JXL)
#include <jxl/codestream_header.h>
#include <jxl/color_encoding.h>
#include <jxl/decode.h>
#include <jxl/encode.h>
#include <jxl/types.h>
#endif
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

#if defined(TINYJPG_HAS_WEBP) || defined(TINYJPG_HAS_AVIF) || defined(TINYJPG_HAS_JXL)
[[nodiscard]] Result<std::vector<std::uint8_t>> read_binary_file(
    const std::filesystem::path& path) {
  auto in = std::ifstream{path, std::ios::binary};
  if (!in) {
    return unexpected(Error::filesystem(path, "failed to open image file"));
  }
  return std::vector<std::uint8_t>{std::istreambuf_iterator<char>{in},
                                   std::istreambuf_iterator<char>{}};
}
#endif

#if defined(TINYJPG_HAS_WEBP)
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

#if defined(TINYJPG_HAS_AVIF)
struct AvifImageDeleter {
  void operator()(avifImage* image) const noexcept {
    if (image != nullptr) {
      avifImageDestroy(image);
    }
  }
};

struct AvifDecoderDeleter {
  void operator()(avifDecoder* decoder) const noexcept {
    if (decoder != nullptr) {
      avifDecoderDestroy(decoder);
    }
  }
};

struct AvifEncoderDeleter {
  void operator()(avifEncoder* encoder) const noexcept {
    if (encoder != nullptr) {
      avifEncoderDestroy(encoder);
    }
  }
};

struct AvifDataGuard {
  avifRWData data = AVIF_DATA_EMPTY;

  ~AvifDataGuard() { avifRWDataFree(&data); }
};

using AvifImagePtr = std::unique_ptr<avifImage, AvifImageDeleter>;
using AvifDecoderPtr = std::unique_ptr<avifDecoder, AvifDecoderDeleter>;
using AvifEncoderPtr = std::unique_ptr<avifEncoder, AvifEncoderDeleter>;

[[nodiscard]] Error avif_error(const char* action, avifResult result) {
  return Error::internal(std::string{action} + ": " + avifResultToString(result));
}

[[nodiscard]] Result<Image> decode_avif(const std::filesystem::path& path) {
  auto bytes = read_binary_file(path);
  if (!bytes) {
    return unexpected(bytes.error());
  }

  auto decoder = AvifDecoderPtr{avifDecoderCreate()};
  if (!decoder) {
    return unexpected(Error::internal("failed to create AVIF decoder"));
  }
  auto decoded = AvifImagePtr{avifImageCreateEmpty()};
  if (!decoded) {
    return unexpected(Error::internal("failed to create AVIF image"));
  }

  const auto result =
      avifDecoderReadMemory(decoder.get(), decoded.get(), bytes->data(), bytes->size());
  if (result != AVIF_RESULT_OK) {
    return unexpected(avif_error("failed to decode AVIF file", result));
  }

  auto checked_width = PositiveInt::from(static_cast<int>(decoded->width), "image.width");
  auto checked_height = PositiveInt::from(static_cast<int>(decoded->height), "image.height");
  if (!checked_width) {
    return unexpected(checked_width.error());
  }
  if (!checked_height) {
    return unexpected(checked_height.error());
  }

  auto pixels = std::vector<std::uint8_t>{};
  pixels.resize(static_cast<std::size_t>(decoded->width) *
                static_cast<std::size_t>(decoded->height) * 3U);

  auto rgb = avifRGBImage{};
  avifRGBImageSetDefaults(&rgb, decoded.get());
  rgb.depth = 8;
  rgb.format = AVIF_RGB_FORMAT_RGB;
  rgb.pixels = pixels.data();
  rgb.rowBytes = decoded->width * 3U;

  const auto conversion = avifImageYUVToRGB(decoded.get(), &rgb);
  if (conversion != AVIF_RESULT_OK) {
    return unexpected(avif_error("failed to convert AVIF pixels", conversion));
  }

  return Image{
      .width = *checked_width,
      .height = *checked_height,
      .pixels = std::move(pixels),
  };
}
#endif

#if defined(TINYJPG_HAS_JXL)
struct JxlDecoderDeleter {
  void operator()(JxlDecoder* decoder) const noexcept {
    if (decoder != nullptr) {
      JxlDecoderDestroy(decoder);
    }
  }
};

struct JxlEncoderDeleter {
  void operator()(JxlEncoder* encoder) const noexcept {
    if (encoder != nullptr) {
      JxlEncoderDestroy(encoder);
    }
  }
};

using JxlDecoderPtr = std::unique_ptr<JxlDecoder, JxlDecoderDeleter>;
using JxlEncoderPtr = std::unique_ptr<JxlEncoder, JxlEncoderDeleter>;

[[nodiscard]] float jxl_distance(Quality quality, FidelityMode mode) noexcept {
  if (mode == FidelityMode::visually_lossless) {
    return 1.0F;
  }
  const auto distance = static_cast<float>(100 - quality.percent()) / 10.0F;
  return std::clamp(distance, 0.5F, 25.0F);
}

[[nodiscard]] int jxl_effort(EffortLevel effort) noexcept {
  switch (effort) {
    case EffortLevel::fast:
      return 4;
    case EffortLevel::balanced:
      return 7;
    case EffortLevel::max:
      return 9;
  }
  return 7;
}

[[nodiscard]] Result<Image> decode_jxl(const std::filesystem::path& path) {
  auto bytes = read_binary_file(path);
  if (!bytes) {
    return unexpected(bytes.error());
  }

  auto decoder = JxlDecoderPtr{JxlDecoderCreate(nullptr)};
  if (!decoder) {
    return unexpected(Error::internal("failed to create JPEG XL decoder"));
  }
  if (JxlDecoderSubscribeEvents(decoder.get(), JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE) !=
      JXL_DEC_SUCCESS) {
    return unexpected(Error::internal("failed to configure JPEG XL decoder"));
  }
  if (JxlDecoderSetInput(decoder.get(), bytes->data(), bytes->size()) != JXL_DEC_SUCCESS) {
    return unexpected(Error::filesystem(path, "failed to read JPEG XL input"));
  }
  JxlDecoderCloseInput(decoder.get());

  auto info = JxlBasicInfo{};
  auto width = std::optional<PositiveInt>{};
  auto height = std::optional<PositiveInt>{};
  auto pixels = std::vector<std::uint8_t>{};
  const auto format = JxlPixelFormat{3, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};

  for (;;) {
    const auto status = JxlDecoderProcessInput(decoder.get());
    if (status == JXL_DEC_ERROR) {
      return unexpected(Error::filesystem(path, "failed to decode JPEG XL file"));
    }
    if (status == JXL_DEC_NEED_MORE_INPUT) {
      return unexpected(Error::filesystem(path, "truncated JPEG XL file"));
    }
    if (status == JXL_DEC_BASIC_INFO) {
      if (JxlDecoderGetBasicInfo(decoder.get(), &info) != JXL_DEC_SUCCESS) {
        return unexpected(Error::filesystem(path, "failed to read JPEG XL metadata"));
      }
      auto checked_width = PositiveInt::from(static_cast<int>(info.xsize), "image.width");
      auto checked_height = PositiveInt::from(static_cast<int>(info.ysize), "image.height");
      if (!checked_width) {
        return unexpected(checked_width.error());
      }
      if (!checked_height) {
        return unexpected(checked_height.error());
      }
      width = *checked_width;
      height = *checked_height;
      continue;
    }
    if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
      auto buffer_size = std::size_t{};
      if (JxlDecoderImageOutBufferSize(decoder.get(), &format, &buffer_size) != JXL_DEC_SUCCESS) {
        return unexpected(Error::filesystem(path, "failed to size JPEG XL output"));
      }
      pixels.resize(buffer_size);
      if (JxlDecoderSetImageOutBuffer(decoder.get(), &format, pixels.data(), pixels.size()) !=
          JXL_DEC_SUCCESS) {
        return unexpected(Error::filesystem(path, "failed to set JPEG XL output"));
      }
      continue;
    }
    if (status == JXL_DEC_FULL_IMAGE) {
      continue;
    }
    if (status == JXL_DEC_SUCCESS) {
      break;
    }
    return unexpected(Error::internal("unexpected JPEG XL decoder state"));
  }

  if (!width || !height) {
    return unexpected(Error::filesystem(path, "missing JPEG XL dimensions"));
  }
  const auto expected_size =
      static_cast<std::size_t>(width->value()) * static_cast<std::size_t>(height->value()) * 3U;
  if (pixels.size() != expected_size) {
    return unexpected(Error::filesystem(path, "unexpected JPEG XL output size"));
  }
  return Image{
      .width = *width,
      .height = *height,
      .pixels = std::move(pixels),
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

[[nodiscard]] Result<EncodedImage> encode_jpeg(const Image& image, Quality quality,
                                               FidelityMode mode) {
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
  const auto jpeg_quality =
      mode == FidelityMode::lossless ? std::max(quality.percent(), 95) : quality.percent();
  jpeg_set_quality(&info, jpeg_quality, TRUE);
  if (mode == FidelityMode::lossless) {
    for (auto component = 0; component < info.num_components; ++component) {
      info.comp_info[component].h_samp_factor = 1;
      info.comp_info[component].v_samp_factor = 1;
    }
  }
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

}  // namespace

Result<EncodedImage> optimize_jpeg_lossless(const std::filesystem::path& path) {
  auto file = open_file(path, "rb");
  if (!file) {
    return unexpected(Error::filesystem(path, "failed to open JPEG file"));
  }

  auto source = jpeg_decompress_struct{};
  auto output = jpeg_compress_struct{};
  auto error = JpegErrorManager{};
  source.err = jpeg_std_error(&error.base);
  output.err = source.err;
  error.base.error_exit = jpeg_error_exit;

  auto* raw_bytes = static_cast<unsigned char*>(nullptr);
  auto raw_size = static_cast<unsigned long>(0);
  auto source_created = false;
  auto output_created = false;

  if (setjmp(error.jump) != 0) {
    if (output_created) {
      jpeg_destroy_compress(&output);
    }
    if (source_created) {
      jpeg_destroy_decompress(&source);
    }
    std::free(raw_bytes);
    return unexpected(Error::filesystem(path, error.message));
  }

  jpeg_create_decompress(&source);
  source_created = true;
  jpeg_stdio_src(&source, file.get());
  jpeg_read_header(&source, TRUE);
  auto* coefficients = jpeg_read_coefficients(&source);

  jpeg_create_compress(&output);
  output_created = true;
  jpeg_mem_dest(&output, &raw_bytes, &raw_size);
  jpeg_copy_critical_parameters(&source, &output);
  output.optimize_coding = TRUE;
  jpeg_simple_progression(&output);
  jpeg_write_coefficients(&output, coefficients);
  jpeg_finish_compress(&output);
  jpeg_finish_decompress(&source);

  auto bytes = std::vector<std::uint8_t>{raw_bytes, raw_bytes + raw_size};
  jpeg_destroy_compress(&output);
  jpeg_destroy_decompress(&source);
  std::free(raw_bytes);

  return EncodedImage{.codec = Codec::jpeg, .bytes = std::move(bytes)};
}

namespace {

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

#if defined(TINYJPG_HAS_AVIF)
[[nodiscard]] Result<EncodedImage> encode_avif(const Image& image, Quality quality,
                                               FidelityMode mode, EffortLevel effort) {
  const auto width = image.width.value();
  const auto height = image.height.value();
  auto avif_image = AvifImagePtr{avifImageCreate(static_cast<std::uint32_t>(width),
                                                 static_cast<std::uint32_t>(height), 8,
                                                 AVIF_PIXEL_FORMAT_YUV444)};
  if (!avif_image) {
    return unexpected(Error::internal("failed to create AVIF image"));
  }

  avif_image->colorPrimaries = AVIF_COLOR_PRIMARIES_SRGB;
  avif_image->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_SRGB;
  avif_image->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_IDENTITY;
  avif_image->yuvRange = AVIF_RANGE_FULL;

  auto rgb = avifRGBImage{};
  avifRGBImageSetDefaults(&rgb, avif_image.get());
  rgb.depth = 8;
  rgb.format = AVIF_RGB_FORMAT_RGB;
  rgb.chromaDownsampling = mode == FidelityMode::lossy ? AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC
                                                       : AVIF_CHROMA_DOWNSAMPLING_BEST_QUALITY;
  rgb.pixels = const_cast<std::uint8_t*>(image.pixels.data());
  rgb.rowBytes = static_cast<std::uint32_t>(width * 3);

  const auto conversion = avifImageRGBToYUV(avif_image.get(), &rgb);
  if (conversion != AVIF_RESULT_OK) {
    return unexpected(avif_error("failed to convert RGB pixels to AVIF", conversion));
  }

  auto encoder = AvifEncoderPtr{avifEncoderCreate()};
  if (!encoder) {
    return unexpected(Error::internal("failed to create AVIF encoder"));
  }
  encoder->quality = mode == FidelityMode::lossless ? AVIF_QUALITY_LOSSLESS : quality.percent();
  encoder->qualityAlpha = AVIF_QUALITY_LOSSLESS;
  encoder->speed = effort == EffortLevel::fast       ? AVIF_SPEED_FASTEST
                   : effort == EffortLevel::balanced ? 6
                                                     : AVIF_SPEED_SLOWEST;

  auto output = AvifDataGuard{};
  const auto result = avifEncoderWrite(encoder.get(), avif_image.get(), &output.data);
  if (result != AVIF_RESULT_OK) {
    return unexpected(avif_error("failed to encode AVIF image", result));
  }

  return EncodedImage{
      .codec = Codec::avif,
      .bytes = std::vector<std::uint8_t>{output.data.data, output.data.data + output.data.size},
  };
}
#endif

#if defined(TINYJPG_HAS_JXL)
[[nodiscard]] Result<EncodedImage> encode_jxl(const Image& image, Quality quality,
                                              FidelityMode mode, EffortLevel effort) {
  auto encoder = JxlEncoderPtr{JxlEncoderCreate(nullptr)};
  if (!encoder) {
    return unexpected(Error::internal("failed to create JPEG XL encoder"));
  }

  auto info = JxlBasicInfo{};
  JxlEncoderInitBasicInfo(&info);
  info.xsize = static_cast<std::uint32_t>(image.width.value());
  info.ysize = static_cast<std::uint32_t>(image.height.value());
  info.bits_per_sample = 8;
  info.exponent_bits_per_sample = 0;
  info.num_color_channels = 3;
  info.uses_original_profile = mode == FidelityMode::lossless ? JXL_TRUE : JXL_FALSE;
  if (JxlEncoderSetBasicInfo(encoder.get(), &info) != JXL_ENC_SUCCESS) {
    return unexpected(Error::internal("failed to configure JPEG XL metadata"));
  }

  auto color = JxlColorEncoding{};
  JxlColorEncodingSetToSRGB(&color, JXL_FALSE);
  if (JxlEncoderSetColorEncoding(encoder.get(), &color) != JXL_ENC_SUCCESS) {
    return unexpected(Error::internal("failed to configure JPEG XL color"));
  }

  auto* frame = JxlEncoderFrameSettingsCreate(encoder.get(), nullptr);
  if (frame == nullptr) {
    return unexpected(Error::internal("failed to create JPEG XL frame settings"));
  }
  if (JxlEncoderFrameSettingsSetOption(frame, JXL_ENC_FRAME_SETTING_EFFORT, jxl_effort(effort)) !=
      JXL_ENC_SUCCESS) {
    return unexpected(Error::internal("failed to configure JPEG XL effort"));
  }
  if (mode == FidelityMode::lossless) {
    if (JxlEncoderSetFrameLossless(frame, JXL_TRUE) != JXL_ENC_SUCCESS) {
      return unexpected(Error::internal("failed to configure lossless JPEG XL"));
    }
  } else if (JxlEncoderSetFrameDistance(frame, jxl_distance(quality, mode)) != JXL_ENC_SUCCESS) {
    return unexpected(Error::internal("failed to configure JPEG XL quality"));
  }

  const auto format = JxlPixelFormat{3, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
  if (JxlEncoderAddImageFrame(frame, &format, image.pixels.data(), image.pixels.size()) !=
      JXL_ENC_SUCCESS) {
    return unexpected(Error::internal("failed to encode JPEG XL frame"));
  }
  JxlEncoderCloseInput(encoder.get());

  auto bytes = std::vector<std::uint8_t>{};
  bytes.resize(4096);
  auto* next = bytes.data();
  auto available = bytes.size();
  auto status = JXL_ENC_NEED_MORE_OUTPUT;
  while (status == JXL_ENC_NEED_MORE_OUTPUT) {
    status = JxlEncoderProcessOutput(encoder.get(), &next, &available);
    if (status == JXL_ENC_NEED_MORE_OUTPUT) {
      const auto offset = static_cast<std::size_t>(next - bytes.data());
      bytes.resize(bytes.size() * 2U);
      next = bytes.data() + offset;
      available = bytes.size() - offset;
    }
  }
  if (status != JXL_ENC_SUCCESS) {
    return unexpected(Error::internal("failed to finalize JPEG XL image"));
  }
  bytes.resize(static_cast<std::size_t>(next - bytes.data()));

  return EncodedImage{
      .codec = Codec::jxl,
      .bytes = std::move(bytes),
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
#if defined(TINYJPG_HAS_AVIF)
  if (has_extension(path, ".avif")) {
    return Codec::avif;
  }
#endif
#if defined(TINYJPG_HAS_JXL)
  if (has_extension(path, ".jxl")) {
    return Codec::jxl;
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
    case Codec::avif:
      path.replace_extension(".avif");
      break;
    case Codec::jxl:
      path.replace_extension(".jxl");
      break;
    case Codec::auto_select:
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
#if defined(TINYJPG_HAS_AVIF)
  if (*codec == Codec::avif) {
    return decode_avif(path);
  }
#endif
#if defined(TINYJPG_HAS_JXL)
  if (*codec == Codec::jxl) {
    return decode_jxl(path);
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
    return encode_jpeg(image, quality, mode);
  }
#if defined(TINYJPG_HAS_WEBP)
  if (codec == Codec::webp) {
    return encode_webp(image, quality, mode);
  }
#endif
#if defined(TINYJPG_HAS_AVIF)
  if (codec == Codec::avif) {
    return encode_avif(image, quality, mode, effort);
  }
#endif
#if defined(TINYJPG_HAS_JXL)
  if (codec == Codec::jxl) {
    return encode_jxl(image, quality, mode, effort);
  }
#endif
  static_cast<void>(mode);
  return unexpected(Error::unsupported("unsupported output codec"));
}

}  // namespace tinyjpg
