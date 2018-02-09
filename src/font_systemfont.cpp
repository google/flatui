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
#include <flatbuffers/flatbuffers.h>

// FreeType2 headers.
// This method of including is odd, but it's the recommended way
// (see https://www.freetype.org/freetype2/docs/tutorial/step1.html).
// It's also necessary to support both Freetype 2.5 and 2.6, since
// the main header is <freetype.h> in 2.5, but <freetype/freetype.h> in 2.6.
#include <ft2build.h>
#include FT_FREETYPE_H

// Platform specific headers.
#ifdef __ANDROID__
#include <fplutil/android_utils.h>
#endif  // __ANDROID__

#ifdef __APPLE__
// Include CoreFoundation headers in macOS/iOS.
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CTFont.h>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <ftw.h>
#include "TargetConditionals.h"
#endif
#include "font_manager.h"

using fplbase::LogInfo;
using fplbase::LogError;

#ifdef __APPLE__
namespace {
  template <typename T>
  struct CFDeleter {
    using pointer = T;
    void operator()(T ref) {
      if (ref) {
        CFRelease(ref);
      }
    }
  };

  template <typename T>
  using cf_ptr = std::unique_ptr<T, CFDeleter<T> >;
}  // namespace
#endif // ifdef __APPLE__

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
    if (!node_stack_.empty()) {
      if (nodes_[node_stack_.back()].child_index_ == kInvalidNode) {
        nodes_[node_stack_.back()].child_index_ = index;
      }
    }
    // Add the node to the vector and the stack.
    sibling_node_ = kInvalidNode;
    node_stack_.push_back(nodes_.size());
    nodes_.push_back(node);
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

// Open specified font by name and return the raw data.
// Current implementation works on macOS/iOS.
#ifdef __APPLE__
#if TARGET_OS_IPHONE

/// @brief Ensures a directory exists at a URL.
/// @return Returns true if it was able to ensure the directory exists.
static bool EnsureDirectory(CFURLRef url) {
  Boolean isReachable = CFURLResourceIsReachable(url, nullptr);
  if (isReachable) {
    return true;
  } else {
    cf_ptr<CFStringRef> dirPath(
        CFURLCopyFileSystemPath(url,
                                kCFURLPOSIXPathStyle));
    char pathBuffer[1024];
    Boolean didGetCString = CFStringGetCString(dirPath.get(),
                                               pathBuffer,
                                               1024,
                                               kCFStringEncodingUTF8);
    if (!didGetCString) {
      return false;
    }

    int mkdirError = mkdir(pathBuffer, 0755);
    return (mkdirError == 0);
  }
}

static bool GetDarwinVersion(char* str, size_t size) {
  int ret = sysctlbyname("kern.osrelease", str, &size, NULL, 0);
  return 0 == ret;
}

static int NftwRemove(const char *fpath,
                      const struct stat *sb,
                      int typeflag,
                      struct FTW *ftwbuf) {
  int rv = remove(fpath);
  if (rv) {
    perror(fpath);
  }

  return rv;
}

static int DeleteDirectory(char *path) {
  return nftw(path, NftwRemove, 64, FTW_DEPTH | FTW_PHYS);
}

