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

#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H

#include <set>
#include <sstream>
#include "fplbase/renderer.h"
#include "fplutil/mutex.h"
#include "flatui/internal/distance_computer.h"
#include "flatui/internal/glyph_cache.h"
#include "flatui/internal/flatui_util.h"
#include "flatui/internal/hb_complex_font.h"

/// @cond FLATUI_INTERNAL
// Use libunibreak for a line breaking.
#if !defined(FLATUI_USE_LIBUNIBREAK)
// For now, it's automatically turned on.
#define FLATUI_USE_LIBUNIBREAK 1
#endif  // !defined(FLATUI_USE_LIBUNIBREAK)

#if defined(__APPLE__) || defined(__ANDROID__)
#define FLATUI_SYSTEM_FONT (1)
#endif  // defined(__APPLE__) || defined(__ANDROID__)

// Forward decls for FreeType.
typedef struct FT_LibraryRec_ *FT_Library;
typedef struct FT_GlyphSlotRec_ *FT_GlyphSlot;
typedef unsigned long FT_ULong;

// Forward decls for Harfbuzz.
typedef const struct hb_language_impl_t *hb_language_t;
/// @endcond

/// @brief Namespace for FlatUI library.
namespace flatui {

/// @file
/// @addtogroup flatui_font_manager
/// @{

/// @cond FLATUI_INTERNAL
// Forward decl.
class FaceData;
class FontTexture;
class FontBuffer;
class FontMetrics;
class WordEnumerator;
struct ScriptInfo;
/// @endcond

/// @var kFreeTypeUnit
///
/// @brief Constant to convert FreeType unit to pixel unit.
///
/// In FreeType & Harfbuzz, the position value unit is 1/64 px whereas
/// configurable in FlatUI. The constant is used to convert FreeType unit
/// to px.
const int32_t kFreeTypeUnit = 64;

/// @var kGlyphCacheWidth
///
/// @brief The default size of the glyph cache width.
const int32_t kGlyphCacheWidth = 1024;

/// @var kGlyphCacheHeight
///
/// @brief The default size of the glyph cache height.
const int32_t kGlyphCacheHeight = 1024;

/// @var kGlyphCacheMaxSlices
///
/// @brief The default size of the max glyph cache slices. The number of cache
/// slices grows up to the value.
const int32_t kGlyphCacheMaxSlices = 4;

/// @var kLineHeightDefault
///
/// @brief Default value for a line height factor.
///
/// The line height is derived as the factor * a font height.
/// To change the line height, use `SetLineHeightScale()` API.
const float kLineHeightDefault = 1.2f;

/// @var kKerningScaleDefault
///
/// @brief Default value for a kerning scale factor.
///
/// The kerning value is derived as the factor * kerning value retrieved from
/// harfbuzz. To change the kerning scale, use `SetKerningScale()` API.
const float kKerningScaleDefault = 1.0f;

/// @var kCaretPositionInvalid
///
/// @brief A sentinel value representing an invalid caret position.
const mathfu::vec2i kCaretPositionInvalid = mathfu::vec2i(-1, -1);

/// @var kDefaultLanguage
///
/// @brief The default language used for a line break.
const char *const kDefaultLanguage = "en";

#ifdef FLATUI_SYSTEM_FONT
/// @var kSystemFont
///
/// @brief A constant to spefify loading a system font. Used with OpenFont() and
/// SelectFont() API
/// Currently the system font is supported on iOS/macOS and Android only.
const char *const kSystemFont = ".SystemFont";
#endif  // FLATUI_SYSTEM_FONT

/// @enum TextLayoutDirection
///
/// @brief Specify how to layout texts.
/// Default value is TextLayoutDirectionLTR.
///
enum TextLayoutDirection {
  kTextLayoutDirectionLTR = 0,
  kTextLayoutDirectionRTL = 1,
  kTextLayoutDirectionTTB = 2,
};

/// @enum TextAlignment
///
/// @brief Alignment of the text.
///
/// @note: Used for a typographic alignment in a label.
/// The enumeration is different from flatui::Alignment as it supports
/// justification setting.
/// Note that in RTL layout direction mode, the setting is flipped.
/// (e.g. kTextAlignmentLeft becomes kTextAlignmentRight)
/// kTextAlignLeft/Right/CenterJustify variants specify how the last line is
/// flushed left, right or centered.
///
/// **Enumerations**:
/// * `kTextAlignmentLeft` - Text is aligned to the left of the given area.
/// Default setting.
/// * `kTextAlignmentRight` - Text is aligned to the right of the given area.
/// * `kTextAlignmentCenter` - Text is aligned to the center of the given
/// area.
/// * `kTextAlignmentJustify` - Text is 'justified'. Spaces between words are
/// stretched to align both the left and right ends of each line of text.
/// * `kTextAlignmentLeftJustify` - An alias of kTextAlignmentJustify. The last
/// line of a paragraph is aligned to the left.
/// * `kTextAlignmentRightJustify` - Text is 'justified'. The last line of a
/// paragraph is aligned to the right.
/// * `kTextAlignmentCenterJustify` - Text is 'justified'. The last line of a
/// paragraph is aligned to the center.
///
/// For more detail of each settings, refer:
/// https://en.wikipedia.org/wiki/Typographic_alignment
enum TextAlignment {
  kTextAlignmentLeft = 0,
  kTextAlignmentRight = 1,
  kTextAlignmentCenter = 2,
  kTextAlignmentJustify = 4,
  kTextAlignmentLeftJustify = kTextAlignmentJustify,
  kTextAlignmentRightJustify = kTextAlignmentJustify | kTextAlignmentRight,
  kTextAlignmentCenterJustify = kTextAlignmentJustify | kTextAlignmentCenter,
};

/// @struct FontBufferParameters
///
/// @brief A class holding font family information. The class provides various
/// ways to support fonts such as a font collection (multiple fonts in a file),
/// referencing a font by a famly name etc.
const int32_t kFontIndexInvalid = -1;
class FontFamily {
 public:
  // Constructors
  FontFamily(const std::string &name, int32_t index, const std::string &lang,
             bool family_name)
      : index_(index), family_name_(family_name) {
    font_name_ = name;
    lang_ = lang;
    if (is_font_collection()) {
      original_name_ = name;
      font_name_ = CreateFontCollectionName(name);
    }
  };
  FontFamily(const char *name, bool family_name)
      : index_(kFontIndexInvalid), family_name_(family_name) {
    font_name_ = name;
  }
  FontFamily(const char *name)
      : index_(kFontIndexInvalid), family_name_(false) {
    font_name_ = name;
  };

