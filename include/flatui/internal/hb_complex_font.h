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
        font_id_(kNullHash),
        system_font_(false),
        scale_(1 << kHbFixedPointPrecision) {}

  /// @brief The destructor for FaceData.
  ///
  /// @note This calls the `Close()` function.
  ~FaceData() { Close(); }

  /// @brief Close the fontface.
  void Close();

  /// @var face_
  ///
  /// @brief freetype's fontface instance.
  FT_Face face_;

  /// @var font_data_
  ///
  /// @brief Opened font file data.
  ///
  /// The file needs to be kept open until FreeType finishes using the file.
  std::string font_data_;

  /// @var font_id_
  /// @brief Hashed value of the font face.
  HashedId font_id_;

  /// @var system_font_
  /// @brief A flag indicating the font is a system font.
  bool system_font_;

  /// @var scale_
  ///
  /// @brief Scale applied for a layout.
  int32_t scale_;
};

/// @class GlyphInfo
///
/// @brief The glyph information that keeps FreeType face data and codepoint in
/// the face data.
class GlyphInfo {
 public:
  GlyphInfo() : face_(nullptr), code_point_(0) {};
  GlyphInfo(const FaceData *face, hb_codepoint_t code_point) {
    face_ = face;
    code_point_ = code_point;
  }

  /// @brief Getter of FT_Face for the glyph.
  FT_Face GetFtFace() const;

  /// @brief Getter of FaceData for the glyph.
  const FaceData *GetFaceData() const { return face_; }

  /// @brief Getter/Setter of the codepoint for the glyph.
  hb_codepoint_t GetCodepoint() const { return code_point_; }
  void SetCodepoint(hb_codepoint_t point) { code_point_ = point; }

 private:
  /// @var face_
  ///
  /// @brief FaceData instance.
  const FaceData *face_;
  /// @var code_point_
  ///
  /// @brief code_point in the face instance.
  hb_codepoint_t code_point_;
};

/// @class HbFont
///
/// @brief The class provides an interface to manage single FreeType font.
///        Implementations of FreeType font handling is derived from hb-ft.cc.
///        https://github.com/behdad/harfbuzz/blob/master/src/hb-ft.cc
class HbFont {
 public:
  HbFont() : glyph_info_(nullptr, 0), harfbuzz_font_(nullptr) {}
  virtual ~HbFont();

  /// @brief Create an instance of HbFont. If a HbFont with same FaceData has
  ///        already been initialized, the API returns a pointer of the
  ///        instance tracked in a cache.
  ///
  /// @param[in] face A reference to FaceData. FreeType face referenced in the
  ///            structure needs to have been initialized.
  ///
  /// @return A pointer to HbFont associated with the FaceData data.
  ///         Return nullptr if a font class initialization failed.
  static HbFont *Open(const FaceData &face);

  /// @brief Look up a font via hash ID.
  ///
  /// @param[in] id A hashed ID for a font.
  ///
  /// @return A pointer to HbFont associated with the FaceData data.
  ///         Return nullptr if a font instance has not been created.
  static HbFont *Open(HashedId id);

  /// @brief Close the HbFont instance associated to FaceData instance.
  ///
  /// @param[in] face A reference to FaceData.
  ///
  /// @note The API closes all HbFont instances that refers the specified
  ///       FaceData.
  static void Close(const FaceData &face);

  /// @brief Set pixel size to the font.
  ///
  /// @param[in] size A font size in pixel.
  ///
  /// @note If the class contains multiple fonts, the API sets same size to all
  ///       associated fonts.
  virtual void SetPixelSize(uint32_t size);

  /// @brief Get a base line of the font.
  ///
  /// @param[in] size A font size in pixel.
  ///
  virtual int32_t GetBaseLine(int32_t size);

  /// @brief Get the GlyphInfo for requested code point.
  ///
  /// @param[in] code_point A unicode code point.
  ///
  /// @return Returns a pointer to GlyphInfo structure that includes face and
  /// glyph index in the face for requested code point.
  virtual const GlyphInfo *GetGlyphInfo(uint32_t code_point);

  /// @brief Get the font Id of an associated font.
  ///
  /// @return Returns HashId of an associated font that can be used as a key of
  ///         disctionaries.
  virtual HashedId GetFontId();

  /// @brief Get a pointer to the harfbuzz font structure.
  ///
  /// @return Returns a pointer to hb_font_t structure.
  hb_font_t *GetHbFont() { return harfbuzz_font_; }

 private:
  /// @var glyph_info_
  ///
  /// @brief the GlyphInfo structure that contains FreeType related
  ///        information.
  GlyphInfo glyph_info_;

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

  /// @var map_fonts_
  ///
  /// @brief A map that keeps font data opened via Open() API.
  /// The map keeps HashedId of fonts as a key and store HbFont values.
  static std::unordered_map<HashedId, std::unique_ptr<HbFont>> map_fonts_;

  /// @var harfbuzz_font_
  ///
  /// @brief harfbuzz's font information instance.
  hb_font_t *harfbuzz_font_;
};

/// @class HbComplexFont
///
/// @brief The HbComplexFont class provides an interface for harfbuzz to have an
///  access to OpenType/TrueType font files opened in FreeType with a fallback
///  mechanism per glyph which APIs provided in hb-ft.cc do not support.
/// The class inherits HbFont as a public base class.
class HbComplexFont : public HbFont {
 public:
  HbComplexFont() : complex_font_id_(kNullHash) {}
  virtual ~HbComplexFont() {};

  /// @brief Create an instance of HbFont. If a HbFont with same FaceData has
  ///        already been initialized, the API returns a pointer of the
  ///        instance tracked in a cache.
  ///
  /// @param[in] id A hashed ID for a font.
  /// @param[in] vec A vector that contains FaceData pointers.
  static HbFont *Open(HashedId id, std::vector<FaceData *> *vec);

  /// @brief Close the HbFont instance with specified ID.
  ///
  /// @param[in] id A hashed ID for a font.
  static void Close(HashedId id);

  /// @brief Overriding virtual methods.
  void SetPixelSize(uint32_t size);
  int32_t GetBaseLine(int32_t size);
  const GlyphInfo *GetGlyphInfo(uint32_t code_point);
  HashedId GetFontId() { return complex_font_id_; }

 private:
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

  /// @var codepoint_cache_
  ///
  /// @brief A cache that keeps GlyphInfo containg FreeType face and
  /// codepoint in the face.
  /// The map keeps unicode values as a key and store correspoinding GlyphInfo
  /// values.
  /// Since a priority of faces can be different in multiple HbComplexFonts,
  /// the cache is per HbComplexFont, not a global cache.
  std::unordered_map<hb_codepoint_t, GlyphInfo> codepoint_cache_;

  /// @var complex_font_id_
  ///
  /// @brief Font ID derived from an array of FreeType face data.
  HashedId complex_font_id_;
};

}  // namespace flatui

#endif  // FPL_HB_COMPLEX_FONT_H