static const void* OpenFontByName(CFStringRef name,
                                  int32_t offset,
                                  int32_t *size) {
  const void* returnValue = nullptr;
  cf_ptr<CFURLRef> homeUrl(CFCopyHomeDirectoryURL());
  cf_ptr<CFURLRef> libraryUrl(
      CFURLCreateCopyAppendingPathComponent(nullptr,
                                            homeUrl.get(),
                                            CFSTR("Library"),
                                            true));
  cf_ptr<CFURLRef> cachesUrl(
      CFURLCreateCopyAppendingPathComponent(nullptr,
                                            libraryUrl.get(),
                                            CFSTR("Caches"),
                                            true));

  cf_ptr<CFURLRef> fontsUrl(
    CFURLCreateCopyAppendingPathComponent(nullptr,
                                          cachesUrl.get(),
                                          CFSTR("flatui-fonts"),
                                          true));

  char darwinVersion[256];
  if (!GetDarwinVersion(darwinVersion, sizeof(darwinVersion))) {
    LogError("OpenFontByName: unable to get darwin version");
    return nullptr;
  }

  cf_ptr<CFStringRef> cfDarwinVersion(
      CFStringCreateWithCString(nullptr,
                                darwinVersion,
                                kCFStringEncodingUTF8));

  cf_ptr<CFURLRef> darwinVersionUrl(
    CFURLCreateCopyAppendingPathComponent(nullptr,
                                          fontsUrl.get(),
                                          cfDarwinVersion.get(),
                                          true));

  // Delete cache if invalidated by new operating system version.
  if (CFURLResourceIsReachable(fontsUrl.get(), nullptr) &&
        !CFURLResourceIsReachable(darwinVersionUrl.get(), nullptr)) {
    cf_ptr<CFStringRef> fontsPath(
        CFURLCopyFileSystemPath(fontsUrl.get(),
                                kCFURLPOSIXPathStyle));

    char fontsPathBuffer[1024];
    Boolean didGetCString = CFStringGetCString(fontsPath.get(),
                                               fontsPathBuffer,
                                               1024,
                                               kCFStringEncodingUTF8);
    if (!didGetCString) {
      return nullptr;
    }

    int err = DeleteDirectory(fontsPathBuffer);
    if (0 != err) {
      return nullptr;
    }
  }

  bool didEnsureFontsUrl = EnsureDirectory(fontsUrl.get());
  if (!didEnsureFontsUrl) {
    LogError("OpenFontByName: unable ensure fonts directory exists");
    return nullptr;
  }

  bool didEnsureDarwinVersionUrl = EnsureDirectory(darwinVersionUrl.get());
  if (!didEnsureDarwinVersionUrl) {
    LogError("OpenFontByName: unable ensure darwin version directory exists");
    return nullptr;
  }

  cf_ptr<CFURLRef> cacheUrl(
      CFURLCreateCopyAppendingPathComponent(nullptr,
                                            darwinVersionUrl.get(),
                                            name,
                                            true));

  cf_ptr<CFStringRef> cachePath(
      CFURLCopyFileSystemPath(cacheUrl.get(),
                              kCFURLPOSIXPathStyle));
  char pathBuffer[1024];
  Boolean didGetCString = CFStringGetCString(cachePath.get(),
                                             pathBuffer,
                                             1024,
                                             kCFStringEncodingUTF8);
  if (!didGetCString) {
    LogError("OpenFontByName: unable to get cstring from cfstringref");
    return nullptr;
  }

  // Check to see if file exists.
  Boolean isReachable = CFURLResourceIsReachable(cacheUrl.get(), nullptr);
  if (!isReachable) {
    cf_ptr<CGFontRef> cgfont(CGFontCreateWithFontName(name));
    if (cgfont.get() == nullptr) {
      LogError("OpenFontByName: unable to open cgfont");
      return nullptr;
    }

    // Load the font file of assets.
    std::string sfntData;
    bool didConvert = CGFontToSFNT(cgfont.get(), &sfntData);
    if (!didConvert) {
      LogError("OpenFontByName: unable to convert to sfnt");
      return nullptr;
    }

    std::ofstream outfile;
    outfile.open(pathBuffer);
    if (outfile.bad()) {
      LogError("OpenFontByName: unable to open output file");
      return nullptr;
    }
    outfile << sfntData;
  }

  returnValue = fplbase::MapFile(pathBuffer, offset, size);

  return returnValue;
}

#endif  // TARGET_OS_IPHONE

static bool OpenFontByName(CFStringRef name, std::string* dest) {
  auto cgfont = CGFontCreateWithFontName(name);
  if (cgfont == nullptr) {
    return false;
  }

  // Load the font file of assets.
  auto ret = CGFontToSFNT(cgfont, dest);
  CFRelease(cgfont);
  return ret;
}
#endif  // __APPLE__

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

static void LogInfoProxy(const char* fmt, ...) {
  // There's an awful resolution order error which using a REAL va_list lets us dodge.
  va_list my_list;
  va_start(my_list, fmt);
  fplbase::LogInfo(fmt, my_list);
  va_end(my_list);
}

