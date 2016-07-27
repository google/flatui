// Copyright 2016 Google Inc. All rights reserved.
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
#include <chrono>
#include <flatbuffers/flatbuffers.h>
#ifdef __APPLE__
// Include CoreFoundation headers in macOS/iOS.
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CTFont.h>
#endif
#include "font_manager.h"

namespace flatui {

#ifdef __APPLE__
// SFNT headers to generate SFNT blob.
struct SFNTHeader {
  int32_t version;
  uint16_t num_tables;
  uint16_t search_range;
  uint16_t entry_selector;
  uint16_t range_shift;
};

struct SFNTTableEntry {
  uint32_t tag;
  uint32_t check_sum;
  uint32_t offset;
  uint32_t length;
};

// Helper functions for CGFontToSFNT().
static uint32_t CalculateTableEntryCheckSum(const uint32_t *table,
                                            uint32_t size) {
  uint32_t sum = 0;
  uint32_t num_long = (size + sizeof(uint32_t) - 1) >> 2;
  while (num_long-- > 0) {
    sum += ntohl(*table++);
  }
  return sum;
}

// Helper function to convert CGFont to in-memory SFNT.
// on iOS/macOS, system font files are not directory accessible from an
// application. The helper extracts glyph data from CGFont and wraps it
// into a in-memory SFNT blob that FreeType can load.
static bool CGFontToSFNT(CGFontRef font, std::string *data) {
  assert(font);
  assert(data);

  // Retrieve tags.
  auto tags = CGFontCopyTableTags(font);
  auto table_count = CFArrayGetCount(tags);
  auto has_cff_table = false;
  auto total = sizeof(SFNTHeader) + sizeof(SFNTTableEntry) * table_count;

  // Allocate a table.
  std::unique_ptr<CFDataRef> table_data(new CFDataRef[table_count]);

  // Retrieve the blob data size.
  const uint32_t kCFFTag = 0x43464620;  //'CFF '
  for (auto i = 0; i < table_count; ++i) {
    uint32_t tag = reinterpret_cast<uint64_t>(CFArrayGetValueAtIndex(tags, i));
    if (tag == kCFFTag) {
      has_cff_table = true;
    }
    auto p = table_data.get()[i] = CGFontCopyTableForTag(font, tag);
    if (p != nullptr) {
      total += mathfu::RoundUpToTypeBoundary<uint32_t>(CFDataGetLength(p));
    }
  }

  // Resize the destination buffer.
  data->resize(total, 0);
  auto buffer = reinterpret_cast<uint8_t *>(&(*data)[0]);
  auto start = buffer;

  // Calculate values for the header.
  // search_range = (Maximum power of 2 <= num_tables) x 16.
  // entry_selector = Log2(maximum power of 2 <= num_tables).
  // range_shift = num_tables x 16 - search_range.
  // For more details of SFNT format, refer
  // https://www.microsoft.com/typography/otspec/otff.htm
  uint16_t entry_selector = 0;
  uint16_t search_range = 1;
  while (search_range < (table_count >> 1)) {
    entry_selector++;
    search_range *= 2;
  }
  search_range <<= 4;
  uint16_t range_shift = (table_count << 4) - search_range;

  // Setup header.
  uint32_t kOTVersion = 0x4f54544f;  //'OTTO'
  auto header = reinterpret_cast<SFNTHeader *>(buffer);
  buffer += sizeof(SFNTHeader);
  header->version =
      has_cff_table ? kOTVersion : flatbuffers::EndianSwap<uint16_t>(1);
  header->num_tables = flatbuffers::EndianSwap<uint16_t>(table_count);
  header->search_range = flatbuffers::EndianSwap<uint16_t>(search_range);
  header->entry_selector = flatbuffers::EndianSwap<uint16_t>(entry_selector);
  header->range_shift = flatbuffers::EndianSwap<uint16_t>(range_shift);

  auto entry = reinterpret_cast<SFNTTableEntry *>(buffer);
  buffer += sizeof(SFNTTableEntry) * table_count;

  for (auto i = 0; i < table_count; ++i) {
    if (table_data.get()[i]) {
      auto p = table_data.get()[i];
      auto size = CFDataGetLength(p);

      // Copy data.
      memcpy(buffer, CFDataGetBytePtr(p), size);

      // Setup a table entry.
      auto tag = reinterpret_cast<uint64_t>(CFArrayGetValueAtIndex(tags, i));
      entry->tag = flatbuffers::EndianSwap<uint32_t>(tag);
      entry->check_sum =
          flatbuffers::EndianSwap<uint32_t>(CalculateTableEntryCheckSum(
              reinterpret_cast<uint32_t *>(buffer), size));

      auto offset = buffer - start;
      entry->offset = flatbuffers::EndianSwap<uint32_t>(offset);
      entry->length = flatbuffers::EndianSwap<uint32_t>(size);
      buffer += mathfu::RoundUpToTypeBoundary<uint32_t>(size);
      ++entry;
      CFRelease(p);
    }
  }

  CFRelease(tags);
  return true;
}
#endif  // __APPLE__

// Open specified font by name and return a raw data.
// Current implementation works on macOS/iOS.
#ifdef __APPLE__
static bool OpenFontByName(CFStringRef name, std::string *dest) {
  auto cgfont = CGFontCreateWithFontName(name);
  if (cgfont == nullptr) {
    CFRelease(name);
    return false;
  }

  // Load the font file of assets.
  if (!CGFontToSFNT(cgfont, dest)) {
    LogInfo("Can't load font reource: %s\n", name);
    return false;
  }

  CFRelease(cgfont);
  return true;
}
#endif  // __APPLE__

bool FontManager::OpenFontByName(const char *font_name, std::string *dest) {
#ifdef __APPLE__
  // Open the font using CGFont API and create font data from them.
  auto name = CFStringCreateWithCString(kCFAllocatorDefault, font_name,
                                        kCFStringEncodingUTF8);
  bool ret = flatui::OpenFontByName(name, dest);
  CFRelease(name);

  return ret;
#else   // __APPLE__
  (void)font_name;
  (void)dest;
  fplbase::LogInfo("OpenFontByName() not implemented on the platform.");
  return false;
#endif  // __APPLE__
}

// Open system's default font with a fallback list.
bool FontManager::OpenSystemFont() {
#ifdef __APPLE__
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_6_0 || \
    __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_8
  if (system_fallback_list_.size()) {
    fplbase::LogInfo("The system font is already loaded.");
    return true;
  }

  // Retrieve the system font fallback list using CFPreferences.
  const CFStringRef kTemporaryFontName = CFSTR("Helvetica");
  auto tmporary_font = CTFontCreateWithName(kTemporaryFontName, 0.0f, nullptr);
  auto languages = static_cast<CFArrayRef>(CFPreferencesCopyAppValue(
      CFSTR("AppleLanguages"), kCFPreferencesCurrentApplication));
  auto cascade_list =
      CTFontCopyDefaultCascadeListForLanguages(tmporary_font, languages);
  auto fallback_count = CFArrayGetCount(cascade_list);

  // Iterate through the list and load each font.
  bool ret = false;
  const int32_t kStringLength = 128;
  char str[kStringLength];

  auto start = std::chrono::system_clock::now();
  auto total_size = 0;
  for (auto i = 0; i < fallback_count; ++i) {
    auto descriptor = static_cast<CTFontDescriptorRef>(
        CFArrayGetValueAtIndex(cascade_list, i));
    auto font_name = static_cast<CFStringRef>(
        CTFontDescriptorCopyAttribute(descriptor, kCTFontNameAttribute));

    // Open the font.
    if (font_name != nullptr) {
      CFStringGetCString(font_name, str, kStringLength, kCFStringEncodingUTF8);
      fplbase::LogInfo("Font name: %s", str);
      if (Open(str, true)) {
        // Retrieve the font size for an information.
        auto it = map_faces_.find(str);
        total_size += it->second->font_data_.size();
        system_fallback_list_.push_back(str);
        ret = true;
      }
      CFRelease(font_name);
    }
  }
  auto end = std::chrono::system_clock::now();

  fplbase::LogInfo(
      "Loaded system font. %d fonts in the fallback list."
      " Total size:%d KB, Duration:%d msec",
      fallback_count, total_size / 1024,
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start));

