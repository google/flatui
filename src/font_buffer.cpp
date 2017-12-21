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

#include "precompiled.h"

#include "font_manager.h"
#include "fplbase/fpl_common.h"

using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec4;

namespace flatui {

void FontBufferContext::SetAttribute(const FontBufferAttributes &attribute) {
  auto it = LookUpAttribute(attribute);

  if (attribute_history_.empty() || it != attribute_history_.back()) {
    attribute_history_.push_back(it);
  }
}

FontBufferContext::attribute_map_it FontBufferContext::LookUpAttribute(
    const FontBufferAttributes &attribute) {
  if (!attribute_map_.empty()) {
    // Check if we already has an entry in the map.
    auto it = attribute_map_.find(attribute);
    if (it != attribute_map_.end()) {
      return it;
    }
  }
  auto ret = attribute_map_.insert(std::make_pair(attribute, kIndexInvalid));
  return ret.first;
}

int32_t FontBuffer::GetBufferIndex(int32_t slice, FontBufferContext *context) {
  auto &attr_history = context->attribute_history();

  // Check if we can use the latest attribute in the attribute stack.
  if (attr_history.back()->first.get_slice_index() == slice) {
    return attr_history.back()->second;
  }
  auto new_attr = attr_history.back()->first;
  // Remove temporary attribute.
  if (new_attr.get_slice_index() == kIndexInvalid) {
    attr_history.pop_back();
  }
  new_attr.set_slice_index(slice);
  auto it = context->LookUpAttribute(new_attr);
  if (it->second == kIndexInvalid) {
    // Resize index buffers.
    it->second = static_cast<int32_t>(slices_.size());
    slices_.push_back(it->first);
    indices_.resize(it->second + 1);
  }
  // Update the attribute stack.
  if (attr_history.empty() || it != attr_history.back()) {
    attr_history.push_back(it);
  }
  assert(it->second < static_cast<int32_t>(indices_.size()));
  return it->second;
}

void FontBuffer::AddIndices(int32_t buffer_idx, int32_t count) {
  // Construct indices array.
  assert(buffer_idx < static_cast<int32_t>(indices_.size()));
  const uint16_t kIndices[] = {0, 1, 2, 1, 3, 2};
  auto &indices = get_indices(buffer_idx);
  for (size_t j = 0; j < FPL_ARRAYSIZE(kIndices); ++j) {
    auto index = kIndices[j];
    indices.push_back(static_cast<unsigned short>(index + count * 4));
  }
}

void FontBuffer::UpdateUnderline(int32_t buffer_idx, int32_t vertex_index,
                                 const mathfu::vec2i &y_pos) {
  // Update underline information if necessary.
  auto &attr = slices_[buffer_idx];
  if (!attr.get_underline()) {
    return;
  }
  attr.UpdateUnderline(vertex_index, y_pos);
}

void FontBuffer::AddVertices(const mathfu::vec2 &pos, int32_t base_line,
                             float scale, const GlyphCacheEntry &entry) {
  mathfu::vec2 scaled_offset = entry.get_offset() * scale;
  mathfu::vec2 scaled_size = mathfu::vec2(entry.get_size()) * scale;
  mathfu::vec2 scaled_advance = mathfu::vec2(entry.get_advance()) * scale;

  auto x = pos.x + scaled_offset.x;
  auto y = pos.y + base_line - scaled_offset.y;
  auto uv = entry.get_uv();
  vertices_.push_back(FontVertex(x, y, 0.0f, uv.x, uv.y));
  vertices_.push_back(FontVertex(x, y + scaled_size.y, 0.0f, uv.x, uv.w));
  vertices_.push_back(FontVertex(x + scaled_size.x, y, 0.0f, uv.z, uv.y));
  vertices_.push_back(
      FontVertex(x + scaled_size.x, y + scaled_size.y, 0.0f, uv.z, uv.w));

  last_pos_ = pos;
  last_advance_ = last_pos_ + scaled_advance;
}

void FontBuffer::UpdateUV(int32_t index, const mathfu::vec4 &uv) {
  vertices_[index * 4].uv_ = uv.xy();
  vertices_[index * 4 + 1].uv_ = mathfu::vec2(uv.x, uv.w);
  vertices_[index * 4 + 2].uv_ = mathfu::vec2(uv.z, uv.y);
  vertices_[index * 4 + 3].uv_ = uv.zw();
}

void FontBuffer::AddCaretPosition(const mathfu::vec2 &pos) {
  mathfu::vec2i rounded_pos = mathfu::vec2i(pos);
  AddCaretPosition(rounded_pos.x, rounded_pos.y);
}

void FontBuffer::AddCaretPosition(int32_t x, int32_t y) {
  assert(caret_positions_.capacity());
  caret_positions_.push_back(mathfu::vec2i(x, y));
}

void FontBuffer::AddWordBoundary(const FontBufferParameters &parameters,
                                 FontBufferContext *context) {
  if (parameters.get_text_alignment() & kTextAlignmentJustify) {
    // Keep the word boundary info for a later use for a justificaiton.
    context->word_boundary().push_back(
        static_cast<uint32_t>(glyph_info_.size()));
    context->word_boundary_caret().push_back(
        static_cast<uint32_t>(caret_positions_.size()));
  }
}

void FontBuffer::UpdateLine(const FontBufferParameters &parameters,
                            TextLayoutDirection layout_direction,
                            FontBufferContext *context) {
  // Update previous line layout if necessary.
  auto align = parameters.get_text_alignment() & ~kTextAlignmentJustify;
  auto justify = parameters.get_text_alignment() & kTextAlignmentJustify;
  if (context->lastline_must_break()) {
    justify = false;
  }
  auto glyph_count = vertices_.size() / kVerticesPerCodePoint;
  auto line_start_index = std::min(line_start_indices_.back(),
                                   static_cast<uint32_t>(glyph_count));

  // Do nothing when the width of the text rect is not specified.
  if ((justify || align != kTextAlignmentLeft) && parameters.get_size().x) {
    // Adjust glyph positions.
    auto offset = 0;  // Offset to add for each glyph position.
                      // When we justify a text, the offset is increased for
                      // each word boundary by boundary_offset_change.
    auto boundary_offset_change = 0;
    auto &word_boundary = context->word_boundary();

    // |--start_pos--|----line-----|
    // |--end_pos------------------|
    // |--size---------------------------------------|

    // Center
    // |--size/2--------------|
    // |--start_pos--| |line/2|
    //    offset     |-|

    // offset = size/2 - line/2 - start_pos
    //        = size/2 - (end_pos - start_pos)/2 - 2*start_pos/2
    //        = (size - end_pos - start_pos)/2

    // Right
    // |--size---------------------------------------|
    // |--start_pos--|   |----line-----|--start_pos--|
    //    offset     |---|

    // offset = size - line - 2*start_pos
    //        = size - (end_pos - start_pos) - 2*start_pos
    //        = size - end_pos - start_pos

    auto start_pos = 0;
    auto end_pos = 0;
    if (!vertices_.empty()) {
      // Do not use vertices' positions, use the position and advance of the
      // glyph layout to match font's intended alignments to remove bias from
      // offsets and SDF in the glyph.
      if (layout_direction == kTextLayoutDirectionLTR) {
        start_pos = 0;
        end_pos = last_advance_.x;
      } else if (layout_direction == kTextLayoutDirectionRTL) {
        start_pos = last_pos_.x;
        end_pos = parameters.get_size().x;
      } else {
        // TTB layout is currently not supported.
        assert(0);
      }
    }
    auto free_width = parameters.get_size().x - end_pos - start_pos;

    if (justify && word_boundary.size() > 1) {
      // With a justification, we add an offset for each word boundary.
      // For each word boundary (e.g. spaces), we stretch them slightly to align
      // both the left and right ends of each line of text.
      boundary_offset_change = static_cast<int32_t>(
          free_width / (word_boundary.size() - 1));
    } else {
      justify = false;
      if (align == kTextAlignmentCenter) {
        offset = free_width / 2;
      } else if (align == kTextAlignmentRight) {
        offset = free_width;
      } // kTextAlignmentLeft has no offset.
    }

    // Keep original offset value.
    auto offset_caret = offset;

    // Update each glyph's position.
    auto boundary_index = 0;
    for (auto idx = line_start_index; idx < glyph_count; ++idx) {
      if (justify && idx >= word_boundary[boundary_index]) {
        boundary_index++;
        offset += boundary_offset_change;
      }
      auto it = vertices_.begin() + idx * kVerticesPerCodePoint;
      for (auto i = 0; i < kVerticesPerCodePoint; ++i) {
        it->position_.data[0] += offset;
        it++;
      }
    }

    // Update caret position, too.
    if (HasCaretPositions()) {
      boundary_index = 0;
      for (auto idx = context->line_start_caret_index();
           idx < caret_positions_.size(); ++idx) {
        if (justify && idx >= context->word_boundary_caret()[boundary_index]) {
          boundary_index++;
          offset_caret += boundary_offset_change;
        }
        caret_positions_[idx].x += offset_caret;
      }
    }
  }

  // Update current line information.
  line_start_indices_.push_back(static_cast<uint32_t>(glyph_info_.size()));
  context->set_lastline_must_break(false);
  context->set_line_start_caret_index(
      static_cast<uint32_t>(caret_positions_.size()));
  context->word_boundary().clear();
  context->word_boundary_caret().clear();
}

float FontBuffer::AdjustCurrentLine(const FontBufferParameters &parameters,
                                    FontBufferContext *context) {
  auto new_ysize = static_cast<int32_t>(parameters.get_font_size());
  auto offset = 0;
  if (new_ysize > context->current_font_size() &&
      !line_start_indices_.empty() && !context->lastline_must_break()) {
    // Adjust glyph positions in current line.
    offset = new_ysize * parameters.get_line_height_scale() -
             context->current_font_size() * parameters.get_line_height_scale();

    for (auto idx = line_start_indices_.back(); idx < glyph_info_.size();
         ++idx) {
      auto it = vertices_.begin() + idx * kVerticesPerCodePoint;
      for (auto i = 0; i < kVerticesPerCodePoint; ++i) {
        it->position_.data[1] += offset;
        it++;
      }
    }
  }
  return offset;
}

static void VertexExtents(const FontVertex *v, vec2 *min_position,
                          vec2 *max_position) {
  // Get extents of the glyph.
  const float kInfinity = std::numeric_limits<float>::infinity();
  vec2 min(kInfinity);
  vec2 max(-kInfinity);
  for (auto i = 0; i < FontBuffer::kVerticesPerCodePoint; ++i) {
    const vec2 position(v[i].position_.data);
    min = vec2::Min(min, position);
    max = vec2::Max(max, position);
  }
  *min_position = min;
  *max_position = max;
}

std::vector<vec4> FontBuffer::CalculateBounds(int32_t start_index,
                                              int32_t end_index) const {
  const float kInfinity = std::numeric_limits<float>::infinity();
  std::vector<vec4> extents;

  start_index = std::max(0, start_index);
  end_index = std::max(0, end_index);

  // Clamp to vertex bounds.
  const uint32_t start = static_cast<uint32_t>(start_index);
  const uint32_t end =
      std::min(static_cast<uint32_t>(vertices_.size()) / kVerticesPerCodePoint,
               static_cast<uint32_t>(end_index));

  // Find `line` such that line_start_indices[line] is the *end* index of
  // start's line.
  assert(start <= end && end <= line_start_indices_.back());
  uint32_t line = 0;
  while (line_start_indices_[line] <= start) ++line;

  // Prime the loop with the first glyph.
  vec2 min(kInfinity);
  vec2 max(-kInfinity);
  for (auto i = start; i < end; ++i) {
    // If i is on the next line, output the current extents.
    const bool is_new_line = line_start_indices_[line] == i;
    if (is_new_line) {
      // Record the extents for the current line.
      extents.push_back(vec4(min, max));

      // Reset the extents for the next line.
      min = vec2(kInfinity);
      max = vec2(-kInfinity);

      // Advance to the next line.
      line++;
      assert(line < line_start_indices_.size());
    }

    // Get extents of the next glyph.
    vec2 glyph_min;
    vec2 glyph_max;
    VertexExtents(&vertices_[kVerticesPerCodePoint * i], &glyph_min,
                  &glyph_max);

    // Update our current bounds.
    min = vec2::Min(min, glyph_min);
    max = vec2::Max(min, glyph_max);
  }

  // Record the extents for the last line.
  extents.push_back(vec4(min, max));
  return extents;
}

void FontBufferAttributes::UpdateUnderline(int32_t vertex_index,
                                           const mathfu::vec2i &y_pos) {
  // TODO: Underline information need to be consolidated into one attribute,
  // rather than distributed to multiple slices. Otherwise, we wouldn't get
  // continuous underline.
  if (!underline_) {
    return;
  }

  if (underline_info_.empty() || underline_info_.back().y_pos_ != y_pos ||
      underline_info_.back().end_vertex_index_ != vertex_index - 1) {
    // Create new underline info.
    UnderlineInfo info(vertex_index, y_pos);
    underline_info_.push_back(info);
  } else {
    // Update existing line.
    underline_info_.back().y_pos_ = y_pos;
    underline_info_.back().end_vertex_index_ = vertex_index;
  }
}

void FontBufferAttributes::WrapUnderline(int32_t vertex_index) {
  if (!underline_) {
    return;
  }

  // Add new line.
  underline_info_.back().end_vertex_index_ = vertex_index;
  UnderlineInfo info(vertex_index + 1, underline_info_.back().y_pos_);
  underline_info_.push_back(info);
}

void GlyphCacheRow::InvalidateReferencingBuffers() {
  auto begin = ref_.begin();
  auto end = ref_.end();
  while (begin != end) {
    auto buffer = *begin;
    buffer->Invalidate();
    begin++;
  }
}

void GlyphCacheRow::ReleaseReferencesFromFontBuffers() {
  while (!ref_.empty()) {
    auto buffer = *ref_.begin();
    // This will lead to the buffer being removed from ref_ via
    // GlyphCacheRow::Release(), so no need to use iterators here.
    buffer->ReleaseCacheRowReference();
  }
}

}  // namespace flatui
