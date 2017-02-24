// Copyright 2016 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/font_util.cpp"  // Include cpp to test its internal functions.

#include "flatui/flatui_generated.h"
#include "flatui/font_util.h"
#include "fplutil/main.h"
#include "gtest/gtest.h"
#include "mocks/flatui_common_mocks.h"
#include "mocks/flatui_mocks.h"
#include "mocks/fplbase_mocks.h"

using flatui::TrimHtmlWhitespace;
using flatui::HtmlSection;

class FlatUIHtmlTest : public ::testing::Test {
 protected:
  static void SetUpTestCase() {}
};

TEST_F(FlatUIHtmlTest, TrimWhitespace_LeadingTrue) {
  std::string trimmed;
  EXPECT_EQ(TrimHtmlWhitespace("text.", true, &trimmed),
                               std::string("text."));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("\ntext.", true, &trimmed),
                               std::string("text."));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace(" text.", true, &trimmed),
                               std::string("text."));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace(" \ntext.", true, &trimmed),
                               std::string("text."));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("\n\ntext.", true, &trimmed),
                               std::string("text."));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("\r\ntext.", true, &trimmed),
                               std::string("text."));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("\v\ftext.", true, &trimmed),
                               std::string("text."));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("\t \r\ntext.", true, &trimmed),
                               std::string("text."));
  trimmed.clear();
}

TEST_F(FlatUIHtmlTest, TrimWhitespace_LeadingFalse) {
  std::string trimmed;
  EXPECT_EQ(TrimHtmlWhitespace("text.", false, &trimmed),
                               std::string("text."));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("\ntext.", false, &trimmed),
                               std::string(" text."));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace(" text.", false, &trimmed),
                               std::string(" text."));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace(" \ntext.", false, &trimmed),
                               std::string(" text."));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("\n\ntext.", false, &trimmed),
                               std::string(" text."));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("\r\ntext.", false, &trimmed),
                               std::string(" text."));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("\v\ftext.", false, &trimmed),
                               std::string(" text."));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("\t \r\ntext.", false, &trimmed),
                               std::string(" text."));
  trimmed.clear();
}

TEST_F(FlatUIHtmlTest, TrimWhitespace_Trailing) {
  std::string trimmed;
  EXPECT_EQ(TrimHtmlWhitespace("text.\n", true, &trimmed),
                               std::string("text. "));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("text. ", true, &trimmed),
                               std::string("text. "));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("text. \n", true, &trimmed),
                               std::string("text. "));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("text.\n\n", true, &trimmed),
                               std::string("text. "));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("text.\r\n", true, &trimmed),
                               std::string("text. "));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("text.\v\f", true, &trimmed),
                               std::string("text. "));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("text.\t \r\n", true, &trimmed),
                               std::string("text. "));
  trimmed.clear();
}

TEST_F(FlatUIHtmlTest, TrimWhitespace_Middle) {
  std::string trimmed;
  EXPECT_EQ(TrimHtmlWhitespace("text text", false, &trimmed),
            std::string("text text"));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("text \ntext", false, &trimmed),
            std::string("text text"));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("text  text", false, &trimmed),
            std::string("text text"));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("text\n\n text", false, &trimmed),
            std::string("text text"));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("text\r\n text", false, &trimmed),
            std::string("text text"));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("text\v\ftext", false, &trimmed),
            std::string("text text"));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("text\t \r\n text", false, &trimmed),
            std::string("text text"));
  trimmed.clear();
}

TEST_F(FlatUIHtmlTest, TrimWhitespace_All) {
  std::string trimmed;
  EXPECT_EQ(TrimHtmlWhitespace("\n\r \t  \nThe\nquick    brown\t\tfox "
                               "jumped\r\nover \n \tthe\t\n\r\r\nlazy dog.\n",
                               true, &trimmed),
            std::string("The quick brown fox jumped over the lazy dog. "));
  trimmed.clear();
  EXPECT_EQ(TrimHtmlWhitespace("\n\r \t  \nThe\nquick    brown\t\tfox "
                               "jumped\r\nover \n \tthe\t\n\r\r\nlazy dog.\n",
                               false, &trimmed),
            std::string(" The quick brown fox jumped over the lazy dog. "));
  trimmed.clear();
}

