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
UP_DIR:=../../..
PIE_NOON_DIR:=$(LOCAL_PATH)/$(UP_DIR)
include $(PIE_NOON_DIR)/jni/android_config.mk

FREETYPE_DIR := $(UP_DIR)/${THIRD_PARTY_ROOT}/freetype

###########################
#
# Freetype build
#
###########################

FREETYPE_SRC_FILES = \
    ${FREETYPE_DIR}/src/base/ftbbox.c \
    ${FREETYPE_DIR}/src/base/ftbitmap.c \
    ${FREETYPE_DIR}/src/base/ftfstype.c \
    ${FREETYPE_DIR}/src/base/ftglyph.c \
    ${FREETYPE_DIR}/src/base/ftlcdfil.c \
    ${FREETYPE_DIR}/src/base/ftstroke.c \
    ${FREETYPE_DIR}/src/base/fttype1.c \
    ${FREETYPE_DIR}/src/base/ftbase.c \
    ${FREETYPE_DIR}/src/base/ftsystem.c \
    ${FREETYPE_DIR}/src/base/ftinit.c \
    ${FREETYPE_DIR}/src/base/ftgasp.c \
    ${FREETYPE_DIR}/src/gzip/ftgzip.c \
    ${FREETYPE_DIR}/src/raster/raster.c \
    ${FREETYPE_DIR}/src/sfnt/sfnt.c \
    ${FREETYPE_DIR}/src/smooth/smooth.c \
    ${FREETYPE_DIR}/src/autofit/autofit.c \
    ${FREETYPE_DIR}/src/truetype/truetype.c \
    ${FREETYPE_DIR}/src/cff/cff.c \
    ${FREETYPE_DIR}/src/psnames/psnames.c \
    ${FREETYPE_DIR}/src/pshinter/pshinter.c \
    $(NULL)

#############################################################
#   build the harfbuzz shared library
#
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_SRC_FILES := \
    $(FREETYPE_SRC_FILES)
LOCAL_C_INCLUDES += \
    $(PIE_NOON_DIR) \
    ${DEPENDENCIES_FREETYPE_DIR}/include

LOCAL_CFLAGS += -DFT2_BUILD_LIBRARY -DFT_CONFIG_MODULES_H=\"$(PIE_NOON_DIR)/cmake/freetype/ftmodule.h\"
LOCAL_MODULE := libfreetype
LOCAL_MODULE_FILENAME := libfreetype

include $(BUILD_STATIC_LIBRARY)