  // Accessors to members.
  /// Font name. When the font is in a font collection file, the name is mangled
  /// with a font collection index.
  const std::string &get_name() const { return font_name_; }
  /// Original font name. When family_name_ is set, it's treated as a family
  /// name.
  const std::string &get_original_name() const {
    return original_name_.length() ? original_name_ : font_name_;
  }
  /// Language. The entry is ignored when opening a font.
  const std::string &get_language() const { return lang_; }
  /// Index in a font collection. kFontIndexInvalid indicates the font is not a
  /// font collection.
  int32_t get_index() const { return index_; }
  /// Check if the font name is a family name.
  bool is_family_name() const { return family_name_; }
  /// Check if the font is in a font collection that holds multiple fonts in a
  /// single file.
  bool is_font_collection() const { return index_ != kFontIndexInvalid; }

 protected:
  std::string CreateFontCollectionName(const std::string &name) {
    // Tweak the file name with a font collection index and return it.
    std::stringstream ss;
    ss << "#" << index_;
    return name + ss.str();
  }
  std::string original_name_;
  std::string font_name_;
  std::string lang_;
  int32_t index_;
  bool family_name_;
};

/// @class FontBufferParameters
///
/// @brief This class that includes font buffer parameters. It is used as a key
/// in the unordered_map to look up FontBuffer.
class FontBufferParameters {
 public:
  /// @brief The default constructor for an empty FontBufferParameters.
  FontBufferParameters()
      : font_id_(kNullHash),
        text_id_(kNullHash),
        cache_id_(kNullHash),
        font_size_(0),
        size_(mathfu::kZeros2i),
        kerning_scale_(kKerningScaleDefault),
        line_height_scale_(kLineHeightDefault),
        flags_value_(0) {}

  /// @brief Constructor for a FontBufferParameters.
  ///
  /// @param[in] font_id The HashedId for the font.
  /// @param[in] text_id The HashedID for the text.
  /// @param[in] font_size A float representing the size of the font.
  /// @param[in] size The requested size of the FontBuffer in pixels.
  /// The created FontBuffer size can be smaller than requested size as the
  /// FontBuffer size is calcuated by the layout and rendering result.
  /// @param[in] text_alignment A horizontal alignment of multi line
  /// buffer.
  /// @param[in] kerning_scale Value indicating kerning scale.
  /// @param[in] line_height_scale Value indicating line height scale.
  /// @param[in] glyph_flags A flag determing SDF generation for the font.
  /// @param[in] caret_info A bool determining if the FontBuffer contains caret
  /// info.
  /// @param[in] ref_count A bool determining if the FontBuffer manages
  /// reference counts of the buffer and referencing glyph cache rows.
  /// @param[in] kerning_scale A float value specifying a scale applied to the
  /// kerning value between glyphs. Default value is kKerningScaleDefault(1.0).
  /// @param[in] line_height_scale A float value specifying a scale applied to
  /// the line height for each line break. Default value is
  /// kLineHeightDefault(1.0).
  /// @param[in] cache_id An extra ID to distinguish cache entry. For instance,
  /// tweak the ID to have multiple FontBuffer instances with the same set of
  /// parameters.
  FontBufferParameters(HashedId font_id, HashedId text_id, float font_size,
                       const mathfu::vec2i &size, TextAlignment text_alignment,
                       GlyphFlags glyph_flags, bool caret_info, bool ref_count,
                       float kerning_scale = kKerningScaleDefault,
                       float line_height_scale = kLineHeightDefault,
                       HashedId cache_id = kNullHash) {
    font_id_ = font_id;
    text_id_ = text_id;
    cache_id_ = cache_id;
    font_size_ = font_size;
    kerning_scale_ = kerning_scale;
    line_height_scale_ = line_height_scale;
    size_ = size;
    flags_value_ = 0;
    flags_.text_alignement = text_alignment;
    flags_.glyph_flags = glyph_flags;
    flags_.caret_info = caret_info;
    flags_.ref_count = ref_count;
  }

  /// @brief The equal-to operator for comparing FontBufferParameters for
  /// equality.
  ///
  /// @param[in] other The other FontBufferParameters to check against for
  /// equality.
  ///
  /// @return Returns `true` if the two FontBufferParameters are equal.
  /// Otherwise it returns `false`.
  bool operator==(const FontBufferParameters &other) const {
    return (font_id_ == other.font_id_ && text_id_ == other.text_id_ &&
            font_size_ == other.font_size_ && size_.x() == other.size_.x() &&
            size_.y() == other.size_.y() &&
            kerning_scale_ == other.kerning_scale_ &&
            line_height_scale_ == other.line_height_scale_ &&
            flags_value_ == other.flags_value_ && cache_id_ == other.cache_id_);
  }

  /// @brief The hash function for FontBufferParameters.
  ///
  /// @param[in] key A FontBufferParameters to use as the key for
  /// hashing.
  ///
  /// @return Returns a `size_t` of the hash of the FontBufferParameters.
  size_t operator()(const FontBufferParameters &key) const {
    // Note that font_id_ and text_id_ are already hashed values.
    size_t value = (font_id_ ^ (text_id_ << 1)) >> 1;
    value = value ^ (std::hash<float>()(key.font_size_) << 1) >> 1;
    value = value ^ (std::hash<float>()(key.kerning_scale_) << 1) >> 1;
    value = value ^ (std::hash<float>()(key.line_height_scale_) << 1) >> 1;
    value = value ^ (std::hash<int32_t>()(key.flags_value_) << 1) >> 1;
    value = value ^ (std::hash<int32_t>()(key.size_.x()) << 1) >> 1;
    value = value ^ (std::hash<int32_t>()(key.size_.y()) << 1) >> 1;
    value = value ^ (std::hash<int32_t>()(cache_id_) << 1) >> 1;
    return value;
  }

  /// @return Returns a hash value of the text.
  HashedId get_text_id() const { return text_id_; }

  /// @return Returns a cache id value.
  HashedId get_cache_id() const { return cache_id_; }

  /// @return Returns the size value.
  const mathfu::vec2i &get_size() const { return size_; }

  /// @return Returns the font size.
  float get_font_size() const { return font_size_; }

  /// @return Returns a text alignment info.
  TextAlignment get_text_alignment() const { return flags_.text_alignement; }

  /// @return Returns the kerning scale.
  float get_kerning_scale() const { return kerning_scale_; }

  /// @return Returns the line height scale.
  float get_line_height_scale() const { return line_height_scale_; }

  /// @return Returns a glyph setting info.
  GlyphFlags get_glyph_flags() const { return flags_.glyph_flags; }

  /// @return Returns a flag indicating if the buffer has caret info.
  bool get_caret_info_flag() const { return flags_.caret_info; }

  /// @return Returns a flag indicating if the buffer manages reference counts.
  bool get_ref_count_flag() const { return flags_.ref_count; }

