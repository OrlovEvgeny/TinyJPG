#include "tinyjpg/core/version.hh"

namespace tinyjpg {

VersionInfo version_info() noexcept {
  return VersionInfo{
      .project_name = TINYJPG_PROJECT_NAME,
      .version = TINYJPG_VERSION,
  };
}

std::string_view version_string() noexcept { return version_info().version; }

}  // namespace tinyjpg
