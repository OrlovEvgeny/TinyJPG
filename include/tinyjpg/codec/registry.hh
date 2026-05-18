#pragma once

#include <string_view>
#include <vector>

#include "tinyjpg/core/types.hh"

namespace tinyjpg {

class CodecRegistry {
 public:
  [[nodiscard]] bool supports(Codec codec) const;
  [[nodiscard]] std::vector<Codec> supported_codecs() const;
};

[[nodiscard]] CodecRegistry default_codec_registry();
[[nodiscard]] std::string_view codec_extension(Codec codec) noexcept;

}  // namespace tinyjpg
