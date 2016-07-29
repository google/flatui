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

LOCAL_PATH:=$(call my-dir)/../../..

# Project directory relative to this file.
FLATUI_DIR:=$(LOCAL_PATH)
include $(FLATUI_DIR)/jni/android_config.mk
include $(DEPENDENCIES_FLATBUFFERS_DIR)/android/jni/include.mk

# realpath-portable from android_config.mk
LOCAL_PATH:=$(call realpath-portable,$(LOCAL_PATH))
FLATUI_DIR:=$(LOCAL_PATH)

include $(CLEAR_VARS)

LOCAL_MODULE := flatui
LOCAL_ARM_MODE := arm

LOCAL_EXPORT_C_INCLUDES := \
  $(DEPENDENCIES_FLATBUFFERS_DIR)/include \
  $(DEPENDENCIES_FPLUTIL_DIR)/libfplutil/include \
  $(DEPENDENCIES_MOTIVE_DIR)/include \
  $(FLATUI_DIR)/include \
  $(FLATUI_GENERATED_OUTPUT_DIR)

LOCAL_C_INCLUDES := \
  $(LOCAL_EXPORT_C_INCLUDES) \
  $(DEPENDENCIES_FREETYPE_DIR)/include \
  $(DEPENDENCIES_HARFBUZZ_DIR)/src \
  $(DEPENDENCIES_LIBUNIBREAK_DIR)/src \
  $(DEPENDENCIES_STB_DIR) \
  $(FLATUI_DIR)/external/include/harfbuzz \
  $(FLATUI_DIR)/include/ \
  $(FLATUI_DIR)/include/flatui \
  $(FLATUI_DIR)/src

LOCAL_CPPFLAGS := -std=c++11

LOCAL_SRC_FILES := \
  src/flatui.cpp \
  src/flatui_common.cpp \
  src/flatui_serialization.cpp \
  src/font_manager.cpp \
  src/font_systemfont.cpp \
  src/glyph_cache.cpp \
  src/hb_complex_font.cpp \
  src/micro_edit.cpp \
  src/script_table.cpp \
  src/version.cpp

LOCAL_STATIC_LIBRARIES := \
  fplbase \
  fplutil \
  flatbuffers \
  libmathfu \
  libmotive \
  libfreetype \
  libharfbuzz \
  libunibreak

FLATUI_SCHEMA_DIR := $(FLATUI_DIR)/schemas
FLATUI_SCHEMA_INCLUDE_DIRS := $(DEPENDENCIES_FPLBASE_DIR)/schemas
FLATUI_SCHEMA_FILES := $(FLATUI_SCHEMA_DIR)/flatui.fbs

FLATBUFFERS_FLATC_ARGS := --gen-mutable

ifeq (,$(FLATUI_RUN_ONCE))
  FLATUI_RUN_ONCE := 1
$(call flatbuffers_header_build_rules, \
  $(FLATUI_SCHEMA_FILES), \
  $(FLATUI_SCHEMA_DIR), \
  $(FLATUI_GENERATED_OUTPUT_DIR)/flatui, \
  $(FLATUI_SCHEMA_INCLUDE_DIRS), \
  $(LOCAL_SRC_FILES), \
  flatui_generated_includes, \
  fplbase_generated_includes)
endif

include $(BUILD_STATIC_LIBRARY)

$(call import-add-path,$(DEPENDENCIES_FLATBUFFERS_DIR)/..)
$(call import-add-path,$(DEPENDENCIES_MATHFU_DIR)/..)
$(call import-add-path,$(DEPENDENCIES_MOTIVE_DIR)/..)
$(call import-add-path,$(DEPENDENCIES_FPLBASE_DIR)/..)
$(call import-add-path,$(DEPENDENCIES_FPLUTIL_DIR)/libfplutil/..)

$(call import-module,flatbuffers/android/jni)
$(call import-module,mathfu/jni)
$(call import-module,motive/jni)
$(call import-module,fplbase/jni)
$(call import-module,fplutil/libfplutil/jni)

