#include "tinyjpg/core/log.hh"

#if defined(TINYJPG_HAS_DOCTEST)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("logging maps configured levels") {
  CHECK(tinyjpg::to_spdlog_level(tinyjpg::LogLevel::trace) == spdlog::level::trace);
  CHECK(tinyjpg::to_spdlog_level(tinyjpg::LogLevel::debug) == spdlog::level::debug);
  CHECK(tinyjpg::to_spdlog_level(tinyjpg::LogLevel::info) == spdlog::level::info);
  CHECK(tinyjpg::to_spdlog_level(tinyjpg::LogLevel::warn) == spdlog::level::warn);
  CHECK(tinyjpg::to_spdlog_level(tinyjpg::LogLevel::error) == spdlog::level::err);
}

TEST_CASE("logger is shared") {
  tinyjpg::configure_logging(tinyjpg::LogLevel::debug);
  const auto first = tinyjpg::logger();
  const auto second = tinyjpg::logger();
  REQUIRE(first != nullptr);
  CHECK(first == second);
  CHECK(first->level() == spdlog::level::debug);
}
#else
int main() {
  if (tinyjpg::to_spdlog_level(tinyjpg::LogLevel::trace) != spdlog::level::trace ||
      tinyjpg::to_spdlog_level(tinyjpg::LogLevel::error) != spdlog::level::err) {
    return 1;
  }

  tinyjpg::configure_logging(tinyjpg::LogLevel::debug);
  const auto first = tinyjpg::logger();
  const auto second = tinyjpg::logger();
  if (first == nullptr || first != second || first->level() != spdlog::level::debug) {
    return 1;
  }

  return 0;
}
#endif
