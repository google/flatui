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

LOCAL_PATH:=$(call my-dir)

# Project directory relative to this file.
IMGUI_RELATIVE_DIR:=../../..
IMGUI_DIR:=$(LOCAL_PATH)/$(IMGUI_RELATIVE_DIR)
include $(IMGUI_DIR)/jni/android_config.mk
include $(DEPENDENCIES_FLATBUFFERS_DIR)/android/jni/include.mk

include $(CLEAR_VARS)

LOCAL_MODULE := imgui
LOCAL_ARM_MODE := arm

IMGUI_GENERATED_OUTPUT_DIR := $(IMGUI_DIR)/gen/include

LOCAL_C_INCLUDES := \
  $(LOCAL_EXPORT_C_INCLUDES) \
  $(DEPENDENCIES_SDL_DIR) \
  $(DEPENDENCIES_SDL_DIR)/include \
  $(DEPENDENCIES_FPLUTIL_DIR)/libfplutil/include \
  $(DEPENDENCIES_FREETYPE_DIR)/include \
  $(DEPENDENCIES_HARFBUZZ_DIR)/src \
  $(DEPENDENCIES_LIBUNIBREAK_DIR)/src \
  ${IMGUI_DIR}/external/include/harfbuzz \
  $(IMGUI_GENERATED_OUTPUT_DIR) \
  src \
  include/ \
  include/imgui

LOCAL_SRC_FILES := \
  $(IMGUI_RELATIVE_DIR)/src/imgui.cpp \
  $(IMGUI_RELATIVE_DIR)/src/font_manager.cpp \

IMGUI_SCHEMA_DIR := $(IMGUI_DIR)/src/flatbufferschemas

IMGUI_SCHEMA_FILES := \

ifeq (,$(IMGUI_RUN_ONCE))
IMGUI_RUN_ONCE := 1
$(call flatbuffers_header_build_rules,\
  $(IMGUI_SCHEMA_FILES),\
  $(IMGUI_SCHEMA_DIR),\
  $(IMGUI_GENERATED_OUTPUT_DIR),\
  $(DEPENDENCIES_PINDROP_DIR)/schemas $(DEPENDENCIES_MOTIVE_DIR)/schemas,\
  $(LOCAL_SRC_FILES))

.PHONY: clean_generated_includes
clean_generated_includes:
	$(hide) rm -rf $(IMGUI_GENERATED_OUTPUT_DIR)
endif

.PHONY: clean
clean: clean_assets clean_generated_includes

LOCAL_STATIC_LIBRARIES := \
  libmathfu \
  SDL2 \
  libfreetype \
  libharfbuzz \
  libflatbuffers

LOCAL_SHARED_LIBRARIES :=

include $(BUILD_STATIC_LIBRARY)

$(call import-add-path,$(DEPENDENCIES_MATHFU_DIR)/..)

$(call import-module,flatbuffers/android/jni)
$(call import-module,motive/jni)
$(call import-module,mathfu/jni)

