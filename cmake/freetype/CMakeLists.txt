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

project(freetype)

set(freetype_dir ${dependencies_freetype_distr_dir})

# Include freetype header files.
include_directories(${freetype_dir}/include)
include_directories(${dependencies_freetype_cmake_dir})

# Include harfbuzz header files used by FT_CONFIG_OPTION_USE_HARFBUZZ.
include_directories(${dependencies_harfbuzz_distr_dir}/src)
include_directories(${dependencies_flatui_dir}/external/include/harfbuzz)
include_directories(${dependencies_stb_dir})

# Include libpng to stbimage header file used by FT_CONFIG_OPTION_USE_PNG.
include_directories(${dependencies_flatui_dir}/src/libpng_to_stbimage)

# Add definition to build library
add_definitions(-DFT2_BUILD_LIBRARY)

# Enable use of libpng for a color font decompression.
add_definitions(-DFT_CONFIG_OPTION_USE_PNG)

# Enable use of harfbuzz to extract better kerning info.
add_definitions(-DFT_CONFIG_OPTION_USE_HARFBUZZ)

# Including freetype source files.
# Note that it includes top level .c module files
# that includes other files.
add_definitions(
    -DFT_CONFIG_MODULES_H=\"${dependencies_freetype_cmake_dir}/ftmodule.h\")

set(libfreetype_SRCS
    ${freetype_dir}/src/base/ftbbox.c
    ${freetype_dir}/src/base/ftbitmap.c
    ${freetype_dir}/src/base/ftfstype.c
    ${freetype_dir}/src/base/ftglyph.c
    ${freetype_dir}/src/base/ftlcdfil.c
    ${freetype_dir}/src/base/ftstroke.c
    ${freetype_dir}/src/base/fttype1.c
    ${freetype_dir}/src/base/ftbase.c
    ${freetype_dir}/src/base/ftsystem.c
    ${freetype_dir}/src/base/ftinit.c
    ${freetype_dir}/src/base/ftgasp.c
    ${freetype_dir}/src/gzip/ftgzip.c
    ${freetype_dir}/src/raster/raster.c
    ${freetype_dir}/src/sfnt/sfnt.c
    ${freetype_dir}/src/smooth/smooth.c
    ${freetype_dir}/src/autofit/autofit.c
    ${freetype_dir}/src/truetype/truetype.c
    ${freetype_dir}/src/cff/cff.c
    ${freetype_dir}/src/psnames/psnames.c
    ${freetype_dir}/src/pshinter/pshinter.c
    ${dependencies_flatui_dir}/src/libpng_to_stbimage/pngshim.cpp)

add_library(libfreetype ${libfreetype_SRCS})

