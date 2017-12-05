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
class HtmlSection {
 public:
  HtmlSection() : size_(0), color_(0) {}
  explicit HtmlSection(const std::string &text)
      : text_(text), size_(0), color_(0) {}
  explicit HtmlSection(const std::string &text, const std::string &link)
      : text_(text), link_(link), size_(0), color_(0) {}
  virtual ~HtmlSection() {}
  // Copy constructor.
  HtmlSection(const HtmlSection &obj)
      : text_(obj.text_),
        link_(obj.link_),
        face_(obj.face_),
        size_(obj.size_),
        color_(obj.color_) {}

  // Accessors of the section type and parameters.
  const std::string &text() const { return text_; }
  std::string &text() { return text_; }

  // Setter/Getter for Anchor tag.
  const std::string &link() const { return link_; }
  void set_link(const char *link) { link_ = link; }

  // Setter/Getter for Font tag properties.
  const std::string &face() const { return face_; }
  void set_face(const char *face) { face_ = face; }
  void set_face(const std::string &face) { face_ = face; }

  int32_t size() const { return size_; }
  void set_size(int32_t size) { size_ = size; }

  // Color value in RGBA.
  uint32_t color() const { return color_; }
  void set_color(uint32_t color) { color_ = color; }

 protected:
  /// The text to render on screen. If `link` is specified, should be rendered
  /// as an HTML link (i.e. underlined and in blue).
  std::string text_;

  /// The HREF link to visit when `text` is clicked.
  std::string link_;

  /// A parameter used in font tag. The contents of the variable depends on a
  /// type.
  std::string face_;
  int32_t size_;
  uint32_t color_;
};

/// @brief Convert HTML into a vector of text strings which have the same
/// formatting.
///
/// @param html HTML text.
/// @param out A vector of HTML sections. For now, these are either sections
/// with
/// links or without, and we don't specify anything like bold or italics.
/// @return true if the given html is successfully parsed.
bool ParseHtml(const char *html, std::vector<HtmlSection> *out);

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
/// @param reverse Reverse the order of the vertices used, for example to
/// preserve mesh normals when using kTextLayoutDirectionRTL.
/// @return A vector of vec3 that includes a triangle strip data representing
/// underline information of given FontBuffer. The strip is divided into pieces
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
    const FontBuffer &buffer, const mathfu::vec2 &pos, bool reverse = false);



/// @brief Generate a triangle mesh data representing padded underline geometry
/// for FontBuffer. Requires special sdf aa shaders.
///
/// @param buffer FontBuffer to generate an underline geometory.
/// @param pos An offset value that is added to generated vertices position.
/// @param padding A padding value that is added around each generated underline
/// segment.
/// @param positions The vector of vec3 that will be cleared and filled with
/// triangle positions representing underline information of given FontBuffer.
/// The strip is divided into pieces with a same width of each underlined
/// character, plus padding at the front and back.
/// @param tex_coords The vector of vec2 that will be cleared and filled with
/// texture coordinates with added padding such that [0,1] in uv space delineate
/// the un-padded underline size. When padding is non-zero, that means that the
/// actual values of tex_coords will be <0 and >1. This is used to add SDF
/// antialiasing and the vector will be the same size as |positions|.
/// @param indices The vector of uint16_t that will be filled with indices into
/// |positions| and |tex_coords| that represent triangles. Its size will be a
/// multiple of 3 and all values less than the size of |positions|.
/// @param reverse Reverse the order of the vertices used, for example to
/// preserve mesh normals when using kTextLayoutDirectionRTL.
/// @return A boolean for the success of this function. If false the output
/// vectors will be untouched.
bool GeneratePaddedUnderlineVertices(
    const FontBuffer &buffer, const mathfu::vec2 &pos,
    const mathfu::vec2 &padding, std::vector<mathfu::vec3>* positions,
    std::vector<mathfu::vec2>* tex_coords, std::vector<uint16_t>* indices,
    bool reverse = false);

}  // namespace flatui

#endif  // FLATUI_FONT_UTIL_H