static void CheckHtmlParsing(const char* html, const HtmlSection* sections,
                             size_t num_sections) {
  std::vector<HtmlSection> s;
  flatui::ParseHtml(html, &s);

  EXPECT_EQ(num_sections, s.size());
  const size_t num_s = std::min(num_sections, s.size());
  for (size_t i = 0; i < num_s; ++i) {
    EXPECT_EQ(sections[i].text(), s[i].text());
    EXPECT_EQ(sections[i].link(), s[i].link());
  }
}

TEST_F(FlatUIHtmlTest, Basic) {
  static const char kHtml[] =
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<body>\n"
      "\n"
      "<h1>My First Heading</h1>\n"
      "<p>My first paragraph.</p>\n"
      "\n"
      "</body>\n"
      "</html>\n";
  static const HtmlSection kParsed[] = {
      HtmlSection("My First Heading\n\n"
                  "My first paragraph.\n\n")};
  CheckHtmlParsing(kHtml, kParsed, FPL_ARRAYSIZE(kParsed));
}

TEST_F(FlatUIHtmlTest, Link) {
  static const char kHtml[] =
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<body>\n"
      "<a href=\"http://address\">Link Text</a>\n"
      "</body>\n"
      "</html>\n";
  static const HtmlSection kParsed[] = {
      HtmlSection("Link Text", "http://address")};
  CheckHtmlParsing(kHtml, kParsed, FPL_ARRAYSIZE(kParsed));
}

TEST_F(FlatUIHtmlTest, AnchorSpace) {
  static const char kHtml[] =
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<body>\n"
      "<a href=\"http://address\">Link Text</a> following text\n"
      "</body>\n"
      "</html>\n";
  static const HtmlSection kParsed[] = {
      HtmlSection("Link Text", "http://address"),
      HtmlSection(" following text ")};
  CheckHtmlParsing(kHtml, kParsed, FPL_ARRAYSIZE(kParsed));
}

TEST_F(FlatUIHtmlTest, AnchorAfterParagraph) {
  static const char kHtml[] =
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<body>\n"
      "<p>Some paragraph.</p>\n"
      "<a href=\"http://foo\">    Link text   </a>\n"
      "</body>\n"
      "</html>\n";
  static const HtmlSection kParsed[] = {
      HtmlSection("Some paragraph.\n\n"),
      HtmlSection("Link text ", "http://foo")
  };
  CheckHtmlParsing(kHtml, kParsed, FPL_ARRAYSIZE(kParsed));
}

TEST_F(FlatUIHtmlTest, ParagraphNoWhitespace) {
  static const char kHtml[] =
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<body>\n"
      "Normal text<p>Paragraph text</p>\n"
      "</body>\n"
      "</html>\n";
  static const HtmlSection kParsed[] = {
      HtmlSection("Normal text\n\nParagraph text\n\n")};
  CheckHtmlParsing(kHtml, kParsed, FPL_ARRAYSIZE(kParsed));
}

TEST_F(FlatUIHtmlTest, DropLeadingNewLines) {
  static const char kHtml[] =
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<body>\n"
      "<p>Paragraph</p>\n\n\n\n"
      "Break<br>\n"
      "</body>\n"
      "</html>\n";
  static const HtmlSection kParsed[] = {
      HtmlSection("Paragraph\n\n"
                  "Break\n"),
  };
  CheckHtmlParsing(kHtml, kParsed, FPL_ARRAYSIZE(kParsed));
}

TEST_F(FlatUIHtmlTest, Spacing) {
  static const char kHtml[] =
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<body>\n"
      "<h1>H1 heading</h1>\n"
      "<h2>H2 heading</h2>\n"
      "<h3>H3 heading</h3>\n"
      "<h4>H4 heading</h4>\n"
      "<h5>H5 heading</h5>\n"
      "Single tag paragraph<p>\n"
      "<p>Complete paragraph</p>\n"
      "Break<br>\n"
      "Two breaks<br><br>\n"
      "Break<br>then text\n"
      "Horiontal rule<hr>\n"
      "plenty\n of \n\nnewlines\n  \nand\nmore  newlines"
      "</body>\n"
      "</html>\n";
  static const HtmlSection kParsed[] = {
      HtmlSection("H1 heading\n\n"
                  "H2 heading\n\n"
                  "H3 heading\n\n"
                  "H4 heading\n\n"
                  "H5 heading\n"
                  "Single tag paragraph\n\n"
                  "Complete paragraph\n\n"
                  "Break\n"
                  "Two breaks\n\n"
                  "Break\nthen text "
                  "Horiontal rule\n\n"
                  "plenty of newlines and more newlines ")};
  CheckHtmlParsing(kHtml, kParsed, FPL_ARRAYSIZE(kParsed));
}

