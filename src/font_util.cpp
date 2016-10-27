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

#include <gumbo.h>

#include "font_manager.h"
#include "font_util.h"
#include "fplbase/fpl_common.h"

namespace flatui {

using mathfu::vec3;
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

// Remove trailing whitespace. Then add a `prefix` if there is any preceeding
// text.
static std::string &StartHtmlLine(const char *prefix, std::string *out) {
  // Trim trailing whitespace.
  while (std::isspace(out->back())) out->pop_back();

  // Append the new lines in `prefix`.
  if (!out->empty()) {
    out->append(prefix);
  }
  return *out;
}

// The very first text output we want to trim the leading whitespace.
// We should also trim if the previous text ends in whitespace.
static bool ShouldTrimLeadingWhitespace(const std::vector<HtmlSection> &s) {
  if (s.size() <= 1) return true;

  const std::string &prev_text =
      s.back().text().empty() ? s[s.size() - 2].text() : s.back().text();
  return std::isspace(prev_text.back());
}

static void GumboTreeToHtmlSections(const GumboNode *node,
                                    std::vector<HtmlSection> *s,
                                    HtmlSection *current_setting) {
  std::string original_face;
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
          if (face != nullptr) {
            // Start a new section for the font tag.
            if (!s->back().text().empty()) {
              s->push_back(HtmlSection());
            }
            // Keep current setting and update font face setting.
            original_face = current_setting->face();
            s->back().set_face(face->value);
            current_setting->set_face(face->value);
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
          if (s->back().face() != original_face) {
            s->push_back(HtmlSection());
            s->back().set_face(original_face);
            current_setting->set_face(original_face);
          }
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

    // Ignore other node types.
    default:
      break;
  }
}

bool ParseHtml(const char *html, std::vector<HtmlSection> *s) {
  // Ensure there is an HtmlSection that can be appended to.
  assert(!s->size());
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
                                                   const mathfu::vec2 &pos) {
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
      const int32_t kVerticesPerGlyph = 4;
      auto regions = slices.at(i).get_underline_info();
      for (size_t i = 0; i < regions.size(); ++i) {
        auto info = regions[i];
        auto y_start = info.y_pos_.x() + pos.y();
        auto y_end = y_start + info.y_pos_.y();

        if (degenerated_triangle) {
          // Add degenerated triangle to connect multiple strips.
          auto start_pos = vec3(vertices.at(info.start_vertex_index_ *
                                            kVerticesPerGlyph).position_);
          vec.push_back(vec.back());
          vec.push_back(
              vec3_packed(vec3(start_pos.x() + pos.x(), y_start, 0.f)));
        }

        // Add vertices.
        for (auto idx = info.start_vertex_index_; idx <= info.end_vertex_index_;
             ++idx) {
          auto strip_pos = vec3(vertices.at(idx * kVerticesPerGlyph).position_);
          vec.push_back(
              vec3_packed(vec3(strip_pos.x() + pos.x(), y_start, 0.f)));
          vec.push_back(vec3_packed(vec3(strip_pos.x() + pos.x(), y_end, 0.f)));
        }

        // Add last 2 vertices.
        auto end_pos =
            vec3(vertices.at(info.end_vertex_index_ * kVerticesPerGlyph +
                             kVerticesPerGlyph - 1).position_);
        vec.push_back(vec3_packed(vec3(end_pos.x() + pos.x(), y_start, 0.f)));
        vec.push_back(vec3_packed(vec3(end_pos.x() + pos.x(), y_end, 0.f)));

        degenerated_triangle = true;
      }
    }
  }
  assert(num_verts == vec.size());
  return vec;
}

}  // namespace flatui
