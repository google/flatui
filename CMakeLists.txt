# Copyright 2015 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
cmake_minimum_required(VERSION 2.8.12)

project(flatui)

# Compile the game with the debug flag
set(flatui_DEBUG ON)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

# Temporary files (like object files) created while compiling projects.
set(tmp_dir ${CMAKE_CURRENT_BINARY_DIR}/obj)

# Generated asset files (like hyphenation patterns) created here.
set(flatui_assets_dir "${CMAKE_BINARY_DIR}/assets")

# Call fplutil to get locations of dependencies and set common build settings.
include("cmake/find_fplutil.cmake")
include("${fplutil_dir}/buildutil/cmake_common.txt")

# Temporarily suppress these Visual Studio warnings.
# TODO(hakuro): fix code so these warnings aren't generated.
if(MSVC)
  set(flatui_temporarily_suppressed_warnings
      "/wd4245 /wd4267 /wd4244 /wd4800 /wd4456 /wd4305 /wd4804")
endif()
set_common_build_variables(${flatui_temporarily_suppressed_warnings})

set(dependencies_harfbuzz_cmake_dir "${CMAKE_MODULE_PATH}/harfbuzz"
    CACHE PATH "Directory containing the harfbuzz cmake project.")
set(dependencies_gumbo_cmake_dir "${CMAKE_MODULE_PATH}/gumbo-parser"
    CACHE PATH "Directory containing the gumbo cmake project.")
set(dependencies_freetype_cmake_dir "${CMAKE_MODULE_PATH}/freetype"
    CACHE PATH "Directory containing the freetype cmake project.")
set(dependencies_libunibreak_cmake_dir "${CMAKE_MODULE_PATH}/libunibreak"
    CACHE PATH "Directory containing the libunibreak cmake project.")
set(dependencies_hyphenation_patterns_cmake_dir "${CMAKE_MODULE_PATH}/hyphenation_patterns"
    CACHE PATH "Directory containing the hyphenation pattern cmake project.")

