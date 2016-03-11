# Copyright 2016 Google Inc. All rights reserved.
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

# TODO: This should be replaced with a call to `set_common_build_variables` in
# fplutil/buildutil/cmake_common.txt. Currently that script is hard coded to
# only work from within top level directory.
if(NOT DEFINED third_party_root)
  get_filename_component(third_party_root
    "${fpl_root}/../../../external" REALPATH)
endif()

# If the dependencies directory exists, assume this is the root directory
# for all libraries required by this project.
if(NOT DEFINED dependencies_root)
  set(dependencies_root "${fpl_root}/flatui/dependencies")
  if(EXISTS "$dependencies_root}")
    set(third_party_root "${dependencies_root}")
  endif()
endif()

#TODO: Remove this once the `dependencies_gtest_dir` is updated in the other
# libraries to use the new `external/googletest` path.
set(dependencies_gtest_dir ${third_party_root}/googletest)

# Import gtest if it is not already present
if(NOT TARGET gtest OR NOT TARGET gmock)
  add_subdirectory(${dependencies_gtest_dir} ${tmp_dir}/googletest)
endif()

# Set some variables describing the gtest file locations.
set(gtest_incdir ${dependencies_gtest_dir}/googletest/include)
set(gmock_incdir ${dependencies_gtest_dir}/googlemock/include)
set(gtest_libdir ${dependencies_gtest_dir}/googletest)

# Include helper functions and macros used by googletest.
include(${gtest_libdir}/cmake/internal_utils.cmake)
config_compiler_and_linker()
string(REPLACE "-W4" "-W3" cxx_default "${cxx_default}")
string(REPLACE "-Wshadow" "" cxx_default "${cxx_default}")
string(REPLACE "-Wextra" "" cxx_default "${cxx_default}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../libs)

include_directories(${gtest_incdir} ${gmock_incdir} ${fpl_root}/flatui
  ${CMAKE_CURRENT_LIST_DIR}/.. ${dependencies_fplutil_dir}/libfplutil/include)

# Common libraries for tests.
if(NOT MSVC)
  find_package(Threads)
endif()

set(COMMON_LIBS "gtest;gmock;${CMAKE_THREAD_LIBS_INIT}")

# Build test executable with mocks.
set(flatui_ref_count_SRC
    flatui_ref_count_test.cpp)

add_executable(flatui_ref_count_test ${flatui_ref_count_SRC})
add_dependencies(flatui_ref_count_test flatui fplbase flatui_generated_includes)
target_link_libraries(flatui_ref_count_test ${COMMON_LIBS} libfreetype fplbase flatui
  libharfbuzz libunibreak)
mathfu_configure_flags(flatui_ref_count_test)
