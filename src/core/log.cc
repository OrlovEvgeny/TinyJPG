#include "tinyjpg/core/log.hh"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>

namespace tinyjpg {

std::shared_ptr<spdlog::logger> logger() {
  auto existing = spdlog::get("tinyjpg");
  if (existing) {
    return existing;
  }

  auto created = spdlog::stdout_color_mt("tinyjpg");
  created->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%^%l%$] %v");
  return created;
}

void configure_logging(LogLevel level) {
  auto active_logger = logger();
  active_logger->set_level(to_spdlog_level(level));
  spdlog::set_default_logger(std::move(active_logger));
}

spdlog::level::level_enum to_spdlog_level(LogLevel level) noexcept {
  switch (level) {
    case LogLevel::trace:
      return spdlog::level::trace;
    case LogLevel::debug:
      return spdlog::level::debug;
    case LogLevel::info:
      return spdlog::level::info;
    case LogLevel::warn:
      return spdlog::level::warn;
    case LogLevel::error:
      return spdlog::level::err;
  }
  return spdlog::level::info;
}

}  // namespace tinyjpg