  /// Retrieve a line length of the text based on given parameters.
  /// a fixed line length (get_size.x()) will be used if the text is justified
  /// or right aligned otherwise the line length will be determined by the text
  /// layout phase.
  /// @return Returns the expected line width.
  int32_t get_line_length() const {
    auto alignment = get_text_alignment();
    if (alignment == kTextAlignmentLeft || alignment == kTextAlignmentCenter) {
      return 0;
    } else {
      // Other settings will use max width of the given area.
      return size_.x() * kFreeTypeUnit;
    }
  }

  /// @return Returns the multi line setting.
  bool get_multi_line_setting() const {
    // size.x() == 0 indicates a single line mode.
    if (!size_.x()) {
      return false;
    }
    if (get_text_alignment() != kTextAlignmentLeft) {
      return true;
    } else {
      return size_.y() == 0 || size_.y() > font_size_;
    }
  }

 private:
  HashedId font_id_;
  HashedId text_id_;
  // An extra ID to allow multiple FontBuffer entries in the cache.
  HashedId cache_id_;
  float font_size_;
  mathfu::vec2i size_;
  float kerning_scale_;
  float line_height_scale_;

  // A structure that defines bit fields to hold multiple flag values related to
  // the font buffer.
  struct FontBufferFlags {
    bool ref_count : 1;
    bool caret_info : 1;
    GlyphFlags glyph_flags : 2;
    TextAlignment text_alignement : 3;
  };
  union {
    uint32_t flags_value_;
    FontBufferFlags flags_;
  };
};

/// @class FontManager
///
/// @brief FontManager manages font rendering with OpenGL utilizing freetype
/// and harfbuzz as a glyph rendering and layout back end.
///
/// It opens speficied OpenType/TrueType font and rasterize to OpenGL texture.
/// An application can use the generated texture for a text rendering.
///
/// @warning The class is not threadsafe, it's expected to be only used from
/// within OpenGL rendering thread.
class FontManager {
 public:
  /// @brief The default constructor for FontManager.
  FontManager();

  /// @brief Constructor for FontManager with a given cache size.
  ///
  /// @note The given size is rounded up to nearest power of 2 internally to be
  /// used as an OpenGL texture sizes.
  ///
  /// @param[in] cache_size The size of the cache, in pixels as x & y values.
  /// @param[in] max_slices The maximum number of cache slices. When a glyph
  /// slice gets full with glyph entries, the cache allocates another slices for
  /// more cache spaces up to the value.
  FontManager(const mathfu::vec2i &cache_size, int32_t max_slices);

  /// @brief The destructor for FontManager.
  ~FontManager();

  /// @brief Open a font face, TTF, OT font.
  ///
  /// In this version it supports only single face at a time.
  ///
  /// @param[in] font_name A C-string in UTF-8 format representing
  /// the name of the font.
  /// @return Returns `false` when failing to open font, such as
  /// a file open error, an invalid file format etc. Returns `true`
  /// if the font is opened successfully.
  bool Open(const char *font_name);

  /// @brief Open a font face, TTF, OT font by FontFamily structure.
  /// Use this version of API when opening a font with a family name, with a
  /// font collection index etc.
  ///
  /// @param[in] family A FontFamily structure indicating font parameters.
  /// @return Returns `false` when failing to open font, such as
  /// a file open error, an invalid file format etc. Returns `true`
  /// if the font is opened successfully.
  bool Open(const FontFamily &family);

  /// @brief Discard a font face that has been opened via `Open()`.
  ///
  /// @param[in] font_name A C-string in UTF-8 format representing
  /// the name of the font.
  ///
  /// @return Returns `true` if the font was closed successfully. Otherwise
  /// it returns `false`.
  bool Close(const char *font_name);

  /// @brief Discard a font face that has been opened via `Open()`.
  /// Use this version of API when closeing a font opened with FontFamily.
  ///
  /// @param[in] family A FontFamily structure indicating font parameters.
  ///
  /// @return Returns `true` if the font was closed successfully. Otherwise
  /// it returns `false`.
  bool Close(const FontFamily &family);

  /// @brief Select the current font face. The font face will be used by a glyph
  /// rendering.
  ///
  /// @note The font face needs to have been opened by `Open()`.
  ///
  /// @param[in] font_name A C-string in UTF-8 format representing
  /// the name of the font.
  ///
  /// @return Returns `true` if the font was selected successfully. Otherwise it
  /// returns false.
  bool SelectFont(const char *font_name);

  /// @brief Select the current font faces with a fallback priority.
  ///
  /// @param[in] font_names An array of C-string corresponding to the name of
  /// the font. Font names in the array are stored in a priority order.
  /// @param[in] count A count of font names in the array.
  ///
  /// @return Returns `true` if the fonts were selected successfully. Otherwise
  /// it returns false.
  bool SelectFont(const char *font_names[], int32_t count);

  /// @brief Select the current font faces with a fallback priority.
  ///
  /// @param[in] family_names An array of FontFamily structure that holds font
  /// family information.
  /// @param[in] count A count of font family in the array.
  ///
  /// @return Returns `true` if the fonts were selected successfully. Otherwise
  /// it returns false.
  bool SelectFont(const FontFamily family_names[], int32_t count);

  /// @brief Retrieve a texture with the given text.
  ///
  /// @note This API doesn't use the glyph cache, instead it writes the string
  /// image directly to the returned texture. The user can use this API when a
  /// font texture is used for a long time, such as a string image used in game
  /// HUD.
  ///
  /// @param[in] text A C-string in UTF-8 format with the text for the texture.
  /// @param[in] length The length of the text string.
  /// @param[in] ysize The height of the texture.
  ///
  /// @return Returns a pointer to the FontTexture.
  FontTexture *GetTexture(const char *text, uint32_t length, float ysize);

  /// @brief Retrieve a vertex buffer for a font rendering using glyph cache.
  ///
  /// @param[in] text A C-string in UTF-8 format with the text for the
  /// FontBuffer.
  /// @param[in] length The length of the text string.
  /// @param[in] parameters The FontBufferParameters specifying the parameters
  /// for the FontBuffer.
  ///
  /// @return Returns `nullptr` if the string does not fit in the glyph cache.
  ///  When this happens, caller may flush the glyph cache with
  /// `FlushAndUpdate()` call and re-try the `GetBuffer()` call.
  FontBuffer *GetBuffer(const char *text, size_t length,
                        const FontBufferParameters &parameters);

  /// @brief Release the FonBuffer instance.
  /// If the FontBuffer is a reference counting buffer, the API decrements the
  /// reference count of the buffer and when it reaches 0, the buffer becomes
  /// invalid.
  /// If the buffer is non-reference counting buffer, the buffer is invalidated
  /// immediately and all references to the FontBuffer with same parameters
  /// become invalid. It is not a problem for those non-reference counting
  /// buffers because they are expected to create/fetch a buffer each render
  /// cycle.
  void ReleaseBuffer(FontBuffer *buffer);

