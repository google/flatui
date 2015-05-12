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
cmake_minimum_required(VERSION 2.8.12)

project(imguitest)

add_executable(imguitest imguitest.cpp)
add_dependencies(imguitest fplbase imgui)
mathfu_configure_flags(imguitest)
target_link_libraries(imguitest fplbase imgui)

# Imgui postprocess
imguiPostProcess(imguitest)

# Copy test resource files
add_custom_command(TARGET imguitest POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_CURRENT_SOURCE_DIR}/assets
                       $<TARGET_FILE_DIR:imguitest>)