  // Clean up.
  CFRelease(cascade_list);
  CFRelease(tmporary_font);
  CFRelease(languages);
  return ret;
#else   // __IPHONE_OS_VERSION_MIN_REQUIRED || __MAC_OS_X_VERSION_MIN_REQUIRED
  fplbase::LogInfo("OpenSystemFont() requires iOS6~/macOS10.8~");
  return false;
#endif  // __IPHONE_OS_VERSION_MIN_REQUIRED || __MAC_OS_X_VERSION_MIN_REQUIRED
#else   // __APPLE__
  fplbase::LogInfo("OpenSystemFont() not implemented on the platform");
  return false;
#endif  // __APPLE__
}

bool FontManager::CloseSystemFont() {
#ifdef __APPLE__
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_6_0 || \
    __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_8
  auto it = system_fallback_list_.begin();
  auto end = system_fallback_list_.end();
  while (it != end) {
    Close(it->c_str());
    it++;
  }
  system_fallback_list_.clear();
  return true;
#else   // __IPHONE_OS_VERSION_MIN_REQUIRED || __MAC_OS_X_VERSION_MIN_REQUIRED
  fplbase::LogInfo("CloseSystemFont() requires iOS6~ or macOS10.8~");
  return false;
#endif  // __IPHONE_OS_VERSION_MIN_REQUIRED || __MAC_OS_X_VERSION_MIN_REQUIRED
#else   // __APPLE__
  fplbase::LogInfo("CloseSystemFont() not implemented on the platform");
  return false;
#endif  // __APPLE__
}

}  // namespace flatui
