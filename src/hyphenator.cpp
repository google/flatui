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

#include "precompiled.h"

// libunibreak header
#include <unibreakdef.h>

#include "flatui.h"
#include "font_manager.h"

namespace flatui {
const uint8_t kCharSoftHyphen = 0xad;

// The following are structs that correspond to tables inside the hyb file
// format

struct AlphabetTable0 {
  uint32_t version;
  uint32_t min_codepoint;
  uint32_t max_codepoint;
  uint8_t data[1];  // actually flexible array, size is known at runtime
};

struct AlphabetTable1 {
  uint32_t version;
  uint32_t n_entries;
  uint32_t data[1];  // actually flexible array, size is known at runtime

  static uint32_t codepoint(uint32_t entry) { return entry >> 11; }
  static uint32_t value(uint32_t entry) { return entry & 0x7ff; }
};

struct Trie {
  uint32_t version;
  uint32_t char_mask;
  uint32_t link_shift;
  uint32_t link_mask;
  uint32_t pattern_shift;
  uint32_t n_entries;
  uint32_t data[1];  // actually flexible array, size is known at runtime
};

struct Pattern {
  uint32_t version;
  uint32_t n_entries;
  uint32_t pattern_offset;
  uint32_t pattern_size;
  uint32_t data[1];  // actually flexible array, size is known at runtime

  // accessors
  static uint32_t len(uint32_t entry) { return entry >> 26; }
  static uint32_t shift(uint32_t entry) { return (entry >> 20) & 0x3f; }
  const uint8_t* buf(uint32_t entry) const {
    return reinterpret_cast<const uint8_t*>(this) + pattern_offset +
           (entry & 0xfffff);
  }
};

struct Header {
  uint32_t magic;
  uint32_t version;
  uint32_t alphabet_offset;
  uint32_t trie_offset;
  uint32_t pattern_offset;
  uint32_t file_size;

