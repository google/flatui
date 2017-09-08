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

#ifndef FPL_HB_COMPLEX_FONT_H
#define FPL_HB_COMPLEX_FONT_H

#include <unordered_map>

/// @cond FLATUI_INTERNAL
// Forward decls for FreeType & Harfbuzz
typedef struct FT_LibraryRec_ *FT_Library;
typedef struct FT_FaceRec_ *FT_Face;
typedef signed long FT_Fixed;
struct hb_font_t;
struct hb_buffer_t;
struct hb_glyph_extents_t;
struct hb_glyph_info_t;
typedef uint32_t hb_codepoint_t;
typedef int32_t hb_position_t;
typedef uint32_t hb_mask_t;
typedef int32_t hb_bool_t;
/// @endcond

namespace flatui {
// Forward decl.
class FontFamily;

// Fixed point precision used in harfbuzz.
const int32_t kHbFixedPointPrecision = 10;

// Fixed point precision used in Freetype.
const int32_t kFtFixedPointPrecision = 16;

/// @class FaceData
///
/// @brief The font face instance data opened via the `Open()` API.
///
/// It keeps a font file data, FreeType fontface instance, and harfbuzz font
/// information.
class FaceData {
 public:
  /// @brief The default constructor for FaceData.
  FaceData()
      : face_(nullptr),
        mapped_data_(nullptr),
        font_size_(0),
        font_id_(kNullHash),
        scale_(1 << kHbFixedPointPrecision),
        current_size_(0),
        harfbuzz_font_(nullptr),
        ref_count_(0) {}

  /// @brief The destructor for FaceData.
  ///
  /// @note This calls the `Close()` function.
  ~FaceData() {
    // Enforce disposing the font data.
    // This happens only when the FontManager instance is going to be
    // destrcuted. In regular scenario, the caller should call Close() API
    // corresponding to Open() call to clean up FaceData instance gracefully.
    if (ref_count_) {
      ref_count_ = 0;
      Close();
    }
  }

  /// @brief Open a TTF/OTF font.
  ///
  /// @param[in] ft A FreeType library instance.
  /// @param[in] family A FontFamily structure specifying a font name to open.
  /// @return true if the specified font is successfully opened.
  bool Open(FT_Library ft, const FontFamily &family);

  /// @brief Close the FaceData.
  void Close();

  /// @brief Open specified font by name and return the mapped data.
  /// Current implementation works on iOS.
  /// @return Returns a mapped pointer. nullptr when failed to map the
  /// file.
  ///
  /// @param[in] font_name A font name to load.
  /// @param[in] offset An offset of the file contents to map.
  /// @param[in/out] size A size to map. A size of 0 indicates to map
  /// whole file.  returns a mapped size of the file.
  const void* OpenFontByName(const char *font_name,
                             int32_t offset,
                             int32_t *size);

  /// @brief Open specified font by name and return the raw data.
  /// Current implementation works on macOS/iOS and Android.
  /// @return Returns true if the font is opened successfully.
  ///
  /// @param[in] font_name A font name to load.
  /// @param[out] dest A string that font data will be loaded into.
  bool OpenFontByName(const char *font_name, std::string *dest);

  /// @brief Set font size to the facedata.
  /// @param[in] size Face size in pixels.
  void SetSize(uint32_t size);

  /// @brief Get font size to the facedata.
  uint32_t GetSize() const { return current_size_; }

  // Getter/Setters.
  int32_t get_scale() const { return scale_; }
  void set_scale(int32_t scale) { scale_ = scale; }

  FT_Face get_face() const { return face_; }
  HashedId get_font_id() const { return font_id_; }
  hb_font_t *get_hb_font() const { return harfbuzz_font_; }
  int32_t get_font_size() const { return font_size_; }
  void set_font_id(HashedId id) { font_id_ = id; }

  // Reference counting.
  int32_t AddRef() { return ++ref_count_; }
  int32_t Release() { return --ref_count_; }

 private:
  /// @var face_
  ///
  /// @brief freetype's fontface instance.
  FT_Face face_;

  /// @var font_data_
  ///
  /// @brief Opened font file data.
  ///
  /// The file needs to be kept open until FreeType finishes using the file.
  const void *mapped_data_;
  std::string font_data_;
  int32_t font_size_;

  /// @var font_id_
  /// @brief Hashed value of the font face.
  HashedId font_id_;

  /// @var scale_
  ///
  /// @brief Scale applied for a layout.
  int32_t scale_;

  /// @var current_size_
  ///
  /// @brief Current font size.
  uint32_t current_size_;

  /// @var harfbuzz_font_
  ///
  /// @brief harfbuzz's font information instance.
  hb_font_t *harfbuzz_font_;

  /// @var ref_count_
  ///
  /// @brief Reference counter.
  int32_t ref_count_;
};

class HbFont;
typedef std::unordered_map<HashedId, std::unique_ptr<HbFont>> HbFontCache;

/// @class HbFont
///
/// @brief The class provides an interface to manage single FreeType font.
///        Implementations of FreeType font handling is derived from hb-ft.cc.
///        https://github.com/behdad/harfbuzz/blob/master/src/hb-ft.cc
class HbFont {
 public:
  HbFont() : face_data_(nullptr) {}
  virtual ~HbFont();