bool FontManager::OpenSystemFontApple() {
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_6_0 || \
    __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_8
  if (!system_fallback_list_.empty()) {
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
  // A set used to check a font coverage while loading system fonts.
  std::set<FT_ULong> font_coverage;
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
    if (font_name != nullptr &&
        CFStringGetCString(font_name, str, kStringLength,
                           kCFStringEncodingUTF8)) {
      LogInfoProxy("Font name %s", str);
      FontFamily family(str, true);
      if (Open(family)) {
        // Retrieve the font size for an information.
        auto it = map_faces_.find(family.get_name());
        if (UpdateFontCoverage(it->second->get_face(), &font_coverage)) {
          total_size += it->second->get_font_size();

          system_fallback_list_.push_back(family);
          ret = true;
        } else {
#ifdef FLATUI_VERBOSE_LOGGING
          fplbase::LogInfo("Skipped loading font:%s %d", str, i);
#endif  // FLATUI_VERBOSE_LOGGING
          Close(str);
        }
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
    Close(it->get_name().c_str());
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

// Helper function to retrieve the system locale on Android.
// We don't just use a locale set to flatui because it would have multiple
// system locale with a priority in API 24~.
static std::vector<std::string> GetSystemLocale() {
  auto api_level = fplbase::AndroidGetApiLevel();
  std::vector<std::string> vec;
  if (api_level >= 24) {
    JniObject locale_list = JniObject::CallStaticObjectMethod(
        "android/os/LocaleList", "getDefault", "()Landroid/os/LocaleList;");
    auto size = locale_list.CallIntMethod("size", "()I");
    for (auto i = 0; i < size; ++i) {
      JniObject locale =
          locale_list.CallObjectMethod("get", "(I)Ljava/util/Locale;", i);
      auto lang =
          locale.CallStringMethod("getLanguage", "()Ljava/lang/String;");
      auto script =
          locale.CallStringMethod("getScript", "()Ljava/lang/String;");
      if (script.length()) {
        lang += "-" + script;
      }

      vec.push_back(lang);
      fplbase::LogInfo("System locale:%s", lang.c_str());
    }
  } else {
    JniObject locale = JniObject::CallStaticObjectMethod(
        "java/util/Locale", "getDefault", "()Ljava/util/Locale;");
    auto lang = locale.CallStringMethod("getLanguage", "()Ljava/lang/String;");
    auto script = locale.CallStringMethod("getScript", "()Ljava/lang/String;");
    if (script.length()) {
      lang += "-" + script;
    }
    vec.push_back(lang);
    fplbase::LogInfo("System locale:%s", lang.c_str());
  }
  return vec;
}

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
  // A set used to check a font coverage while loading system fonts.
  std::set<FT_ULong> font_coverage;
#ifdef FLATUI_PROFILE_SYSTEM_FONT_SEARCH
  auto start = std::chrono::system_clock::now();
#endif  // FLATUI_PROFILE_SYSTEM_FONT_SEARCH
  auto total_size = 0;
  const char* kSystemFontFolder = "/system/fonts/";
  std::vector<FontFamily> font_list;

  // Generate font fallback list.
  while (index != kInvalidNode) {
    XMLNode family = nodes[index];
    if (family.child_index_ != kInvalidNode) {
      XMLNode font = nodes[family.child_index_];

      std::string str = kSystemFontFolder + font.text_;
      auto idx = font.attributes_["index"];
      auto font_index = kIndexInvalid;
      if (idx.length()) {
        font_index = atoi(idx.c_str());
      }
      FontFamily fnt(str, font_index, family.attributes_["lang"], false);
      font_list.push_back(fnt);
    }
    index = family.sibling_index_;
  }

  // Reorder system fonts based on the system locale setting.
  ReorderSystemFonts(&font_list);

  // Add default font to the font list.
  const char* kDefaultFont = "/system/fonts/Roboto-Regular.ttf";
  FontFamily fnt(kDefaultFont, false);
  font_list.insert(font_list.begin(), std::move(fnt));

  // Load fonts.
  auto font_it = font_list.begin();
  auto font_end = font_list.end();
  while (font_it != font_end) {
    if (Open(*font_it)) {
      // Retrieve the font size for an information.
      auto face = map_faces_.find(font_it->get_name());
      if (UpdateFontCoverage(face->second->get_face(), &font_coverage)) {
        total_size += face->second->get_font_size();
        system_fallback_list_.push_back(std::move(*font_it));
        ret = true;
      } else {
#ifdef FLATUI_VERBOSE_LOGGING
        fplbase::LogInfo("Skipped loading font: %s",
                         font_it->get_name().c_str());
#endif  // FLATUI_VERBOSE_LOGGING
        Close(*font_it);
      }
    }
    font_it++;
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

void FontManager::ReorderSystemFonts(std::vector<FontFamily>* font_list) const {
  // Re-order the list based on the system locale setting.
  auto locale = GetSystemLocale();
  auto it = locale.rbegin();
  auto end = locale.rend();
  while (it != end) {
    auto font_it = font_list->begin();
    auto font_end = font_list->end();
    while (font_it != font_end) {
      if (font_it->get_language().compare(0, it->length(), *it) == 0) {
        // Found a font with a priority locale in the fallback list. Bring the
        // font to the top of the list.
        auto font = std::move(*font_it);
        fplbase::LogInfo("Found a priority font: %s",
                         font.get_language().c_str());
        font_list->erase(font_it);
        font_list->insert(font_list->begin(), std::move(font));
        break;
      }
      font_it++;
    }
    it++;
  }
}

bool FontManager::CloseSystemFontAndroid() {
  auto it = system_fallback_list_.begin();
  auto end = system_fallback_list_.end();
  while (it != end) {
    Close(it->get_name().c_str());
    it++;
  }
  system_fallback_list_.clear();
  return true;
}
#endif  // __ANDROID__

bool FontManager::UpdateFontCoverage(FT_Face face,
                                     std::set<FT_ULong>* font_coverage) {
  auto has_new_coverage = false;
  FT_UInt index = 0;
  FT_ULong code = FT_Get_First_Char(face, &index);
  int32_t new_glyph = 0;
  while (index != 0) {
    if (font_coverage->find(code) == font_coverage->end()) {
      has_new_coverage = true;
      font_coverage->insert(code);
      new_glyph++;
    }
    code = FT_Get_Next_Char(face, code, &index);
  }
#ifdef FLATUI_VERBOSE_LOGGING
  fplbase::LogInfo("Has %d new glyphs", new_glyph);
#endif  // FLATUI_VERBOSE_LOGGING
  return has_new_coverage;
}

const void* FaceData::OpenFontByName(const char* font_name,
                                     int32_t offset,
                                     int32_t *size) {
#ifdef __APPLE__
#if TARGET_OS_IPHONE
  // Open the font using CGFont API and create font data from them.
  auto name = CFStringCreateWithCString(kCFAllocatorDefault, font_name,
                                        kCFStringEncodingUTF8);
  const void* ret = flatui::OpenFontByName(name, offset, size);
  if (!ret) {
    LogInfo("Can't load font resource: %s\n", font_name);
  }
  CFRelease(name);
  return ret;
#else  // TARGET_OS_IPHONE
  (void)font_name;
  (void)offset;
  (void)size;
  fplbase::LogInfo("OpenFontByName() not implemented on the platform.");
  return nullptr;
#endif  // TARGET_OS_IPHONE
#else   // __APPLE__
  (void)font_name;
  (void)offset;
  (void)size;
  fplbase::LogInfo("OpenFontByName() not implemented on the platform.");
  return nullptr;
#endif  // __APPLE__
}

bool FaceData::OpenFontByName(const char* font_name, std::string* dest) {
#ifdef __APPLE__
  // Open the font using CGFont API and create font data from them.
  auto name = CFStringCreateWithCString(kCFAllocatorDefault, font_name,
                                        kCFStringEncodingUTF8);
  bool ret = flatui::OpenFontByName(name, dest);
  if (!ret) {
    LogInfo("Can't load font resource: %s\n", font_name);
  }
  CFRelease(name);
  return ret;
#else   // __APPLE__
  (void)font_name;
  (void)dest;
  fplbase::LogInfo("OpenFontByName() not implemented on the platform.");
  return false;
#endif  // __APPLE__
}
}  // namespace flatui
