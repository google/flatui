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

# Configurable locations of dependencies of this project.
if(NOT DEFINED fpl_root)
  get_filename_component(fpl_root "${CMAKE_CURRENT_LIST_DIR}/../../libs"
       REALPATH)
  if(NOT EXISTS "${fpl_root}")
    set(fpl_root "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
  endif ()
endif()

# Extra MSVC compile flags
#    wd4316: object allocated on the heap may not be aligned for required type
#            in glyph_cache.h
include (${fpl_root}/fplutil/buildutil/cmake_common.txt)
set_common_build_variables()

set(dependencies_harfbuzz_cmake_dir "${CMAKE_MODULE_PATH}/harfbuzz"
    CACHE PATH "Directory containing the harfbuzz cmake project.")
set(dependencies_freetype_cmake_dir "${CMAKE_MODULE_PATH}/freetype"
    CACHE PATH "Directory containing the freetype cmake project.")
set(dependencies_libunibreak_cmake_dir "${CMAKE_MODULE_PATH}/libunibreak"
    CACHE PATH "Directory containing the libunibreak cmake project.")

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

# Use pregenerated headers on Windows & OSX.
if(WIN32 OR ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
set(use_pregenerated_headers ON)
endif()

# Include MathFu in this project with test and benchmark builds disabled.
set(mathfu_build_benchmarks OFF CACHE BOOL "")
set(mathfu_build_tests OFF CACHE BOOL "")
add_subdirectory(${dependencies_mathfu_dir} ${tmp_dir}/mathfu)

# Include harfbuzz, freetype and libunibreak.
if(NOT TARGET harfbuzz)
  set_compiler_flags_for_external_libraries()
  add_subdirectory("${dependencies_harfbuzz_cmake_dir}" ${tmp_dir}/harfbuzz)
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
if(flatui_build_tests OR flatui_build_samples)
# Add FPLbase
add_subdirectory("${fpl_root}/fplbase" ${tmp_dir}/fplbase)
endif()

# Include motive.
if(NOT TARGET motive)
 set(motive_build_samples OFF CACHE BOOL "")
 set(motive_build_tests OFF CACHE BOOL "")
 add_subdirectory("${dependencies_motive_dir}" ${tmp_dir}/motive)
endif()

# flatui source files.
set(flatui_SRCS
    include/flatui/flatui.h
    include/flatui/flatui_common.h
    include/flatui/font_manager.h
    include/flatui/internal/distance_computer.h
    include/flatui/internal/glyph_cache.h
    include/flatui/internal/flatui_util.h
    include/flatui/internal/flatui_layout.h
    include/flatui/internal/hb_complex_font.h
    include/flatui/internal/micro_edit.h
    include/flatui/version.h
    src/font_manager.cpp
    src/font_systemfont.cpp
    src/micro_edit.cpp
    src/flatui.cpp
    src/flatui_common.cpp
    src/glyph_cache.cpp
    src/hb_complex_font.cpp
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
include_directories(${dependencies_motive_dir}/include)
include_directories(${dependencies_fplutil_dir}/libfplutil/include)
include_directories(${dependencies_flatbuffers_dir}/include)

# Generate source files for all FlatBuffers schema files under the schemas
# directory.
set(FLATUI_FLATBUFFERS_GENERATED_INCLUDES_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}${CMAKE_FILES_DIRECTORY}/include)
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

# libunibreak includes.
include_directories(${dependencies_libunibreak_distr_dir}/src)

# Include generated FlatBuffer files.
include_directories(${FLATUI_FLATBUFFERS_GENERATED_INCLUDES_DIR})
include_directories(${dependencies_fplbase_dir}${CMAKE_FILES_DIRECTORY}/include)

if(flatui_DEBUG)
  # if we want to define this, it needs to be only in debug builds
  #add_definitions(-D_DEBUG)
endif()

# Executable target.
add_library(flatui ${flatui_SRCS})

# Dependencies on the generated FlatBuffer files.
add_dependencies(flatui flatui_generated_includes)
add_dependencies(flatui fplbase_generated_includes)

# Dependencies to libraries.
target_link_libraries(flatui fplbase libfreetype libharfbuzz libunibreak motive)

# Additional flags for the target.
mathfu_configure_flags(flatui)

# Post process function.
function(flatui_post_process target folder_name)
# Copy flatui resource files
add_custom_command(TARGET ${target} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${dependencies_flatui_dir}/assets
                       ${dependencies_flatui_dir}/${folder_name}/assets)
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
