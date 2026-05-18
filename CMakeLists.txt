cmake_minimum_required(VERSION 3.28)

project(
  TinyJPG
  VERSION 0.1.0
  DESCRIPTION "Cross-platform image optimizer"
  HOMEPAGE_URL "https://github.com/OrlovEvgeny/TinyJPG"
  LANGUAGES CXX)

option(TINYJPG_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(TINYJPG_ENABLE_IPO "Enable interprocedural optimization in Release" ON)

include(CTest)
include(GNUInstallDirs)

find_package(CLI11 CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(libavif CONFIG QUIET)
find_package(PNG REQUIRED)
find_package(WebP CONFIG QUIET)
find_package(PkgConfig QUIET)
find_package(spdlog CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)

if(NOT WebP_FOUND AND PkgConfig_FOUND)
  pkg_check_modules(WEBP QUIET IMPORTED_TARGET libwebp)
endif()

if(NOT TARGET avif AND NOT TARGET avif::avif AND PkgConfig_FOUND)
  pkg_check_modules(AVIF QUIET IMPORTED_TARGET libavif)
endif()

if(PkgConfig_FOUND)
  pkg_check_modules(JXL QUIET IMPORTED_TARGET libjxl)
endif()

add_library(tinyjpg_core)
add_library(tinyjpg::tinyjpg ALIAS tinyjpg_core)

target_sources(
  tinyjpg_core
  PRIVATE src/app/cli.cc
          src/codec/registry.cc
          src/core/config.cc
          src/core/config_io.cc
          src/core/log.cc
          src/core/types.cc
          src/core/version.cc
          src/image/image_io.cc
          src/image/resize.cc
          src/pipeline/job.cc
          src/pipeline/planner.cc
          src/service/service.cc
          src/platform/atomic_file.cc)

target_include_directories(
  tinyjpg_core PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_features(tinyjpg_core PUBLIC cxx_std_23)
target_link_libraries(
  tinyjpg_core
  PUBLIC CLI11::CLI11
  PRIVATE JPEG::JPEG PNG::PNG spdlog::spdlog tomlplusplus::tomlplusplus)

if(TARGET WebP::webp)
  target_link_libraries(tinyjpg_core PRIVATE WebP::webp)
  target_compile_definitions(tinyjpg_core PUBLIC TINYJPG_HAS_WEBP=1)
elseif(WEBP_FOUND)
  target_link_libraries(tinyjpg_core PRIVATE PkgConfig::WEBP)
  target_compile_definitions(tinyjpg_core PUBLIC TINYJPG_HAS_WEBP=1)
endif()

if(TARGET avif)
  target_link_libraries(tinyjpg_core PRIVATE avif)
  target_compile_definitions(tinyjpg_core PUBLIC TINYJPG_HAS_AVIF=1)
elseif(TARGET avif::avif)
  target_link_libraries(tinyjpg_core PRIVATE avif::avif)
  target_compile_definitions(tinyjpg_core PUBLIC TINYJPG_HAS_AVIF=1)
elseif(AVIF_FOUND)
  target_link_libraries(tinyjpg_core PRIVATE PkgConfig::AVIF)
  target_compile_definitions(tinyjpg_core PUBLIC TINYJPG_HAS_AVIF=1)
endif()

if(JXL_FOUND)
  target_link_libraries(tinyjpg_core PRIVATE PkgConfig::JXL)
  target_compile_definitions(tinyjpg_core PUBLIC TINYJPG_HAS_JXL=1)
endif()

set_target_properties(
  tinyjpg_core
  PROPERTIES CXX_EXTENSIONS OFF
             OUTPUT_NAME tinyjpg_core
             EXPORT_NAME tinyjpg)

target_compile_definitions(
  tinyjpg_core
  PUBLIC TINYJPG_VERSION="${PROJECT_VERSION}"
         TINYJPG_PROJECT_NAME="${PROJECT_NAME}")

function(tinyjpg_apply_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /utf-8 /wd4324 /wd4611)
    if(TINYJPG_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    if(TINYJPG_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

tinyjpg_apply_warnings(tinyjpg_core)

add_executable(tinyjpg src/app/main.cc)
target_link_libraries(tinyjpg PRIVATE tinyjpg::tinyjpg)
target_compile_features(tinyjpg PRIVATE cxx_std_23)
set_target_properties(tinyjpg PROPERTIES CXX_EXTENSIONS OFF)
tinyjpg_apply_warnings(tinyjpg)

include(CheckIPOSupported)
check_ipo_supported(RESULT tinyjpg_ipo_supported OUTPUT tinyjpg_ipo_error)
if(TINYJPG_ENABLE_IPO AND tinyjpg_ipo_supported)
  set_property(TARGET tinyjpg_core tinyjpg PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

if(BUILD_TESTING)
  find_package(doctest CONFIG QUIET)

  function(tinyjpg_add_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE tinyjpg::tinyjpg)
    target_compile_features(${name} PRIVATE cxx_std_23)
    set_target_properties(${name} PROPERTIES CXX_EXTENSIONS OFF)
    tinyjpg_apply_warnings(${name})
    if(doctest_FOUND)
      target_link_libraries(${name} PRIVATE doctest::doctest)
      target_compile_definitions(${name} PRIVATE TINYJPG_HAS_DOCTEST=1)
    endif()
    add_test(NAME ${name} COMMAND ${name})
  endfunction()

  tinyjpg_add_test(tinyjpg_core_types_test tests/core_types_test.cc)
  tinyjpg_add_test(tinyjpg_config_test tests/config_test.cc)
  tinyjpg_add_test(tinyjpg_config_io_test tests/config_io_test.cc)
  tinyjpg_add_test(tinyjpg_cli_test tests/cli_test.cc)
  tinyjpg_add_test(tinyjpg_codec_matrix_test tests/codec_matrix_test.cc)
  tinyjpg_add_test(tinyjpg_image_pipeline_test tests/image_pipeline_test.cc)
  tinyjpg_add_test(tinyjpg_log_test tests/log_test.cc)
  tinyjpg_add_test(tinyjpg_planner_test tests/planner_test.cc)
  tinyjpg_add_test(tinyjpg_service_test tests/service_test.cc)
  tinyjpg_add_test(tinyjpg_version_test tests/version_test.cc)
  add_test(NAME tinyjpg.version.cli COMMAND tinyjpg --version)

  add_executable(tinyjpg_codec_size_benchmark bench/codec_size_benchmark.cc)
  target_link_libraries(tinyjpg_codec_size_benchmark PRIVATE tinyjpg::tinyjpg)
  target_compile_features(tinyjpg_codec_size_benchmark PRIVATE cxx_std_23)
  set_target_properties(tinyjpg_codec_size_benchmark PROPERTIES CXX_EXTENSIONS OFF)
  tinyjpg_apply_warnings(tinyjpg_codec_size_benchmark)
  add_test(NAME tinyjpg.codec_size_benchmark COMMAND tinyjpg_codec_size_benchmark)
endif()

install(TARGETS tinyjpg RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES README.md LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})

set(TINYJPG_PACKAGING_DIR "${CMAKE_CURRENT_BINARY_DIR}/packaging")

configure_file(packaging/systemd/tinyjpg.service.in
               "${TINYJPG_PACKAGING_DIR}/systemd/tinyjpg.service" @ONLY)
configure_file(packaging/launchd/org.tinyjpg.plist.in
               "${TINYJPG_PACKAGING_DIR}/launchd/org.tinyjpg.plist" @ONLY)
configure_file(packaging/homebrew/tinyjpg.rb.in
               "${TINYJPG_PACKAGING_DIR}/homebrew/tinyjpg.rb" @ONLY)
configure_file(packaging/scoop/tinyjpg.json.in
               "${TINYJPG_PACKAGING_DIR}/scoop/tinyjpg.json" @ONLY)

install(FILES "${TINYJPG_PACKAGING_DIR}/systemd/tinyjpg.service"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/tinyjpg/systemd")
install(FILES packaging/systemd/tinyjpg.sysusers packaging/systemd/tinyjpg.tmpfiles
        DESTINATION "${CMAKE_INSTALL_DATADIR}/tinyjpg/systemd")
install(FILES "${TINYJPG_PACKAGING_DIR}/launchd/org.tinyjpg.plist"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/tinyjpg/launchd")
install(FILES packaging/windows/install-service.ps1 packaging/windows/uninstall-service.ps1
        DESTINATION "${CMAKE_INSTALL_DATADIR}/tinyjpg/windows")
install(FILES "${TINYJPG_PACKAGING_DIR}/homebrew/tinyjpg.rb"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/tinyjpg/homebrew")
install(FILES "${TINYJPG_PACKAGING_DIR}/scoop/tinyjpg.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/tinyjpg/scoop")

if(BUILD_TESTING)
  add_test(
    NAME tinyjpg.packaging.assets
    COMMAND
      ${CMAKE_COMMAND} -DTINYJPG_SOURCE_DIR=${PROJECT_SOURCE_DIR}
      -DTINYJPG_BINARY_DIR=${PROJECT_BINARY_DIR} -P
      ${PROJECT_SOURCE_DIR}/tests/packaging_check.cmake)
endif()

set(CPACK_PACKAGE_NAME "TinyJPG")
set(CPACK_PACKAGE_VENDOR "TinyJPG")
set(CPACK_PACKAGE_CONTACT "maintainers@tinyjpg.local")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross-platform image optimizer")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_GENERATOR TGZ ZIP)

if(UNIX AND NOT APPLE)
  list(APPEND CPACK_GENERATOR DEB)
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "TinyJPG Maintainers")
  set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
  set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
endif()

include(CPack)
