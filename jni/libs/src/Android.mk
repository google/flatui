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

include $(CLEAR_VARS)

LOCAL_MODULE := flatui
LOCAL_ARM_MODE := arm

LOCAL_EXPORT_C_INCLUDES := $(FLATUI_DIR)/include

LOCAL_C_INCLUDES := \
  $(LOCAL_EXPORT_C_INCLUDES) \
  $(DEPENDENCIES_FREETYPE_DIR)/include \
  $(DEPENDENCIES_HARFBUZZ_DIR)/src \
  $(DEPENDENCIES_LIBUNIBREAK_DIR)/src \
  $(FLATUI_DIR)/external/include/harfbuzz \
  $(FLATUI_DIR)/include/ \
  $(FLATUI_DIR)/include/flatui \
  $(FLATUI_DIR)/src

LOCAL_CPPFLAGS := -std=c++11

LOCAL_SRC_FILES := \
  src/flatui.cpp \
  src/flatui_common.cpp \
  src/font_manager.cpp \
  src/micro_edit.cpp

LOCAL_STATIC_LIBRARIES := \
  fplbase \
  libmathfu \
  libfreetype \
  libharfbuzz \
  libunibreak

include $(BUILD_STATIC_LIBRARY)

$(call import-add-path,$(DEPENDENCIES_MATHFU_DIR)/..)
$(call import-add-path,$(DEPENDENCIES_FPLBASE_DIR)/..)

$(call import-module,mathfu/jni)
$(call import-module,fplbase/jni)

