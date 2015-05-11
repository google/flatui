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

project(imgui)

# Compile the game with the debug flag
set(imgui_DEBUG ON)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

# Temporary files (like object files) created while compiling projects.
set(tmp_dir ${CMAKE_CURRENT_BINARY_DIR}/obj)

# Directory which contains the source for 3rd party libraries.
get_filename_component(
    third_party_root "${CMAKE_SOURCE_DIR}/../../../../external" REALPATH)

# Directory which contains source for FPL libraries.
get_filename_component(
    fpl_root "${CMAKE_SOURCE_DIR}/../../libs" REALPATH)

# If the dependencies directory exists, assume this is the root directory for
# all libraries required by this project.
set(dependencies_root "${CMAKE_SOURCE_DIR}/dependencies")
if(EXISTS "${dependencies_root}")
  set(third_party_root "${dependencies_root}")
  set(fpl_root "${dependencies_root}")
endif()

# Configurable locations of dependencies of this project.
set(dependencies_mathfu_dir "${fpl_root}/mathfu"
    CACHE PATH "Directory containing the MathFu library.")
set(dependencies_fplbase_dir "${fpl_root}/fplbase"
    CACHE PATH "Directory containing the FPLBase library.")
set(dependencies_harfbuzz_cmake_dir "${CMAKE_MODULE_PATH}/harfbuzz"
    CACHE PATH "Directory containing the harfbuzz cmake project.")
set(dependencies_harfbuzz_distr_dir "${third_party_root}/harfbuzz"
    CACHE PATH "Directory containing the harfbuzz library.")
set(dependencies_freetype_cmake_dir "${CMAKE_MODULE_PATH}/freetype"
    CACHE PATH "Directory containing the freetype cmake project.")
set(dependencies_freetype_distr_dir "${third_party_root}/freetype"
    CACHE PATH "Directory containing the freetype library.")
set(dependencies_libunibreak_cmake_dir "${CMAKE_MODULE_PATH}/libunibreak"
    CACHE PATH "Directory containing the libunibreak cmake project.")
set(dependencies_libunibreak_distr_dir "${third_party_root}/libunibreak"
    CACHE PATH "Directory containing the libunibreak library.")

# Option to enable / disable the sample build.
option(imgui_build_samples "Build samples for this project." OFF)

# Option to enable / disable the test build.
option(imgui_build_tests "Build tests for this project." OFF)

# Include MathFu in this project with test and benchmark builds disabled.
set(mathfu_build_benchmarks OFF CACHE BOOL "")
set(mathfu_build_tests OFF CACHE BOOL "")
add_subdirectory(${dependencies_mathfu_dir} ${tmp_dir}/mathfu)

# Include harfbuzz, freetype and libunibreak.
if(NOT TARGET harfbuzz)
  add_subdirectory("${dependencies_harfbuzz_cmake_dir}" ${tmp_dir}/harfbuzz)
endif()
if(NOT TARGET freetype)
  add_subdirectory("${dependencies_freetype_cmake_dir}" ${tmp_dir}/freetype)
endif()
if(NOT TARGET libunibreak)
  add_subdirectory("${dependencies_libunibreak_cmake_dir}"
    ${tmp_dir}/libunibreak)
endif()
if(imgui_build_tests OR imgui_build_samples)
# Add FPLbase
add_subdirectory("${fpl_root}/fplbase" ${tmp_dir}/fplbase)
endif()

# IMGUI source files.
set(imgui_SRCS
    include/imgui/imgui.h
    include/imgui/font_manager.h
    include/imgui/internal/glyph_cache.h
    src/font_manager.cpp
    src/imgui.cpp)

# Includes for this project.
include_directories(src include include/imgui)
if(WIN32)
  include_directories(external/include/windows)
  include_directories(external/include/harfbuzz)
endif()
include_directories(${dependencies_mathfu_dir}/include)
include_directories(${dependencies_fplbase_dir}/include)

if(imgui_build_tests OR imgui_build_samples)
# SDL includes.
include_directories(${tmp_dir}/fplbase/obj/sdl/include)
endif()

# SDL includes.
include_directories(${dependencies_sdl_dir}/include)

# harfbuzz includes.
include_directories(${dependencies_harfbuzz_distr_dir}/src)

# freetype includes.
include_directories(${dependencies_freetype_distr_dir}/include)

# libunibreak includes.
include_directories(${dependencies_libunibreak_distr_dir}/include)

# Detect clang
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_COMPILER_IS_CLANGXX 1)
endif()

# Compiler flags.
set(C_FLAGS_WARNINGS "")
if(MSVC)
  set(C_FLAGS_WARNINGS "/W4 /WX /wd4065 /wd4355")
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR
       CMAKE_COMPILER_IS_CLANGXX)
  add_definitions(-g)
  set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -std=c++0x -Wall -pedantic -Werror -Wextra -Wno-unused-function")
  # -Wno-unused-function is because of webp encode.h
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_FLAGS_WARNINGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C_FLAGS_WARNINGS}")

if(imgui_DEBUG)
  # if we want to define this, it needs to be only in debug builds
  #add_definitions(-D_DEBUG)
endif()

# Executable target.
add_library(imgui ${imgui_SRCS})

# Dependencies to libraries.
target_link_libraries(imgui libfreetype libharfbuzz libunibreak)

# Additional flags for the target.
mathfu_configure_flags(imgui)

# Tests.
if(imgui_build_tests)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)
endif()

# Samples.
if(imgui_build_samples)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/samples)
endif()
