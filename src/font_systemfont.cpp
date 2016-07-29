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
#ifdef __ANDROID__
#include <fplutil/android_utils.h>
#endif  // __ANDROID__

#ifdef __APPLE__
// Include CoreFoundation headers in macOS/iOS.
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CTFont.h>
#endif
#include "font_manager.h"

namespace flatui {
#ifdef _DEBUG
#define FLATUI_PROFILE_SYSTEM_FONT_SEARCH (1)
#endif  // _DEBUG

#ifdef __ANDROID__
using fplutil::JniObject;

// XML parser on Android.
// Enum indicating a tag type used while parsing the XML source.
enum {
  kStartDocument = 0,
  kEndDocument = 1,
  kStartTag = 2,
  kEndTag = 3,
  kText = 4,
};

// A tree node structure represents XML nodes.
const int32_t kInvalidNode = -1;
class XMLNode {
 public:
  XMLNode() : child_index_(kInvalidNode), sibling_index_(kInvalidNode) {}
  int32_t child_index_;
  int32_t sibling_index_;
  std::string name_;
  std::string text_;
  std::map<std::string, std::string> attributes_;
};

// The XML parser uses Android's XmlPullParser implementation using JNI
// interface and constructs a node tree by XMLNode. An application can access
// the node information
// through get_nodes() API. The first element of the array is a root node of the
// parsed XML.
class XMLParser {
 public:
  XMLParser() : sibling_node_(kInvalidNode) {};
  bool Open(std::string& str) {
    // Create XML parser object.
    JniObject parser =
        JniObject::CallStaticObjectMethod("android/util/Xml", "newPullParser",
                                          "()Lorg/xmlpull/v1/XmlPullParser;");

    // Open the source file.
    JniObject src = JniObject::CreateString(str);
    JniObject reader = JniObject::CreateObject(
        "java/io/StringReader", "(Ljava/lang/String;)V", src.get_object());
    parser.CallVoidMethod("setInput", "(Ljava/io/Reader;)V",
                          reader.get_object());

    // Construct a node tree at once.
    auto event = parser.CallIntMethod("getEventType", "()I");
    while (event != kEndDocument) {
      if (event == kStartDocument) {
        //        System.out.println("Start document");
      } else if (event == kStartTag) {
        std::string name =
            parser.CallStringMethod("getName", "()Ljava/lang/String;");
        AddNode(name);
        auto count = parser.CallIntMethod("getAttributeCount", "()I");
        for (auto i = 0; i < count; ++i) {
          std::string attr_name = parser.CallStringMethod(
              "getAttributeName", "(I)Ljava/lang/String;", i);
          std::string value = parser.CallStringMethod(
              "getAttributeValue", "(I)Ljava/lang/String;", i);
          SetAttribute(attr_name, value);
        }
      } else if (event == kEndTag) {
        PopNode();
      } else if (event == kText) {
        std::string text =
            parser.CallStringMethod("getText", "()Ljava/lang/String;");
        SetText(text);
      }
      event = parser.CallIntMethod("next", "()I");
    }

    return true;
  }
  std::vector<XMLNode>& get_nodes() { return nodes_; }

 private:
  void AddNode(std::string& name) {
    // Set up the node.
    XMLNode node;
    node.name_ = name;

    // Set up links.
    auto index = nodes_.size();
    if (sibling_node_ != kInvalidNode) {
      if (nodes_[sibling_node_].sibling_index_ == kInvalidNode) {
        nodes_[sibling_node_].sibling_index_ = index;
      }
    }
    if (node_stack_.size()) {
      if (nodes_[node_stack_.back()].child_index_ == kInvalidNode) {
        nodes_[node_stack_.back()].child_index_ = index;
      }
    }
    // Add the node to the vector and the stack.
    sibling_node_ = kInvalidNode;
    node_stack_.push_back(nodes_.size());
    nodes_.emplace_back(node);
  }

  void PopNode() {
    // Pop the node from the stack.
    sibling_node_ = node_stack_.back();
    node_stack_.pop_back();
  }

  void SetAttribute(std::string& name, std::string& value) {
    nodes_.back().attributes_[name] = value;
  }

  void SetText(std::string& text) {
    if (nodes_.back().text_.length() == 0) {
      nodes_.back().text_ = text;
    }
  }
  std::vector<XMLNode> nodes_;
  std::vector<int32_t> node_stack_;
  int32_t sibling_node_;
};
#endif  // __ANDROID__

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
static uint32_t CalculateTableEntryCheckSum(const uint32_t* table,
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
static bool CGFontToSFNT(CGFontRef font, std::string* data) {
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
  auto buffer = reinterpret_cast<uint8_t*>(&(*data)[0]);
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
  auto header = reinterpret_cast<SFNTHeader*>(buffer);
  buffer += sizeof(SFNTHeader);
  header->version =
      has_cff_table ? kOTVersion : flatbuffers::EndianSwap<uint16_t>(1);
  header->num_tables = flatbuffers::EndianSwap<uint16_t>(table_count);
  header->search_range = flatbuffers::EndianSwap<uint16_t>(search_range);
  header->entry_selector = flatbuffers::EndianSwap<uint16_t>(entry_selector);
  header->range_shift = flatbuffers::EndianSwap<uint16_t>(range_shift);

  auto entry = reinterpret_cast<SFNTTableEntry*>(buffer);
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
              reinterpret_cast<uint32_t*>(buffer), size));

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
static bool OpenFontByName(CFStringRef name, std::string* dest) {
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

bool FontManager::OpenFontByName(const char* font_name, std::string* dest) {
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
  return OpenSystemFontApple();
#elif defined(__ANDROID__)
  return OpenSystemFontAndroid();
#else   // __APPLE__ || __ANDROID__
  fplbase::LogInfo("OpenSystemFont() not implemented on the platform");
  return false;
#endif  // __APPLE__ || __ANDROID__
}

bool FontManager::CloseSystemFont() {
#ifdef __APPLE__
  return CloseSystemFontApple();
#elif defined(__ANDROID__)
  return CloseSystemFontAndroid();
#else   // __APPLE__ || __ANDROID__
  fplbase::LogInfo("CloseSystemFont() not implemented on the platform");
  return false;
#endif  // __APPLE__ || __ANDROID__
}

// A system font access implementation for iOS/macOS.
#ifdef __APPLE__
bool FontManager::OpenSystemFontApple() {
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

#ifdef FLATUI_PROFILE_SYSTEM_FONT_SEARCH
  auto start = std::chrono::system_clock::now();
#endif  // FLATUI_PROFILE_SYSTEM_FONT_SEARCH
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

        FontFamily family;
        family.family_name_ = str;
        system_fallback_list_.emplace_back(family);
        ret = true;
      }
      CFRelease(font_name);
    }
  }
