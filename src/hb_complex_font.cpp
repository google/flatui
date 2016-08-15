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

#include "fplbase/fpl_common.h"
#include "fplbase/utilities.h"
#include "internal/flatui_util.h"
#include "internal/hb_complex_font.h"

using fplbase::LogInfo;
using fplbase::LogError;

namespace flatui {

// Threshold value to flush a codepoint cache.
// In the codepoint cache,
static const int32_t kCodepointCacheFlushThreshold = 64 * 1024;

// Static member that keeps the HbFont instances.
std::unordered_map<HashedId, std::unique_ptr<HbFont>> HbFont::map_fonts_;

HbFont *HbComplexFont::Open(HashedId id, std::vector<FaceData *> *vec) {
  auto it = map_fonts_.find(id);
  if (it != map_fonts_.end()) {
    auto p = reinterpret_cast<HbComplexFont *>(it->second.get());
    if (p->codepoint_cache_.size() > kCodepointCacheFlushThreshold) {
      p->codepoint_cache_.clear();
    }

    // The font has been already opened.
    return it->second.get();
  }

  // Insert the created entry to the hash map.
  auto insert = map_fonts_.insert(std::pair<HashedId, std::unique_ptr<HbFont>>(
      id, std::unique_ptr<HbFont>(new HbComplexFont)));
  auto font = static_cast<HbComplexFont *>(insert.first->second.get());
  font->faces_ = *vec;
  font->complex_font_id_ = id;

  // Create harfbuzz font information from freetype face.
  auto face = (*vec)[0]->face_;
  font->harfbuzz_font_ = hb_ft_font_create(face, NULL);
  if (!font->harfbuzz_font_) {
    map_fonts_.erase(insert.first);
    return nullptr;
  }
  // Override callbacks.
  auto table = hb_font_funcs_create();
  auto null_callback = [](void *) { return; };
  hb_font_funcs_set_glyph_func(table, HbGetGlyph, font, null_callback);
  hb_font_funcs_set_glyph_h_advance_func(table, HbGetHorizontalAdvance, font,
                                         null_callback);
  hb_font_funcs_set_glyph_v_advance_func(table, HbGetVerticalAdvance, font,
                                         null_callback);
  hb_font_funcs_set_glyph_v_origin_func(table, HbGetVerticalOrigin, font,
                                        null_callback);
  hb_font_funcs_set_glyph_h_kerning_func(table, HbGetHorizontalKerning, font,
                                         null_callback);
  hb_font_funcs_set_glyph_extents_func(table, HbGetExtents, font,
                                       null_callback);
  hb_font_funcs_set_glyph_contour_point_func(table, HgGetContourPoint, font,
                                             null_callback);
  hb_font_funcs_set_glyph_name_func(table, HbGetName, font, null_callback);
  hb_font_funcs_make_immutable(table);
  hb_font_set_funcs(font->harfbuzz_font_, table, face, null_callback);

  return font;
}

void HbComplexFont::Close(HashedId id) {
  auto it = map_fonts_.find(id);
  if (it == map_fonts_.end()) {
    return;
  }
  map_fonts_.erase(it);
}

const GlyphInfo *HbComplexFont::GetGlyphInfo(uint32_t code_point) {
  auto it = codepoint_cache_.find(code_point);
  assert(it != codepoint_cache_.end());
  return &it->second;
}

int32_t HbComplexFont::GetBaseLine(int32_t size) {
  // Return a baseline in the first prioritized font.
  auto face = faces_[0];
  int32_t unit_per_em = face->face_->ascender - face->face_->descender;
  int32_t base_line = size * face->face_->ascender / unit_per_em;
  if (base_line > size) {
    base_line = size;
  }
  return base_line;
}

void HbComplexFont::SetPixelSize(uint32_t size) {
  auto it = faces_.begin();
  auto end = faces_.end();
  for (; it != end; ++it) {
    // Set scaling for non scalable bitmap font.
    auto face = (*it)->face_;
    FT_Set_Pixel_Sizes(face, 0, size);
    if (!FT_IS_SCALABLE(face)) {
      // TODO:Choose the closest size if multiple sizes are available.
      auto available_size = face->available_sizes[0].height;
      (*it)->scale_ = (static_cast<uint64_t>(size) << kHbFixedPointPrecision) /
                      available_size;
    }
  }
}

hb_bool_t HbComplexFont::HbGetGlyph(hb_font_t *font, void *font_data,
                                    hb_codepoint_t unicode,
                                    hb_codepoint_t variation_selector,
                                    hb_codepoint_t *glyph, void *user_data) {
  (void)font;
  (void)font_data;
  auto p = static_cast<HbComplexFont *>(user_data);

  // Fast path for a glyphs in the cache.
  auto it = p->codepoint_cache_.find(unicode);
  if (it != p->codepoint_cache_.end()) {
    *glyph = unicode;
    return true;
  }

  // Iterate all faces and check if one of faces supports specified glyph.
  hb_codepoint_t code_point = 0;
  auto iterator = p->faces_.begin();
  auto end = p->faces_.end();
  for (; iterator != end; ++iterator) {
    code_point = p->GetGlyph((*iterator)->face_, unicode, variation_selector);
    if (code_point) {
      *glyph = unicode;
      p->codepoint_cache_.emplace(
          std::make_pair(unicode, GlyphInfo(*iterator, code_point)));
      break;
    }
  }
  return code_point != 0;
}

hb_position_t HbComplexFont::HbGetHorizontalAdvance(hb_font_t *font,
                                                    void *font_data,
                                                    hb_codepoint_t glyph,
                                                    void *user_data) {
  (void)font;
  (void)font_data;
  if (!glyph) return 0;
  auto p = static_cast<HbComplexFont *>(user_data);
  auto glyph_info = p->codepoint_cache_[glyph];
  return p->GetGlyphAdvance(glyph_info.GetFtFace(), glyph_info.GetCodepoint(),
                            glyph_info.GetFaceData()->scale_,
                            FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING);
}

hb_position_t HbComplexFont::HbGetVerticalAdvance(hb_font_t *font,
                                                  void *font_data,
                                                  hb_codepoint_t glyph,
                                                  void *user_data) {
  (void)font;
  (void)font_data;
  auto p = static_cast<HbComplexFont *>(user_data);
  auto glyph_info = p->codepoint_cache_[glyph];
  return p->GetGlyphAdvance(
      glyph_info.GetFtFace(), glyph_info.GetCodepoint(),
      glyph_info.GetFaceData()->scale_,
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
  auto glyph_info = p->codepoint_cache_[glyph];
  mathfu::vec2i origin;
  auto scale = glyph_info.GetFaceData()->scale_;
  auto b = p->GetGlyphVerticalOrigin(glyph_info.GetFtFace(),
                                     glyph_info.GetCodepoint(),
                                     mathfu::vec2i(scale, scale), &origin);
  *x = origin.x();
  *y = origin.y();
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
  auto left_font = p->codepoint_cache_[left_glyph];
  if (left_font.GetFtFace() != p->codepoint_cache_[right_glyph].GetFtFace()) {
    // Does not support a kerning between different fonts.
    return 0;
  }

  uint32_t x_ppem, y_ppem;
  hb_font_get_ppem(p->harfbuzz_font_, &x_ppem, &y_ppem);
  return p->GetGlyphHorizontalKerning(
      left_font.GetFtFace(), left_font.GetCodepoint(), right_glyph, x_ppem);
}

hb_bool_t HbComplexFont::HbGetExtents(hb_font_t *font, void *font_data,
                                      hb_codepoint_t glyph,
                                      hb_glyph_extents_t *extents,
                                      void *user_data) {
  (void)font;
  (void)font_data;
  auto p = static_cast<HbComplexFont *>(user_data);
  auto glyph_info = p->codepoint_cache_[glyph];
  return p->GetGlyphExtents(glyph_info.GetFtFace(), glyph_info.GetCodepoint(),
                            extents);
}

hb_bool_t HbComplexFont::HgGetContourPoint(hb_font_t *font, void *font_data,
                                           hb_codepoint_t glyph,
                                           unsigned int point_index,
                                           hb_position_t *x, hb_position_t *y,
                                           void *user_data) {
  (void)font;
  (void)font_data;
  auto p = static_cast<HbComplexFont *>(user_data);
  auto glyph_info = p->codepoint_cache_[glyph];
  return p->GetGlyphContourPoint(glyph_info.GetFtFace(),
                                 glyph_info.GetCodepoint(), point_index, x, y);
}

hb_bool_t HbComplexFont::HbGetName(hb_font_t *font, void *font_data,
                                   hb_codepoint_t glyph, char *name,
                                   unsigned int size, void *user_data) {
  (void)font;
  (void)font_data;
  auto p = static_cast<HbComplexFont *>(user_data);
  auto glyph_info = p->codepoint_cache_[glyph];
  return p->GetGlyphName(glyph_info.GetFtFace(), glyph_info.GetCodepoint(),
                         name, size);
}

HbFont::~HbFont() { hb_font_destroy(harfbuzz_font_); }

HbFont *HbFont::Open(const FaceData &face) {
  auto it = map_fonts_.find(face.font_id_);
  if (it != map_fonts_.end()) {
    // The font has been already opened.
    return it->second.get();
  }

  // Insert the created entry to the hash map.
  auto insert = map_fonts_.insert(std::pair<HashedId, std::unique_ptr<HbFont>>(
      face.font_id_, std::unique_ptr<HbFont>(new HbFont)));
  auto font = insert.first->second.get();
  font->glyph_info_ = GlyphInfo(&face, 0);

  // Create harfbuzz font information from freetype face.
  font->harfbuzz_font_ = hb_ft_font_create(face.face_, NULL);
  if (!font->harfbuzz_font_) {
    map_fonts_.erase(insert.first);
    return nullptr;
  }
  return font;
}

HbFont *HbFont::Open(HashedId id) {
  auto it = map_fonts_.find(id);
  if (it != map_fonts_.end()) {
    // The font has been already opened.
    return it->second.get();
  }
  return nullptr;
}

void HbFont::Close(const FaceData &face) {
  auto it = map_fonts_.find(face.font_id_);
  if (it == map_fonts_.end()) {
    return;
  }
  map_fonts_.erase(it);
}

int32_t HbFont::GetBaseLine(int32_t size) {
  int32_t unit_per_em =
      glyph_info_.GetFtFace()->ascender - glyph_info_.GetFtFace()->descender;
  int32_t base_line = size * glyph_info_.GetFtFace()->ascender / unit_per_em;
  if (base_line > size) {
    base_line = size;
  }
  return base_line;
}

void HbFont::SetPixelSize(uint32_t size) {
  FT_Set_Pixel_Sizes(glyph_info_.GetFtFace(), 0, size);
}

const GlyphInfo *HbFont::GetGlyphInfo(uint32_t code_point) {
  glyph_info_.SetCodepoint(code_point);
  return &glyph_info_;
}

HashedId HbFont::GetFontId() { return glyph_info_.GetFaceData()->font_id_; }

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

  origin->x() = (scale.x() * static_cast<int64_t>(x)) >> kHbFixedPointPrecision;
  origin->y() = (scale.y() * static_cast<int64_t>(y)) >> kHbFixedPointPrecision;
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

FT_Face GlyphInfo::GetFtFace() const { return face_->face_; }

}  // namespace flatui