  /// @brief Set the renderer to be used to create texture instances.
  ///
  /// @param[in] renderer The Renderer to set for creating textures.
  void SetRenderer(fplbase::Renderer &renderer);

  /// @return Returns `true` if a font has been loaded. Otherwise it returns
  /// `false`.
  bool FontLoaded() { return face_initialized_; }

  /// @brief Indicate a start of new layout pass.
  ///
  /// Call the API each time the user starts a layout pass.
  ///
  /// @note The user of the font_manager class is expected to use the class in
  /// 2 passes: a layout pass and a render pass.
  ///
  /// During the layout pass, the user invokes `GetBuffer()` calls to fetch all
  /// required glyphs into the cache. Once in the cache, the user can retrieve
  /// the size of the strings to position them.
  ///
  /// Once the layout pass finishes, the user invokes `StartRenderPass()` to
  /// upload cache images to the atlas texture, and use the texture in the
  /// render pass. This design helps to minimize the frequency of the atlas
  /// texture upload.
  void StartLayoutPass();

  /// @brief Flush the existing glyph cache contents and start new layout pass.
  ///
  /// Call this API while in a layout pass when the glyph cache is fragmented.
  void FlushAndUpdate() { UpdatePass(true); }

  /// @brief Flush the existing FontBuffer in the cache.
  ///
  /// Call this API when FontBuffers are not used anymore.
  void FlushLayout() { map_buffers_.clear(); }

  /// @brief Indicates a start of new render pass.
  ///
  /// Call the API each time the user starts a render pass.
  void StartRenderPass() { UpdatePass(false); }

  /// @return Returns font atlas texture.
  ///
  /// @param[in] slice an index indicating an atlas texture.
  /// An index with kGlyphFormatsColor indicates that the slice is a multi
  /// channel buffer for color glyphs.
  fplbase::Texture *GetAtlasTexture(int32_t slice) {
    fplutil::MutexLock lock(*cache_mutex_);
    if (!(slice & kGlyphFormatsColor)) {
      return glyph_cache_->get_monochrome_buffer()->get_texture(slice);
    } else {
      return glyph_cache_->get_color_buffer()->get_texture(slice &
                                                           ~kGlyphFormatsColor);
    }
  }

  /// @brief The user can supply a size selector function to adjust glyph sizes
  /// when storing a glyph cache entry. By doing that, multiple strings with
  /// slightly different sizes can share the same glyph cache entry, so that the
  /// user can reduce a total # of cached entries.
  ///
  /// @param[in] selector The callback argument is a request for the glyph
  /// size in pixels. The callback should return the desired glyph size that
  /// should be used for the glyph sizes.
  void SetSizeSelector(std::function<int32_t(const int32_t)> selector) {
    size_selector_.swap(selector);
  }

  /// @param[in] locale  A C-string corresponding to the of the
  /// language defined in ISO 639 and the country code difined in ISO 3166
  /// separated
  /// by '-'. (e.g. 'en-US').
  ///
  /// @note The API sets language, script and layout direction used for
  /// following text renderings based on the locale.
  void SetLocale(const char *locale);

  /// @return Returns the current language setting as a std::string.
  const char *GetLanguage() { return language_.c_str(); }

  /// @brief Set a script used for a script layout.
  ///
  /// @param[in] language ISO 15924 based script code. Default setting is
  /// 'Latn' (Latin).
  /// For more detail, refer http://unicode.org/iso15924/iso15924-codes.html
  ///
  void SetScript(const char *script);

  /// @brief Set a script layout direction.
  ///
  /// @param[in] direction Text layout direction.
  ///            TextLayoutDirectionLTR & TextLayoutDirectionRTL are supported.
  ///
  void SetLayoutDirection(const TextLayoutDirection direction) {
    if (direction == kTextLayoutDirectionTTB) {
      fplbase::LogError("TextLayoutDirectionTTB is not supported yet.");
      return;
    }

    // Flush layout cache if we switch a direction.
    if (direction != layout_direction_) {
      FlushLayout();
    }
    layout_direction_ = direction;
  }

  /// @return Returns the current layout direciton.
  TextLayoutDirection GetLayoutDirection() { return layout_direction_; }

  /// @return Returns the current font.
  HbFont *GetCurrentFont() { return current_font_; }

  /// @brief Enable an use of color glyphs in supporting OTF/TTF font.
  /// The API will initialize internal glyph cache for color glyphs.
  void EnableColorGlyph();

  /// @brief Set an ellipsis string used in label/edit widgets.
  ///
  /// @param[in] ellipsis A C-string specifying characters used as an ellipsis.
  /// Can be multiple characters, typically '...'. When a string in a widget
  /// doesn't fit to the given size, the string is truncated to fit the ellipsis
  /// string appended at the end.
  /// Note: FontManager doesn't support dynamic change of the ellipsis string
  /// in current version. FontBuffer contents that has been created are not
  /// updated when the ellipsis string is changed.
  void SetTextEllipsis(const char *ellipsis) { ellipsis_ = ellipsis; }

 private:
  // Pass indicating rendering pass.
  static const int32_t kRenderPass = -1;

  // Initialize static data associated with the class.
  void Initialize();

  // Clean up static data associated with the class.
  // Note that after the call, an application needs to call Initialize() again
  // to use the class.
  static void Terminate();

  // Expand a texture image buffer when the font metrics is changed.
  // Returns true if the image buffer was reallocated.
  static bool ExpandBuffer(int32_t width, int32_t height,
                           const FontMetrics &original_metrics,
                           const FontMetrics &new_metrics,
                           std::unique_ptr<uint8_t[]> *image);

  // Layout text and update harfbuzz_buf_.
  // Returns the width of the text layout in pixels.
  uint32_t LayoutText(const char *text, size_t length, uint32_t max_width = 0,
                      uint32_t current_width = 0, int32_t *rewind = nullptr);

  // Helper function to add string information to the buffer.
  bool UpdateBuffer(const WordEnumerator &word_enum,
                    const FontBufferParameters &parameters, int32_t base_line,
                    bool lastline_must_break, FontBuffer *buffer,
                    std::vector<int32_t> *atlas_indices, mathfu::vec2 *pos,
                    FontMetrics *metrics);

  // Helper function to remove entries from the buffer for specified width.
  void RemoveEntries(const FontBufferParameters &parameters,
                     uint32_t required_width, FontBuffer *buffer,
                     std::vector<int32_t> *atlas_indices, mathfu::vec2 *pos);

  // Helper function to append ellipsis to the buffer.
  bool AppendEllipsis(const WordEnumerator &word_enum,
                      const FontBufferParameters &parameters, int32_t base_line,
                      FontBuffer *buffer, mathfu::vec2 *pos,
                      std::vector<int32_t> *atlas_indices,
                      FontMetrics *metrics);

