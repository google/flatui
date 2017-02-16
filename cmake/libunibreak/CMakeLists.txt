# Copyright 2014 Google Inc. All rights reserved.
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
cmake_minimum_required(VERSION 2.8.7)

project(libunibreak)

set(libunibreak_dir ${dependencies_libunibreak_distr_dir})

# Include libunibreak header files.
include_directories(${libunibreak_dir}/src)

# Add definition to build library

# Including libunibreak source files.

set(libunibreak_SRCS
    ${libunibreak_dir}/src/linebreak.c
    ${libunibreak_dir}/src/linebreakdata.c
    ${libunibreak_dir}/src/linebreakdef.c
    ${libunibreak_dir}/src/unibreakdef.c
    ${libunibreak_dir}/src/wordbreak.c)

set_compiler_flags_for_external_libraries()
add_library(libunibreak ${libunibreak_SRCS})
restore_compiler_flags()
