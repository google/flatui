// Copyright 2017 Google Inc. All rights reserved.
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

#ifndef FONT_BUFFER_H
#define FONT_BUFFER_H

#include "flatui/internal/flatui_util.h"
#include "flatui/internal/glyph_cache.h"
#include "flatui/internal/hb_complex_font.h"

/// @brief Namespace for FlatUI library.
namespace flatui {

/// @file
/// @addtogroup flatui_font_manager
/// @{

/// @cond FLATUI_INTERNAL
// Forward decl.
class FontManager;
/// @endcond

/// @var kFreeTypeUnit
///
/// @brief Constant to convert FreeType unit to pixel unit.
///
/// In FreeType & Harfbuzz, the position value unit is 1/64 px whereas
/// configurable in FlatUI. The constant is used to convert FreeType unit
/// to px.
const int32_t kFreeTypeUnit = 64;

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

// Constant that indicates invalid index.
const int32_t kIndexInvalid = -1;

// Constant that indicates default color of the attributed buffer.
const uint32_t kDefaultColor = 0xffffffff;

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

/// @enum FontBufferStatus
/// @brief A status of FontBuffer correspoinding current glyph cache contents.
/// **Enumerations**:
/// * `kFontBufferStatusReady` - The font buffer is ready to use.
/// * `kFontBufferStatusNeedReconstruct` - The glyph cache has been flushed.
/// * The font buffer needs to be reconstructed.
/// * `kFontBufferStatusNeedCacheUpdate` - The glyph cache texture needs to be
/// * uploaded.
enum FontBufferStatus {
  kFontBufferStatusReady = 0,
  kFontBufferStatusNeedReconstruct = 1,
  kFontBufferStatusNeedCacheUpdate = 2,
};

/// @enum EllipsisMode
/// @brief A flag controlling appending behavior of the ellipsis.
/// **Enumerations**:
/// * `kEllipsisModeTruncateCharacter` - Truncate characters to make a space.
/// * `kEllipsisModeTruncateWord` - Truncate whole word to make a space.
enum EllipsisMode {
  kEllipsisModeTruncateCharacter = 0,
  kEllipsisModeTruncateWord = 1,
};

/// @struct FontFamily
///
/// @brief A class holding font family information. The class provides various
/// ways to support fonts such as a font collection (multiple fonts in a file),
/// referencing a font by a famly name etc.
class FontFamily {
 public:
  // Constructors
  FontFamily(const std::string &name, int32_t index, const std::string &lang,
             bool family_name)
      : index_(index), family_name_(family_name) {
    // Normalize the font name.
    font_name_ = NormalizeFontName(name);
    lang_ = lang;
    if (is_font_collection()) {
      font_name_ = CreateFontCollectionName(font_name_);
    }
    original_name_ = name;
  };
  FontFamily(const char *name, bool family_name)
      : index_(kIndexInvalid), family_name_(family_name) {
    font_name_ = NormalizeFontName(name);
    original_name_ = name;
  }
  FontFamily(const char *name) : index_(kIndexInvalid), family_name_(false) {
    font_name_ = NormalizeFontName(name);
    original_name_ = name;
  };
  FontFamily(const std::string &name)
      : index_(kIndexInvalid), family_name_(false) {
    font_name_ = NormalizeFontName(name);
    original_name_ = name;
  };

  // Accessors to members.
  /// Font name. When the font is in a font collection file, the name is mangled
  /// with a font collection index.
  const std::string &get_name() const { return font_name_; }
  /// Original font name. When family_name_ is set, it's treated as a family
  /// name.
  const std::string &get_original_name() const { return original_name_; }
  /// Language. The entry is ignored when opening a font.
  const std::string &get_language() const { return lang_; }
  /// Index in a font collection. kIndexInvalid indicates the font is not a
  /// font collection.
  int32_t get_index() const { return index_; }
  /// Check if the font name is a family name.
  bool is_family_name() const { return family_name_; }
  /// Check if the font is in a font collection that holds multiple fonts in a
  /// single file.
  bool is_font_collection() const { return index_ != kIndexInvalid; }

 protected:
  std::string CreateFontCollectionName(const std::string &name) {
    // Tweak the file name with a font collection index and return it.
    std::stringstream ss;
    ss << "#" << index_;
    return name + ss.str();
  }