  // Calculate internal/external leading value and expand a buffer if
  // necessary.
  // Returns true if the size of metrics has been changed.
  bool UpdateMetrics(int32_t top, int32_t height,
                     const FontMetrics &current_metrics,
                     FontMetrics *new_metrics);

  // Retrieve cached entry from the glyph cache.
  // If an entry is not found in the glyph cache, the API tries to create new
  // cache entry and returns it if succeeded.
  // Returns nullptr if,
  // - The font doesn't have the requested glyph.
  // - The glyph doesn't fit into the cache (even after trying to evict some
  // glyphs in cache based on LRU rule).
  // (e.g. Requested glyph size too large or the cache is highly fragmented.)
  const GlyphCacheEntry *GetCachedEntry(uint32_t code_point, uint32_t y_size,
                                        GlyphFlags flags);

  // Update font manager, check glyph cache if the texture atlas needs to be
  // updated.
  // If start_subpass == true,
  // the API uploads the current atlas texture, flushes cache and starts
  // a sub layout pass. Use the feature when the cache is full and needs to
  // flushed during a rendering pass.
  void UpdatePass(bool start_subpass);

  // Update UV value in the FontBuffer.
  // Returns nullptr if one of UV values couldn't be updated.
  FontBuffer *UpdateUV(int32_t ysize, GlyphFlags flags, FontBuffer *buffer);

  // Convert requested glyph size using SizeSelector if it's set.
  int32_t ConvertSize(int32_t size);

  // Retrieve a caret count in a specific glyph from linebreak and halfbuzz
  // glyph information.
  int32_t GetCaretPosCount(const WordEnumerator &enumerator,
                           const hb_glyph_info_t *info, int32_t glyph_count,
                           int32_t index);

  // Create FontBuffer with requested parameters.
  // The function may return nullptr if the glyph cache is full.
  FontBuffer *CreateBuffer(const char *text, uint32_t length,
                           const FontBufferParameters &parameters);

  // Update language related settings.
  void SetLanguageSettings();

  // Look up a supported locale.
  // Returns nullptr if the API doesn't find the specified locale.
  const ScriptInfo *FindLocale(const char *locale);

  // Check if speficied language is supported in the font manager engine.
  bool IsLanguageSupported(const char *language);

  /// @brief Set a line height for a multi-line text.
  ///
  /// @param[in] line_height A float representing the line height for a
  /// multi-line text.
  void SetLineHeightScale(float line_height_scale) {
    line_height_scale_ = line_height_scale;
  }

  /// @brief Set a kerning scale value.
  ///
  /// @param[in] kerning_scale A float representing the kerning scale value
  /// applied to the kerning values retrieved from Harfbuzz used in the text
  /// rendering.
  void SetKerningScale(float kerning_scale) { kerning_scale_ = kerning_scale; }

  /// @brief Open specified font by name and return the raw data.
  /// Current implementation works on macOS/iOS and Android.
  /// @return Returns true if the font is opened successfully.
  ///
  /// @param[in] font_name A font name to load.
  /// @param[out] dest A string that font data will be loaded into.
  /// @return true if the specified font is successfully opened.
  bool OpenFontByName(const char *font_name, std::string *dest);

  /// @brief  Retrieve the system's font fallback list and all fonts in the
  /// list.
  /// The implementation is platform specific.
  /// @return true if the system font is successfully opened.
  bool OpenSystemFont();

  /// @brief  Helper function to check font coverage.
  /// @param[in] face FreeType face to check font coverage.
  /// @param[out] font_coverage A set updated for the coverage map of the face.
  /// @return true if the specified font has a new glyph entry.
  bool UpdateFontCoverage(FT_Face face, std::set<FT_ULong> *font_coverage);

/// @brief Platform specific implementation of a system font access.
#ifdef __APPLE__
  bool OpenSystemFontApple();
  bool CloseSystemFontApple();
#endif  // __APPLE__
#ifdef __ANDROID__
  bool OpenSystemFontAndroid();
  bool CloseSystemFontAndroid();
  void ReorderSystemFonts(std::vector<FontFamily> *font_list) const;
#endif  // __ANDROID__

  /// @brief  Close all fonts in the system's font fallback list opened by
  /// OpenSystemFont().
  bool CloseSystemFont();

  // Renderer instance.
  fplbase::Renderer *renderer_;

  // flag indicating if a font file has loaded.
  bool face_initialized_;

  // Map that keeps opened face data instances.
  std::unordered_map<std::string, std::unique_ptr<FaceData>> map_faces_;

  // Pointer to active font face instance.
  HbFont *current_font_;

  // Texture cache for a rendered string image.
  // Using the FontBufferParameters as keys.
  // The map is used for GetTexture() API.
  std::unordered_map<FontBufferParameters, std::unique_ptr<FontTexture>,
                     FontBufferParameters> map_textures_;

  // Cache for a texture atlas + vertex array rendering.
  // Using the FontBufferParameters as keys.
  // The map is used for GetBuffer() API.
  std::unordered_map<FontBufferParameters, std::unique_ptr<FontBuffer>,
                     FontBufferParameters> map_buffers_;

  // Singleton instance of Freetype library.
  static FT_Library *ft_;

  // Harfbuzz buffer
  static hb_buffer_t *harfbuzz_buf_;

  // Unique pointer to a glyph cache.
  std::unique_ptr<GlyphCache> glyph_cache_;

  // Current atlas texture's contents revision.
  uint32_t current_atlas_revision_;

  // Current pass counter.
  // Current implementation only supports up to 2 passes in a rendering cycle.
  int32_t current_pass_;

  // Size selector function object used to adjust a glyph size.
  std::function<int32_t(const int32_t)> size_selector_;

  // Language of input strings.
  // Used to determine line breaking depending on a language.
  uint32_t script_;
  std::string language_;
  std::string locale_;
  TextLayoutDirection layout_direction_;
  static const ScriptInfo script_table_[];
  static const char *language_table_[];
  hb_language_t hb_language_;

  // Line height for a multi line text.
  float line_height_scale_;
  float kerning_scale_;

  // Ellipsis settings.
  std::string ellipsis_;

  // Line break info buffer used in libunibreak.
  std::vector<char> wordbreak_info_;

  // An instance of signed distance field generator.
  // To avoid redundant initializations, the FontManager holds an instnce of the
  // class.
  DistanceComputer<uint8_t> sdf_computer_;

  // Mutex guarding glyph cache's buffer access.
  fplutil::Mutex *cache_mutex_;

