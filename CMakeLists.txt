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

add_library(tinyjpg_core)
add_library(tinyjpg::tinyjpg ALIAS tinyjpg_core)

target_sources(tinyjpg_core PRIVATE src/app/cli.cc src/core/config.cc src/core/types.cc
                                    src/core/version.cc)

target_include_directories(
  tinyjpg_core PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_features(tinyjpg_core PUBLIC cxx_std_23)

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
    target_compile_options(${target} PRIVATE /W4)
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
  tinyjpg_add_test(tinyjpg_version_test tests/version_test.cc)
  add_test(NAME tinyjpg.version.cli COMMAND tinyjpg --version)
endif()

install(TARGETS tinyjpg RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
