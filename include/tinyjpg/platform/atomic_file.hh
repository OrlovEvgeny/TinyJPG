#pragma once

#include <cstdint>
#include <filesystem>
#include <span>

#include "tinyjpg/core/error.hh"

namespace tinyjpg {

[[nodiscard]] Result<void> write_file_atomic(const std::filesystem::path& path,
                                             std::span<const std::uint8_t> bytes);

}  // namespace tinyjpg
