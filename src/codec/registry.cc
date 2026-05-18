#include "tinyjpg/codec/registry.hh"

#include <algorithm>
#include <array>
#include <ranges>
#include <string_view>
#include <vector>

namespace tinyjpg {
namespace {

constexpr auto kSupportedCodecs = std::array{
    Codec::jpeg, Codec::png,
#if defined(TINYJPG_HAS_WEBP)
    Codec::webp,
#endif
#if defined(TINYJPG_HAS_AVIF)
    Codec::avif,
#endif
#if defined(TINYJPG_HAS_JXL)
    Codec::jxl,
#endif
};

}  // namespace

bool CodecRegistry::supports(Codec codec) const {
  return std::ranges::find(kSupportedCodecs, codec) != kSupportedCodecs.end();
}

std::vector<Codec> CodecRegistry::supported_codecs() const {
  return {kSupportedCodecs.begin(), kSupportedCodecs.end()};
}

CodecRegistry default_codec_registry() { return {}; }

std::string_view codec_extension(Codec codec) noexcept {
  switch (codec) {
    case Codec::jpeg:
      return ".jpg";
    case Codec::png:
      return ".png";
    case Codec::webp:
      return ".webp";
    case Codec::avif:
      return ".avif";
    case Codec::jxl:
      return ".jxl";
    case Codec::auto_select:
      return {};
  }
  return {};
}

}  // namespace tinyjpg