  static std::string NormalizeFontName(const std::string &name) {
    std::size_t found = name.find_last_of("/\\");
    if (found == std::string::npos) {
      return name;
    }
    return name.substr(found + 1);
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
  /// @param[in] enable_hyphenation A bool determining if the hyphenation is
  /// enabled for the FontBuffer.
  /// @param[in] rtl_layout A bool determining if the FontBuffer is for RTL
  /// (right to left) layout.
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
                       bool enable_hyphenation = false, bool rtl_layout = false,
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
    flags_.rtl_layout = rtl_layout;
    flags_.enable_hyphenation = enable_hyphenation;
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
    if (cache_id_ != kNullHash) {
      return cache_id_ == other.cache_id_;
    }
    return (font_id_ == other.font_id_ && text_id_ == other.text_id_ &&
            font_size_ == other.font_size_ && size_.x == other.size_.x &&
            size_.y == other.size_.y &&
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
    // Note that font_id_, text_id_ and cache_id_ are already hashed values.
    size_t value = key.cache_id_;
    if (key.cache_id_ == kNullHash) {
      value = HashCombine<float>(value, key.font_size_);
      value = HashCombine<float>(value, key.kerning_scale_);
      value = HashCombine<float>(value, key.line_height_scale_);
      value = HashCombine<int32_t>(value, key.flags_value_);
      value = HashCombine<int32_t>(value, key.size_.x);
      value = HashCombine<int32_t>(value, key.size_.y);
    }
    return value;
  }

  /// @brief The compare operator for FontBufferParameters.
  bool operator()(const FontBufferParameters &lhs,
                  const FontBufferParameters &rhs) const {
    if (lhs.cache_id_ != kNullHash && rhs.cache_id_ != kNullHash) {
      return lhs.cache_id_ < rhs.cache_id_;
    }
    return std::tie(lhs.font_id_, lhs.text_id_, lhs.font_size_,
                    lhs.kerning_scale_, lhs.line_height_scale_,
                    lhs.flags_value_, lhs.size_.x, lhs.size_.y) <
           std::tie(rhs.font_id_, rhs.text_id_, rhs.font_size_,
                    rhs.kerning_scale_, rhs.line_height_scale_,
                    rhs.flags_value_, rhs.size_.x, rhs.size_.y);
  }

  /// @return Returns a font hash id.
  HashedId get_font_id() const { return font_id_; }

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

  /// @return Returns a flag indicating if the buffer is for RTL layout.
  bool get_rtl_layout_flag() const { return flags_.rtl_layout; }

  /// @return Returns a flag indicating if the hyphenation is enabled.
  bool get_enable_hyphenation_flag() const { return flags_.enable_hyphenation; }

  /// Retrieve a line length of the text based on given parameters.
  /// a fixed line length (get_size.x) will be used if the text is justified
  /// or right aligned otherwise the line length will be determined by the text
  /// layout phase.
  /// @return Returns the expected line width.
  int32_t get_line_length() const {
    auto alignment = get_text_alignment();
    if (alignment == kTextAlignmentLeft || alignment == kTextAlignmentCenter) {
      return 0;
    } else {
      // Other settings will use max width of the given area.
      return size_.x * kFreeTypeUnit;
    }
  }

  /// @return Returns the multi line setting.
  bool get_multi_line_setting() const {
    // size.x == 0 indicates a single line mode.
    if (!size_.x) {
      return false;
    }
    if (get_text_alignment() != kTextAlignmentLeft) {
      return true;
    } else {
      return size_.y == 0 || size_.y > font_size_;
    }
  }

  /// @Brief Set font size.
  void set_font_size(float size) { font_size_ = size; }

  /// @Brief Set the size value.
  void set_size(mathfu::vec2i &size) { size_ = size; }

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
    bool rtl_layout : 1;
    bool enable_hyphenation : 1;
    GlyphFlags glyph_flags : 2;
    TextAlignment text_alignement : 3;
  };
  union {
    uint32_t flags_value_;
    FontBufferFlags flags_;
  };
};

/// @struct LinkInfo
/// @brief HTML link href, and the location of the link text in FontBuffer.
struct LinkInfo {
  LinkInfo() : start_glyph_index(0), end_glyph_index(0) {}
  LinkInfo(const std::string &link, int32_t start_glyph_index,
           int32_t end_glyph_index)
      : link(link),
        start_glyph_index(start_glyph_index),
        end_glyph_index(end_glyph_index) {}

