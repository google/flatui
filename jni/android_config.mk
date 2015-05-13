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

# Locations of 3rd party and FPL libraries.
THIRD_PARTY_ROOT:=$(PIE_NOON_DIR)/../../../../external
PREBUILTS_ROOT:=$(PIE_NOON_DIR)/../../../../prebuilts
FPL_ROOT:=$(PIE_NOON_DIR)/../../libs
# If the dependencies directory exists either as a subdirectory or as the
# container of this project directory, assume the dependencies directory is
# the root directory for all libraries required by this project.
DEPENDENCIES_ROOT?=$(wildcard $(PIE_NOON_DIR)/dependencies)
ifneq ($(DEPENDENCIES_ROOT),)
  THIRD_PARTY_ROOT:=$(DEPENDENCIES_ROOT)
  FPL_ROOT:=$(DEPENDENCIES_ROOT)
  PREBUILTS_ROOT:=$(DEPENDENCIES_ROOT)
endif

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

ifeq (,$(DETERMINED_DEPENDENCY_DIRS))
DETERMINED_DEPENDENCY_DIRS:=1
$(eval DEPENDENCIES_DIR_VALUE:=$$(DEPENDENCIES_$(DEP_DIR)_DIR))
print_dependency:
	@echo $(abspath $(DEPENDENCIES_DIR_VALUE))
endif
