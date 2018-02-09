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

// Freetype2 header
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H
#include FT_TRUETYPE_TABLES_H

// Harfbuzz header
#include <hb.h>
#include <hb-ft.h>
#include <hb-ot.h>

// libunibreak header
#include <unibreakdef.h>

#include "font_manager.h"
#include "fplbase/fpl_common.h"
#include "fplbase/utilities.h"
#include "internal/flatui_util.h"
#include "internal/hb_complex_font.h"

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif  // __APPLE__

static const bool kShouldMapFontByName =
#ifdef __APPLE__
#if TARGET_OS_IPHONE
  true;
#else  // TARGET_OS_IPHONE
  false;
#endif  // TARGET_OS_IPHONE
#else
  false;
#endif  // __APPLE__

using fplbase::LogInfo;
using fplbase::LogError;

namespace flatui {

HbFont *HbComplexFont::Open(HashedId id, std::vector<FaceData *> *vec,
                            HbFontCache *cache) {
  auto it = cache->find(id);
  if (it != cache->end()) {
    // The font has been already opened.
    return it->second.get();
  }

  // Insert the created entry to the hash map.
  auto insert = cache->insert(std::pair<HashedId, std::unique_ptr<HbFont>>(
      id, std::unique_ptr<HbFont>(new HbComplexFont)));
  auto font = static_cast<HbComplexFont *>(insert.first->second.get());
  font->faces_ = *vec;
  font->complex_font_id_ = id;
  return font;
}

void HbComplexFont::Close(HashedId id, HbFontCache *cache) {
  auto it = cache->find(id);
  if (it == cache->end()) {
    return;
  }
  cache->erase(it);
}

static inline void NullCallback(void*) {}

void HbComplexFont::OverrideCallbacks(int32_t i) {
  // Override callbacks.
  auto table = hb_font_funcs_create();
  hb_font_funcs_set_glyph_func(table, HbGetGlyph, this, NullCallback);
  hb_font_funcs_set_glyph_h_advance_func(table, HbGetHorizontalAdvance, this,
                                         NullCallback);
  hb_font_funcs_set_glyph_v_advance_func(table, HbGetVerticalAdvance, this,
                                         NullCallback);
  hb_font_funcs_set_glyph_v_origin_func(table, HbGetVerticalOrigin, this,
                                        NullCallback);
  hb_font_funcs_set_glyph_h_kerning_func(table, HbGetHorizontalKerning, this,
                                         NullCallback);
  hb_font_funcs_set_glyph_extents_func(table, HbGetExtents, this,
                                       NullCallback);
  hb_font_funcs_set_glyph_contour_point_func(table, HgGetContourPoint, this,
                                             NullCallback);
  hb_font_funcs_set_glyph_name_func(table, HbGetName, this, NullCallback);
  hb_font_funcs_make_immutable(table);

  hb_font_set_funcs(faces_[i]->get_hb_font(), table, faces_[i], NullCallback);
}

int32_t HbComplexFont::AnalyzeFontFaceRun(const char *text, size_t length,
                                          std::vector<int32_t> *font_data_index)
    const {
  font_data_index->resize(length);
  std::fill(font_data_index->begin(), font_data_index->end(), kIndexInvalid);
  auto run = 0;
  size_t i = 0;
  size_t current_face = kIndexInvalid;
  size_t text_idx = 0;
  while (text_idx < length) {
    auto unicode = ub_get_next_char_utf8(reinterpret_cast<const utf8_t *>(text),
                                         length, &i);
    // Current face has a priority since we want to have longer run for a font.
    if (current_face != static_cast<size_t>(kIndexInvalid) &&
        FT_Get_Char_Index(faces_[current_face]->get_face(), unicode)) {
      (*font_data_index)[text_idx] = current_face;
    } else {
      // Check if any font has the glyph.
      size_t face_idx = 0;
      for (face_idx = 0; face_idx < faces_.size(); ++face_idx) {
        if (face_idx != current_face &&
            FT_Get_Char_Index(faces_[face_idx]->get_face(), unicode)) {
          (*font_data_index)[text_idx] = face_idx;
          if (face_idx != current_face) {
            run++;
            current_face = face_idx;
          }
          break;
        }
      }
      if (face_idx == faces_.size()) {
        fplbase::LogError("Requested glyph %x didn't match any font.", unicode);
      }
    }
    text_idx = i;
  }
  return run;
}

const FaceData &HbComplexFont::GetFaceData() const {
  return *faces_[current_face_index_];
}

int32_t HbComplexFont::GetBaseLine(int32_t size) const {
  // Return a baseline in the first prioritized font.
  auto face = faces_[0];
  float unit_per_em = face->get_face()->ascender - face->get_face()->descender;
  float base_line = size * face->get_face()->ascender / unit_per_em;
  if (base_line > size) {
    base_line = size;
  }
  return base_line + 0.5f;
}

mathfu::vec2i HbComplexFont::GetUnderline(int32_t size) const {
  // Return a underline info in the first prioritized font.
  auto face = faces_[0]->get_face();
  float unit_per_em = face->ascender - face->descender;
  float underline =
      size * (face->ascender - face->underline_position) / unit_per_em;
  float underline_thickness = size * face->underline_thickness / unit_per_em;
  return mathfu::vec2i(underline - underline_thickness + 0.5f,
                       underline_thickness + 0.5f);
}

void HbComplexFont::SetPixelSize(uint32_t size) { pixel_size_ = size; }

hb_bool_t HbComplexFont::HbGetGlyph(hb_font_t *font, void *font_data,
                                    hb_codepoint_t unicode,
                                    hb_codepoint_t variation_selector,
                                    hb_codepoint_t *glyph, void *user_data) {
  (void)font;
  (void)font_data;
  auto p = static_cast<HbComplexFont *>(user_data);
  auto &face_data = p->GetFaceData();
  auto code_point =
      p->GetGlyph(face_data.get_face(), unicode, variation_selector);
  if (!code_point) {
    return false;
  }
  *glyph = code_point;
  return true;
}

hb_position_t HbComplexFont::HbGetHorizontalAdvance(hb_font_t *font,
                                                    void *font_data,
                                                    hb_codepoint_t glyph,
                                                    void *user_data) {
  (void)font;
  (void)font_data;
  if (!glyph) return 0;
  auto p = static_cast<HbComplexFont *>(user_data);
  auto &face_data = p->GetFaceData();
  return p->GetGlyphAdvance(face_data.get_face(), glyph, face_data.get_scale(),
                            FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING);
}

hb_position_t HbComplexFont::HbGetVerticalAdvance(hb_font_t *font,
                                                  void *font_data,
                                                  hb_codepoint_t glyph,
                                                  void *user_data) {
  (void)font;
  (void)font_data;
  auto p = static_cast<HbComplexFont *>(user_data);
  auto &face_data = p->GetFaceData();
  return p->GetGlyphAdvance(
      face_data.get_face(), glyph, face_data.get_scale(),
      FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING | FT_LOAD_VERTICAL_LAYOUT);
}

hb_bool_t HbComplexFont::HbGetVerticalOrigin(hb_font_t *font, void *font_data,
                                             hb_codepoint_t glyph,
                                             hb_position_t *x, hb_position_t *y,
                                             void *user_data) {
  (void)font;
  (void)font_data;
  if (!glyph) return false;
  auto p = static_cast<HbComplexFont *>(user_data);
  auto &face_data = p->GetFaceData();
  mathfu::vec2i origin;
  auto scale = face_data.get_scale();
  auto b = p->GetGlyphVerticalOrigin(face_data.get_face(), glyph,
                                     mathfu::vec2i(scale, scale), &origin);
  *x = origin.x;
  *y = origin.y;
  return b;
}

hb_position_t HbComplexFont::HbGetHorizontalKerning(hb_font_t *font,
                                                    void *font_data,
                                                    hb_codepoint_t left_glyph,
                                                    hb_codepoint_t right_glyph,
                                                    void *user_data) {
  (void)font;
  (void)font_data;
  auto p = static_cast<HbComplexFont *>(user_data);
  auto &face_data = p->GetFaceData();

  uint32_t x_ppem, y_ppem;
  hb_font_get_ppem(p->GetHbFont(), &x_ppem, &y_ppem);
  return p->GetGlyphHorizontalKerning(face_data.get_face(), left_glyph,
                                      right_glyph, x_ppem);
}

hb_bool_t HbComplexFont::HbGetExtents(hb_font_t *font, void *font_data,
                                      hb_codepoint_t glyph,
                                      hb_glyph_extents_t *extents,
                                      void *user_data) {
  (void)font;
  (void)font_data;
  auto p = static_cast<HbComplexFont *>(user_data);
  auto &face_data = p->GetFaceData();
  return p->GetGlyphExtents(face_data.get_face(), glyph, extents);
}

hb_bool_t HbComplexFont::HgGetContourPoint(hb_font_t *font, void *font_data,
                                           hb_codepoint_t glyph,
                                           unsigned int point_index,
                                           hb_position_t *x, hb_position_t *y,
                                           void *user_data) {
  (void)font;
  (void)font_data;
  auto p = static_cast<HbComplexFont *>(user_data);
  auto &face_data = p->GetFaceData();
  return p->GetGlyphContourPoint(face_data.get_face(), glyph, point_index, x,
                                 y);
}

hb_bool_t HbComplexFont::HbGetName(hb_font_t *font, void *font_data,
                                   hb_codepoint_t glyph, char *name,
                                   unsigned int size, void *user_data) {
  (void)font;
  (void)font_data;
  auto p = static_cast<HbComplexFont *>(user_data);
  auto &face_data = p->GetFaceData();
  return p->GetGlyphName(face_data.get_face(), glyph, name, size);
}

void HbComplexFont::SetCurrentFaceIndex(int32_t index) {
  // Not to crash even a glyph is not found within current font face set.
  if (index == kIndexInvalid) {
    index = 0;
  }
  current_face_index_ = index;
  OverrideCallbacks(current_face_index_);
  // Set font size to the active font.
  faces_[current_face_index_]->SetSize(pixel_size_);
}

HbFont::~HbFont() {}

HbFont *HbFont::Open(FaceData &face, HbFontCache *cache) {
  auto it = cache->find(face.get_font_id());
  if (it != cache->end()) {
    // The font has been already opened.
    return it->second.get();
  }

  // Insert the created entry to the hash map.
  auto insert = cache->insert(std::pair<HashedId, std::unique_ptr<HbFont>>(
      face.get_font_id(), std::unique_ptr<HbFont>(new HbFont)));
  auto font = insert.first->second.get();
  font->face_data_ = &face;

  return font;
}

HbFont *HbFont::Open(HashedId id, HbFontCache *cache) {
  auto it = cache->find(id);
  if (it != cache->end()) {
    // The font has been already opened.
    return it->second.get();
  }
  return nullptr;
}

void HbFont::Close(const FaceData &face, HbFontCache *cache) {
  auto it = cache->find(face.get_font_id());
  if (it == cache->end()) {
    return;
  }
  cache->erase(it);
}

int32_t HbFont::GetBaseLine(int32_t size) const {
  auto face = face_data_->get_face();
  float unit_per_em = face->ascender - face->descender;
  float base_line = size * face->ascender / unit_per_em;
  if (base_line > size) {
    base_line = size;
  }
  return base_line + 0.5f;
}

mathfu::vec2i HbFont::GetUnderline(int32_t size) const {
  auto face = face_data_->get_face();
  float unit_per_em = face->ascender - face->descender;
  float underline =
      size * (face->ascender - face->underline_position) / unit_per_em;
  float underline_thickness =
      size * face->underline_thickness / unit_per_em + 0.5f;
  return mathfu::vec2i(underline - underline_thickness + 0.5f,
                       underline_thickness + 0.5f);
}

void HbFont::SetPixelSize(uint32_t size) { face_data_->SetSize(size); }

uint32_t HbFont::GetPixelSize() const { return face_data_->GetSize(); }

const FaceData &HbFont::GetFaceData() const { return *face_data_; }

HashedId HbFont::GetFontId() const { return face_data_->get_font_id(); }

hb_codepoint_t HbFont::GetGlyph(FT_Face face, hb_codepoint_t unicode,
                                hb_codepoint_t variation_selector) {
  if (variation_selector) {
    return FT_Face_GetCharVariantIndex(face, unicode, variation_selector);
  } else {
    return FT_Get_Char_Index(face, unicode);
  }
}

hb_position_t HbFont::ToHbPosition(FT_Fixed fixed) {
  return (fixed + (1 << (kHbFixedPointPrecision - 1))) >>
         kHbFixedPointPrecision;
}

hb_position_t HbFont::GetGlyphAdvance(FT_Face face, hb_codepoint_t glyph,
                                      int32_t scale, int32_t flags) {
  FT_Fixed v = 0;
  if (FT_HAS_COLOR(face) && !FT_IS_SCALABLE(face)) {
    // Calculate an advance manually for color glyphs as FreeType seems not
    // returning correct advance.
    auto num = face->num_fixed_sizes;
    if (num > 0) {
      v = face->available_sizes[0].width << kFtFixedPointPrecision;
    }
  } else {
    if (FT_Get_Advance(face, glyph, flags, &v)) {
      return 0;
    }
  }
  // Convert from FT_Fixed to hb_position_t.
  return ToHbPosition((scale * static_cast<int64_t>(v)) >>
                      kHbFixedPointPrecision);
}

bool HbFont::GetGlyphVerticalOrigin(FT_Face face, hb_codepoint_t glyph,
                                    const mathfu::vec2i &scale,
                                    mathfu::vec2i *origin) {
  int32_t x = 0;
  int32_t y = 0;
  if (!FT_HAS_COLOR(face) && FT_IS_SCALABLE(face)) {
    int flags = FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING;
    if (FT_Load_Glyph(face, glyph, flags)) {
      return false;
    }
    x = face->glyph->metrics.horiBearingX - face->glyph->metrics.vertBearingX;
    y = face->glyph->metrics.horiBearingY + face->glyph->metrics.vertBearingY;
  }

  origin->x = (scale.x * static_cast<int64_t>(x)) >> kHbFixedPointPrecision;
  origin->y = (scale.y * static_cast<int64_t>(y)) >> kHbFixedPointPrecision;
  return true;
}

hb_position_t HbFont::GetGlyphHorizontalKerning(FT_Face face,
                                                hb_codepoint_t left_glyph,
                                                hb_codepoint_t right_glyph,
                                                uint32_t x_ppem) {
  FT_Vector kerningv;
  FT_Kerning_Mode mode = x_ppem ? FT_KERNING_DEFAULT : FT_KERNING_UNFITTED;
  if (FT_Get_Kerning(face, left_glyph, right_glyph, mode, &kerningv)) {
    return 0;
  }
  return kerningv.x;
}

bool HbFont::GetGlyphExtents(FT_Face face, hb_codepoint_t glyph,
                             hb_glyph_extents_t *extents) {
  int flags = FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING;
  if (FT_Load_Glyph(face, glyph, flags)) {
    return false;
  }
  extents->x_bearing = face->glyph->metrics.horiBearingX;
  extents->y_bearing = face->glyph->metrics.horiBearingY;
  extents->width = face->glyph->metrics.width;
  extents->height = -face->glyph->metrics.height;
  return true;
}

bool HbFont::GetGlyphContourPoint(FT_Face face, hb_codepoint_t glyph,
                                  uint32_t point_index, hb_position_t *x,
                                  hb_position_t *y) {
  if (FT_Load_Glyph(face, glyph, FT_LOAD_DEFAULT)) {
    return false;
  }
  if (face->glyph->format == FT_GLYPH_FORMAT_OUTLINE ||
      point_index < static_cast<uint32_t>(face->glyph->outline.n_points)) {
    return false;
  }

  *x = face->glyph->outline.points[point_index].x;
  *y = face->glyph->outline.points[point_index].y;
  return true;
}

bool HbFont::GetGlyphName(FT_Face face, hb_codepoint_t glyph, char *name,
                          uint32_t size) {
  hb_bool_t ret = !FT_Get_Glyph_Name(face, glyph, name, size);
  return (size && *name == 0) ? false : (ret != 0);
}

bool FaceData::Open(FT_Library ft, const FontFamily &family) {
  const char *font_name = family.get_name().c_str();
  auto by_name = family.is_family_name();

  // Load the font file of assets.
  auto index = 0;
  if (family.is_font_collection()) {
    font_name = family.get_original_name().c_str();
    index = family.get_index();
  }
  // Try to map the file first.
  auto size = 0;
  auto p = fplbase::MapFile(family.get_original_name().c_str(), 0, &size);
  if (p) {
    mapped_data_ = p;
    font_size_ = size;
  } else if (by_name) {
    if (kShouldMapFontByName) {
      p = OpenFontByName(font_name, 0, &size);
      if (!p) {
        LogError("Can't load font resource: %s\n", font_name);
        return false;
      }
      mapped_data_ = p;
      font_size_ = size;
    } else {
      if (!OpenFontByName(font_name, &font_data_)) {
        LogError("Can't load font resource: %s\n", font_name);
        return false;
      }
      p = font_data_.c_str();
      font_size_ = font_data_.size();
    }
  } else {
    // Fallback to regular file load.
    if (!fplbase::LoadFile(family.get_original_name().c_str(), &font_data_)) {
      // Fallback to open the specified font as a font name.
      return false;
    }
    p = font_data_.c_str();
    font_size_ = font_data_.size();
  }
  // Open the font using FreeType API.
  FT_Error err =
      FT_New_Memory_Face(ft, reinterpret_cast<const unsigned char *>(p),
                         static_cast<FT_Long>(get_font_size()), index, &face_);
  if (err) {
    // Failed to open font.
    LogError("Failed to initialize font:%s FT_Error:%d\n", font_name, err);
    return false;
  }

  // Create harfbuzz font information from the FreeType face.
  harfbuzz_font_ = hb_ft_font_create(face_, NULL);
  if (!harfbuzz_font_) {
    Close();
    return false;
  }

  // Set up parameters.
  font_id_ = HashId(family.get_name().c_str());

  return true;
}

void FaceData::Close() {
  // Don't release if it's still referenced.
  if (ref_count_) {
    return;
  }

  // Remove the font data associated to this face data.
  if (harfbuzz_font_) {
    hb_font_destroy(harfbuzz_font_);
    harfbuzz_font_ = nullptr;
  }

  if (face_) {
    FT_Done_Face(face_);
    face_ = nullptr;
  }
  if (mapped_data_) {
    fplbase::UnmapFile(mapped_data_, font_size_);
    mapped_data_ = nullptr;
  } else {
    font_data_.clear();
  }
}

void FaceData::SetSize(uint32_t size) {
  assert(size);
  if (current_size_ == size) return;

  FT_Set_Pixel_Sizes(get_face(), 0, size);
  if (!FT_IS_SCALABLE(get_face())) {
    // TODO:Choose the closest size if multiple sizes are available.
    auto available_size = get_face()->available_sizes[0].height;
    set_scale((static_cast<uint64_t>(size) << kHbFixedPointPrecision) /
              available_size);
  }
  current_size_ = size;
}

}  // namespace flatui
