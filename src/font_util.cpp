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

#include <cctype>
#include <sstream>

#if defined(FLATUI_HAS_GUMBO)
#include <gumbo.h>
#endif  // defined(FLATUI_HAS_GUMBO)

#include "flatui.h"
#include "font_manager.h"
#include "font_util.h"
#include "fplbase/fpl_common.h"

namespace flatui {

using mathfu::vec2;
using mathfu::vec3;
using mathfu::vec2_packed;
using mathfu::vec3_packed;

// Replace whitespace with a single space.
// This emulates how HTML processes raw text fields.
// Appends trimmed text to `out`.
// Returns reference to `out`.
std::string &TrimHtmlWhitespace(const char *text, bool trim_leading_whitespace,
                                std::string *out) {
  const char *t = text;

  if (trim_leading_whitespace) {
    while (std::isspace(*t)) ++t;
  }

  for (;;) {
    // Output non-whitespace.
    while (!std::isspace(*t) && *t != '\0') out->push_back(*t++);

    // Break out if at end of input. Note that we do *not* trim all
    // trailing whitespace.
    if (*t == '\0') break;

    // Skip whitespace.
    while (std::isspace(*t)) ++t;

    // Insert single space to compensate for trimmed whitespace.
    out->push_back(' ');
  }
  return *out;
}

#if defined(FLATUI_HAS_GUMBO)
// Remove trailing whitespace. Then add a `prefix` if there is any preceeding
// text.
static std::string &StartHtmlLine(const char *prefix, std::string *out) {
  // Trim trailing whitespace.
  while (!out->empty() && std::isspace(out->back())) out->pop_back();

  // Append the new lines in `prefix`.
  if (!out->empty()) {
    out->append(prefix);
  }
  return *out;
}

// The very first text output we want to trim the leading whitespace.
// We should also trim if the previous text ends in whitespace.
static bool ShouldTrimLeadingWhitespace(const std::vector<HtmlSection> &s) {
  if (s.empty()) return true;
  if (s.back().text().empty() && s.size() < 2) return true;

  const std::string &prev_text =
      s.back().text().empty() ? s[s.size() - 2].text() : s.back().text();
  return std::isspace(prev_text.back());
}

static bool ParseColorValue(const char *value, uint32_t *color) {
  // We only support a hex formatted parameter.
  if (*value++ != '#') {
    return false;
  }
  uint32_t ret = static_cast<uint32_t>(std::strtol(value, nullptr, 16));
  if (ret >= 0xffffff) {
    return false;
  }
  // Convert RGB value to RGBA.
  ret = ret << 8 | 0xff;
  *color = ret;
  return true;
}

static void GumboTreeToHtmlSections(const GumboNode *node,
                                    std::vector<HtmlSection> *s,
                                    HtmlSection *current_setting) {
  auto original_face = current_setting->face();
  auto original_color = current_setting->color();
  auto original_size = current_setting->size();

  switch (node->type) {
    // Process non-text elements, possibly recursing into child nodes.
    case GUMBO_NODE_ELEMENT: {
      const GumboElement &element = node->v.element;

      // Tree prefix processing.
      switch (element.tag) {
        case GUMBO_TAG_A: {
          // Record the link address.
          GumboAttribute *href =
              gumbo_get_attribute(&element.attributes, "href");
          if (href != nullptr) {
            // Start a new section for the anchor.
            if (!s->back().text().empty()) {
              s->push_back(HtmlSection());
            }
            s->back().set_link(href->value);
          }
          break;
        }
        case GUMBO_TAG_FONT: {
          // Record the properties.
          GumboAttribute *face =
              gumbo_get_attribute(&element.attributes, "face");
          GumboAttribute *color =
              gumbo_get_attribute(&element.attributes, "color");
          GumboAttribute *size =
              gumbo_get_attribute(&element.attributes, "size");

          // Start a new section for the font tag.
          bool new_params =
              face != nullptr || color != nullptr || size != nullptr;
          if (new_params && !s->back().text().empty()) {
            s->push_back(HtmlSection());
          }
          if (face != nullptr) {
            // Keep current setting and update font face setting.
            original_face = current_setting->face();
            s->back().set_face(face->value);
            current_setting->set_face(face->value);
          } else {
            s->back().set_face(current_setting->face());
          }
          if (color != nullptr) {
            uint32_t color_value = kDefaultColor;
            if (ParseColorValue(color->value, &color_value)) {
              // Keep current setting and update font face setting.
              original_color = current_setting->color();
              s->back().set_color(color_value);
              current_setting->set_color(color_value);
            } else {
              LogInfo("Failed to parse a value: %s", color->value);
            }
          } else {
            s->back().set_color(current_setting->color());
          }
          if (size != nullptr) {
            auto value =
                static_cast<int>(std::strtol(size->value, nullptr, 10));
            if (value) {
              // Convert the virtual size to physical size.
              value = VirtualToPhysical(vec2(0, value)).y;

              // Keep current setting and update font face setting.
              original_size = current_setting->size();
              s->back().set_size(value);
              current_setting->set_size(value);
            } else {
              LogInfo("Failed to parse a value: %s", size->value);
            }
          } else {
            s->back().set_size(current_setting->size());
          }
          break;
        }
        case GUMBO_TAG_P:
        case GUMBO_TAG_H1:  // fallthrough
        case GUMBO_TAG_H2:  // fallthrough
        case GUMBO_TAG_H3:  // fallthrough
        case GUMBO_TAG_H4:  // fallthrough
        case GUMBO_TAG_H5:  // fallthrough
        case GUMBO_TAG_H6:  // fallthrough
          StartHtmlLine("\n\n", &s->back().text());
          break;

        default:
          break;
      }

      // Tree children processing via recursion.
      for (unsigned int i = 0; i < element.children.length; ++i) {
        const GumboNode *child =
            static_cast<GumboNode *>(element.children.data[i]);
        GumboTreeToHtmlSections(child, s, current_setting);
      }

      // Tree postfix processing.
      switch (element.tag) {
        case GUMBO_TAG_A:
          // Start a new section for the non-anchor.
          s->push_back(HtmlSection());
          break;
        case GUMBO_TAG_FONT: {
          // Restore the font setting if it's changed.
          bool new_params = s->back().face() != original_face ||
                            s->back().color() != original_color ||
                            s->back().size() != original_size;
          if (new_params) {
            s->push_back(HtmlSection());
          }

          // Restore settings.
          current_setting->set_face(original_face);
          s->back().set_face(original_face);
          current_setting->set_color(original_color);
          s->back().set_color(original_color);
          current_setting->set_size(original_size);
          s->back().set_size(original_size);
          break;
        }
        case GUMBO_TAG_HR:
        case GUMBO_TAG_P:  // fallthrough
          s->back().text().append("\n\n");
          break;

        case GUMBO_TAG_H1:  // fallthrough
        case GUMBO_TAG_H2:  // fallthrough
        case GUMBO_TAG_H3:  // fallthrough
        case GUMBO_TAG_H4:  // fallthrough
        case GUMBO_TAG_H5:  // fallthrough
        case GUMBO_TAG_H6:  // fallthrough
        case GUMBO_TAG_BR:
          s->back().text().append("\n");
          break;

        default:
          break;
      }
      break;
    }

    // Append text without excessive whitespaces.
    case GUMBO_NODE_TEXT:
      TrimHtmlWhitespace(node->v.text.text, ShouldTrimLeadingWhitespace(*s),
                         &s->back().text());
      break;

    // Append a single whitespace where appropriate.
    case GUMBO_NODE_WHITESPACE:
      if (!ShouldTrimLeadingWhitespace(*s)) {
        s->back().text().append(" ");
      }
      break;

    // Ignore other node types.
    default:
      break;
  }
}
#endif  // defined(FLATUI_HAS_GUMBO)

bool ParseHtml(const char *html, std::vector<HtmlSection> *s) {
#if defined(FLATUI_HAS_GUMBO)
  // Ensure there is an HtmlSection that can be appended to.
  assert(s->empty());
  s->push_back(HtmlSection());

  // Parse html into tree, with Gumbo, then process the tree.
  GumboOutput *gumbo = gumbo_parse(html);
  HtmlSection current_setting;
  GumboTreeToHtmlSections(gumbo->root, s, &current_setting);
  gumbo_destroy_output(&kGumboDefaultOptions, gumbo);

  // Prune empty last section.
  if (s->back().text().empty()) {
    s->pop_back();
  }
  return true;
#else
  (void)html;
  (void)s;
  return false;
#endif  // defined(FLATUI_HAS_GUMBO)
}

static size_t NumUnderlineVertices(const FontBuffer &buffer) {
  const int32_t kUnderlineVerticesPerGlyph = 2;
  const int32_t kExtraUnderlineVerticesPerInfo = 2;
  size_t num_verts = 0;
  auto &slices = buffer.get_slices();
  for (size_t i = 0; i < slices.size(); ++i) {
    auto regions = slices.at(i).get_underline_info();
    for (size_t i = 0; i < regions.size(); ++i) {
      auto info = regions[i];
      num_verts += (info.end_vertex_index_ - info.start_vertex_index_ + 2) *
                   kUnderlineVerticesPerGlyph;
      num_verts += kExtraUnderlineVerticesPerInfo;
    }
  }
  if (num_verts) {
    // Subtract the amount of the first strip which doesn't need a triangle to
    // stitch.
    num_verts -= kExtraUnderlineVerticesPerInfo;
  }
  return num_verts;
}

std::vector<vec3_packed> GenerateUnderlineVertices(const FontBuffer &buffer,
                                                   const mathfu::vec2 &pos,
                                                   bool reverse) {
  std::vector<vec3_packed> vec;
  auto num_verts = NumUnderlineVertices(buffer);
  if (!num_verts) {
    return vec;
  }
  vec.reserve(num_verts);
  auto &slices = buffer.get_slices();
  auto &vertices = buffer.get_vertices();
  bool degenerated_triangle = false;
  for (size_t i = 0; i < slices.size(); ++i) {
    if (slices.at(i).get_underline()) {
      // Generate underline strips.
      auto regions = slices.at(i).get_underline_info();
      for (size_t i = 0; i < regions.size(); ++i) {
        auto info = regions[i];
        auto y_start = info.y_pos_.x + pos.y;
        auto y_end = y_start + info.y_pos_.y;
        int start_vertex_index = info.start_vertex_index_;
        int end_vertex_index = info.end_vertex_index_;
        const int index_count = end_vertex_index - start_vertex_index;
        int index_direction = 1;
        if (reverse) {
          std::swap(start_vertex_index, end_vertex_index);
          index_direction = -1;
        }

        if (degenerated_triangle) {
          // Add degenerated triangle to connect multiple strips.
          auto start_pos = vec3(vertices.at(start_vertex_index *
                                            kVerticesPerGlyph).position_);
          vec.push_back(vec.back());
          vec.push_back(vec3_packed(vec3(start_pos.x + pos.x, y_start, 0.f)));
        }

        // Add vertices.
        for (int i = 0; i <= index_count; ++i) {
          int idx = start_vertex_index + i * index_direction;
          auto strip_pos = vec3(vertices.at(idx * kVerticesPerGlyph).position_);
          vec.push_back(vec3_packed(vec3(strip_pos.x + pos.x, y_start, 0.f)));
          vec.push_back(vec3_packed(vec3(strip_pos.x + pos.x, y_end, 0.f)));
        }

        // Add last 2 vertices.
        auto end_pos =
            vec3(vertices.at(end_vertex_index * kVerticesPerGlyph +
                             kVerticesPerGlyph - 1).position_);
        vec.push_back(vec3_packed(vec3(end_pos.x + pos.x, y_start, 0.f)));
        vec.push_back(vec3_packed(vec3(end_pos.x + pos.x, y_end, 0.f)));

        degenerated_triangle = true;
      }
    }
  }
  assert(num_verts == vec.size());
  return vec;
}

static void NumPaddedUnderlineVertices(const FontBuffer &buffer,
                                       size_t* num_verts, size_t* num_indices) {
  *num_verts = 0;
  *num_indices = 0;
  const int32_t kUnderlineVerticesPerGlyph = 2;
  const int32_t kUnderlineIndicesPerGlyph = 6;
  const auto& slices = buffer.get_slices();
  for (size_t i = 0; i < slices.size(); ++i) {
    const auto& regions = slices[i].get_underline_info();
    for (size_t i = 0; i < regions.size(); ++i) {
      const auto& info = regions[i];
      const auto num_glyphs =
          info.end_vertex_index_ - info.start_vertex_index_ + 1;
      *num_verts += (num_glyphs + 1) * kUnderlineVerticesPerGlyph;
      *num_indices += num_glyphs * kUnderlineIndicesPerGlyph;
    }
  }
}

bool GeneratePaddedUnderlineVertices(
    const FontBuffer &buffer, const mathfu::vec2 &pos,
    const mathfu::vec2 &padding, std::vector<mathfu::vec3>* positions,
    std::vector<mathfu::vec2>* tex_coords, std::vector<uint16_t>* indices,
    bool reverse) {
  size_t num_verts, num_indices;
  NumPaddedUnderlineVertices(buffer, &num_verts, &num_indices);
  if (num_verts == 0 || num_indices == 0) {
    return false;
  }
  positions->clear();
  positions->reserve(num_verts);
  tex_coords->clear();
  tex_coords->reserve(num_verts);
  indices->clear();
  indices->reserve(num_indices);

  const int32_t kUnderlineVerticesPerGlyph = 2;
  const auto& slices = buffer.get_slices();
  const auto& vertices = buffer.get_vertices();
  for (size_t i = 0; i < slices.size(); ++i) {
    const auto& regions = slices[i].get_underline_info();
    for (size_t i = 0; i < regions.size(); ++i) {
      const auto& info = regions[i];
      // Positions where tex_coord will be 0 or 1.
      const float tex_y_start = pos.y + info.y_pos_.x;
      const float tex_y_end = tex_y_start + info.y_pos_.y;
      // Positions after adding padding.
      const float y_start = tex_y_start - padding.y;
      const float y_end = tex_y_end + padding.y;
      int start_vertex_index = info.start_vertex_index_;
      int end_vertex_index = info.end_vertex_index_;
      const int num_glyphs = end_vertex_index - start_vertex_index + 1;
      int index_direction = 1;
      if (reverse) {
        std::swap(start_vertex_index, end_vertex_index);
        index_direction = -1;
      }
      const size_t start_index = positions->size();

      // Add first 2 vertices.
      const float tex_x_start =
          vertices[start_vertex_index * kVerticesPerGlyph].position_.x + pos.x;
      positions->emplace_back(tex_x_start - padding.x, y_start, 0.f);
      positions->emplace_back(tex_x_start - padding.x, y_end, 0.f);

      // Add middle vertices.
      for (int i = 1; i < num_glyphs; ++i) {
        const int idx = start_vertex_index + i * index_direction;
        const vec3_packed& strip_pos =
            vertices[idx * kVerticesPerGlyph].position_;
        positions->emplace_back(strip_pos.x + pos.x, y_start, 0.f);
        positions->emplace_back(strip_pos.x + pos.x, y_end, 0.f);
      }

      // Add last 2 vertices.
      const int end_idx =
          (end_vertex_index + 1) * kVerticesPerGlyph + kVertexOfRightEdge;
      const float tex_x_end = vertices[end_idx].position_.x + pos.x;
      positions->emplace_back(tex_x_end + padding.x, y_start, 0.f);
      positions->emplace_back(tex_x_end + padding.x, y_end, 0.f);

      // Add tex_coords for every position, such that tex_x/y_start is (0,0) and
      // tex_x/y_end is (1,1).
      const float y_size = tex_y_end - tex_y_start;
      const float x_size = tex_x_end - tex_x_start;
      while (tex_coords->size() < positions->size()) {
        const auto& p = positions->at(tex_coords->size());
        tex_coords->emplace_back((p.x - tex_x_start) / x_size,
                                 (p.y - tex_y_start) / y_size);
      }

      // Add indices.
      for (int i = 0; i < num_glyphs; ++i) {
        const uint16_t index =
            static_cast<uint16_t>(start_index + i * kUnderlineVerticesPerGlyph);
        indices->emplace_back(index);
        indices->emplace_back(static_cast<uint16_t>(index + 1));
        indices->emplace_back(static_cast<uint16_t>(index + 2));

        // Keep same winding order.
        indices->emplace_back(static_cast<uint16_t>(index + 2));
        indices->emplace_back(static_cast<uint16_t>(index + 1));
        indices->emplace_back(static_cast<uint16_t>(index + 3));
      }
    }
  }
  assert(num_verts == positions->size());
  assert(num_verts == tex_coords->size());
  assert(num_indices == indices->size());
  return true;
}

}  // namespace flatui