  // accessors
  const uint8_t* bytes() const {
    return reinterpret_cast<const uint8_t*>(this);
  }
  uint32_t alphabetVersion() const {
    return *reinterpret_cast<const uint32_t*>(bytes() + alphabet_offset);
  }
  const AlphabetTable0* alphabetTable0() const {
    return reinterpret_cast<const AlphabetTable0*>(bytes() + alphabet_offset);
  }
  const AlphabetTable1* alphabetTable1() const {
    return reinterpret_cast<const AlphabetTable1*>(bytes() + alphabet_offset);
  }
  const Trie* trieTable() const {
    return reinterpret_cast<const Trie*>(bytes() + trie_offset);
  }
  const Pattern* patternTable() const {
    return reinterpret_cast<const Pattern*>(bytes() + pattern_offset);
  }
};

bool Hyphenator::Open(const char* hyb_name) {
  // Close existing data.
  Close();

  auto size = 0;
  auto p = fplbase::MapFile(hyb_name, 0, &size);
  if (p) {
    pattern_data_ = static_cast<const uint8_t*>(p);
    size_ = size;
  } else {
    // Fallback to regular file load.
    if (!fplbase::LoadFile(hyb_name, &pattern_file_)) {
      fplbase::LogError("Can't load hyphenation pattern: %s\n", hyb_name);
      return false;
    }
    p = pattern_file_.c_str();
    size_ = pattern_file_.size();
  }
  return true;
}

bool Hyphenator::Close() {
  if (pattern_data_ == nullptr) {
    return true;
  }

  if (pattern_file_.empty()) {
    fplbase::UnmapFile(pattern_data_, size_);
    pattern_data_ = nullptr;
  } else {
    pattern_file_.clear();
  }
  return true;
}

void Hyphenator::Hyphenate(const uint8_t* word, size_t len,
                           std::vector<uint8_t>* result) {
  result->clear();
  if (pattern_data_ != nullptr && len >= kMinPrefix + kMinSuffix) {
    std::vector<uint16_t> alpha_codes;
    if (AlphabetLookup(word, len, &alpha_codes)) {
      result->resize(alpha_codes.size() - 2);
      HyphenateFromCodes(alpha_codes, result);
      return;
    }
    // TODO: try NFC normalization
    // TODO: handle non-BMP Unicode (requires remapping of offsets)
  }
  HyphenateSoft(word, len, result);
}

// If any soft hyphen is present in the word, use soft hyphens to decide
// hyphenation,
// as recommended in UAX #14 (Use of Soft Hyphen)
void Hyphenator::HyphenateSoft(const uint8_t* word, size_t len,
                               std::vector<uint8_t>* result) {
  size_t idx = 0;
  while (idx < len) {
    auto c = ub_get_next_char_utf8(word, len, &idx);
    result->push_back(c == kCharSoftHyphen);
  }
}

bool Hyphenator::AlphabetLookup(const uint8_t* word, size_t len,
                                std::vector<uint16_t>* alpha_codes) {
  const Header* header = GetHeader();
  // TODO: check header magic
  uint32_t alphabetVersion = header->alphabetVersion();
  if (alphabetVersion == 0) {
    const AlphabetTable0* alphabet = header->alphabetTable0();
    uint32_t min_codepoint = alphabet->min_codepoint;
    uint32_t max_codepoint = alphabet->max_codepoint;
    alpha_codes->push_back(0);  // word start

    size_t idx = 0;
    while (idx < len) {
      auto c = ub_get_next_char_utf8(word, len, &idx);
      if (c < min_codepoint || c >= max_codepoint) {
        continue;
      }
      uint8_t code = alphabet->data[c - min_codepoint];
      if (code == 0) {
        return false;
      }
      alpha_codes->push_back(code);
    }
    alpha_codes->push_back(0);  // word termination
    return true;
  } else if (alphabetVersion == 1) {
    const AlphabetTable1* alphabet = header->alphabetTable1();
    size_t n_entries = alphabet->n_entries;
    const uint32_t* begin = alphabet->data;
    const uint32_t* end = begin + n_entries;
    alpha_codes->push_back(0);  // word start

    size_t idx = 0;
    while (idx < len) {
      auto c = ub_get_next_char_utf8(word, len, &idx);
      auto p = std::lower_bound(begin, end, c << 11);
      if (p == end) {
        return false;
      }
      uint32_t entry = *p;
      if (AlphabetTable1::codepoint(entry) != c) {
        continue;
      }
      alpha_codes->push_back(AlphabetTable1::value(entry));
    }

    alpha_codes->push_back(0);  // word termination
    return true;
  }
  return false;
}

/**
 * Internal implementation, after conversion to codes. All case folding and
 *normalization
 * has been done by now, and all characters have been found in the alphabet.
 * Note: len here is the padded length including 0 codes at start and end.
 **/
void Hyphenator::HyphenateFromCodes(const std::vector<uint16_t>& codes,
                                    std::vector<uint8_t>* result) {
  const Header* header = GetHeader();
  const Trie* trie = header->trieTable();
  const Pattern* pattern = header->patternTable();

  auto len = codes.size();
  uint32_t char_mask = trie->char_mask;
  uint32_t link_shift = trie->link_shift;
  uint32_t link_mask = trie->link_mask;
  uint32_t pattern_shift = trie->pattern_shift;
  size_t maxOffset = len - kMinSuffix - 1;
  for (size_t i = 0; i < len - 1; i++) {
    uint32_t node = 0;  // index into Trie table
    for (size_t j = i; j < len; j++) {
      uint16_t c = codes[j];
      uint32_t entry = trie->data[node + c];
      if ((entry & char_mask) == c) {
        node = (entry & link_mask) >> link_shift;
      } else {
        break;
      }
      uint32_t pat_ix = trie->data[node] >> pattern_shift;
      // pat_ix contains a 3-tuple of length, shift (number of trailing zeros),
      // and an offset
      // into the buf pool. This is the pattern for the substring (i..j) we just
      // matched,
      // which we combine (via point-wise max) into the result vector.
      if (pat_ix != 0) {
        uint32_t pat_entry = pattern->data[pat_ix];
        int32_t pat_len = Pattern::len(pat_entry);
        int32_t pat_shift = Pattern::shift(pat_entry);
        const uint8_t* pat_buf = pattern->buf(pat_entry);
        int32_t offset = j + 1 - (pat_len + pat_shift);
        // offset is the index within result that lines up with the start of
        // pat_buf
        auto start = std::max(kMinPrefix - offset, 0);
        auto end = std::min(pat_len, static_cast<int32_t>(maxOffset) - offset);
        for (auto k = start; k < end; k++) {
          result->at(offset + k) = std::max(result->at(offset + k), pat_buf[k]);
        }
      }
    }
  }

  // Since the above calculation does not modify values outside
  // [kMinPrefix, len - kMinSuffix], they are left as 0.
  for (size_t i = kMinPrefix; i < maxOffset; i++) {
    result->at(i) &= 1;
  }
}

}  // namespace flatui
