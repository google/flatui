// Copyright 2014 Google Inc. All rights reserved.
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

// Harfbuzz header
#include <hb.h>
#include <hb-ft.h>
#include <hb-ot.h>

#include "font_manager.h"
#include "font_util.h"
#include "fplbase/fpl_common.h"
#include "fplbase/utilities.h"

// libUnibreak header
#include "linebreak.h"

// STB_image to resize PNG glyph.
// Disable warnings in STB_image_resize.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100)  // Disable 'unused reference' warning.
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif /* _MSC_VER */
#include "stb_image_resize.h"
// Pop warning status.
#ifdef _MSC_VER
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif

using fplbase::LogInfo;
using fplbase::LogError;
using fplbase::Texture;
using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec4;

namespace flatui {

// Singleton object of FreeType&Harfbuzz.
FT_Library *FontManager::ft_;
hb_buffer_t *FontManager::harfbuzz_buf_;

// Static members in FontBuffer.
std::vector<uint32_t> FontBuffer::word_boundary_;
std::vector<uint32_t> FontBuffer::word_boundary_caret_;
std::map<FontBufferAttributes, int32_t, FontBufferAttributes>
    FontBuffer::attribute_map_;
std::vector<FontBuffer::attribute_map_it> FontBuffer::attribute_history_;
uint32_t FontBuffer::line_start_caret_index_;

// Constants.
const int32_t kVerticesPerGlyph = 4;
const int32_t kIndicesPerGlyph = 6;
static const FontBufferAttributes kHtmlLinkAttributes(true, 0x0000FFFF);
static const FontBufferAttributes kHtmlNormalAttributes(false, 0xFFFFFFFF);

// Enumerate words in a specified buffer using line break information generated
// by libunibreak.
class WordEnumerator {
 private:
  // Delete default constructor.
  WordEnumerator() {}

 public:
  // buffer: a buffer contains a linebreak information.
  // Note that the class accesses the given buffer while it's lifetime.
  WordEnumerator(const std::vector<char> &buffer,
                 const std::vector<int32_t> &face_index_buffer, bool multi_line)
      : current_index_(0),
        current_length_(0),
        buffer_(&buffer),
        face_index_buffer_(&face_index_buffer),
        finished_(false),
        multi_line_(multi_line) {}

  // Advance the current word index
  // return: true if the buffer has next word. false if the buffer finishes.
  bool Advance() {
    assert(buffer_);
    if (!multi_line_ && finished_ == false) {
      // For a single line, allow one iteration.
      finished_ = true;
      current_length_ = buffer_->size();
      return true;
    }

    current_index_ += current_length_;
    if (current_index_ >= buffer_->size() || finished_) {
      return false;
    }

    size_t index = current_index_;
    auto current_face_index = GetFaceIndex(index);

    while (index < buffer_->size()) {
      auto word_info = (*buffer_)[index];
      if (word_info == LINEBREAK_MUSTBREAK ||
          word_info == LINEBREAK_ALLOWBREAK) {
        current_length_ = index - current_index_ + 1;
        break;
      }

      // Check if the font face needs to be switched.
      if (face_index_buffer_->size()) {
        auto i = GetFaceIndex(index);
        if (i != kIndexInvalid && i != current_face_index) {
          current_length_ = index - current_index_;
          break;
        }
      }
      index++;
    }
    return true;
  }

  // Rewind characters and keep them for the next Advance() iteration.
  void Rewind(int32_t characters_to_rewind) {
    current_length_ -= characters_to_rewind;
  }

  // Check if current word is the last one.
  bool IsLastWord() {
    return current_index_ + current_length_ >= buffer_->size() || finished_;
  }

  // Get an index of the current word in the given text buffer.
  size_t GetCurrentWordIndex() const {
    assert(buffer_);
    return current_index_;
  }

  // Get an index of the current font face index.
  int32_t GetCurrentFaceIndex() const { return GetFaceIndex(current_index_); }

  int32_t GetFaceIndex(int32_t index) const {
    if (face_index_buffer_->empty()) {
      return 0;
    }
    return (*face_index_buffer_)[index];
  }

  // Get a length of the current word in the given text buffer.
  size_t GetCurrentWordLength() const {
    assert(buffer_);
    return current_length_;
  }

  // Retrieve if current word must break a line.
  bool CurrentWordMustBreak() {
    assert(buffer_);
    // Check if the first advance is made.
    if (!multi_line_ || current_index_ + current_length_ == 0) return false;
    return (*buffer_)[current_index_ + current_length_ - 1] ==
           LINEBREAK_MUSTBREAK;
  }

  // Retrieve the word buffer.
  const std::vector<char> *GetBuffer() const { return buffer_; }