set(flatui_standalone_mode OFF)
if("${CMAKE_CURRENT_LIST_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
  set(flatui_standalone_mode ON)
  message(STATUS "FlatUI standalone: building tests and samples")
else()
  message(STATUS "FlatUI library: not building tests and samples")
endif()

# Option to enable / disable the sample build.
option(flatui_build_samples "Build samples for this project."
       ${flatui_standalone_mode})

# Option to enable / disable the test build.
option(flatui_build_tests "Build tests for this project."
       ${flatui_standalone_mode})

# Option to use pregenerated headers on Linux.
option(use_pregenerated_headers "Use pregenerated headers for Harfbuzz." OFF)

# HTML support only available with C99 compilers, since Gumbo is written in C99.
# Visual Studio acquired C99 support in version 2013.
if (NOT MSVC OR MSVC_VERSION GREATER 1700) # Visual Studio 2013 or later
  set(flatui_has_gumbo ON)
endif()

# Use pregenerated headers on Windows & OSX.
if(WIN32 OR ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
set(use_pregenerated_headers ON)
endif()

# Include MathFu in this project with test and benchmark builds disabled.
set(mathfu_build_benchmarks OFF CACHE BOOL "")
set(mathfu_build_tests OFF CACHE BOOL "")
add_subdirectory(${dependencies_mathfu_dir} ${tmp_dir}/mathfu)

# Include harfbuzz, gumbo, freetype, libunibreak, hyphenation patterns.
if(NOT TARGET harfbuzz)
  set_compiler_flags_for_external_libraries()
  add_subdirectory("${dependencies_harfbuzz_cmake_dir}" ${tmp_dir}/harfbuzz)
  restore_compiler_flags()
endif()
if(NOT TARGET gumbo AND flatui_has_gumbo)
  set_compiler_flags_for_external_libraries()
  add_subdirectory("${dependencies_gumbo_cmake_dir}" ${tmp_dir}/gumbo-parser)
  restore_compiler_flags()
endif()
if(NOT TARGET freetype)
  set_compiler_flags_for_external_libraries()
  add_subdirectory("${dependencies_freetype_cmake_dir}" ${tmp_dir}/freetype)
  restore_compiler_flags()
endif()
if(NOT TARGET libunibreak)
  set_compiler_flags_for_external_libraries()
  add_subdirectory("${dependencies_libunibreak_cmake_dir}"
    ${tmp_dir}/libunibreak)
  restore_compiler_flags()
endif()
if(NOT TARGET hyphenation_patterns)
  add_subdirectory("${dependencies_hyphenation_patterns_cmake_dir}")
endif()
if(flatui_build_tests OR flatui_build_samples)
# Add FPLbase
add_subdirectory("${dependencies_fplbase_dir}" ${tmp_dir}/fplbase)
endif()

# Include motive.
if(NOT TARGET motive)
 set(motive_build_samples OFF CACHE BOOL "")
 set(motive_build_tests OFF CACHE BOOL "")
 set(motive_build_viewer OFF CACHE BOOL "")
 add_subdirectory("${dependencies_motive_dir}" ${tmp_dir}/motive)
endif()

# flatui source files.
set(flatui_SRCS
    include/flatui/flatui.h
    include/flatui/flatui_common.h
    include/flatui/flatui_core.h
    include/flatui/font_buffer.h
    include/flatui/font_manager.h
    include/flatui/font_util.h
    include/flatui/internal/distance_computer.h
    include/flatui/internal/glyph_cache.h
    include/flatui/internal/flatui_util.h
    include/flatui/internal/flatui_layout.h
    include/flatui/internal/hb_complex_font.h
    include/flatui/internal/hyphenator.h
    include/flatui/internal/micro_edit.h
    include/flatui/version.h
    src/font_buffer.cpp
    src/font_manager.cpp
    src/font_systemfont.cpp
    src/font_util.cpp
    src/micro_edit.cpp
    src/flatui.cpp
    src/flatui_common.cpp
    src/glyph_cache.cpp
    src/hb_complex_font.cpp
    src/hyphenator.cpp
    src/flatui_serialization.cpp
    src/script_table.cpp
    src/version.cpp)

# Includes for this project.
include_directories(src include include/flatui)
if(use_pregenerated_headers)
  include_directories(${dependencies_flatui_dir}/external/include/harfbuzz)
endif()
include_directories(${dependencies_mathfu_dir}/include)
include_directories(${dependencies_fplbase_dir}/include)
if(WIN32)
  include_directories(${dependencies_fplbase_dir}/external/include)
endif()
include_directories(${dependencies_motive_dir}/include)
include_directories(${dependencies_fplutil_dir}/libfplutil/include)
include_directories(${dependencies_flatbuffers_dir}/include)

# Generate source files for all FlatBuffers schema files under the schemas
# directory.
set(FLATUI_FLATBUFFERS_GENERATED_INCLUDES_DIR
    ${CMAKE_CURRENT_BINARY_DIR}/include)
file(GLOB_RECURSE FLATUI_FLATBUFFERS_SCHEMAS
                  ${CMAKE_CURRENT_LIST_DIR}/schemas/*.fbs)
build_flatbuffers("${FLATUI_FLATBUFFERS_SCHEMAS}"
                  "${dependencies_fplbase_dir}/schemas"
                  flatui_generated_includes
                  ""
                  ${FLATUI_FLATBUFFERS_GENERATED_INCLUDES_DIR}/flatui
                  ""
                  "")

if(flatui_build_tests OR flatui_build_samples)
# SDL includes.
include_directories(${tmp_dir}/fplbase/obj/sdl/include)
endif()

# SDL includes.
include_directories(${dependencies_sdl_dir}/include)

# STB includes.
include_directories(${dependencies_stb_dir})

# harfbuzz includes.
include_directories(${dependencies_harfbuzz_distr_dir}/src)

# freetype includes.
include_directories(${dependencies_freetype_distr_dir}/include)

# gumbo includes.
if(EXISTS ${dependencies_gumbo_distr_dir}/src)
  include_directories(${dependencies_gumbo_distr_dir}/src)
else()
  include_directories(${dependencies_gumbo_distr_dir})
endif()

# libunibreak includes.
include_directories(${dependencies_libunibreak_distr_dir}/src)

# Include generated FlatBuffer files.
get_property(FPLBASE_FLATBUFFERS_GENERATED_INCLUDES_DIR
             TARGET fplbase_generated_includes
             PROPERTY GENERATED_INCLUDES_DIR)
include_directories(${FLATUI_FLATBUFFERS_GENERATED_INCLUDES_DIR})
include_directories(${FPLBASE_FLATBUFFERS_GENERATED_INCLUDES_DIR})

if(flatui_DEBUG)
  # if we want to define this, it needs to be only in debug builds
  #add_definitions(-D_DEBUG)
endif()

if(flatui_has_gumbo)
  add_definitions(-DFLATUI_HAS_GUMBO)
endif()

# Executable target.
add_library(flatui ${flatui_SRCS})

# Dependencies on the generated FlatBuffer files.
add_dependencies(flatui flatui_generated_includes)
add_dependencies(flatui fplbase_generated_includes)

# Dependencies to libraries.
target_link_libraries(flatui fplbase libfreetype libharfbuzz libunibreak motive)
if(flatui_has_gumbo)
  target_link_libraries(flatui gumbo)
endif()

# Additional flags for the target.
mathfu_configure_flags(flatui)

# Post process function.
function(flatui_post_process target folder_name)
  # Copy flatui resource files from flatui/assets
  # and flatui/${folder_name}/assets into
  # bin/assets.
  add_custom_command(
      TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${dependencies_flatui_dir}/assets ${flatui_assets_dir}
      COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${dependencies_flatui_dir}/${folder_name}/assets ${flatui_assets_dir}
      COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${dependencies_flatui_dir}/schemas ${flatui_assets_dir}/schemas
      COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${dependencies_flatui_dir}/sample/serialization/schemas ${flatui_assets_dir}/schemas
      COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${dependencies_fplbase_dir}/schemas ${flatui_assets_dir}/schemas
      COMMAND ${CMAKE_COMMAND} -E make_directory ${flatui_assets_dir}/serialization
      COMMAND ${CMAKE_COMMAND} -E copy
          ${dependencies_flatui_dir}/sample/serialization/first_menu.json ${flatui_assets_dir}/serialization
      COMMAND ${CMAKE_COMMAND} -E copy
          ${dependencies_flatui_dir}/sample/serialization/second_menu.json ${flatui_assets_dir}/serialization)
endfunction()

if(MSVC)
  # TODO: remove this. SDL sets this, but for some reason
  # link_directories doesn't perculate up to parents.
  link_directories($ENV{DXSDK_DIR}\\lib\\x86)
endif()

# Tests.
if(flatui_build_tests)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/test)
endif()

# Samples.
if(flatui_build_samples)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/sample)
endif()
