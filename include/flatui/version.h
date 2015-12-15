// Copyright 2015 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FLATUI_VERSION_H_
#define FLATUI_VERSION_H_

namespace flatui {

struct FlatUiVersion {
  // Version number, updated only on major releases.
  unsigned char major;

  // Version number, updated for point releases.
  unsigned char minor;

  // Version number, updated for tiny releases, for example, bug fixes.
  unsigned char revision;

  // Text string holding the name and version of library.
  const char* text;
};

const FlatUiVersion& Version();

}  // namespace flatui

#endif  // FLATUI_VERSION_H_
