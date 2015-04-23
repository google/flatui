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

# Directory which contains source for FPL libraries.
get_filename_component(
    fpl_root "${CMAKE_CURRENT_SOURCE_DIR}/../../libs" REALPATH)

# If the dependencies directory exists, assume this is the root directory for
# all libraries required by this project.
set(dependencies_root "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
if(EXISTS "${dependencies_root}")
  set(fpl_root "${dependencies_root}")
endif()

# Configurable locations of dependencies of this project.
set(dependencies_flatbuffers_dir "${fpl_root}/flatbuffers"
    CACHE PATH "Directory containing the Flatbuffers library.")
set(dependencies_mathfu_dir "${fpl_root}/mathfu"
    CACHE PATH "Directory containing the MathFu library.")
set(dependencies_fplbase_dir "${fpl_root}/fplbase"
    CACHE PATH "Directory containing the FPLBase library.")

# Option to enable / disable the test build.
option(imgui_build_tests "Build tests for this project." OFF)
option(imgui_build_samples "Build samples for this project." OFF)

# Option to only build flatc
option(motive_only_flatc "Only build FlatBuffers compiler." OFF)

# Include MathFu in this project with test and benchmark builds disabled.
set(mathfu_build_benchmarks OFF CACHE BOOL "")
set(mathfu_build_tests OFF CACHE BOOL "")
add_subdirectory(${dependencies_mathfu_dir} ${tmp_dir}/mathfu)

# Include FlatBuffers in this project.
set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "")
if(NOT TARGET flatc)
  add_subdirectory("${dependencies_flatbuffers_dir}" ${tmp_dir}/flatbuffers)
endif()

# Generate source files for all FlatBuffers schema files under the src
# directory.
set(IMGUI_FLATBUFFERS_GENERATED_INCLUDES_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}${CMAKE_FILES_DIRECTORY}/include)
get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
  set(IMGUI_FLATBUFFERS_GENERATED_INCLUDES_DIR
      ${IMGUI_FLATBUFFERS_GENERATED_INCLUDES_DIR} PARENT_SCOPE)
endif()

file(GLOB_RECURSE FLATBUFFERS_SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/schemas/*.fbs)

# Generate rules to build the set of output files from the set of input
# schema files.
foreach(flatbuffers_schema ${FLATBUFFERS_SCHEMAS})
  get_filename_component(filename ${flatbuffers_schema} NAME_WE)
  set(flatbuffers_include
    ${IMGUI_FLATBUFFERS_GENERATED_INCLUDES_DIR}/${filename}_generated.h)
  add_custom_command(
    OUTPUT ${flatbuffers_include}
    COMMAND flatc --gen-includes
        -o ${IMGUI_FLATBUFFERS_GENERATED_INCLUDES_DIR}
        -c ${flatbuffers_schema}
    DEPENDS flatc ${flatbuffers_schema})
  list(APPEND FLATBUFFERS_GENERATED_INCLUDES ${flatbuffers_include})
endforeach()
add_custom_target(imgui_generated_includes DEPENDS ${FLATBUFFERS_GENERATED_INCLUDES})

# IMGUI source files.
set(imgui_SRCS
    include/imgui/imgui.h
    src/imgui.cpp)

# Includes for this project.
include_directories(src include)
if(WIN32)
  include_directories(external/include/windows)
endif()
include_directories(${IMGUI_FLATBUFFERS_GENERATED_INCLUDES_DIR})
include_directories(${dependencies_flatbuffers_dir}/include)
include_directories(${dependencies_mathfu_dir}/include)
include_directories(${dependencies_fplbase_dir}/include)

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

# Dependencies for the executable target.
add_dependencies(imgui imgui_generated_includes)

# Additional flags for the target.
mathfu_configure_flags(imgui)

# Tests.
if(imgui_build_tests)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/tests)
endif()

# Samples.
if(imgui_build_samples)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/samples)
endif()