  /// @brief Create an instance of HbFont.
  ///
  /// @param[in] face A reference to FaceData. FreeType face referenced in the
  ///            structure needs to have been initialized.
  /// @param cache If a HbFont with same FaceData has already been initialized,
  ///        the API returns a pointer of the instance tracked in a cache.
  ///
  /// @return A pointer to HbFont associated with the FaceData data.
  ///         Return nullptr if a font class initialization failed.
  static HbFont *Open(FaceData &face, HbFontCache *cache);

  /// @brief Look up a font via hash ID.
  ///
  /// @param[in] id A hashed ID for a font.
  /// @param cache If a HbFont with same FaceData has already been initialized,
  ///        the API returns a pointer of the instance tracked in a cache.
  ///
  /// @return A pointer to HbFont associated with the FaceData data.
  ///         Return nullptr if a font instance has not been created.
  static HbFont *Open(HashedId id, HbFontCache *HbFontCache);

  /// @brief Close the HbFont instance associated to FaceData instance.
  ///
  /// @param[in] face A reference to FaceData.
  /// @param cache The cache of reusable fonts, from which it will be removed.
  ///
  /// @note The API closes all HbFont instances that refers the specified
  ///       FaceData.
  static void Close(const FaceData &face, HbFontCache *cache);

  /// @brief Set pixel size to the font.
  ///
  /// @param[in] size A font size in pixel.
  ///
  /// @note If the class contains multiple fonts, the API sets same size to all
  ///       associated fonts.
  virtual void SetPixelSize(uint32_t size);

  /// @brief Get pixel size to the font.
  virtual uint32_t GetPixelSize() const;

  /// @brief Get a base line of the font.
  ///
  /// @param[in] size A font size in pixel.
  ///
  virtual int32_t GetBaseLine(int32_t size) const;

  /// @brief Get the underline information for the font.
  ///
  /// @param[in] size The font size in pixels.
  ///
  /// @return Returns a 2 components vector that represents x for the vertical
  /// offset of the underline and y for the thickness of the underline.
  virtual mathfu::vec2i GetUnderline(int32_t size) const;

  /// @brief Get the FaceData for requested code point.
  ///
  /// @return Returns a reference to current FaceData structure.
  virtual const FaceData &GetFaceData() const;

  /// @brief Get the font Id of an associated font.
  ///
  /// @return Returns HashId of an associated font that can be used as a key of
  ///         disctionaries.
  virtual HashedId GetFontId() const;

  /// @brief Get a pointer to the harfbuzz font structure.
  ///
  /// @return Returns a pointer to hb_font_t structure.
  virtual hb_font_t *GetHbFont() { return face_data_->get_hb_font(); }

  /// @brief Check if the font is a complex font with multiple font faces.
  ///
  /// @return Returns true if the font is a complex font.
  virtual bool IsComplexFont() { return false; }

  /// @brief Set the current  font face index in the font.
  virtual void SetCurrentFaceIndex(int32_t /*index*/) {}

  /// @brief Get an ID of the the current font face.
  virtual HashedId GetCurrentFaceId() const { return GetFontId(); }

 private:
  /// @var face_data_
  ///
  /// @brief the FaceData structure that contains FreeType related
  ///        information.
  FaceData *face_data_;

 protected:
  ///
  /// @brief Helpers to retrive glyph information from FreeType face.
  ///
  hb_codepoint_t GetGlyph(const FT_Face face, hb_codepoint_t unicode,
                          hb_codepoint_t variation_selector);
  hb_position_t GetGlyphAdvance(const FT_Face face, hb_codepoint_t glyph,
                                int32_t scale, int32_t flags);
  bool GetGlyphVerticalOrigin(const FT_Face face, hb_codepoint_t glyph,
                              const mathfu::vec2i &scale,
                              mathfu::vec2i *origin);
  hb_position_t GetGlyphHorizontalKerning(const FT_Face face,
                                          hb_codepoint_t left_glyph,
                                          hb_codepoint_t right_glyph,
                                          uint32_t x_ppem);
  bool GetGlyphExtents(const FT_Face face, hb_codepoint_t glyph,
                       hb_glyph_extents_t *extents);
  bool GetGlyphContourPoint(const FT_Face face, hb_codepoint_t glyph,
                            uint32_t point_index, hb_position_t *x,
                            hb_position_t *y);
  bool GetGlyphName(const FT_Face face, hb_codepoint_t glyph, char *name,
                    uint32_t size);
  hb_position_t ToHbPosition(FT_Fixed fixed);
};

/// @class HbComplexFont
///
/// @brief The HbComplexFont class provides an interface for harfbuzz to have an
///  access to OpenType/TrueType font files opened in FreeType with a fallback
///  mechanism per glyph which APIs provided in hb-ft.cc do not support.
/// The class inherits HbFont as a public base class.
class HbComplexFont : public HbFont {
 public:
  HbComplexFont()
      : complex_font_id_(kNullHash), current_face_index_(0), pixel_size_(0) {}
  virtual ~HbComplexFont() {};