#ifdef FLATUI_PROFILE_SYSTEM_FONT_SEARCH
  auto end = std::chrono::system_clock::now();

  fplbase::LogInfo(
      "Loaded system font. %d fonts in the fallback list."
      " Total size:%d KB, Duration:%d msec",
      fallback_count, total_size / 1024,
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start));
#endif  // FLATUI_PROFILE_SYSTEM_FONT_SEARCH

  // Clean up.
  CFRelease(cascade_list);
  CFRelease(tmporary_font);
  CFRelease(languages);
  return ret;
#else   // __IPHONE_OS_VERSION_MIN_REQUIRED || __MAC_OS_X_VERSION_MIN_REQUIRED
  fplbase::LogInfo("OpenSystemFont() requires iOS6~/macOS10.8~");
  return false;
#endif  // __IPHONE_OS_VERSION_MIN_REQUIRED || __MAC_OS_X_VERSION_MIN_REQUIRED
}

bool FontManager::CloseSystemFontApple() {
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_6_0 || \
    __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_8
  auto it = system_fallback_list_.begin();
  auto end = system_fallback_list_.end();
  while (it != end) {
    Close(it->family_name_.c_str());
    it++;
  }
  system_fallback_list_.clear();
  return true;
#else   // __IPHONE_OS_VERSION_MIN_REQUIRED || __MAC_OS_X_VERSION_MIN_REQUIRED
  fplbase::LogInfo("CloseSystemFont() requires iOS6~ or macOS10.8~");
  return false;
#endif  // __IPHONE_OS_VERSION_MIN_REQUIRED || __MAC_OS_X_VERSION_MIN_REQUIRED
}
#endif  // __APPLE__

// A system font access implementation for Android.
#ifdef __ANDROID__
bool FontManager::OpenSystemFontAndroid() {
  const char* filename = "/system/etc/fonts.xml";
  std::string file;
  if (!fplbase::LoadFile(filename, &file)) {
    fplbase::LogInfo("Failed loading the setting file.");
    return false;
  }

  // Set the environment to the helper.
  JniObject::SetEnv(fplbase::AndroidGetJNIEnv());

  XMLParser parser;
  parser.Open(file);

  // Retrieve the font fallback list.
  std::vector<XMLNode> nodes = parser.get_nodes();

  // Retrieve the first 'family' entry without 'name' attribute.
  auto index = kInvalidNode;
  auto size = nodes.size();
  for (size_t i = 0; i < size; ++i) {
    if (nodes[i].name_ == "family" &&
        nodes[i].attributes_.find("name") == nodes[i].attributes_.end()) {
      index = i;
      break;
    }
  }

  // Generates a font name list.
  auto ret = false;
#ifdef FLATUI_PROFILE_SYSTEM_FONT_SEARCH
  auto start = std::chrono::system_clock::now();
#endif  // FLATUI_PROFILE_SYSTEM_FONT_SEARCH
  auto total_size = 0;
  const char* kSystemFontFolder = "/system/fonts/";
  while (index != kInvalidNode) {
    XMLNode family = nodes[index];
    if (family.child_index_ != kInvalidNode) {
      XMLNode font = nodes[family.child_index_];

      std::string str = kSystemFontFolder + font.text_;
      fplbase::LogInfo("Font name: %s", str.c_str());
      if (Open(str.c_str())) {
        // Retrieve the font size for an information.
        auto it = map_faces_.find(str);
        total_size += it->second->font_data_.size();

        FontFamily fnt;
        fnt.file_name_ = str;
        fnt.lang_ = family.attributes_["lang"];
        auto idx = font.attributes_["index"];
        if (idx.length()) {
          fnt.index_ = atoi(idx.c_str());
        }
        system_fallback_list_.emplace_back(fnt);
        ret = true;
      }
    }
    index = family.sibling_index_;
  }
#ifdef FLATUI_PROFILE_SYSTEM_FONT_SEARCH
  auto end = std::chrono::system_clock::now();

  fplbase::LogInfo(
      "Loaded system font. %d fonts in the fallback list."
      " Total size:%d KB, Duration:%d msec",
      system_fallback_list_.size(), total_size / 1024,
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start));
#endif  // FLATUI_PROFILE_SYSTEM_FONT_SEARCH

  return ret;
}

bool FontManager::CloseSystemFontAndroid() {
  auto it = system_fallback_list_.begin();
  auto end = system_fallback_list_.end();
  while (it != end) {
    Close(it->file_name_.c_str());
    it++;
  }
  system_fallback_list_.clear();
  return true;
}
#endif  // __ANDROID__

}  // namespace flatui
