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

#ifndef FLATUI_FONT_UTIL_H
#define FLATUI_FONT_UTIL_H

#include <string>
#include <vector>

#include "mathfu/glsl_mappings.h"

/// @brief Namespace for FlatUI library.
namespace flatui {

/// @cond FLATUI_INTERNAL
// Forward decl.
class FontBuffer;
/// @endcond

/// @class HtmlSection
///
/// @brief A string of text that has the same formatting, and optional HREF.
struct HtmlSection {
  HtmlSection() {}
  explicit HtmlSection(const std::string &text) : text(text) {}
  HtmlSection(const std::string &text, const std::string &link)
      : text(text), link(link) {}

  /// The text to render on screen. If `link` is specified, should be rendered
  /// as an HTML link (i.e. underlined and in blue).
  std::string text;

  /// The HREF link to visit when `text` is clicked.
  std::string link;
};

/// @brief Convert HTML into a vector of text strings which have the same
/// formatting.
///
/// @param html HTML text.
/// @return A vector of HTML sections. For now, these are either sections with
/// links or without, and we don't specify anything like bold or italics.
std::vector<HtmlSection> ParseHtml(const char *html);

/// @brief Reprocess whitespace the in the manner of an HTML parser.
///
/// Replace intermediate whitespace with a single space.
/// Also, remove leading whitespace if so requested.
///
/// This emulates how HTML processes raw text fields.
///
/// Appends trimmed text to `out`.
/// @return reference to `out`.
std::string &TrimHtmlWhitespace(const char *text, bool trim_leading_whitespace,
                                std::string *out);

/// @brief Generate a triangle strip data representing underline geometry for
/// FontBuffer.
///
/// @param buffer FontBuffer to generate an underline geometory.
/// @param pos An offset value that is added to generated vertices position.
/// @return A vector of vec3 that includes a triangle strip data representing
/// underline information of given FontBuffer. The strip is devided into pieces
/// with a same width of each underlined character.
///
/// Below is an example to use API and render the underline strip using FPLBase
/// API.
/// auto line = GenerateUnderlineVertices(buffer, pos);
/// color_shader_->Set(renderer_);
/// static const fplbase::Attribute format[] = {fplbase::kPosition3f,
/// fplbase::kEND};
/// fplbase::Mesh::RenderArray(fplbase::Mesh::kTriangleStrip, line.size(),
///                            format, sizeof(mathfu::vec3_packed),
///                            &line[0]);
/// Note that a stride value of vec3 is 16 bytes.
std::vector<mathfu::vec3_packed> GenerateUnderlineVertices(
    const FontBuffer &buffer, const mathfu::vec2 &pos);

}  // namespace flatui

#endif  // FLATUI_FONT_UTIL_H
