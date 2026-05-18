#include <span>

#include "tinyjpg/app/cli.hh"

int main(int argc, char* argv[]) {
  const auto args =
      std::span<char const* const>{argv, static_cast<std::size_t>(argc)};
  return static_cast<int>(tinyjpg::app::run(args));
}