  /// Link address. If from HTML, this is the `href` text.
  std::string link;

  /// First glyph for the link text in the FontBuffer holding the rendered html.
  /// Call FontBuffer::CalculateBounds(start_glyph_index, end_glyph_index) to
  /// get the bounding boxes for the link text.
  int32_t start_glyph_index;

  /// First glyph not in the link text in the FontBuffer.
  int32_t end_glyph_index;
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

/// @class FontBufferAttributes
///
/// @brief A structure holding attribute information of texts in a FontBuffer.
class FontBufferAttributes {
 public:
  FontBufferAttributes()
      : slice_index_(kIndexInvalid), underline_(false), color_(kDefaultColor) {}

  /// @brief The constructor to set default values.
  /// @param[in] underline A flag indicating if the attribute has underline.
  /// @param[in] color A color value of the attribute in RGBA8888.
  FontBufferAttributes(bool underline, uint32_t color)
      : slice_index_(kIndexInvalid), underline_(underline), color_(color) {}

  /// @brief The equal-to operator for comparing FontBufferAttributes for
  /// equality.
  ///
  /// @param[in] other The other FontBufferAttributes to check against for
  /// equality.
  ///
  /// @return Returns `true` if the two FontBufferAttributes are equal.
  /// Otherwise it returns `false`.
  bool operator==(const FontBufferAttributes &other) const {
    return (slice_index_ == other.slice_index_ &&
            underline_ == other.underline_ && color_ == other.color_);
  }

  /// @brief The hash function for FontBufferAttributes.
  ///
  /// @param[in] key A FontBufferAttributes to use as the key for
  /// hashing.
  ///
  /// @return Returns a `size_t` of the hash of the FontBufferAttributes.
  size_t operator()(const FontBufferAttributes &key) const {
    // Note that font_id_, text_id_ and cache_id_ are already hashed values.
    size_t value = HashValue(key.slice_index_);
    value = HashCombine<bool>(value, key.underline_);
    value = HashCombine<uint32_t>(value, key.color_);
    return value;
  }

  /// @brief The compare operator for FontBufferAttributes.
  bool operator()(const FontBufferAttributes &lhs,
                  const FontBufferAttributes &rhs) const {
    return std::tie(lhs.slice_index_, lhs.underline_, lhs.color_) <
           std::tie(rhs.slice_index_, rhs.underline_, rhs.color_);
  }

  // Struct for the underline information.
  struct UnderlineInfo {
    UnderlineInfo(int32_t index, const mathfu::vec2i &y_pos)
        : start_vertex_index_(index), end_vertex_index_(index), y_pos_(y_pos) {}
    int32_t start_vertex_index_;
    int32_t end_vertex_index_;
    mathfu::vec2i y_pos_;
  };

  // Getter of the color attribute. Color values are in RGBA8888.
  uint32_t get_color() const { return color_; }

  // Getter of the underline attribute.
  bool get_underline() const { return underline_; }

  // Getter of the underline information.
  const std::vector<UnderlineInfo> &get_underline_info() const {
    return underline_info_;
  }

  // Getter/Setter of the slice index.
  int32_t get_slice_index() const { return slice_index_; }
  void set_slice_index(int32_t slice_index) { slice_index_ = slice_index; }

 private:
  // Friend classes.
  friend FontManager;
  friend FontBuffer;

  /// Update underline information.
  void UpdateUnderline(int32_t vertex_index, const mathfu::vec2i &y_pos);
  void WrapUnderline(int32_t vertex_index);

  /// @var texture_index_
  ///
  /// @brief A index value indicating a texture slice in the texture cache used
  /// to render the indices.
  int32_t slice_index_;

  /// @var underline_
  ///
  /// @brief A flag indicating if the buffer is underline buffer. An underline
  /// buffer need to be rendered with a shader that fills all the regions.
  bool underline_;

  /// @var underline_info
  ///
  /// @brief A vector holding underline information.
  std::vector<UnderlineInfo> underline_info_;

