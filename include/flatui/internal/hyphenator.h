// Copyright (C) 2015 The Android Open Source Project
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

// FlatUI port of Minikin::Hyphenator from AOSP.
// Original code is in:
// https://android.googlesource.com/platform/frameworks/minikin/+/master/libs/minikin/Hyphenator.cpp
// Note that this version is converted for UTF-8 string.
// Modifications are copyright 2016 Google Inc. All rights reserved.
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

#ifndef FLATUI_HYPHENATOR_H
#define FLATUI_HYPHENATOR_H

namespace flatui {
#ifdef __ANDROID__
const char* const kAndroidDefaultHybPath = "/system/usr/hyphen-data";
#endif  //__ANDROID__

// hyb file header; implementation details are in the .cpp file
struct Header;
class Hyphenator {
 public:
  Hyphenator() : pattern_data_(nullptr), size_(0) {}

  // Compute the hyphenation of a word, storing the hyphenation in result
  // vector. Each entry in the vector is a "hyphen edit" to be applied at the
  // corresponding code unit offset in the word. Currently 0 means no hyphen and
  // 1 means insert hyphen and break, but this will be expanded to other edits
  // for nonstandard hyphenation.
  // Example: word is "hyphen", result is [0 0 1 0 0 0], corresponding to
  // "hy-phen".
  void Hyphenate(const uint8_t* word, size_t len, std::vector<uint8_t>* result);

  // Open Hyphenation pattern binary file (.hyb).
  bool Open(const char* hyb_name);
  bool Close();

 private:
  // apply soft hyphens only, ignoring patterns
  void HyphenateSoft(const uint8_t* word, size_t len,
                     std::vector<uint8_t>* result);

  // try looking up word in alphabet table, return false if any code units fail
  // to map
  // Note that this methor writes len+2 entries into alpha_codes (including
  // start and stop)
  bool AlphabetLookup(const uint8_t* word, size_t len,
                      std::vector<uint16_t>* alpha_codes);

  // calculate hyphenation from patterns, assuming alphabet lookup has already
  // been done
  void HyphenateFromCodes(const std::vector<uint16_t>& codes,
                          std::vector<uint8_t>* result);

  // TODO(hakuro): these should become parameters, as they might vary by locale,
  // screen size, and possibly explicit user control.
  static const int32_t kMinPrefix = 2;
  static const int32_t kMinSuffix = 3;

  // accessors for binary data
  const Header* GetHeader() const {
    return reinterpret_cast<const Header*>(pattern_data_);
  }

  const uint8_t* pattern_data_;
  int32_t size_;
  std::string pattern_file_;
};
}  // namespace flatui

#endif  // FLATUI_HYPHENATOR_H
