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

# TODO: Remove when the LOCAL_PATH expansion bug in the NDK is fixed.
# Portable version of $(realpath) that omits drive letters on Windows.
realpath-portable = $(join $(filter %:,$(subst :,: ,$1)),\
                      $(realpath $(filter-out %:,$(subst :,: ,$1))))

# Locations of 3rd party and FPL libraries.
FPL_ROOT:=$(FLATUI_DIR)/../../libs
# If the dependencies directory exists either as a subdirectory or as the
# container of this project directory, assume the dependencies directory is
# the root directory for all libraries required by this project.
$(foreach dep_dir,$(wildcard $(FLATUI_DIR)/dependencies) \
                  $(wildcard $(FLATUI_DIR)/../../dependencies),\
  $(eval DEPENDENCIES_ROOT?=$(dep_dir)))
ifneq ($(DEPENDENCIES_ROOT),)
  THIRD_PARTY_ROOT:=$(DEPENDENCIES_ROOT)
  FPL_ROOT:=$(DEPENDENCIES_ROOT)
  PREBUILTS_ROOT:=$(DEPENDENCIES_ROOT)
else
  THIRD_PARTY_ROOT:=$(FPL_ROOT)/../../../external
  PREBUILTS_ROOT:=$(FPL_ROOT)/../../../prebuilts
endif

FLATUI_GENERATED_OUTPUT_DIR := $(FLATUI_DIR)/gen/include

# Location of the SDL library.
DEPENDENCIES_SDL_DIR?=$(THIRD_PARTY_ROOT)/sdl
# Location of the Flatbuffers library.
DEPENDENCIES_FLATBUFFERS_DIR?=$(FPL_ROOT)/flatbuffers
# Location of the fplutil library.
DEPENDENCIES_FPLUTIL_DIR?=$(FPL_ROOT)/fplutil
# Location of the Freetype library.
DEPENDENCIES_FREETYPE_DIR?=$(THIRD_PARTY_ROOT)/freetype
# Location of the HarfBuzz library.
DEPENDENCIES_HARFBUZZ_DIR?=$(THIRD_PARTY_ROOT)/harfbuzz
# Location of the libunibreak library.
DEPENDENCIES_LIBUNIBREAK_DIR?=$(THIRD_PARTY_ROOT)/libunibreak
# Location of the MathFu library.
DEPENDENCIES_MATHFU_DIR?=$(FPL_ROOT)/mathfu
# Location of the motive library.
DEPENDENCIES_MOTIVE_DIR?=$(FPL_ROOT)/motive
# Location of the fplbase library.
DEPENDENCIES_FPLBASE_DIR?=$(FPL_ROOT)/fplbase
# Location of the Cardboard java library (required for fplbase)
DEPENDENCIES_CARDBOARD_DIR?=$(PREBUILTS_ROOT)/cardboard-java/CardboardSample
# Location of the flatui library (for samples and tests).
DEPENDENCIES_FLATUI_DIR?=$(FLATUI_DIR)
# Location of the STB library.
DEPENDENCIES_STB_DIR?=$(THIRD_PARTY_ROOT)/stb

ifeq (,$(DETERMINED_DEPENDENCY_DIRS))
DETERMINED_DEPENDENCY_DIRS:=1
$(eval DEPENDENCIES_DIR_VALUE:=$$(DEPENDENCIES_$(DEP_DIR)_DIR))
print_dependency:
	@echo $(abspath $(DEPENDENCIES_DIR_VALUE))
endif