  /// @brief Create an instance of HbFont. If a HbFont with same FaceData has
  ///        already been initialized, the API returns a pointer of the
  ///        instance tracked in a cache.
  ///
  /// @param[in] id A hashed ID for a font.
  /// @param[in] vec A vector that contains FaceData pointers.
  /// @param cache If a HbFont with same FaceData has already been initialized,
  ///        the API returns a pointer of the instance tracked in a cache.
  static HbFont *Open(HashedId id, std::vector<FaceData *> *vec,
                      HbFontCache *cache);

  /// @brief Close the HbFont instance with specified ID.
  ///
  /// @param[in] id A hashed ID for a font.
  /// @param cache The cache of reusable fonts, from which it will be removed.
  static void Close(HashedId id, HbFontCache *cache);

  /// @brief Overriding virtual methods.
  void SetPixelSize(uint32_t size);
  uint32_t GetPixelSize() const { return pixel_size_; }
  int32_t GetBaseLine(int32_t size) const;
  mathfu::vec2i GetUnderline(int32_t size) const;

  const FaceData &GetFaceData() const;
  HashedId GetFontId() const { return complex_font_id_; }

  hb_font_t *GetHbFont() { return faces_[current_face_index_]->get_hb_font(); }
  bool IsComplexFont() { return true; }

  /// @brief Analyze given text stream and construct a vector holding font face
  /// index used for each glyph.
  /// @return Retuns a number of runs in the text.
  int32_t AnalyzeFontFaceRun(const char *text, size_t length,
                             std::vector<int32_t> *font_data_index) const;

  void SetCurrentFaceIndex(int32_t index);
  HashedId GetCurrentFaceId() const {
    return faces_[current_face_index_]->get_font_id();
  }

 private:
  void OverrideCallbacks(int32_t i);

  /// @brief HarfBuzz callback functions. They need to be a static member
  /// function to match callback signatures defined in harfbuzz.
  static hb_bool_t HbGetGlyph(hb_font_t *font, void *font_data,
                              hb_codepoint_t unicode,
                              hb_codepoint_t variation_selector,
                              hb_codepoint_t *glyph, void *user_data);
  static hb_position_t HbGetHorizontalAdvance(hb_font_t *font, void *font_data,
                                              hb_codepoint_t glyph,
                                              void *user_data);
  static hb_position_t HbGetVerticalAdvance(hb_font_t *font, void *font_data,
                                            hb_codepoint_t glyph,
                                            void *user_data);
  static hb_bool_t HbGetVerticalOrigin(hb_font_t *font, void *font_data,
                                       hb_codepoint_t glyph, hb_position_t *x,
                                       hb_position_t *y, void *user_data);
  static hb_bool_t HbGetHorizontalOrigin(hb_font_t *font, void *font_data,
                                         hb_codepoint_t glyph, hb_position_t *x,
                                         hb_position_t *y, void *user_data);
  static hb_position_t HbGetHorizontalKerning(hb_font_t *font, void *font_data,
                                              hb_codepoint_t left_glyph,
                                              hb_codepoint_t right_glyph,
                                              void *user_data);
  static hb_position_t HbGetVerticalKerning(hb_font_t *font, void *font_data,
                                            hb_codepoint_t left_glyph,
                                            hb_codepoint_t right_glyph,
                                            void *user_data);
  static hb_bool_t HbGetExtents(hb_font_t *font, void *font_data,
                                hb_codepoint_t glyph,
                                hb_glyph_extents_t *extents, void *user_data);

  static hb_bool_t HgGetContourPoint(hb_font_t *font, void *font_data,
                                     hb_codepoint_t glyph,
                                     unsigned int point_index, hb_position_t *x,
                                     hb_position_t *y, void *user_data);
  static hb_bool_t HbGetName(hb_font_t *font, void *font_data,
                             hb_codepoint_t glyph, char *name,
                             unsigned int size, void *user_data);
  static hb_bool_t HbGetFromName(hb_font_t *font, void *font_data,
                                 const char *name, int len,
                                 hb_codepoint_t *glyph, void *user_data);

  /// @var faces_
  ///
  /// @brief A vector of FreeType faces used in the complex font.
  std::vector<FaceData *> faces_;

  /// @var complex_font_id_
  ///
  /// @brief Font ID derived from an array of FreeType face data.
  HashedId complex_font_id_;

  /// @var current_face_index_
  ///
  /// @brief Index of the current font face.
  int32_t current_face_index_;

  /// @var pixel_size_
  ///
  /// @brief Pixel sizse of the complex font.
  uint32_t pixel_size_;
};

}  // namespace flatui

#endif  // FPL_HB_COMPLEX_FONT_H