TEST_F(FlatUIHtmlTest, SpecialCharacters) {
  static const char kHtml[] =
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<body>\n"
      "&lt;&gt;&amp;&quot;&apos;\n"
      "</body>\n"
      "</html>\n";
  static const HtmlSection kParsed[] = {HtmlSection("<>&\"' ")};
  CheckHtmlParsing(kHtml, kParsed, FPL_ARRAYSIZE(kParsed));
}

TEST_F(FlatUIHtmlTest, ManyLinks) {
  static const char kHtml[] =
      "<tbody>\n"
      "<tr>\n"
      "<td style=\"width: 132px; height: 207px;\" nowrap>\n"
      "<div style=\"text-align: center;\"><b>EZ TEMPLATING<br>\n"
      "<br>\n"
      "</b></div>\n"
      "<a "
      "href=\"https://sites.google.com/a/google.com/google-code/ez-templates/"
      "ezt-introduction\">Introduction</a><br>\n"
      "<a "
      "href=\"https://sites.google.com/a/google.com/google-code/ez-templates/"
      "faq\">FAQ</a><br>\n"
      "<br>\n"
      "<a "
      "href=\"https://sites.google.com/a/google.com/google-code/ez-templates/"
      "directory-structure\">Directory Structure</a><br>\n"
      "<a "
      "href=\"https://sites.google.com/a/google.com/google-code/ez-templates/"
      "markup-rules\">Markup Rules</a>\n"
      "<br>\n"
      "<br>\n"
      "<a "
      "href=\"https://sites.google.com/a/google.com/google-code/ez-templates/"
      "ezt-doc-page\">Set up Doc Pages</a>\n"
      "<br>\n"
      "<a "
      "href=\"https://sites.google.com/a/google.com/google-code/ez-templates/"
      "ezt-home-page\">Set up a Home Page</a>\n"
      "<br>\n"
      "<br>\n"
      "<b>HTML / CSS Reference</b><br>\n"
      "<a "
      "href=\"https://sites.google.com/a/google.com/google-code/ez-templates/"
      "ezt-reference\">Reference</a><br>\n"
      "<br>\n"
      "<a "
      "href=\"https://sites.google.com/a/google.com/google-code/ez-templates/"
      "tips-tricks\">Tips and Tricks</a></td>\n"
      "</tr>\n"
      "</tbody>\n";
  static const HtmlSection kParsed[] = {
      HtmlSection("EZ TEMPLATING\n\n"),
      HtmlSection("Introduction",
                  "https://sites.google.com/a/google.com/google-code/"
                  "ez-templates/ezt-introduction"),
      HtmlSection("\n"),
      HtmlSection(
          "FAQ",
          "https://sites.google.com/a/google.com/google-code/ez-templates/faq"),
      HtmlSection("\n\n"),
      HtmlSection("Directory Structure",
                  "https://sites.google.com/a/google.com/google-code/"
                  "ez-templates/directory-structure"),
      HtmlSection("\n"), HtmlSection("Markup Rules",
                                     "https://sites.google.com/a/google.com/"
                                     "google-code/ez-templates/markup-rules"),
      HtmlSection("\n\n"), HtmlSection("Set up Doc Pages",
                                       "https://sites.google.com/a/google.com/"
                                       "google-code/ez-templates/ezt-doc-page"),
      HtmlSection("\n"), HtmlSection("Set up a Home Page",
                                     "https://sites.google.com/a/google.com/"
                                     "google-code/ez-templates/ezt-home-page"),
      HtmlSection("\n\nHTML / CSS Reference\n"),
      HtmlSection("Reference",
                  "https://sites.google.com/a/google.com/google-code/"
                  "ez-templates/ezt-reference"),
      HtmlSection("\n\n"), HtmlSection("Tips and Tricks",
                                       "https://sites.google.com/a/google.com/"
                                       "google-code/ez-templates/tips-tricks"),
  };
  CheckHtmlParsing(kHtml, kParsed, FPL_ARRAYSIZE(kParsed));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
