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

LOCAL_PATH:=$(call my-dir)

# Project directory relative to this file.
FLATUI_RELATIVE_DIR:=../../..
FLATUI_DIR=$(LOCAL_PATH)/$(FLATUI_RELATIVE_DIR)
include $(FLATUI_DIR)/jni/android_config.mk
FLATUI_ABSPATH:=$(abspath $(FLATUI_DIR))
FLATUI_RELATIVEPATH:=$(call realpath-portable,$(FLATUI_DIR))

# realpath-portable from android_config.mk
LOCAL_PATH:=$(call realpath-portable,$(DEPENDENCIES_FREETYPE_DIR))

###########################
#
# Freetype build
#
###########################

FREETYPE_SRC_FILES = \
    src/base/ftbbox.c \
    src/base/ftbitmap.c \
    src/base/ftfstype.c \
    src/base/ftglyph.c \
    src/base/ftlcdfil.c \
    src/base/ftstroke.c \
    src/base/fttype1.c \
    src/base/ftbase.c \
    src/base/ftsystem.c \
    src/base/ftinit.c \
    src/base/ftgasp.c \
    src/gzip/ftgzip.c \
    src/raster/raster.c \
    src/sfnt/sfnt.c \
    src/smooth/smooth.c \
    src/autofit/autofit.c \
    src/truetype/truetype.c \
    src/cff/cff.c \
    src/psnames/psnames.c \
    src/pshinter/pshinter.c \
    $(FLATUI_RELATIVEPATH)/src/libpng_to_stbimage/pngshim.cpp

#############################################################
#   build the harfbuzz shared library
#
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_SRC_FILES := $(FREETYPE_SRC_FILES)
LOCAL_C_INCLUDES += $(DEPENDENCIES_FREETYPE_DIR)/include\
  $(DEPENDENCIES_HARFBUZZ_DIR)/src\
  $(FLATUI_ABSPATH)/external/include/harfbuzz\
  $(FLATUI_ABSPATH)/src/libpng_to_stbimage\
  $(DEPENDENCIES_STB_DIR)
LOCAL_CFLAGS += \
  -DFT2_BUILD_LIBRARY \
  -DFT_CONFIG_MODULES_H=\"$(FLATUI_ABSPATH)/cmake/freetype/ftmodule.h\"\
  -DFT_CONFIG_OPTION_USE_PNG \
  -DFT_CONFIG_OPTION_USE_HARFBUZZ
LOCAL_MODULE := libfreetype
LOCAL_MODULE_FILENAME := libfreetype

include $(BUILD_STATIC_LIBRARY)
