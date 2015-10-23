#!/bin/bash -eu
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

# Reconfigure harfbuzz if the configuration script has changed relative to the
# Makefile.

if [[ $# -eq 0 ]]; then
  echo "Usage: $(basename $0) harfbuzz_dir" >&2
  exit 1
fi

declare -r harfbuzz_dir="${1}"
declare -r makefile="${harfbuzz_dir}/Makefile"
declare -r configuration="${harfbuzz_dir}/configure.ac"
declare -r configure_script="${harfbuzz_dir}/autogen.sh"
if [[ ! -e ${makefile} || \
      ( $(stat -c %Y "${makefile}") -lt $(stat -c %Y "${configuration}") ) ]];
   then
  pushd "${harfbuzz_dir}"
  ${configure_script}
fi
