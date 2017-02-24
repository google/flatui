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

LOCAL_PATH:=$(call my-dir)

# Project directory relative to this file.
FLATUI_RELATIVE_DIR:=../../..
FLATUI_DIR=$(LOCAL_PATH)/$(FLATUI_RELATIVE_DIR)
include $(FLATUI_DIR)/jni/android_config.mk
FLATUI_ABSPATH:=$(abspath $(FLATUI_DIR))
FLATUI_RELATIVEPATH:=$(call realpath-portable,$(FLATUI_DIR))

# realpath-portable from android_config.mk
LOCAL_PATH:=$(call realpath-portable,$(DEPENDENCIES_GUMBO_DIR))

###########################
#
# Gumbo build
#
###########################
ifeq ($(wildcard $(DEPENDENCIES_GUMBO_DIR)/src),)
  # Gumbo 0.9.0
  GUMBO_SRC_FILES := \
      attribute.c \
      char_ref.c \
      error.c \
      parser.c \
      string_buffer.c \
      string_piece.c \
      tag.c \
      tokenizer.c \
      utf8.c \
      util.c \
      vector.c
  GUMBO_INCLUDE_DIRS := \
      $(DEPENDENCIES_GUMBO_INCLUDE_DIR) \
      $(DEPENDENCIES_GUMBO_INCLUDE_DIR)/../..
else
  # Gumbo 0.10.1
  GUMBO_SRC_FILES := \
      src/attribute.c \
      src/char_ref.c \
      src/error.c \
      src/parser.c \
      src/string_buffer.c \
      src/string_piece.c \
      src/tag.c \
      src/tokenizer.c \
      src/utf8.c \
      src/util.c \
      src/vector.c
  GUMBO_INCLUDE_DIRS := $(DEPENDENCIES_GUMBO_INCLUDE_DIR)
endif

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_SRC_FILES := $(GUMBO_SRC_FILES)
LOCAL_C_INCLUDES += $(GUMBO_INCLUDE_DIRS)
LOCAL_EXPORT_C_INCLUDES := $(DEPENDENCIES_GUMBO_INCLUDE_DIR)
LOCAL_CFLAGS += -std=c99
LOCAL_MODULE := gumbo-parser
LOCAL_MODULE_FILENAME := gumbo-parser

include $(BUILD_STATIC_LIBRARY)
