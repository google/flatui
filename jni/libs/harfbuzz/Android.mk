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
FLATUI_DIR:=$(LOCAL_PATH)/$(FLATUI_RELATIVE_DIR)
include $(FLATUI_DIR)/jni/android_config.mk

# realpath-portable from android_config.mk
LOCAL_PATH:=$(call realpath-portable,$(DEPENDENCIES_HARFBUZZ_DIR))

###########################
#
# Harfbuzz build
#
###########################

HARFBUZZ_SRC_FILES = \
    src/hb-blob.cc \
    src/hb-buffer-serialize.cc \
    src/hb-buffer.cc \
    src/hb-common.cc \
    src/hb-face.cc \
    src/hb-font.cc \
    src/hb-ft.cc \
    src/hb-ot-tag.cc \
    src/hb-set.cc \
    src/hb-shape.cc \
    src/hb-shape-plan.cc \
    src/hb-shaper.cc \
    src/hb-unicode.cc \
    src/hb-warning.cc \
    src/hb-ot-font.cc \
    src/hb-ot-layout.cc \
    src/hb-ot-map.cc \
    src/hb-ot-shape.cc \
    src/hb-ot-shape-complex-arabic.cc \
    src/hb-ot-shape-complex-default.cc \
    src/hb-ot-shape-complex-hangul.cc \
    src/hb-ot-shape-complex-hebrew.cc \
    src/hb-ot-shape-complex-indic.cc \
    src/hb-ot-shape-complex-indic-table.cc \
    src/hb-ot-shape-complex-myanmar.cc \
    src/hb-ot-shape-complex-thai.cc \
    src/hb-ot-shape-complex-tibetan.cc \
    src/hb-ot-shape-complex-use.cc \
    src/hb-ot-shape-complex-use-table.cc \
    src/hb-ot-shape-normalize.cc \
    src/hb-ot-shape-fallback.cc \
    src/hb-ot-var.cc

#############################################################
#   build the harfbuzz shared library
#
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_SRC_FILES := $(HARFBUZZ_SRC_FILES)
LOCAL_CPP_EXTENSION := .cc
LOCAL_C_INCLUDES += \
    $(FLATUI_DIR)/external/include/harfbuzz \
    $(DEPENDENCIES_HARFBUZZ_DIR)/src \
    $(DEPENDENCIES_HARFBUZZ_DIR)/src/hb-ucdn \
    $(DEPENDENCIES_FREETYPE_DIR)/include
LOCAL_CFLAGS += -DHAVE_OT -DHB_NO_MT -DHB_NO_UNICODE_FUNCS
LOCAL_MODULE := libharfbuzz
LOCAL_MODULE_FILENAME := libharfbuzz
LOCAL_STATIC_LIBRARIES := libfreetype

include $(BUILD_STATIC_LIBRARY)
