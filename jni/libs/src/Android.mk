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
FLATUI_RELATIVE_DIR:=../../..
FLATUI_DIR:=$(LOCAL_PATH)/$(FLATUI_RELATIVE_DIR)
include $(FLATUI_DIR)/jni/android_config.mk
include $(DEPENDENCIES_FLATBUFFERS_DIR)/android/jni/include.mk

include $(CLEAR_VARS)

LOCAL_MODULE := flatui
LOCAL_ARM_MODE := arm

LOCAL_EXPORT_C_INCLUDES := $(FLATUI_DIR)/include

LOCAL_C_INCLUDES := \
  $(LOCAL_EXPORT_C_INCLUDES) \
  $(DEPENDENCIES_SDL_DIR) \
  $(DEPENDENCIES_SDL_DIR)/include \
  $(DEPENDENCIES_FPLBASE_DIR)/include \
  $(DEPENDENCIES_FPLUTIL_DIR)/libfplutil/include \
  $(DEPENDENCIES_FREETYPE_DIR)/include \
  $(DEPENDENCIES_HARFBUZZ_DIR)/src \
  $(DEPENDENCIES_LIBUNIBREAK_DIR)/src \
  ${FLATUI_DIR}/external/include/harfbuzz \
  ${FLATUI_DIR}/include/ \
  ${FLATUI_DIR}/include/flatui \
  ${FLATUI_DIR}/src \

LOCAL_SRC_FILES := \
  $(FLATUI_RELATIVE_DIR)/src/flatui.cpp \
  $(FLATUI_RELATIVE_DIR)/src/font_manager.cpp \
  $(FLATUI_RELATIVE_DIR)/src/micro_edit.cpp \

.PHONY: clean
clean: clean_assets clean_generated_includes

LOCAL_STATIC_LIBRARIES := \
  libmathfu \
  SDL2 \
  libfreetype \
  libharfbuzz \
  libunibreak

LOCAL_SHARED_LIBRARIES :=

include $(BUILD_STATIC_LIBRARY)

$(call import-add-path,$(DEPENDENCIES_MATHFU_DIR)/..)

$(call import-module,mathfu/jni)

