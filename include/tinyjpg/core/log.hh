#pragma once

#include <spdlog/logger.h>

#include <memory>
#include <string_view>

#include "tinyjpg/core/config.hh"

namespace tinyjpg {

[[nodiscard]] std::shared_ptr<spdlog::logger> logger();
void configure_logging(LogLevel level);
[[nodiscard]] spdlog::level::level_enum to_spdlog_level(LogLevel level) noexcept;

}  // namespace tinyjpg
