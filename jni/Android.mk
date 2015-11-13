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
# Override the default behavior of local-source-file-path to workaround
# a bug which prevents the build of deeply nested projects when NDK_OUT is
# set.
local-source-file-path = \
  $(if $(call host-path-is-absolute,$1),$1,$(realpath $(LOCAL_PATH)/$1))

include $(call all-makefiles-under,$(call my-dir)/libs)
