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

/// @brief Namespace for FlatUI library.
namespace flatui {

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
/// Remove leading and trailing whitespace, and replace intermediate whitespace
/// with a single space. This emulates how HTML processes raw text fields.
/// Appends trimmed text to `out`.
///
/// @return reference to `out`.
std::string &TrimHtmlWhitespace(const char *text, std::string *out);

}  // namespace flatui

#endif  // FLATUI_FONT_UTIL_H