  /// @var color_
  ///
  /// @brief A value holds a text color specified for the glyghs.
  uint32_t color_;
};

/// @class FontBufferContext
/// @brief Temporary buffers used while generating FontBuffer.
/// Word boundary information. This information is used only with a typography
/// layout with a justification.
class FontBufferContext {
 public:
  FontBufferContext()
      : line_start_caret_index_(0),
        lastline_must_break_(false),
        appending_buffer_(false),
        original_font_(nullptr),
        original_font_size_(0.0f),
        current_font_size_(0.0f),
        original_base_line_(0) {}

  /// @var Type defining an interator to the attribute map that is tracking
  /// FontBufferAttribute.
  typedef std::map<FontBufferAttributes, int32_t,
                   FontBufferAttributes>::iterator attribute_map_it;

  /// @brief Clear the temporary buffers.
  void Clear() {
    word_boundary_.clear();
    word_boundary_caret_.clear();
    line_start_caret_index_ = 0;
    lastline_must_break_ = false;
    attribute_map_.clear();
    attribute_history_.clear();
    original_font_ = nullptr;
    original_font_size_ = 0.0f;
    current_font_size_ = 0.0f;
    original_base_line_ = 0;
  }

  /// @brief Set attribute to the FontBuffer. The attribute is used while
  /// constructing a FontBuffer.
  void SetAttribute(const FontBufferAttributes &attribute);

  /// @brief Look up an attribute from the attribute map while constructing
  /// attributed FontBuffer.
  attribute_map_it LookUpAttribute(const FontBufferAttributes &attribute);

  // Getter/Setter
  bool lastline_must_break() const { return lastline_must_break_; }
  void set_lastline_must_break(bool b) { lastline_must_break_ = b; }

  bool appending_buffer() const { return appending_buffer_; }
  void set_appending_buffer(bool b) { appending_buffer_ = b; }

  uint32_t line_start_caret_index() const { return line_start_caret_index_; }
  void set_line_start_caret_index(uint32_t i) { line_start_caret_index_ = i; }

  std::vector<attribute_map_it> &attribute_history() {
    return attribute_history_;
  };
  std::vector<uint32_t> &word_boundary() {
    return word_boundary_;
  };
  std::vector<uint32_t> &word_boundary_caret() {
    return word_boundary_caret_;
  };

  HbFont *original_font() const { return original_font_; }
  void set_original_font(HbFont *font) { original_font_ = font; }

  float original_font_size() const { return original_font_size_; }
  void set_original_font_size(float size) { original_font_size_ = size; }

  float current_font_size() const { return current_font_size_; }
  void set_current_font_size(float size) { current_font_size_ = size; }

  int32_t original_base_line() const { return original_base_line_; }
  void set_original_base_line(int32_t base_line) {
    original_base_line_ = base_line;
  }

 private:
  std::vector<uint32_t> word_boundary_;
  std::vector<uint32_t> word_boundary_caret_;
  std::map<FontBufferAttributes, int32_t, FontBufferAttributes> attribute_map_;
  std::vector<attribute_map_it> attribute_history_;
  uint32_t line_start_caret_index_;
  bool lastline_must_break_;
  // flag indicating it's appending a font buffer.
  bool appending_buffer_;

  HbFont *original_font_;
  float original_font_size_;
  float current_font_size_;
  int32_t original_base_line_;
};

/// @struct GlyphInfo
///
/// @brief This struct holds the glyph information that is used when re-creating
/// a FontBuffer.
///
struct GlyphInfo {
  GlyphInfo(HashedId face_id, uint32_t code_point, float size)
      : face_id_(face_id), code_point_(code_point), size_(size) {}

  /// @var face_id
  /// @brief An ID indicating a font face.
  HashedId face_id_;

  /// @var code_point
  /// @brief A code point in the font face.
  uint32_t code_point_;

  /// @var size
  /// @brief A glyph size.
  float size_;
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
  typedef std::map<FontBufferParameters, std::unique_ptr<FontBuffer>,
                   FontBufferParameters>::iterator fontbuffer_map_it;

