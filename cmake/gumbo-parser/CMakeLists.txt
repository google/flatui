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
cmake_minimum_required(VERSION 2.8.7)

project(gumbo)

set(gumbo_dir ${dependencies_gumbo_distr_dir})

# Gumbo source files.
set(gumbo_0_10_1_SRCS
    ${gumbo_dir}/src/attribute.c
    ${gumbo_dir}/src/attribute.h
    ${gumbo_dir}/src/char_ref.c
    ${gumbo_dir}/src/char_ref.h
    ${gumbo_dir}/src/error.c
    ${gumbo_dir}/src/error.h
    ${gumbo_dir}/src/insertion_mode.h
    ${gumbo_dir}/src/parser.c
    ${gumbo_dir}/src/parser.h
    ${gumbo_dir}/src/string_buffer.c
    ${gumbo_dir}/src/string_buffer.h
    ${gumbo_dir}/src/string_piece.c
    ${gumbo_dir}/src/string_piece.h
    ${gumbo_dir}/src/tag.c
    ${gumbo_dir}/src/tag_enum.h
    ${gumbo_dir}/src/tag_gperf.h
    ${gumbo_dir}/src/tag_strings.h
    ${gumbo_dir}/src/tag_sizes.h
    ${gumbo_dir}/src/token_type.h
    ${gumbo_dir}/src/tokenizer.c
    ${gumbo_dir}/src/tokenizer.h
    ${gumbo_dir}/src/tokenizer_states.h
    ${gumbo_dir}/src/utf8.c
    ${gumbo_dir}/src/utf8.h
    ${gumbo_dir}/src/util.c
    ${gumbo_dir}/src/util.h
    ${gumbo_dir}/src/vector.c
    ${gumbo_dir}/src/vector.h)

set(gumbo_0_9_0_SRCS
    ${gumbo_dir}/attribute.c
    ${gumbo_dir}/attribute.h
    ${gumbo_dir}/char_ref.c
    ${gumbo_dir}/char_ref.h
    ${gumbo_dir}/error.c
    ${gumbo_dir}/error.h
    ${gumbo_dir}/gumbo.h
    ${gumbo_dir}/insertion_mode.h
    ${gumbo_dir}/parser.c
    ${gumbo_dir}/parser.h
    ${gumbo_dir}/string_buffer.c
    ${gumbo_dir}/string_buffer.h
    ${gumbo_dir}/string_piece.c
    ${gumbo_dir}/string_piece.h
    ${gumbo_dir}/tag.c
    ${gumbo_dir}/token_type.h
    ${gumbo_dir}/tokenizer.c
    ${gumbo_dir}/tokenizer.h
    ${gumbo_dir}/tokenizer_states.h
    ${gumbo_dir}/utf8.c
    ${gumbo_dir}/utf8.h
    ${gumbo_dir}/util.c
    ${gumbo_dir}/util.h
    ${gumbo_dir}/vector.c
    ${gumbo_dir}/vector.h)

if (EXISTS ${gumbo_dir}/src/tag_enum.h)
  set(gumbo_SRCS ${gumbo_0_10_1_SRCS})
  include_directories(${gumbo_dir}/src)
else()
  set(gumbo_SRCS ${gumbo_0_9_0_SRCS})
  include_directories(${gumbo_dir} ${gumbo_dir}/../..)
endif()

# Compile as C99 (default is C89, which Gumbo is not written in).
if(CMAKE_COMPILER_IS_GNUCC OR
   CMAKE_COMPILER_IS_GNUCXX OR
   ${CMAKE_CXX_COMPILER_ID} STREQUAL Clang)
  set(CMAKE_C_FLAGS "-std=c99")
endif()

# Library target.
add_library(gumbo ${gumbo_SRCS})

