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

HARFBUZZ_DIR := $(UP_DIR)/${THIRD_PARTY_ROOT}/harfbuzz

###########################
#
# Harfbuzz build
#
###########################

HARFBUZZ_SRC_FILES = \
    $(HARFBUZZ_DIR)/src/hb-blob.cc \
    $(HARFBUZZ_DIR)/src/hb-buffer-serialize.cc \
    $(HARFBUZZ_DIR)/src/hb-buffer.cc \
    $(HARFBUZZ_DIR)/src/hb-common.cc \
    $(HARFBUZZ_DIR)/src/hb-face.cc \
    $(HARFBUZZ_DIR)/src/hb-font.cc \
    $(HARFBUZZ_DIR)/src/hb-ft.cc \
    $(HARFBUZZ_DIR)/src/hb-ot-tag.cc \
    $(HARFBUZZ_DIR)/src/hb-set.cc \
    $(HARFBUZZ_DIR)/src/hb-shape.cc \
    $(HARFBUZZ_DIR)/src/hb-shape-plan.cc \
    $(HARFBUZZ_DIR)/src/hb-shaper.cc \
    $(HARFBUZZ_DIR)/src/hb-unicode.cc \
    $(HARFBUZZ_DIR)/src/hb-warning.cc \
    $(HARFBUZZ_DIR)/src/hb-ot-font.cc \
    $(HARFBUZZ_DIR)/src/hb-ot-layout.cc \
    $(HARFBUZZ_DIR)/src/hb-ot-map.cc \
    $(HARFBUZZ_DIR)/src/hb-ot-shape.cc \
    $(HARFBUZZ_DIR)/src/hb-ot-shape-complex-arabic.cc \
    $(HARFBUZZ_DIR)/src/hb-ot-shape-complex-default.cc \
    $(HARFBUZZ_DIR)/src/hb-ot-shape-complex-hangul.cc \
    $(HARFBUZZ_DIR)/src/hb-ot-shape-complex-hebrew.cc \
    $(HARFBUZZ_DIR)/src/hb-ot-shape-complex-indic.cc \
    $(HARFBUZZ_DIR)/src/hb-ot-shape-complex-indic-table.cc \
    $(HARFBUZZ_DIR)/src/hb-ot-shape-complex-myanmar.cc \
    $(HARFBUZZ_DIR)/src/hb-ot-shape-complex-sea.cc \
    $(HARFBUZZ_DIR)/src/hb-ot-shape-complex-thai.cc \
    $(HARFBUZZ_DIR)/src/hb-ot-shape-complex-tibetan.cc \
    $(HARFBUZZ_DIR)/src/hb-ot-shape-normalize.cc \
    $(HARFBUZZ_DIR)/src/hb-ot-shape-fallback.cc \
    $(NULL)

#############################################################
#   build the harfbuzz shared library
#
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_SRC_FILES := \
    $(HARFBUZZ_SRC_FILES)
LOCAL_CPP_EXTENSION := .cc
LOCAL_C_INCLUDES += \
    ${PIE_NOON_DIR}/external/include/harfbuzz \
    $(DEPENDENCIES_HARFBUZZ_DIR)/src \
    $(DEPENDENCIES_HARFBUZZ_DIR)/src/hb-ucdn \
    ${DEPENDENCIES_FREETYPE_DIR}/include
LOCAL_CFLAGS += -DHAVE_OT -DHB_NO_MT -DHB_NO_UNICODE_FUNCS
LOCAL_MODULE := libharfbuzz
LOCAL_MODULE_FILENAME := libharfbuzz

include $(BUILD_STATIC_LIBRARY)