  // A font fallback list retrieved from the current system.
  std::vector<FontFamily> system_fallback_list_;
};

/// @class FontMetrics
///
/// @brief This class has additional properties for font metrics.
//
/// For details of font metrics, refer http://support.microsoft.com/kb/32667
/// In this class, ascender and descender values are retrieved from FreeType
/// font property.
///
/// And internal/external leading values are updated based on rendering glyph
/// information. When rendering a string, the leading values tracks max (min for
/// internal leading) value in the string.
class FontMetrics {
 public:
  /// The default constructor for FontMetrics.
  FontMetrics()
      : base_line_(0),
        internal_leading_(0),
        ascender_(0),
        descender_(0),
        external_leading_(0) {}

  /// The constructor with initialization parameters for FontMetrics.
  FontMetrics(int32_t base_line, int32_t internal_leading, int32_t ascender,
              int32_t descender, int32_t external_leading)
      : base_line_(base_line),
        internal_leading_(internal_leading),
        ascender_(ascender),
        descender_(descender),
        external_leading_(external_leading) {
    assert(internal_leading >= 0);
    assert(ascender >= 0);
    assert(descender <= 0);
    assert(external_leading <= 0);
  }

  /// The destructor for FontMetrics.
  ~FontMetrics() {}

  /// @return Returns the baseline value as an int32_t.
  int32_t base_line() const { return base_line_; }

  /// @brief set the baseline value.
  ///
  /// @param[in] base_line An int32_t to set as the baseline value.
  void set_base_line(int32_t base_line) { base_line_ = base_line; }

  /// @return Returns the internal leading parameter as an int32_t.
  int32_t internal_leading() const { return internal_leading_; }

  /// @brief Set the internal leading parameter value.
  ///
  /// @param[in] internal_leading An int32_t to set as the internal
  /// leading value.
  void set_internal_leading(int32_t internal_leading) {
    assert(internal_leading >= 0);
    internal_leading_ = internal_leading;
  }

  /// @return Returns the ascender value as an int32_t.
  int32_t ascender() const { return ascender_; }

  /// @brief Set the ascender value.
  ///
  /// @param[in] ascender An int32_t to set as the ascender value.
  void set_ascender(int32_t ascender) {
    assert(ascender >= 0);
    ascender_ = ascender;
  }

  /// @return Returns the descender value as an int32_t.
  int32_t descender() const { return descender_; }

  /// @brief Set the descender value.
  ///
  /// @param[in] descender An int32_t to set as the descender value.
  void set_descender(int32_t descender) {
    assert(descender <= 0);
    descender_ = descender;
  }

  /// @return Returns the external leading parameter value as an int32_t.
  int32_t external_leading() const { return external_leading_; }

  /// @brief Set the external leading parameter value.
  ///
  /// @param[in] external_leading An int32_t to set as the external
  /// leading value.
  void set_external_leading(int32_t external_leading) {
    assert(external_leading <= 0);
    external_leading_ = external_leading;
  }

  /// @return Returns the total height of the glyph.
  int32_t total() const {
    return internal_leading_ + ascender_ - descender_ - external_leading_;
  }

 private:
  // Baseline: Baseline of the glpyhs.
  // When rendering multiple glyphs in the same horizontal group,
  // baselines need be aligned.
  // Most of glyphs fit within the area of ascender + descender.
  // However some glyphs may render in internal/external leading area.
  // (e.g. Å, underlines)
  int32_t base_line_;

  // Internal leading: Positive value that describes the space above the
  // ascender.
  int32_t internal_leading_;

  // Ascender: Positive value that describes the size of the ascender above
  // the baseline.
  int32_t ascender_;

  // Descender: Negative value that describes the size of the descender below
  // the baseline.
  int32_t descender_;

  // External leading : Negative value that describes the space below
  // the descender.
  int32_t external_leading_;
};

/// @class FontTexture
///
/// @brief This class is the actual Texture for fonts.
class FontTexture : public fplbase::Texture {
 public:
  /// @brief The default constructor for a FontTexture.
  FontTexture()
      : fplbase::Texture(nullptr, fplbase::kFormatLuminance,
                         fplbase::kTextureFlagsNone) {}

  /// @brief The destructor for FontTexture.
  ~FontTexture() {}

  /// @return Returns a const reference to the FontMetrics that specifies
  /// the metrics parameters for the FontTexture.
  const FontMetrics &metrics() const { return metrics_; }

  /// @brief Sets the FontMetrics that specifies the metrics parameters for
  /// the FontTexture.
  void set_metrics(const FontMetrics &metrics) { metrics_ = metrics; }

 private:
  FontMetrics metrics_;
};

/// @struct FontVertex
///
/// @brief This struct holds all the font vertex data.
struct FontVertex {
  /// @brief The constructor for a FontVertex.
  ///
  /// @param[in] x A float representing the `x` position of the vertex.
  /// @param[in] y A float representing the `y` position of the vertex.
  /// @param[in] z A float representing the `z` position of the vertex.
  /// @param[in] u A float representing the `u` value in the UV mapping.
  /// @param[in] v A float representing the `v` value in the UV mapping.
  FontVertex(float x, float y, float z, float u, float v) {
    position_.data[0] = x;
    position_.data[1] = y;
    position_.data[2] = z;
    uv_.data[0] = u;
    uv_.data[1] = v;
  }

  /// @cond FONT_MANAGER_INTERNAL
  mathfu::vec3_packed position_;
  mathfu::vec2_packed uv_;
  /// @endcond
};

/// @class FontBuffer
///
/// @brief this is used with the texture atlas rendering.
class FontBuffer {
 public:
  /// @var kIndiciesPerCodePoint
  ///
  /// @brief The number of indices per code point.
  static const int32_t kIndiciesPerCodePoint = 6;

  /// @var kVerticesPerCodePoint
  ///
  /// @brief The number of vertices per code point.
  static const int32_t kVerticesPerCodePoint = 4;

  /// @var Type defining an interator to the internal map that is tracking all
  /// FontBuffer instances.
  typedef std::unordered_map<FontBufferParameters, std::unique_ptr<FontBuffer>,
                             FontBufferParameters>::iterator fontbuffer_map_it;

  /// @brief The default constructor for a FontBuffer.
  FontBuffer()
      : revision_(0),
        line_start_index_(0),
        line_start_caret_index_(0),
        ref_count_(0),
        valid_(true) {}

  /// @brief The constructor for FontBuffer with a given buffer size.
  ///
  /// @param[in] size A size of the FontBuffer in a number of glyphs.
  /// @param[in] caret_info Indicates if the FontBuffer also maintains a caret
  /// position buffer.
  ///
  /// @note Caret position does not match to glpyh position 1 to 1, because a
  /// glyph can have multiple caret positions (e.g. Single 'ff' glyph can have 2
  /// caret positions).
  ///
  /// Since it has a strong relationship to rendering positions, we store the
  /// caret position information in the FontBuffer.
  FontBuffer(uint32_t size, bool caret_info)
      : revision_(0),
        line_start_index_(0),
        line_start_caret_index_(0),
        ref_count_(0),
        valid_(true) {
    code_points_.reserve(size);
    if (caret_info) {
      caret_positions_.reserve(size + 1);
    }
  }

