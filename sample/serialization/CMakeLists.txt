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
cmake_minimum_required(VERSION 2.8.12)

project(flatuiserializationsample)

build_flatbuffers(${CMAKE_CURRENT_SOURCE_DIR}/custom_widgets.fbs
                  ""
                  flatui_sample_generated_includes
                  ""
                  ${FLATUI_FLATBUFFERS_GENERATED_INCLUDES_DIR}/flatui
                  ""
                  "")

add_executable(flatuiserializationsample flatuiserializationsample.cpp)
add_dependencies(flatuiserializationsample fplbase flatui flatbuffers
    flatui_sample_generated_includes)
mathfu_configure_flags(flatuiserializationsample)
target_link_libraries(flatuiserializationsample fplbase flatui flatbuffers)

# FlatUI postprocess
flatui_post_process(flatuiserializationsample "sample")
