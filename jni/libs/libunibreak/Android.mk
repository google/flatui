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

LIBUNIBREAK_DIR := $(UP_DIR)/${THIRD_PARTY_ROOT}/libunibreak

###########################
#
# libunibreak build
#
###########################

LIBUNIBREAK_SRC_FILES = \
    ${LIBUNIBREAK_DIR}/src/linebreak.c \
    ${LIBUNIBREAK_DIR}/src/linebreakdata.c \
    ${LIBUNIBREAK_DIR}/src/linebreakdef.c \
    ${LIBUNIBREAK_DIR}/src/unibreakdef.c \
    ${LIBUNIBREAK_DIR}/src/wordbreak.c \
    $(NULL)

#############################################################
#   build the harfbuzz shared library
#
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_SRC_FILES := \
    $(LIBUNIBREAK_SRC_FILES)
LOCAL_C_INCLUDES += \
    $(PIE_NOON_DIR) \
    ${DEPENDENCIES_LIBUNIBREAK_DIR}/src

LOCAL_CFLAGS += 
LOCAL_MODULE := libunibreak
LOCAL_MODULE_FILENAME := libunibreak

include $(BUILD_STATIC_LIBRARY)