  /// The destructor for FontBuffer.
  ~FontBuffer() {}

  /// @return Returns the FontMetrics metrics parameters for the font
  /// texture.
  const FontMetrics &metrics() const { return metrics_; }

  /// @return Returns the slices array as a const std::vector<int32_t>.
  const std::vector<int32_t> *get_slices() const { return &slices_; }

  /// @return Returns the indices array as a const std::vector<uint16_t>.
  const std::vector<uint16_t> *get_indices(int32_t index) const {
    return &indices_[index];
  }

  /// @return Returns the vertices array as a const std::vector<FontVertex>.
  const std::vector<FontVertex> *get_vertices() const { return &vertices_; }

  /// @return Returns the non const version of vertices array as a
  /// std::vector<FontVertex>.
  /// Note that the updating the number of elements in the buffer would result
  /// undefined behavior.
  std::vector<FontVertex> *get_vertices() { return &vertices_; }

  /// @return Returns the array of code points as a const std::vector<uint32_t>.
  const std::vector<uint32_t> *get_code_points() const { return &code_points_; }

  /// @return Returns the size of the string as a const vec2i reference.
  const mathfu::vec2i &get_size() const { return size_; }

  /// @return Returns the glyph cache revision counter as a uint32_t.
  ///
  /// @note Each time a contents of the glyph cache is updated, the revision of
  /// the cache is updated.
  ///
  /// If the cache revision and the buffer revision is different, the
  /// font_manager try to re-construct the buffer.
  uint32_t get_revision() const { return revision_; }

  /// @return Returns the pass counter as an int32_t.
  ///
  /// @note In the render pass, this value is used if the user of the class
  /// needs to call `StartRenderPass()` to upload the atlas texture.
  int32_t get_pass() const { return pass_; }

  /// @brief Verifies that the sizes of the arrays used in the buffer are
  /// correct.
  ///
  /// @warning Asserts if the array sizes are incorrect.
  ///
  /// @return Returns `true`.
  bool Verify() const {
    assert(vertices_.size() == code_points_.size() * kVerticesPerCodePoint);
    size_t sum_indices = 0;
    for (size_t i = 0; i < indices_.size(); ++i) {
      sum_indices += indices_[i].size();
    }
    assert(sum_indices == code_points_.size() * kIndiciesPerCodePoint);
    return valid_;
  }

  /// @brief Retrieve a caret positions with a given index.
  ///
  /// @param[in] index The index of the caret position.
  ///
  /// @return Returns a vec2i containing the caret position. Otherwise it
  /// returns `kCaretPositionInvalid` if the buffer does not contain
  /// caret information at the given index, or if the index is out of range.
  mathfu::vec2i GetCaretPosition(size_t index) const {
    if (!caret_positions_.capacity() || index > caret_positions_.size())
      return kCaretPositionInvalid;
    return caret_positions_[index];
  }

  /// @return Returns a const reference to the caret positions buffer.
  const std::vector<mathfu::vec2i> &GetCaretPositions() const {
    return caret_positions_;
  }

  /// @return Returns `true` if the FontBuffer contains any caret positions.
  /// If the caret positions array has 0 elements, it will return `false`.
  bool HasCaretPositions() const { return caret_positions_.capacity() != 0; }

  /// @brief Return current glyph count stored in the buffer.
  int32_t get_glyph_count() const {
    return static_cast<int32_t>(vertices_.size()) / 4;
  }

  /// @brief Gets the reference count of the buffer.
  uint32_t get_ref_count() const { return ref_count_; }

 private:
  // Make the FontManager and related classes as a friend class so that
  // FontManager can manage FontBuffer class using it's private methods.
  friend FontManager;
  friend GlyphCacheRow;

  /// @return Returns the array of the slices that is used in the FontBuffer as
  /// a std::vector<int32_t>. When rendering the buffer, retrieve corresponding
  /// atlas texture from the glyph cache and bind the texture.
  std::vector<int32_t> *get_slices() { return &slices_; }

  /// @return Returns the indices array as a std::vector<uint16_t>.
  std::vector<uint16_t> *get_indices(int32_t index) { return &indices_[index]; }

  /// @brief Sets the FontMetrics metrics parameters for the font
  /// texture.
  ///
  /// @param[in] metrics The FontMetrics to set for the font texture.
  void set_metrics(const FontMetrics &metrics) { metrics_ = metrics; }

  /// @brief Set the size of the string.
  ///
  /// @param[in] size A const vec2i reference to the size of the string.
  void set_size(const mathfu::vec2i &size) { size_ = size; }

  /// @brief Sets the glyph cache revision counter.
  ///
  /// @param[in] revision The uint32_t containing the new counter to set.
  ///
  /// @note Each time a contents of the glyph cache is updated, the revision of
  /// the cache is updated.
  ///
  /// If the cache revision and the buffer revision is different, the
  /// font_manager try to re-construct the buffer.
  void set_revision(uint32_t revision) { revision_ = revision; }

  /// @return Returns the pass counter as an int32_t.
  ///
  /// @note In the render pass, this value is used if the user of the class
  /// needs to call `StartRenderPass()` to upload the atlas texture.
  void set_pass(int32_t pass) { pass_ = pass; }

  /// @return Sets an iterator pointing the FontManager's internal map.
  void set_iterator(fontbuffer_map_it it) { it_map_ = it; }

  /// @return Returns an iterator pointing the map in the FontManager.
  fontbuffer_map_it get_iterator() { return it_map_; }

  /// @brief Adds a codepoint of a glyph to the codepoint array.
  ///
  /// @param[in] codepoint A codepoint of the glyph.
  void AddCodepoint(uint32_t codepoint) { code_points_.push_back(codepoint); }

  /// @brief Adds 4 vertices to be used for a glyph rendering to the
  /// vertex array.
  ///
  /// @param[in] pos A vec2 containing the `x` and `y` position of the first,
  /// unscaled vertex.
  /// @param[in] base_line A const int32_t representing the baseline for the
  /// vertices.
  /// @param[in] scale A float used to scale the size and offset.
  /// @param[in] entry A const GlyphCacheEntry reference whose offset and size
  /// are used in the scaling.
  void AddVertices(const mathfu::vec2 &pos, int32_t base_line, float scale,
                   const GlyphCacheEntry &entry);

  /// @brief Adds 4 indices to be used for a glyph rendering to the
  /// index array.
  ///
  /// @param[in] slice_idx An index of the index array to add the indices.
  void AddIndices(int32_t slice_idx);

  /// @brief Add the given caret position to the buffer.
  ///
  /// @param[in] x The `x` position of the caret.
  /// @param[in] y The `y` position of the caret.
  void AddCaretPosition(int32_t x, int32_t y);

