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

# realpath-portable from android_config.mk
LOCAL_PATH:=$(call realpath-portable,$(DEPENDENCIES_LIBUNIBREAK_DIR))

###########################
#
# libunibreak build
#
###########################

LIBUNIBREAK_SRC_FILES = \
    src/linebreak.c \
    src/linebreakdata.c \
    src/linebreakdef.c \
    src/unibreakdef.c \
    src/wordbreak.c

#############################################################
#   build the harfbuzz shared library
#
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_SRC_FILES := $(LIBUNIBREAK_SRC_FILES)
LOCAL_C_INCLUDES += $(DEPENDENCIES_LIBUNIBREAK_DIR)/src
LOCAL_MODULE := libunibreak
LOCAL_MODULE_FILENAME := libunibreak

include $(BUILD_STATIC_LIBRARY)