 private:
  size_t current_index_;
  size_t current_length_;
  const std::vector<char> *buffer_;
  const std::vector<int32_t> *face_index_buffer_;
  bool finished_;
  bool multi_line_;
};

FontManager::FontManager() {
  // Initialize variables and libraries.
  Initialize();

  // Initialize glyph cache.
  fplutil::MutexLock lock(*cache_mutex_);
  glyph_cache_.reset(
      new GlyphCache(mathfu::vec2i(kGlyphCacheWidth, kGlyphCacheHeight),
                     kGlyphCacheMaxSlices));
}

FontManager::FontManager(const mathfu::vec2i &cache_size, int32_t max_slices) {
  // Initialize variables and libraries.
  Initialize();

  // Initialize glyph cache.
  fplutil::MutexLock lock(*cache_mutex_);
  glyph_cache_.reset(new GlyphCache(cache_size, max_slices));
}

FontManager::~FontManager() { delete cache_mutex_; }

void FontManager::Initialize() {
  // Initialize variables.
  renderer_ = nullptr;
  face_initialized_ = false;
  current_atlas_revision_ = 0;
  atlas_last_flush_revision_ = kNeverFlushed;
  current_pass_ = 0;
  line_height_scale_ = kLineHeightDefault;
  kerning_scale_ = kLineHeightDefault;
  current_font_ = nullptr;
  version_ = &FontVersion();
  SetLocale(kDefaultLanguage);
  cache_mutex_ = new fplutil::Mutex(fplutil::Mutex::Mode::kModeNonRecursive);
  appending_buffer_ = false;
  line_width_ = 0;

  if (ft_ == nullptr) {
    ft_ = new FT_Library;
    FT_Error err = FT_Init_FreeType(ft_);
    if (err) {
      // Error! Please fix me.
      LogError("Can't initialize freetype. FT_Error:%d\n", err);
      assert(0);
    }
  }

  if (harfbuzz_buf_ == nullptr) {
    // Create a buffer for harfbuzz.
    harfbuzz_buf_ = hb_buffer_create();
  }

  // Initialize libunibreak
  init_linebreak();
}

void FontManager::Terminate() {
  assert(ft_ != nullptr);
  hb_buffer_destroy(harfbuzz_buf_);
  harfbuzz_buf_ = nullptr;

  FT_Done_FreeType(*ft_);
  ft_ = nullptr;
}

void FontManager::SetRenderer(fplbase::Renderer &renderer) {
  renderer_ = &renderer;
}

FontBuffer *FontManager::GetBuffer(const char *text, size_t length,
                                   const FontBufferParameters &parameter) {
  auto buffer = CreateBuffer(text, static_cast<uint32_t>(length), parameter);
  if (buffer == nullptr) {
    // Flush glyph cache & Upload a texture
    FlushAndUpdate();

    // Try to create buffer again.
    buffer = CreateBuffer(text, static_cast<uint32_t>(length), parameter);
    if (buffer == nullptr) {
      LogError(
          "The given text '%s' with size:%d does not fit a glyph cache. "
          "Try to increase a cache size or use GetTexture() API ",
          "instead.\n", text, parameter.get_size().y());
    }
  }

  return buffer;
}

FontBuffer *FontManager::GetHtmlBuffer(const char *html,
                                       const FontBufferParameters &parameters) {
  {
    // Acquire cache mutex.
    fplutil::MutexLock lock(*cache_mutex_);

    // Check cache if we already have a FontBuffer generated.
    auto buffer = FindBuffer(parameters);
    if (buffer != nullptr) {
      return buffer;
    }
  }

  if (parameters.get_cache_id() == kNullHash) {
    LogInfo(
        "Note that an attributed FontBuffer needs to have unique cache ID"
        "to have correct linked list set up.");
  }

  // Convert HTML into subsections that have the same formatting.
  std::vector<HtmlSection> html_sections = ParseHtml(html);

  // Indicate that it is going to append multiple FontBuffer.
  appending_buffer_ = true;

  // Otherwise create new buffer.
  FontBufferParameters params = parameters;
  mathfu::vec2 pos = mathfu::kZeros2f;
  FontBuffer *buffer = CreateBuffer("", 0, params, &pos);
  if (buffer == nullptr) {
    fplbase::LogError("Failed to create buffer.");
    return nullptr;
  }

  for (size_t i = 0; i < html_sections.size(); ++i) {
    auto &s = html_sections[i];

    // Get the glyph index before appending text.
    const int32_t start_glyph_index = buffer->get_glyph_count();

    // Set the attributes for either link (underlined & blue),
    // or normal (non-underlined & black).
    const bool has_link = !s.link.empty();
    buffer->SetAttribute(has_link ? kHtmlLinkAttributes
                                  : kHtmlNormalAttributes);

    // Append text as per usual.
    {
      fplutil::MutexLock lock(*cache_mutex_);
      buffer =
          AppendBuffer(s.text.c_str(), s.text.length(), params, buffer, &pos);
    }

    // Record link info.
    if (has_link) {
      buffer->links_.push_back(
          LinkInfo(s.link, start_glyph_index, buffer->get_glyph_count()));
    }

    // If the buffer has an ellipsis, won't add text any more.
    if (buffer->HasEllipsis()) {
      break;
    }
  }

  appending_buffer_ = false;
  buffer->UpdateLine(parameters, layout_direction_, true);
  buffer->ClearTemporaryBuffer();

  return buffer;
}

FontBuffer *FontManager::AppendBuffer(const char *text, uint32_t length,
                                      const FontBufferParameters &parameters,
                                      FontBuffer *buffer,
                                      mathfu::vec2 *text_pos) {
  if (!FillBuffer(text, length, parameters, buffer, text_pos)) {
    return nullptr;
  }
  return buffer;
}

FontBuffer *FontManager::FindBuffer(const FontBufferParameters &parameters) {
  auto it = map_buffers_.find(parameters);
  if (it != map_buffers_.end()) {
    // Update current pass.
    if (current_pass_ != kRenderPass) {
      it->second->set_pass(current_pass_);
    }

    // Update UV of the buffer
    auto ysize = static_cast<int32_t>(parameters.get_font_size());
    auto converted_ysize = ConvertSize(ysize);
    auto ret = UpdateUV(converted_ysize, parameters.get_glyph_flags(),
                        it->second.get());

    // Increment the reference count if the buffer is ref counting buffer.
    if (parameters.get_ref_count_flag()) {
      ret->set_ref_count(ret->get_ref_count() + 1);
    }
    return ret;
  }
  return nullptr;
}

FontBuffer *FontManager::CreateBuffer(const char *text, uint32_t length,
                                      const FontBufferParameters &parameters,
                                      mathfu::vec2 *text_pos) {
  // Acquire cache mutex.
  fplutil::MutexLock lock(*cache_mutex_);

  // Check cache if we already have a FontBuffer generated.
  auto ret = FindBuffer(parameters);
  if (ret != nullptr) {
    return ret;
  }

  // Otherwise, create new FontBuffer.

  // Create FontBuffer with derived string length.
  std::unique_ptr<FontBuffer> buffer(
      new FontBuffer(length, parameters.get_caret_info_flag()));

  // Set initial attribute.
  buffer->ClearTemporaryBuffer();
  buffer->SetAttribute(FontBufferAttributes());
  line_width_ = 0;

  if (!FillBuffer(text, length, parameters, buffer.get(), text_pos)) {
    return nullptr;
  }

  // Initialize reference counter.
  buffer->set_ref_count(1);

  // Set current pass.
  if (current_pass_ != kRenderPass) {
    buffer->set_pass(current_pass_);
  }

  // Verify the buffer.
  assert(buffer->Verify());

  // Insert the created entry to the hash map.
  auto insert = map_buffers_.insert(
      std::pair<FontBufferParameters, std::unique_ptr<FontBuffer>>(
          parameters, std::move(buffer)));

  // Set up a back reference from the buffer to the map.
  insert.first->second->set_iterator(insert.first);
  return insert.first->second.get();
}

FontBuffer *FontManager::FillBuffer(const char *text, uint32_t length,
                                    const FontBufferParameters &parameters,
                                    FontBuffer *buffer,
                                    mathfu::vec2 *pos_offset) {
  auto size = parameters.get_size();
  auto multi_line = parameters.get_multi_line_setting();
  auto caret_info = parameters.get_caret_info_flag();
  auto ysize = static_cast<int32_t>(parameters.get_font_size());
  auto converted_ysize = ConvertSize(ysize);
  float scale = ysize / static_cast<float>(converted_ysize);

  // Update text metrices.
  SetLineHeightScale(parameters.get_line_height_scale());
  SetKerningScale(parameters.get_kerning_scale());

  // Set freetype settings.
  current_font_->SetPixelSize(converted_ysize);

  // Retrieve word breaking information using libunibreak.
  wordbreak_info_.resize(length);
  if (length) {
    set_linebreaks_utf8(reinterpret_cast<const utf8_t *>(text), length,
                        language_.c_str(), &wordbreak_info_[0]);
  }
  if (current_font_->IsComplexFont()) {
    // Analyze the text and set up an array of font face indices.
    auto font = reinterpret_cast<HbComplexFont *>(current_font_);
    if (font->AnalyzeFontFaceRun(text, length, &fontface_index_) > 1) {
      // If we need to switch faces for the text, take a multi line path.
      multi_line = true;
    }
  } else {
    fontface_index_.clear();
  }
  WordEnumerator word_enum(wordbreak_info_, fontface_index_, multi_line);

  // Initialize font metrics parameters.
  int32_t base_line = current_font_->GetBaseLine(ysize);
  FontMetrics initial_metrics(base_line, 0, base_line, base_line - ysize, 0);

  // Set up positions.
  float pos_start = 0;
  if (layout_direction_ == kTextLayoutDirectionRTL) {
    // In RTL layout, the glyph position start from right.
    pos_start = static_cast<float>(size.x());
  }
  mathfu::vec2 pos = vec2(pos_start, 0);
  if (pos_offset != nullptr) {
    pos += *pos_offset;
  }

  int32_t max_line_width = parameters.get_line_length();
  int32_t total_height = ysize;
  bool lastline_must_break = false;
  bool first_character = true;
  auto line_height = ysize * line_height_scale_;

  if (buffer->get_size().y()) {
    // Seems like the buffer is already initialized and we are appending to the
    // buffer. Retrieve information from the buffer.
    max_line_width = buffer->get_size().x() * kFreeTypeUnit;
    total_height = buffer->get_size().y();
    initial_metrics = buffer->metrics();
    base_line = initial_metrics.base_line();
  }

  // Find words and layout them.
  while (word_enum.Advance()) {
    // Set font face index for current word.
    current_font_->SetCurrentFontIndex(word_enum.GetCurrentFaceIndex());

    auto max_width = size.x() * kFreeTypeUnit;
    if (!multi_line) {
      // Single line text.
      // In this mode, it layouts all string into single line.
      max_line_width = static_cast<int32_t>(LayoutText(text, length) * scale) +
                       buffer->get_size().x() * kFreeTypeUnit;

      if (size.x() && max_line_width > max_width && !caret_info) {
        // The text size exceeds given size.
        // Rewind the buffers and add an ellipsis if it's speficied.
        if (!AppendEllipsis(word_enum, parameters, base_line, buffer, &pos,
                            &initial_metrics)) {
          return nullptr;
        }
      }

      if (layout_direction_ == kTextLayoutDirectionRTL && size.x() == 0) {
        pos.x() = static_cast<float>(max_line_width / kFreeTypeUnit);
      }
    } else {
      // Multi line text.
      // In this mode, it layouts every single word in the order of the text and
      // performs a line break if either current word exceeds the max line
      // width or indicated a line break must happen due to a line break
      // character etc.

      auto rewind = 0;
      auto word_width = static_cast<int32_t>(
          LayoutText(text + word_enum.GetCurrentWordIndex(),
                     word_enum.GetCurrentWordLength(), max_width, line_width_,
                     &rewind) *
          scale);
      if (rewind) {
        // Force a linebreak and rewind some characters for a next line.
        word_enum.Rewind(rewind);
      }

      if (lastline_must_break ||
          ((line_width_ + word_width) / kFreeTypeUnit > size.x() && size.x())) {
        auto new_pos = vec2(pos_start, pos.y() + line_height);
        total_height += static_cast<int32_t>(line_height);
        first_character = lastline_must_break;
        if (size.y() && total_height > size.y() && !caret_info) {
          // The text size exceeds given size.
          // Rewind the buffers and add an ellipsis if it's speficied.
          if (!AppendEllipsis(word_enum, parameters, base_line, buffer, &pos,
                              &initial_metrics)) {
            return nullptr;
          }
          // Update alignment after an ellipsis is appended.
          buffer->UpdateLine(parameters, layout_direction_,
                             lastline_must_break);
          hb_buffer_clear_contents(harfbuzz_buf_);
          break;
        }
        // Line break.
        buffer->UpdateLine(parameters, layout_direction_, lastline_must_break);
        pos = new_pos;

        if (word_width > max_width) {
          std::string s = std::string(&text[word_enum.GetCurrentWordIndex()],
                                      word_enum.GetCurrentWordLength());
          LogInfo(
              "A single word '%s' exceeded the given line width setting.\n"
              "Currently multiline label doesn't support a hyphenation",
              s.c_str());
        }
        // Reset the line width.
        line_width_ = word_width;
      } else {
        line_width_ += word_width;
      }
      // In case of the layout is left/center aligned, max line width is
      // adjusted based on layout results.
      max_line_width = std::max(max_line_width, line_width_);
      lastline_must_break = word_enum.CurrentWordMustBreak();
    }

    // Update the first caret position.
    if (caret_info && first_character) {
      buffer->AddCaretPosition(pos + vec2(0, base_line * scale));
      first_character = false;
    }

    // Add string information to the buffer.
    if (!UpdateBuffer(word_enum, parameters, base_line, lastline_must_break,
                      buffer, &pos, &initial_metrics)) {
      return nullptr;
    }

    // Add word boundary information.
    buffer->AddWordBoundary(parameters);

    // Cleanup buffer contents.
    hb_buffer_clear_contents(harfbuzz_buf_);
  }

  // Add the last caret.
  if (caret_info) {
    buffer->AddCaretPosition(pos + vec2(0, base_line * scale));
  }

  // Update the last line.
  if (appending_buffer_ == false) {
    buffer->UpdateLine(parameters, layout_direction_, true);
    buffer->ClearTemporaryBuffer();
  }

  // Set buffer revision using glyph cache revision.
  buffer->set_revision(glyph_cache_->get_revision());

  // Setup size.
  buffer->set_size(vec2i(max_line_width / kFreeTypeUnit, total_height));

  // Setup font metrics.
  buffer->set_metrics(initial_metrics);

  // Return the position.
  if (pos_offset != nullptr) {
    *pos_offset = pos;
  }

  return buffer;
}

void FontManager::ReleaseBuffer(FontBuffer *buffer) {
  // Acquire cache mutex.
  fplutil::MutexLock lock(*cache_mutex_);

  assert(buffer->get_ref_count() >= 1);
  buffer->set_ref_count(buffer->get_ref_count() - 1);
  if (!buffer->get_ref_count()) {

    // Clear references in the buffer.
    buffer->ReleaseCacheRowReference();

    // Remove an instance of the buffer.
    map_buffers_.erase(buffer->get_iterator());
  }
}

bool FontManager::UpdateBuffer(const WordEnumerator &word_enum,
                               const FontBufferParameters &parameters,
                               int32_t base_line, bool lastline_must_break,
                               FontBuffer *buffer, mathfu::vec2 *pos,
                               FontMetrics *metrics) {
  auto ysize = static_cast<int32_t>(parameters.get_font_size());
  auto converted_ysize = ConvertSize(ysize);
  float scale = ysize / static_cast<float>(converted_ysize);

  // Retrieve layout info.
  uint32_t glyph_count;
  auto glyph_info = hb_buffer_get_glyph_infos(harfbuzz_buf_, &glyph_count);
  auto glyph_pos = hb_buffer_get_glyph_positions(harfbuzz_buf_, &glyph_count);

  auto idx = 0;
  auto idx_advance = 1;
  if (layout_direction_ == kTextLayoutDirectionRTL) {
    idx = glyph_count - 1;
    idx_advance = -1;
  }

  for (size_t i = 0; i < glyph_count; ++i, idx += idx_advance) {
    auto code_point = glyph_info[idx].codepoint;
    if (!code_point) {
      continue;
    }
    auto cache = GetCachedEntry(code_point, converted_ysize,
                                parameters.get_glyph_flags());
    if (cache == nullptr) {
      // Cleanup buffer contents.
      hb_buffer_clear_contents(harfbuzz_buf_);
      return false;
    }

    auto pos_advance =
        mathfu::vec2(
            static_cast<float>(glyph_pos[idx].x_advance) * kerning_scale_,
            static_cast<float>(-glyph_pos[idx].y_advance)) *
        scale / static_cast<float>(kFreeTypeUnit);
    // Advance positions before rendering in RTL.
    if (layout_direction_ == kTextLayoutDirectionRTL) {
      *pos -= pos_advance;
    }

    // Register vertices only when the glyph has a size.
    if (cache->get_size().x() && cache->get_size().y()) {
      // Add the code point to the buffer. This information is used when
      // re-fetching UV information when the texture atlas is updated.
      buffer->AddCodepoint(code_point);

      // Calculate internal/external leading value and expand a buffer if
      // necessary.
      FontMetrics new_metrics;
      if (UpdateMetrics(cache->get_offset().y(), cache->get_size().y(),
                        *metrics, &new_metrics)) {
        *metrics = new_metrics;
      }

      // Expand buffer if necessary.
      auto buffer_idx = buffer->GetBufferIndex(cache->get_pos().z());
      buffer->AddIndices(buffer_idx, buffer->get_glyph_count());

      // Construct intermediate vertices array.
      // The vertices array is update in the render pass with correct
      // glyph size & glyph cache entry information.

      // Update vertices and UVs
      buffer->AddVertices(*pos, base_line, scale, *cache);

      // Update underline information if necessary.
      if (buffer->get_slices().at(buffer_idx).get_underline()) {
        buffer->UpdateUnderline(
            buffer_idx, (buffer->get_vertices().size() - 1) / kVerticesPerGlyph,
            current_font_->GetUnderline(converted_ysize) + vec2i(pos->y(), 0));
      }

      // Update references if the buffer is ref counting buffer.
      if (parameters.get_ref_count_flag()) {
        auto row = cache->get_row();
        row->AddRef(buffer);
        buffer->AddCacheRowReference(&*row);
      }
    }

    // Advance positions after rendering in LTR.
    if (layout_direction_ == kTextLayoutDirectionLTR) {
      *pos += pos_advance;
    }

    // Update caret information if it has been requested.
    bool end_of_line = lastline_must_break == true && i == glyph_count - 1;
    if (parameters.get_caret_info_flag() && end_of_line == false) {
      // Is the current glyph a ligature?
      // We are not using hb_ot_layout_get_ligature_carets() as the API barely
      // work with existing fonts.
      // https://bugs.freedesktop.org/show_bug.cgi?id=90962 tracks a request
      // for the issue.
      auto carets = GetCaretPosCount(word_enum, glyph_info,
                                     static_cast<int32_t>(glyph_count),
                                     static_cast<int32_t>(idx));

      auto scaled_offset = cache->get_offset().x() * scale;
      float scaled_base_line = base_line * scale;
      // Add caret points
      for (auto caret = 1; caret <= carets; ++caret) {
        buffer->AddCaretPosition(
            *pos + vec2(idx_advance * (scaled_offset - pos_advance.x() +
                                       caret * pos_advance.x() / carets),
                        scaled_base_line));
      }
    }
  }
  return true;
}

bool FontManager::AppendEllipsis(const WordEnumerator &word_enum,
                                 const FontBufferParameters &parameters,
                                 int32_t base_line, FontBuffer *buffer,
                                 mathfu::vec2 *pos, FontMetrics *metrics) {
  buffer->has_ellipsis_ = true;

  if (!ellipsis_.length()) {
    return true;
  }

  auto max_width = parameters.get_size().x() * kFreeTypeUnit;

  // Dump current string to the buffer.
  if (!UpdateBuffer(word_enum, parameters, base_line, false, buffer, pos,
                    metrics)) {
    return false;
  }

  // Calculate ellipsis string information.
  hb_buffer_clear_contents(harfbuzz_buf_);
  auto ellipsis_width = LayoutText(ellipsis_.c_str(), ellipsis_.length());
  if (ellipsis_width > max_width) {
    LogInfo("The ellipsis string width exceeded the given line width.\n");
    ellipsis_width = max_width;
  }

  // Remove some glyph entries from the buffer to have a room for the
  // ellipsis string.
  RemoveEntries(parameters, ellipsis_width, buffer, pos);

  // Add ellipsis string to the buffer.
  if (!UpdateBuffer(word_enum, parameters, base_line, false, buffer, pos,
                    metrics)) {
    return false;
  }

  // Cleanup buffer contents.
  hb_buffer_clear_contents(harfbuzz_buf_);

  return true;
}

void FontManager::RemoveEntries(const FontBufferParameters &parameters,
                                uint32_t required_width, FontBuffer *buffer,
                                mathfu::vec2 *pos) {
  auto max_width = parameters.get_size().x();

  // Determine how many letters to remove.
  const int32_t kLastElementIndex = -2;
  auto &vertices = buffer->get_vertices();
  auto vert_index = vertices.size() - 1;
  while (max_width - vertices.at(vert_index).position_.data[0] <
             required_width / kFreeTypeUnit &&
         vert_index >= kVerticesPerGlyph) {
    vert_index -= kVerticesPerGlyph;
  }

  auto entries_to_remove =
      static_cast<int32_t>(vertices.size() - 1 - vert_index) /
      kVerticesPerGlyph;
  auto removing_index =
      static_cast<int32_t>(vertices.size()) + kLastElementIndex;
  auto latest_attribute = buffer->attribute_history_.back();

  assert(entries_to_remove >= 0 && removing_index >= 0);
  while (entries_to_remove--) {
    // Remove indices.
    auto buffer_idx = latest_attribute->second;
    auto &indices = buffer->get_indices(buffer_idx);
    if (indices.back() == removing_index) {
      for (size_t j = 0; j < kIndicesPerGlyph; ++j) {
        indices.pop_back();
      }
      removing_index -= kVerticesPerGlyph;
    } else {
      // There is no indices to remove in the index buffer.
      // Switch to prior buffer.
      buffer->attribute_history_.pop_back();
      latest_attribute = buffer->attribute_history_.back();
      entries_to_remove++;
      continue;
    }

    // Remove codepoint.
    buffer->code_points_.pop_back();

    // Keep the x position of removed glyph and use it for a start of the
    // ellipsis string.
    pos->x() = (buffer->vertices_.end() - 3)->position_.data[0];

    // Remove vertices.
    for (size_t j = 0; j < kVerticesPerGlyph; ++j) {
      buffer->vertices_.pop_back();
    }
  }
}

int32_t FontManager::GetCaretPosCount(const WordEnumerator &word_enum,
                                      const hb_glyph_info_t *glyph_info,
                                      int32_t glyph_count, int32_t index) {
  // Retrieve a byte range for the glyph in the wordbreak buffer from harfbuzz
  // buffer.
  auto byte_index = glyph_info[index].cluster;
  auto byte_size = 0;
  auto direction = layout_direction_ == kTextLayoutDirectionLTR ? 1 : -1;

  if (index >= -direction && index < glyph_count - direction) {
    // Has next word. Calculate a difference between them.
    byte_size = glyph_info[index + direction].cluster - byte_index;
  } else {
    // Up until end of the buffer.
    byte_size = static_cast<int>(word_enum.GetCurrentWordLength() - byte_index);
  }

  // Count the number of characters in the given range in the wordbreak bufer.
  auto num_characters = 0;
  for (auto i = 0; i < byte_size; ++i) {
    if (word_enum.GetBuffer()->at(word_enum.GetCurrentWordIndex() + byte_index +
                                  i) != LINEBREAK_INSIDEACHAR) {
      num_characters++;
    }
  }
  return num_characters;
}

FontBuffer *FontManager::UpdateUV(int32_t ysize, GlyphFlags flags,
                                  FontBuffer *buffer) {
  if (GetFontBufferStatus(*buffer) == kFontBufferStatusNeedReconstruct) {
    // Cache revision has been updated.
    // Some referencing glyph cache entries might have been evicted.
    // So we need to check glyph cache entries again while we can still use
    // layout information.

    // Set freetype settings.
    current_font_->SetPixelSize(ysize);
    buffer->ClearTemporaryBuffer();

    // Keep original buffer.
    std::vector<std::vector<uint16_t>> original_indices =
        std::move(buffer->indices_);
    std::vector<FontBufferAttributes> original_slices =
        std::move(buffer->slices_);

    // TODO: Current code wouldn't work with a AtributedFontBuffer as
    // it would potentically be able to have multiple font face.
    // Consider having fontID in FontBuffer attribute and respect it.
    auto &code_points = buffer->get_code_points();
    for (size_t j = 0; j < original_indices.size(); ++j) {
      auto indices = original_indices[j];
      for (size_t i = 0; i < indices.size(); i += kIndicesPerGlyph) {
        auto index = indices[i] / kVerticesPerGlyph;
        auto code_point = code_points.at(index);
        auto cache = GetCachedEntry(code_point, ysize, flags);
        if (cache == nullptr) {
          return nullptr;
        }

        // Reconstruct indices.
        auto attr = original_slices[j];
        attr.slice_index_ = kIndexInvalid;
        buffer->SetAttribute(attr);

        // Expand buffer if necessary.
        auto buffer_idx = buffer->GetBufferIndex(cache->get_pos().z());
        buffer->AddIndices(buffer_idx, index);

        // Update UV.
        buffer->UpdateUV(static_cast<int32_t>(index), cache->get_uv());
      }
    }
    // Update revision.
    buffer->set_revision(glyph_cache_->get_revision());
    buffer->ClearTemporaryBuffer();
  }
  return buffer;
}

FontTexture *FontManager::GetTexture(const char *text, uint32_t length,
                                     float original_ysize) {
  // Round up y size if the size selector is set.
  int32_t ysize = ConvertSize(static_cast<int32_t>(original_ysize));

  // Note: In the API it uses HbFont's fontID (would include multiple fonts)
  // rather than a fontID for single font file. A texture can have multiple font
  // faces so that we need a fontID that represents all fonts used.
  auto parameter =
      FontBufferParameters(current_font_->GetFontId(), flatui::HashId(text),
                           static_cast<float>(ysize), mathfu::kZeros2i,
                           kTextAlignmentLeft, kGlyphFlagsNone, false, false,
                           layout_direction_ == kTextLayoutDirectionRTL);

  // Check cache if we already have a texture.
  auto it = map_textures_.find(parameter);
  if (it != map_textures_.end()) {
    return it->second.get();
  }

  // Otherwise, create new texture.

  // Set font size.
  current_font_->SetPixelSize(ysize);

  // Layout text.
  auto string_width = LayoutText(text, length) / kFreeTypeUnit;

  // Retrieve layout info.
  uint32_t glyph_count;
  hb_glyph_info_t *glyph_info =
      hb_buffer_get_glyph_infos(harfbuzz_buf_, &glyph_count);
  hb_glyph_position_t *glyph_pos =
      hb_buffer_get_glyph_positions(harfbuzz_buf_, &glyph_count);

  // Calculate texture size. The texture may be expanded later depending on
  // glyph sizes.
  int32_t width = mathfu::RoundUpToPowerOf2(string_width);
  int32_t height = mathfu::RoundUpToPowerOf2(ysize);

  // Initialize font metrics parameters.
  int32_t base_line = current_font_->GetBaseLine(ysize);
  FontMetrics initial_metrics(base_line, 0, base_line, base_line - ysize, 0);

  // rasterized image format in FreeType is 8 bit gray scale format.
  std::unique_ptr<uint8_t[]> image(new uint8_t[width * height]);
  memset(image.get(), 0, width * height);

  // TODO: make padding values configurable.
  float kGlyphPadding = 0.0f;
  mathfu::vec2 pos(kGlyphPadding, kGlyphPadding);

  for (size_t i = 0; i < glyph_count; ++i) {
    auto code_point = glyph_info[i].codepoint;
    if (!code_point) continue;

    auto &face = current_font_->GetFaceData();
    FT_GlyphSlot glyph = face.get_face()->glyph;
    FT_Error err = FT_Load_Glyph(face.get_face(), code_point, FT_LOAD_RENDER);

    // Load glyph using harfbuzz layout information.
    // Note that harfbuzz takes care of ligatures.
    if (err) {
      // Error. This could happen typically the loaded font does not support
      // particular glyph.
      LogInfo("Can't load glyph '%c' FT_Error:%d\n", text[i], err);
      return nullptr;
    }

    // Calculate internal/external leading value and expand a buffer if
    // necessary.
    FontMetrics new_metrics;
    if (UpdateMetrics(glyph->bitmap_top, glyph->bitmap.rows, initial_metrics,
                      &new_metrics)) {
      if (new_metrics.total() != initial_metrics.total()) {
        // Expand buffer and update height if necessary.
        if (ExpandBuffer(width, height, initial_metrics, new_metrics, &image)) {
          height = mathfu::RoundUpToPowerOf2(new_metrics.total());
        }
      }
      initial_metrics = new_metrics;
    }

    if (i == 0 && glyph->bitmap_left < 0) {
      // Slightly shift all text to right.
      pos.x() = static_cast<float>(-glyph->bitmap_left);
    }

    // Copy the texture
    uint32_t y_offset = initial_metrics.base_line() - glyph->bitmap_top;
    for (uint32_t y = 0; y < glyph->bitmap.rows; ++y) {
      memcpy(&image[(static_cast<size_t>(pos.y()) + y + y_offset) * width +
                    static_cast<size_t>(pos.x()) + glyph->bitmap_left],
             &glyph->bitmap.buffer[y * glyph->bitmap.pitch],
             glyph->bitmap.width);
    }

    // Advance positions.
    pos += mathfu::vec2(static_cast<float>(glyph_pos[i].x_advance),
                        static_cast<float>(-glyph_pos[i].y_advance)) /
           static_cast<float>(kFreeTypeUnit);
  }

  // Create new texture.
  FontTexture *tex = new FontTexture();
  tex->LoadFromMemory(image.get(), vec2i(width, height),
                      fplbase::kFormatLuminance);

  // Setup font metrics.
  tex->set_metrics(initial_metrics);

  // Cleanup buffer contents.
  hb_buffer_clear_contents(harfbuzz_buf_);

  // Put to the dic.
  map_textures_[parameter].reset(tex);

  return tex;
}

bool FontManager::ExpandBuffer(int32_t width, int32_t height,
                               const FontMetrics &original_metrics,
                               const FontMetrics &new_metrics,
                               std::unique_ptr<uint8_t[]> *image) {
  // Returns immediately if the size has not been changed.
  if (original_metrics.total() == new_metrics.total()) {
    return false;
  }

  int32_t new_height = mathfu::RoundUpToPowerOf2(new_metrics.total());
  int32_t internal_leading_change =
      new_metrics.internal_leading() - original_metrics.internal_leading();
  // Internal leading tracks max value of it in rendering glyphs, so we can
  // assume internal_leading_change >= 0.
  assert(internal_leading_change >= 0);

  if (height != new_height) {
    // Reallocate buffer.
    std::unique_ptr<uint8_t[]> old_image = std::move(*image);
    image->reset(new uint8_t[width * new_height]);

    // Copy existing contents.
    memcpy(&(*image)[width * internal_leading_change], old_image.get(),
           width * original_metrics.total());

    // Clear top.
    memset(image->get(), 0, width * internal_leading_change);

    // Clear bottom.
    int32_t clear_offset = original_metrics.total() + internal_leading_change;
    memset(&(*image)[width * clear_offset], 0,
           width * (new_height - clear_offset));
    return true;
  } else {
    if (internal_leading_change > 0) {
      // Move existing contents down and make a space for additional internal
      // leading.
      memcpy(&(*image)[width * internal_leading_change], image->get(),
             width * original_metrics.total());

      // Clear the top
      memset(image->get(), 0, width * internal_leading_change);
    }
    // Bottom doesn't have to be cleared since the part is already clean.
  }

  return false;
}

bool FontManager::Open(const FontFamily &family) {
  const char *font_name = family.get_name().c_str();
  auto it = map_faces_.find(font_name);
  if (it != map_faces_.end()) {
    // The font has been already opened.
    LogInfo("Specified font '%s' is already opened.", font_name);
    return true;
  }

  // Create FaceData and insert the created entry to the hash map.
  auto insert =
      map_faces_.insert(std::pair<std::string, std::unique_ptr<FaceData>>(
          font_name, std::unique_ptr<FaceData>(new FaceData())));
  auto face = insert.first->second.get();

#if defined(FLATUI_SYSTEM_FONT)
  if (!strcmp(font_name, flatui::kSystemFont)) {
    // Load system font.
    face->set_font_id(kSystemFontId);
    return OpenSystemFont();
  }
#endif

  // Initialize the face.
  if (!face->Open(*ft_, family)) {
    map_faces_.erase(insert.first);
    return false;
  }

  // Set first opened font as a default font.
  if (!face_initialized_) {
    if (!SelectFont(font_name)) {
      Close(font_name);
      return false;
    }
  }
  face_initialized_ = true;
  return true;
}

bool FontManager::Close(const FontFamily &family) {
  auto it = map_faces_.find(family.get_name());
  if (it == map_faces_.end()) {
    return false;
  }

#if defined(FLATUI_SYSTEM_FONT)
  if (it->second->get_font_id() == kSystemFontId) {
    // Close the system font.
    CloseSystemFont();
  }
#endif  // FLATUI_SYSTEM_FONT

  // Acquire cache mutex.
  fplutil::MutexLock lock(*cache_mutex_);

  // Clean up face instance data.
  HbFont::Close(*it->second, &font_cache_);
  it->second->Close();

  // Flush the texture cache.
  map_textures_.clear();
  map_buffers_.clear();

  map_faces_.erase(it);

  if (!map_faces_.size()) {
    face_initialized_ = false;
  }

  return true;
}

bool FontManager::Open(const char *font_name) {
  FontFamily family(font_name);
  return Open(family);
}

bool FontManager::Close(const char *font_name) {
  FontFamily family(font_name);
  return Close(family);
}

bool FontManager::SelectFont(const char *font_name) {
  FontFamily family(font_name);
  auto it = map_faces_.find(family.get_name());
  if (it == map_faces_.end()) {
    LogError("SelectFont error: '%s'", font_name);
    return false;
  }

#if defined(FLATUI_SYSTEM_FONT)
  if (it->second->get_font_id() == kSystemFontId) {
    // Select the system font.
    return SelectFont(&font_name, 1);
  }
#endif  // FLATUI_SYSTEM_FONT

  current_font_ = HbFont::Open(*it->second.get(), &font_cache_);
  return current_font_ != nullptr;
}

bool FontManager::SelectFont(const FontFamily *font_families, int32_t count) {
  std::vector<const char *> vec_name;
  for (auto i = 0; i < count; ++i) {
    vec_name.push_back(font_families[i].get_name().c_str());
  }
  return SelectFont(&vec_name[0], count);
}

bool FontManager::SelectFont(const char *font_names[], int32_t count) {
#if defined(FLATUI_SYSTEM_FONT)
  if (count == 1 && strcmp(font_names[0], kSystemFont)) {
    return SelectFont(font_names[0]);
  }
#endif
  // Normalize the font names and create hash.
  auto id = kInitialHashValue;
  for (auto i = 0; i < count; ++i) {
    FontFamily family = FontFamily(font_names[i]);
    id = HashId(family.get_name().c_str(), id);
  }
  current_font_ = HbFont::Open(id, &font_cache_);

  if (current_font_ == nullptr) {
    std::vector<FaceData *> v;
    for (auto i = 0; i < count; ++i) {
#if defined(FLATUI_SYSTEM_FONT)
      if (!strcmp(font_names[i], kSystemFont)) {
        // Select the system font.
        if (i != count - 1) {
          LogInfo(
              "SelectFont::kSystemFont would be in the last element of the "
              "font list.");
        }
        // Add the system fonts to the list.
        auto it = system_fallback_list_.begin();
        auto end = system_fallback_list_.end();
        while (it != end) {
          auto font = map_faces_.find(it->get_name().c_str());
          if (font == map_faces_.end()) {
            LogError("SelectFont error: '%s'", it->get_name().c_str());
            return false;
          }
          v.push_back(font->second.get());
          it++;
        }
      } else {
#endif
        FontFamily family = FontFamily(font_names[i]);
        auto it = map_faces_.find(family.get_name());
        if (it == map_faces_.end()) {
          LogError("SelectFont error: '%s'", font_names[i]);
          return false;
        }
        v.push_back(it->second.get());
#if defined(FLATUI_SYSTEM_FONT)
      }
#endif
    }
    current_font_ = HbComplexFont::Open(id, &v, &font_cache_);
  }
  return current_font_ != nullptr;
}

void FontManager::StartLayoutPass() {
  // Reset pass.
  current_pass_ = 0;
}

bool FontManager::UpdatePass(bool start_subpass) {
  // Guard glyph cache buffer access. Do nothing when the lock failed.
  fplutil::MutexTryLock lock;
  if (!lock.Try(*cache_mutex_)) {
    return false;
  }

  // Increment a cycle counter in glyph cache.
  glyph_cache_->Update();

  // Resolve glyph cache's dirty rects.
  if (glyph_cache_->get_dirty_state() && current_pass_ <= 0) {
    glyph_cache_->ResolveDirtyRect();
    // Cache texture is updated. Update counters as well.
    current_atlas_revision_ = glyph_cache_->get_revision();
    atlas_last_flush_revision_ = glyph_cache_->get_last_flush_revision();
  }

  if (start_subpass) {
    if (current_pass_ > 0) {
      LogInfo(
          "Multiple subpasses in one rendering pass is not supported. "
          "When this happens, increase the glyph cache size not to "
          "flush the atlas texture multiple times in one rendering pass.");
    }
    glyph_cache_->Flush();
    current_pass_++;
  } else {
    // Reset pass.
    current_pass_ = kRenderPass;
  }
  return true;
}

int32_t FontManager::LayoutText(const char *text, size_t length,
                                int32_t max_width, int32_t current_width,
                                int32_t *rewind) {
  // Update language settings.
  SetLanguageSettings();

  // Layout the text.
  hb_buffer_add_utf8(harfbuzz_buf_, text, static_cast<uint32_t>(length), 0,
                     static_cast<int32_t>(length));
  hb_shape(current_font_->GetHbFont(), harfbuzz_buf_, nullptr, 0);

  // Retrieve layout info.
  uint32_t glyph_count;
  hb_glyph_info_t *glyph_info =
      hb_buffer_get_glyph_infos(harfbuzz_buf_, &glyph_count);
  hb_glyph_position_t *glyph_pos =
      hb_buffer_get_glyph_positions(harfbuzz_buf_, &glyph_count);

  // Retrieve a width of the string.
  float string_width = 0.0f;
  for (uint32_t i = 0; i < glyph_count; ++i) {
    auto advance = static_cast<float>(glyph_pos[i].x_advance) * kerning_scale_;
    if (max_width && string_width + advance > max_width) {
      // If a single word exceeds the max width, the word is forced to
      // linebreak.

      // Find a string length that fits to an avaialble space AND the available
      // space can have at least one letter.
      auto available_space = max_width - current_width;
      while (string_width > available_space &&
             available_space >=
                 static_cast<float>(glyph_pos[0].x_advance) * kerning_scale_) {
        --i;
        string_width -=
            static_cast<float>(glyph_pos[i].x_advance) * kerning_scale_;
      }
      assert(i > 0);

      // Calculate # of characters to rewind.
      *rewind = static_cast<int32_t>(length - glyph_info[i].cluster);
      hb_buffer_set_length(harfbuzz_buf_, i);
      break;
    }
    string_width += advance;
  }
  return static_cast<uint32_t>(string_width);
}

bool FontManager::UpdateMetrics(int32_t top, int32_t height,
                                const FontMetrics &current_metrics,
                                FontMetrics *new_metrics) {
  // Calculate internal/external leading value and expand a buffer if
  // necessary.
  if (top > current_metrics.ascender() ||
      static_cast<int32_t>(top - height) < current_metrics.descender()) {
    *new_metrics = current_metrics;
    new_metrics->set_internal_leading(std::max(
        current_metrics.internal_leading(), top - current_metrics.ascender()));
    new_metrics->set_external_leading(std::min(
        current_metrics.external_leading(),
        static_cast<int32_t>(top - height - current_metrics.descender())));
    new_metrics->set_base_line(new_metrics->internal_leading() +
                               new_metrics->ascender());

    return true;
  }
  return false;
}

void FontManager::SetLocale(const char *locale) {
  if (locale_ == locale) {
    return;
  }
  // Retrieve the language in the locale string.
  std::string language = locale;
  language = language.substr(0, language.find("-"));
  // Set the linebreak language.
  if (IsLanguageSupported(language.c_str())) {
    language_ = language;
  } else {
    language_ = kDefaultLanguage;
  }

  // Set the script and the layout direction.
  auto layout_info = FindLocale(locale);
  if (layout_info == nullptr) {
    // Lookup with the language.
    layout_info = FindLocale(language.c_str());
  }
  if (layout_info != nullptr) {
    SetLayoutDirection(layout_info->direction);
    SetScript(layout_info->script);
  }
  locale_ = locale;

  // Retrieve harfbuzz's language info.
  hb_language_ =
      hb_language_from_string(locale, static_cast<int>(strlen(locale)));
}

void FontManager::SetScript(const char *script) {
  uint32_t s = *reinterpret_cast<const uint32_t *>(script);
  script_ = static_cast<hb_script_t>(s >> 24 | (s & 0xff0000) >> 8 |
                                     (s & 0xff00) << 8 | s << 24);
}

void FontManager::SetLanguageSettings() {
  assert(harfbuzz_buf_);
  // Set harfbuzz settings.
  hb_buffer_set_direction(harfbuzz_buf_,
                          layout_direction_ == kTextLayoutDirectionRTL
                              ? HB_DIRECTION_RTL
                              : HB_DIRECTION_LTR);
  hb_buffer_set_script(harfbuzz_buf_, static_cast<hb_script_t>(script_));
  hb_buffer_set_language(harfbuzz_buf_, hb_language_);
}

const GlyphCacheEntry *FontManager::GetCachedEntry(uint32_t code_point,
                                                   uint32_t ysize,
                                                   GlyphFlags flags) {
  auto &face_data = current_font_->GetFaceData();
  GlyphKey key(face_data.get_font_id(), code_point, ysize, flags);
  auto cache = glyph_cache_->Find(key);

  if (cache == nullptr) {
    auto face = face_data.get_face();
    auto ft_flags = FT_LOAD_RENDER;
    if (FT_HAS_COLOR(face)) {
      ft_flags |= FT_LOAD_COLOR;
    }
    if (!FT_IS_SCALABLE(face)) {
      // Selecting first bitmap font for now.
      // TODO: Select optimal font size when available.
      FT_Select_Size(face, 0);
    }
    // Load glyph using harfbuzz layout information.
    // Note that harfbuzz takes care of ligatures.
    FT_Error err = FT_Load_Glyph(face, code_point, ft_flags);
    if (err) {
      // Error. This could happen typically the loaded font does not support
      // particular glyph.
      LogInfo("Can't load glyph %c FT_Error:%d\n", code_point, err);
      return nullptr;
    }

    // Store the glyph to cache.
    FT_GlyphSlot g = face->glyph;
    GlyphCacheEntry entry;
    entry.set_code_point(code_point);
    bool color_glyph = g->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA;

    // Does not support SDF for color glyphs.
    if (flags & (kGlyphFlagsOuterSDF | kGlyphFlagsInnerSDF) &&
        g->bitmap.width && g->bitmap.rows && !color_glyph) {
      // Adjust a glyph size and an offset with a padding.
      entry.set_offset(vec2i(g->bitmap_left - kGlyphCachePaddingSDF,
                             g->bitmap_top + kGlyphCachePaddingSDF));
      entry.set_size(vec2i(g->bitmap.width + kGlyphCachePaddingSDF * 2,
                           g->bitmap.rows + kGlyphCachePaddingSDF * 2));
      cache = glyph_cache_->Set(nullptr, key, entry);
      if (cache != nullptr) {
        // Generates SDF.
        auto buffer = glyph_cache_->get_monochrome_buffer();
        auto pos = cache->get_pos();
        auto stride = buffer->get_size().x();
        auto p = buffer->get(pos.z()) + pos.x() + pos.y() * stride;
        Grid<uint8_t> src(g->bitmap.buffer,
                          vec2i(g->bitmap.width, g->bitmap.rows),
                          kGlyphCachePaddingSDF, g->bitmap.width);
        Grid<uint8_t> dest(p, cache->get_size(), 0, stride);
        sdf_computer_.Compute(src, &dest, flags);
      }
    } else {
      if (color_glyph) {
        entry.set_color_glyph(true);

        // FreeType returns fixed sized bitmap for color glyphs.
        // Rescale bitmap here for better quality and performance.
        auto glyph_scale = static_cast<float>(ysize) / g->bitmap.rows;
        int32_t new_width = static_cast<int32_t>(g->bitmap.width * glyph_scale);
        int32_t new_height = static_cast<int32_t>(g->bitmap.rows * glyph_scale);
        auto out_buffer = malloc(new_width * new_height * sizeof(uint32_t));

        // TODO: Evaluate generating mipmap for a dirty rect and see
        // it would improve performance and produces acceptable quality.
        stbir_resize_uint8(g->bitmap.buffer, g->bitmap.width, g->bitmap.rows, 0,
                           static_cast<uint8_t *>(out_buffer), new_width,
                           new_height, 0, sizeof(uint32_t));
        const float kEmojiBaseLine = 0.85f;
        entry.set_offset(vec2i(
            vec2(g->bitmap_left * glyph_scale, new_height * kEmojiBaseLine)));
        entry.set_size(vec2i(new_width, new_height));
        cache = glyph_cache_->Set(out_buffer, key, entry);
        free(out_buffer);
      } else {
        entry.set_offset(vec2i(g->bitmap_left, g->bitmap_top));
        entry.set_size(vec2i(g->bitmap.width, g->bitmap.rows));
        cache = glyph_cache_->Set(g->bitmap.buffer, key, entry);
      }
    }

    if (cache == nullptr) {
      // Glyph cache need to be flushed.
      // Returning nullptr here for a retry.
      LogInfo("Glyph cache is full. Need to flush and re-create.\n");
      return nullptr;
    }
  }

  return cache;
}

int32_t FontManager::ConvertSize(int32_t original_ysize) {
  if (size_selector_ != nullptr) {
    return size_selector_(original_ysize);
  } else {
    return original_ysize;
  }
}

void FontManager::EnableColorGlyph() {
  // Pass the command to the glyph cache.
  fplutil::MutexLock lock(*cache_mutex_);
  glyph_cache_->EnableColorGlyph();
}

FontBufferStatus FontManager::GetFontBufferStatus(const FontBuffer &font_buffer)
    const {
  if (font_buffer.get_revision() <= atlas_last_flush_revision_) {
    return kFontBufferStatusNeedReconstruct;
  } else if (font_buffer.get_revision() > current_atlas_revision_ &&
             current_pass_ == kRenderPass) {
    return kFontBufferStatusNeedCacheUpdate;
  }
  return kFontBufferStatusReady;
}

void FontBuffer::SetAttribute(const FontBufferAttributes &attribute) {
  auto it = LookUpAttribute(attribute);

  if (attribute_history_.size() == 0 || it != attribute_history_.back()) {
    attribute_history_.push_back(it);
  }
}

FontBuffer::attribute_map_it FontBuffer::LookUpAttribute(
    const FontBufferAttributes &attribute) {
  if (attribute_map_.size()) {
    // Check if we already has an entry in the map.
    auto it = attribute_map_.find(attribute);
    if (it != attribute_map_.end()) {
      return it;
    }
  }
  auto ret = attribute_map_.insert(std::make_pair(attribute, kIndexInvalid));
  return ret.first;
}

int32_t FontBuffer::GetBufferIndex(int32_t slice) {
  // Check if we can use the latest attribute in the attribute stack.
  if (attribute_history_.back()->first.get_slice_index() == slice) {
    return attribute_history_.back()->second;
  }
  auto new_attr = attribute_history_.back()->first;
  // Remove temporary attribute.
  if (new_attr.get_slice_index() == kIndexInvalid) {
    attribute_history_.pop_back();
  }
  new_attr.set_slice_index(slice);
  auto it = LookUpAttribute(new_attr);
  if (it->second == kIndexInvalid) {
    // Resize index buffers.
    it->second = static_cast<int32_t>(slices_.size());
    slices_.push_back(it->first);
    indices_.resize(it->second + 1);
  }
  // Update the attribute stack.
  if (!attribute_history_.size() || it != attribute_history_.back()) {
    attribute_history_.push_back(it);
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
  mathfu::vec2i rounded_pos = mathfu::vec2i(pos);
  auto scaled_offset = mathfu::vec2(entry.get_offset()) * scale;
  auto scaled_size = mathfu::vec2(entry.get_size()) * scale;
  float scaled_base_line = base_line * scale;

  auto x = rounded_pos.x() + scaled_offset.x();
  auto y = rounded_pos.y() + scaled_base_line - scaled_offset.y();
  auto uv = entry.get_uv();
  vertices_.push_back(FontVertex(x, y, 0.0f, uv.x(), uv.y()));
  vertices_.push_back(FontVertex(x, y + scaled_size.y(), 0.0f, uv.x(), uv.w()));
  vertices_.push_back(FontVertex(x + scaled_size.x(), y, 0.0f, uv.z(), uv.y()));
  vertices_.push_back(FontVertex(x + scaled_size.x(), y + scaled_size.y(), 0.0f,
                                 uv.z(), uv.w()));
}

void FontBuffer::UpdateUV(int32_t index, const mathfu::vec4 &uv) {
  vertices_[index * 4].uv_ = uv.xy();
  vertices_[index * 4 + 1].uv_ = mathfu::vec2(uv.x(), uv.w());
  vertices_[index * 4 + 2].uv_ = mathfu::vec2(uv.z(), uv.y());
  vertices_[index * 4 + 3].uv_ = uv.zw();
}

void FontBuffer::AddCaretPosition(const mathfu::vec2 &pos) {
  mathfu::vec2i rounded_pos = mathfu::vec2i(pos);
  AddCaretPosition(rounded_pos.x(), rounded_pos.y());
}

void FontBuffer::AddCaretPosition(int32_t x, int32_t y) {
  assert(caret_positions_.capacity());
  caret_positions_.push_back(mathfu::vec2i(x, y));
}

void FontBuffer::AddWordBoundary(const FontBufferParameters &parameters) {
  if (parameters.get_text_alignment() & kTextAlignmentJustify) {
    // Keep the word boundary info for a later use for a justificaiton.
    word_boundary_.push_back(static_cast<uint32_t>(code_points_.size()));
    word_boundary_caret_.push_back(
        static_cast<uint32_t>(caret_positions_.size()));
  }
}

void FontBuffer::UpdateLine(const FontBufferParameters &parameters,
                            TextLayoutDirection layout_direction,
                            bool last_line) {
  // Update previous line layout if necessary.
  auto align = parameters.get_text_alignment() & ~kTextAlignmentJustify;
  auto justify = parameters.get_text_alignment() & kTextAlignmentJustify;
  if (last_line) {
    justify = false;
  }

  // Do nothing when the width of the text rect is not specified.
  if ((justify || align != kTextAlignmentLeft) && parameters.get_size().x()) {
    // Adjust glyph positions.
    auto offset = 0;  // Offset to add for each glyph position.
                      // When we justify a text, the offset is increased for
                      // each word boundary by boundary_offset_change.
    auto boundary_offset_change = 0;

    // Retrieve the line width from glyph's vertices.
    auto line_width = 0;
    if (vertices_.size()) {
      const int32_t kEndPosOffset = 2;
      auto it_start = vertices_.begin() +
                      line_start_indices_.back() * kVerticesPerCodePoint;
      auto it_end =
          vertices_.begin() + (code_points_.size() - 1) * kVerticesPerCodePoint;
      if (layout_direction == kTextLayoutDirectionLTR) {
        auto start_pos = it_start->position_.data[0];
        auto end_pos = (it_end + kEndPosOffset)->position_.data[0];
        line_width = end_pos - start_pos;
      } else if (layout_direction == kTextLayoutDirectionRTL) {
        auto start_pos = (it_start + kEndPosOffset)->position_.data[0];
        auto end_pos = it_end->position_.data[0];
        line_width = start_pos - end_pos;
      } else {
        // TTB layout is currently not supported.
        assert(0);
      }
    }

    if (justify && word_boundary_.size() > 1) {
      // With a justification, we add an offset for each word boundary.
      // For each word boundary (e.g. spaces), we stretch them slightly to align
      // both the left and right ends of each line of text.
      boundary_offset_change =
          static_cast<int32_t>((parameters.get_size().x() - line_width) /
                               (word_boundary_.size() - 1));
    } else {
      justify = false;
      offset = parameters.get_size().x() - line_width;
      if (align == kTextAlignmentCenter) {
        offset = offset / 2;  // Centering.
      }
    }

    // Flip the value if the text layout is in RTL.
    if (layout_direction == kTextLayoutDirectionRTL) {
      offset = -offset;
      boundary_offset_change = -boundary_offset_change;
    }
    // Keep original offset value.
    auto offset_caret = offset;

    // Update each glyph's position.
    auto boundary_index = 0;
    for (auto idx = line_start_indices_.back(); idx < code_points_.size();
         ++idx) {
      if (justify && idx >= word_boundary_[boundary_index]) {
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
      for (auto idx = line_start_caret_index_; idx < caret_positions_.size();
           ++idx) {
        if (justify && idx >= word_boundary_caret_[boundary_index]) {
          boundary_index++;
          offset_caret += boundary_offset_change;
        }
        caret_positions_[idx].x() += offset_caret;
      }
    }

    // Update underline information if necessary.
    if (attribute_history_.back()->first.get_underline()) {
      auto index = attribute_history_.back()->second;
      slices_[index]
          .WrapUnderline((get_vertices().size() - 1) / kVerticesPerGlyph);
    }
  }

  // Update current line information.
  line_start_indices_.push_back(static_cast<uint32_t>(code_points_.size()));
  line_start_caret_index_ = static_cast<uint32_t>(caret_positions_.size());
  word_boundary_.clear();
  word_boundary_caret_.clear();
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

  // Clamp to vertex bounds.
  const uint32_t start = std::max(0u, static_cast<uint32_t>(start_index));
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

  if (underline_info_.empty() || underline_info_.back().y_pos_ != y_pos) {
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

}  // namespace flatui
