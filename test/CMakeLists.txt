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

project(flatuitest)

# gtest seems to prefer the non-DLL runtime on Windows, which conflicts with
# everything else.
option(
  gtest_force_shared_crt
  "Use shared (DLL) run-time lib even when Google Test is built as static lib."
  ON)

# Import gmock if it is not already present.
# Note: gmock includes gtest.
if(NOT TARGET gtest OR NOT TARGET gmock)
  set_compiler_flags_for_external_libraries()
  add_subdirectory(${dependencies_gmock_dir} ${tmp_dir}/googlemock)
  restore_compiler_flags()
endif()

# Set some variables describing the gtest file locations.
set(gtest_incdir ${dependencies_gtest_dir}/include)
set(gtest_libdir ${dependencies_gtest_dir})
set(gmock_incdir ${dependencies_gmock_dir}/include)

# Include helper functions and macros used by googletest.
include(${gtest_libdir}/cmake/internal_utils.cmake)
config_compiler_and_linker()
string(REPLACE "-W4" "-W3" cxx_default "${cxx_default}")
string(REPLACE "-Wshadow" "" cxx_default "${cxx_default}")
string(REPLACE "-Wextra" "" cxx_default "${cxx_default}")

include_directories(${gtest_incdir}
                    ${gmock_incdir}
                    ${CMAKE_CURRENT_LIST_DIR}
                    ${dependencies_flatui_dir}
                    ${dependencies_fplutil_dir}/libfplutil/include)
fpl_absolute_include_dir(${dependencies_gtest_dir})

# Common libraries for tests.
if(NOT MSVC)
  find_package(Threads)
endif()

set(COMMON_LIBS "gtest;gmock;${CMAKE_THREAD_LIBS_INIT}")

# PUT ADDITIONAL UNIT TEST BINARIES BELOW!
# The commands should be of the form:
#
# test_executable(<test-name>)
#
# Where <test-name> is the name of the output test executable and the basename
# of the source file for the test.  For example, test_executable(CallbackTests)
# generates an executable called CallbackTests which is the result of compiling
# CallbackTests.cpp.
function(test_executable name)
  cxx_executable_with_flags("flatui_${name}_test"
      "${cxx_default}"
      "${COMMON_LIBS}"
      ${CMAKE_CURRENT_LIST_DIR}/${name}/flatui_${name}_test.cpp
      ${ARGN})
  add_dependencies(flatui_${name}_test
      flatui
      flatui_generated_includes
      fplbase)
  target_link_libraries(flatui_${name}_test
      ${COMMON_LIBS}
      libfreetype
      fplbase
      flatui
      libharfbuzz
      libunibreak)
  mathfu_configure_flags(flatui_${name}_test)
endfunction()

test_executable(html)
test_executable(ref_count)
test_executable(serialization)