  ///
  /// @param[in] pos The position of the caret.
  void AddCaretPosition(const mathfu::vec2 &pos);

  /// @brief Tell the buffer a word boundary.
  /// It may use the information to justify text layout later.
  ///
  /// @param[in] parameters Text layout parameters used to update the layout.
  void AddWordBoundary(const FontBufferParameters &parameters);

  /// @brief Update UV information of a glyph entry.
  ///
  /// @param[in] index The index of the glyph entry that should be updated.
  /// @param[in] uv The `uv` vec4 should include the top-left corner of UV value
  /// as `x` and `y`, and the bottom-right of UV value as the `w` and `z`
  /// components of the vector.
  void UpdateUV(int32_t index, const mathfu::vec4 &uv);

  /// @brief Indicates a start of new line. It may update a previous line's
  /// buffer contents based on typography layout settings.
  ///
  /// @param[in] parameters Text layout parameters used to update the layout.
  /// @param[in] last_line Indicate if the line is the last line in the
  /// paragraph.
  void UpdateLine(const FontBufferParameters &parameters,
                  TextLayoutDirection layout_direction, bool last_line);

  /// @brief Sets the reference count of the buffer.
  void set_ref_count(uint32_t ref_count) { ref_count_ = ref_count; }

  /// @brief Expand internal buffers when the glyph cache grows.
  void ExpandGlyphBuffers(int32_t new_size) { indices_.resize(new_size); }

  /// @brief Retrieve an internal index of an atlas slice in the FontBuffer.
  int32_t UpdateSliceIndex(int32_t slice, std::vector<int32_t> *atlas_indices);

  // @brief Invalidate the FontBuffer.
  void Invalidate() { valid_ = false; }

  /// @brief Add a reference to the glyph cache row that is referenced in the
  /// FontBuffer.
  void AddCacheRowReference(GlyphCacheRow *p) { referencing_row_.insert(p); }

  /// @brief Release references to the glyph cache row that is referenced in the
  /// FontBuffer.
  void ReleaseCacheRowReference() {
    auto begin = referencing_row_.begin();
    auto end = referencing_row_.end();
    while (begin != end) {
      auto row = *begin;
      row->Release(this);
      begin++;
    }
  }

  /// @brief Reset FontBuffer's index array.
  void ResetIndices() {
    indices_.clear();
    slices_.clear();
  }

  // Font metrics information.
  FontMetrics metrics_;

  // Arrays for font indices, vertices and code points.
  // They are hold as a separate vector because OpenGL draw call needs them to
  // be a separate array.

  // Slices that is used in the FontBuffer.
  std::vector<int32_t> slices_;

  // Indices of the font buffer.
  std::vector<std::vector<uint16_t>> indices_;

  // Vertices data of the font buffer.
  std::vector<FontVertex> vertices_;

  // Code points used in the buffer. This array is used to fetch and update UV
  // entries when the glyph cache is flushed.
  std::vector<uint32_t> code_points_;

  // Caret positions in the buffer. We need to track them differently than a
  // vertices information because we support ligatures so that single glyph
  // can include multiple caret positions.
  std::vector<mathfu::vec2i> caret_positions_;

  // Word boundary information. This information is used only with a typography
  // layout with a justification.
  std::vector<uint32_t> word_boundary_;
  std::vector<uint32_t> word_boundary_caret_;

  // Size of the string in pixels.
  mathfu::vec2i size_;

  // Revision of the FontBuffer corresponding glyph cache revision.
  // Caller needs to check the revision value if glyph texture has referencing
  // entries by checking the revision.
  uint32_t revision_;

  // Pass id. Each pass should have it's own texture atlas contents.
  int32_t pass_;

  // Start index of current line.
  uint32_t line_start_index_;
  uint32_t line_start_caret_index_;

  // Reference count related variables.
  uint32_t ref_count_;

  // A flag indicating the FontBuffer's validity. When a FontBuffer is a
  // reference counting buffer and a glyph cache row which the buffer references
  // are evicted, the buffer is marked as 'invalid'. In that case, the user need
  // to
  // re-create the buffer (and release one reference count).
  bool valid_;

  // Set holding iterators in glyph cache rows. When the buffer is released,
  // the API releases all glyph rows in the set.
  std::set<GlyphCacheRow *> referencing_row_;

  // Back reference to the map. When releasing a buffer, the API uses the
  // iterator to remove the entry from the map.
  fontbuffer_map_it it_map_;
};

/// @struct ScriptInfo
///
/// @brief This struct holds the script information used for a text layout.
///
struct ScriptInfo {
  /// @var locale
  /// @brief A C-string corresponding to the of the
  /// language defined in ISO 639 and the country code difined in ISO 3166
  /// separated
  /// by '-'. (e.g. 'en-US').
  const char *locale;

  /// @var script
  /// @brief ISO 15924 Script code.
  const char *script;
  /// @var direction
  /// @brief Script layout direciton.
  TextLayoutDirection direction;
};
/// @}

/// @class FontShader
///
/// @brief Helper class to handle shaders for a font rendering.
/// The class keeps a reference to a shader, and a location of uniforms with a
/// fixed names.
/// A caller is responsive not to call set_* APIs that specified shader doesn't
/// support.
class FontShader {
 public:
  void set(fplbase::Shader *shader) {
    assert(shader);
    shader_ = shader;
    pos_offset_ = shader->FindUniform("pos_offset");
    color_ = shader->FindUniform("color");
    clipping_ = shader->FindUniform("clipping");
    threshold_ = shader->FindUniform("threshold");
  }
  void set_renderer(const fplbase::Renderer &renderer) {
    shader_->Set(renderer);
  }

  void set_position_offset(const mathfu::vec3 &vec) {
    assert(pos_offset_ >= 0);
    shader_->SetUniform(pos_offset_, vec);
  }
  void set_color(const mathfu::vec4 &vec) {
    assert(color_ >= 0);
    shader_->SetUniform(color_, vec);
  }
  void set_clipping(const mathfu::vec4 &vec) {
    assert(clipping_ >= 0);
    shader_->SetUniform(clipping_, vec);
  }
  void set_threshold(float f) {
    assert(threshold_ >= 0);
    shader_->SetUniform(threshold_, &f, 1);
  }

  fplbase::UniformHandle clipping_handle() { return clipping_; }
  fplbase::UniformHandle color_handle() { return color_; }
  fplbase::UniformHandle position_offset_handle() { return pos_offset_; }
  fplbase::UniformHandle threshold_handle() { return threshold_; }

 private:
  fplbase::Shader *shader_;
  fplbase::UniformHandle pos_offset_;
  fplbase::UniformHandle color_;
  fplbase::UniformHandle clipping_;
  fplbase::UniformHandle threshold_;
};
/// @}

}  // namespace flatui

#endif  // FONT_MANAGER_H