  /// @brief The default constructor for a FontBuffer.
  FontBuffer()
      : size_(mathfu::kZeros2i),
        last_pos_(mathfu::kZeros2f),
        last_advance_(mathfu::kZeros2f),
        revision_(0),
        ref_count_(0),
        has_ellipsis_(false),
        valid_(true) {
    line_start_indices_.push_back(0);
  }

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
      : size_(mathfu::kZeros2i),
        last_pos_(mathfu::kZeros2f),
        last_advance_(mathfu::kZeros2f),
        revision_(0),
        ref_count_(0),
        has_ellipsis_(false),
        valid_(true) {
    glyph_info_.reserve(size);
    if (caret_info) {
      caret_positions_.reserve(size + 1);
    }
    line_start_indices_.push_back(0);
  }

  /// The destructor for FontBuffer.
  ~FontBuffer() { ReleaseCacheRowReference(); }

  /// @return Returns the FontMetrics metrics parameters for the font
  /// texture.
  const FontMetrics &metrics() const { return metrics_; }

  /// @return Returns the slices array as a const std::vector<int32_t>.
  const std::vector<FontBufferAttributes> &get_slices() const {
    return slices_;
  }

  /// @return Returns the indices array as a const std::vector<uint16_t>.
  const std::vector<uint16_t> &get_indices(int32_t index) const {
    return indices_[index];
  }

  /// @return Returns the vertices array as a const std::vector<FontVertex>.
  const std::vector<FontVertex> &get_vertices() const { return vertices_; }

  /// @return Returns the non const version of vertices array as a
  /// std::vector<FontVertex>.
  /// Note that the updating the number of elements in the buffer would result
  /// undefined behavior.
  std::vector<FontVertex> &get_vertices() { return vertices_; }

  /// @return Returns the array of GlyphInfo as a const std::vector<GlyphInfo>.
  const std::vector<GlyphInfo> &get_glyph_info() const { return glyph_info_; }

  /// @return Returns the size of the string as a const vec2i reference.
  const mathfu::vec2i get_size() const { return size_; }

  /// @return Returns the glyph cache revision counter as a uint32_t.
  ///
  /// @note Each time a contents of the glyph cache is updated, the revision of
  /// the cache is updated.
  ///
  /// If the cache revision and the buffer revision is different, the
  /// font_manager try to re-construct the buffer.
  int32_t get_revision() const { return revision_; }

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
    assert(vertices_.size() == glyph_info_.size() * kVerticesPerCodePoint);
    size_t sum_indices = 0;
    for (size_t i = 0; i < indices_.size(); ++i) {
      sum_indices += indices_[i].size();
    }
    assert(sum_indices == glyph_info_.size() * kIndiciesPerCodePoint);
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

  /// @return Returns `true` if the FontBuffer has an ellipsis appended.
  bool HasEllipsis() const { return has_ellipsis_; }

  /// @brief Helper to retrieve AABB info for glyph index range.
  /// Note that we return multiple AABB bounds if the glyphs span multiple
  /// lines. Each vec4 is in the format (min_x, min_y, max_x, max_y).
  std::vector<mathfu::vec4> CalculateBounds(int32_t start_index,
                                            int32_t end_index) const;

  /// @brief Return glyph indices and linked-to address of any HREF links that
  /// have been rendered to this FontBuffer.
  const std::vector<LinkInfo> &get_links() const { return links_; }

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
  /// a std::vector<FontBufferAttributes>. When rendering the buffer, retrieve
  /// corresponding
  /// atlas texture from the glyph cache and bind the texture.
  std::vector<FontBufferAttributes> &get_slices() { return slices_; }

  /// @return Returns the indices array as a std::vector<uint16_t>.
  std::vector<uint16_t> &get_indices(int32_t index) { return indices_[index]; }

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

  /// @brief Adds a codepoint and related info of a glyph to the glyph info
  /// array.
  ///
  /// @param[in] codepoint A codepoint of the glyph.
  void AddGlyphInfo(HashedId face_id, uint32_t codepoint, float size) {
    glyph_info_.push_back(GlyphInfo(face_id, codepoint, size));
  }

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
  /// @param[in] buffer_idx An index of the index array to add the indices.
  void AddIndices(int32_t buffer_idx, int32_t count);

  /// @brief Update underline information of attributed FontBuffer.
  ///
  /// @param[in] buffer_idx An index of the attribute to add the underline.
  void UpdateUnderline(int32_t buffer_idx, int32_t vertex_index,
                       const mathfu::vec2i &y_pos);

  /// @brief Add the given caret position to the buffer.
  ///
  /// @param[in] x The `x` position of the caret.
  /// @param[in] y The `y` position of the caret.
  void AddCaretPosition(int32_t x, int32_t y);

  /// @brief Add the caret position to the buffer.
  ///
  /// @param[in] pos The position of the caret.
  void AddCaretPosition(const mathfu::vec2 &pos);

  /// @brief Tell the buffer a word boundary.
  /// It may use the information to justify text layout later.
  ///
  /// @param[in] parameters Text layout parameters used to update the layout.
  /// @param[in] context FontBuffer context that is used while constructing a
  /// FontBuffer.
  void AddWordBoundary(const FontBufferParameters &parameters,
                       FontBufferContext *context);

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
  /// @param[in] context FontBuffer context that is used while constructing a
  /// FontBuffer.
  void UpdateLine(const FontBufferParameters &parameters,
                  TextLayoutDirection layout_direction,
                  FontBufferContext *context);
  /// @brief Adjust glyph positions in current line when a glyph size is changed
  /// to larger size while appending buffers.
  ///
  /// @param[in] parameters Text layout parameters used to update the layout.
  /// @param[in] context FontBuffer context that is used while constructing a
  /// FontBuffer.
  /// @return return an offset value for glyph's y position.
  float AdjustCurrentLine(const FontBufferParameters &parameters,
                          FontBufferContext *context);

  /// @brief Sets the reference count of the buffer.
  void set_ref_count(uint32_t ref_count) { ref_count_ = ref_count; }

  /// @brief Retrieve an internal buffer index of accounting atlas slice in the
  /// FontBuffer.
  int32_t GetBufferIndex(int32_t slice, FontBufferContext *context);

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
    referencing_row_.clear();
  }

  // Font metrics information.
  FontMetrics metrics_;

  // Arrays for font indices, vertices and code points.
  // They are hold as a separate vector because OpenGL draw call needs them to
  // be a separate array.

  // Slices that is used in the FontBuffer.
  std::vector<FontBufferAttributes> slices_;

  // Indices of the font buffer.
  std::vector<std::vector<uint16_t>> indices_;

  // Vertices data of the font buffer.
  std::vector<FontVertex> vertices_;

  // Code points and related mapping information used in the buffer. This array
  // is used to fetch and update UV entries when the glyph cache is flushed.
  std::vector<GlyphInfo> glyph_info_;

  // Caret positions in the buffer. We need to track them differently than a
  // vertices information because we support ligatures so that single glyph
  // can include multiple caret positions.
  std::vector<mathfu::vec2i> caret_positions_;

  // Glyph indices and linked-to address of any links that have been rendered
  // in this FontBuffer. Call FontBuffer::CalculateBounds() to get the
  // bounding boxes for the link text.
  std::vector<LinkInfo> links_;

  // Size of the string in pixels.
  mathfu::vec2i size_;

  // The position of last glyph added to the font buffer.
  mathfu::vec2 last_pos_;

  // The advance of last glyph added to the font buffer.
  mathfu::vec2 last_advance_;

  // Revision of the FontBuffer corresponding glyph cache revision.
  // Caller needs to check the revision value if glyph texture has referencing
  // entries by checking the revision.
  int32_t revision_;

  // Pass id. Each pass should have it's own texture atlas contents.
  int32_t pass_;

  // Reference count related variables.
  uint32_t ref_count_;

  // A flag indicating if the font buffer has an ellipsis appended.
  bool has_ellipsis_;

  // A flag indicating the FontBuffer's validity. When a FontBuffer is a
  // reference counting buffer and a glyph cache row which the buffer references
  // are evicted, the buffer is marked as 'invalid'. In that case, the user need
  // to
  // re-create the buffer (and release one reference count).
  bool valid_;

  // Start glyph index of every line.
  std::vector<uint32_t> line_start_indices_;

  // Set holding iterators in glyph cache rows. When the buffer is released,
  // the API releases all glyph rows in the set.
  std::set<GlyphCacheRow *> referencing_row_;

  // Back reference to the map. When releasing a buffer, the API uses the
  // iterator to remove the entry from the map.
  fontbuffer_map_it it_map_;
};

/// @}

}  // namespace flatui

#endif  // FONT_BUFFER_H
