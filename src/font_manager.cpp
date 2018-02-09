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
#include "fplbase/fpl_common.h"
#include "fplbase/utilities.h"

// libUnibreak header
#include <unibreakdef.h>
#include "linebreak.h"

// font_manager is now using fast version of the distance computer.
#define FONT_MANAGER_FAST_SDF_GENERATOR (1)
#ifdef FONT_MANAGER_FAST_SDF_GENERATOR
#include "flatui/internal/fast_antialias_distance_computer.h"
#else
#include "flatui/internal/antialias_distance_computer.h"
#endif

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

namespace {
  DistanceComputer<uint8_t>* CreateAntialiasDistanceComputer() {
#ifdef FONT_MANAGER_FAST_SDF_GENERATOR
    return new FastAntialiasDistanceComputer<uint8_t>();
#else
    return new AntialiasDistanceComputer<uint8_t>();
#endif
  }
}

// Constants.
static const FontBufferAttributes kHtmlLinkAttributes(true, 0x0000FFFF);

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
      if (!face_index_buffer_->empty()) {
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

DistanceComputer<uint8_t>*(*FontManager::DistanceComputerFactory)(void) =
  &CreateAntialiasDistanceComputer;

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

FontManager::~FontManager() {
  // Clear font faces.
  map_faces_.clear();
  delete cache_mutex_;

  // Destroy the harfbuzz buffer.
  hb_buffer_destroy(harfbuzz_buf_);
  harfbuzz_buf_ = nullptr;

  assert(ft_ != nullptr);
  FT_Done_FreeType(*ft_);
}

void FontManager::Initialize() {
  // Initialize variables.
  sdf_computer_ =
    std::unique_ptr<DistanceComputer<uint8_t>>(DistanceComputerFactory());
  face_initialized_ = false;
  current_atlas_revision_ = 0;
  atlas_last_flush_revision_ = kNeverFlushed;
  current_pass_ = 0;
  line_height_scale_ = kLineHeightDefault;
  kerning_scale_ = kLineHeightDefault;
  current_font_ = nullptr;
  version_ = &FontVersion();
  SetLocale(kDefaultLanguage);
  cache_mutex_ = new fplutil::Mutex(fplutil::Mutex::kModeNonRecursive);
  line_width_ = 0;
  ellipsis_mode_ = kEllipsisModeTruncateCharacter;

#ifdef __ANDROID__
  hyb_path_ = kAndroidDefaultHybPath;
#endif  //__ANDROID__

  std::unique_ptr<FT_Library> freetype_lib(new FT_Library);
  ft_ = std::move(freetype_lib);
  FT_Error err = FT_Init_FreeType(ft_.get());
  if (err) {
    // Error! Please fix me.
    LogError("Can't initialize freetype. FT_Error:%d\n", err);
    assert(0);
  }

  // Create a buffer for harfbuzz.
  harfbuzz_buf_ = hb_buffer_create();

  // Initialize libunibreak
  init_linebreak();
}

void FontManager::Terminate() {}

FontBuffer *FontManager::GetBuffer(const char *text, size_t length,
                                   const FontBufferParameters &parameter) {
  ErrorType buffer_error = kErrorTypeSuccess;
  auto buffer = CreateBuffer(text, static_cast<uint32_t>(length), parameter,
                             /* text_pos = */ nullptr, &buffer_error);
  if (buffer == nullptr && buffer_error == kErrorTypeCacheIsFull) {
    // Flush glyph cache & Upload a texture
    FlushAndUpdate();

    // Try to create buffer again.
    buffer = CreateBuffer(text, static_cast<uint32_t>(length), parameter);
    if (buffer == nullptr) {
      LogError(
          "The given text '%s' with size:%d does not fit a glyph cache. "
          "Try to increase a cache size or use GetTexture() API "
          "instead.\n",
          text, parameter.get_size().y);
    }
  }

  return buffer;
}

void FontManager::SetFontProperties(const HtmlSection &font_section,
                                    FontBufferParameters *param,
                                    FontBufferContext *ctx) {
  auto &face = font_section.face();
  if (face != "") {
    SelectFont(face);
  } else {
    current_font_ = ctx->original_font();
  }
  bool has_link = !font_section.link().empty();
  // Set the attributes for either link (underlined & blue),
  // or normal (with a color attribute).
  if (has_link) {
    ctx->SetAttribute(kHtmlLinkAttributes);
  } else {
    auto attribute = FontBufferAttributes(
        false, font_section.color() ? font_section.color() : kDefaultColor);
    ctx->SetAttribute(attribute);
  }

  auto size = font_section.size();
  if (size) {
    param->set_font_size(size);
  } else {
    // Restore original size.
    param->set_font_size(ctx->original_font_size());
  }
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
  std::vector<HtmlSection> html_sections;
  const bool parsed = ParseHtml(html, &html_sections);
  if (!parsed) {
    fplbase::LogError("Failed to parse HTML.");
    return nullptr;
  }

  // Otherwise create new buffer.
  FontBufferContext ctx;
  ctx.set_appending_buffer(true);
  ctx.set_original_font(current_font_);
  ctx.set_original_font_size(parameters.get_font_size());
  ctx.set_current_font_size(parameters.get_font_size());

  mathfu::vec2 pos = GetStartPosition(parameters);
  ErrorType buffer_error = kErrorTypeSuccess;
  FontBuffer *buffer = CreateBuffer("", 0, parameters, &pos, &buffer_error);
  if (buffer == nullptr) {
    fplbase::LogError("Failed to create buffer (%d).", buffer_error);
    return nullptr;
  }

  // Use non-const version of the parameter for a font size change.
  auto param = parameters;
  for (size_t i = 0; i < html_sections.size(); ++i) {
    auto &s = html_sections[i];

    // Get the glyph index before appending text.
    const int32_t start_glyph_index = buffer->get_glyph_count();

    SetFontProperties(s, &param, &ctx);

    // Append text as per usual.
    if (s.text().length()) {
      fplutil::MutexLock lock(*cache_mutex_);
      FillBuffer(s.text().c_str(), s.text().length(), param, buffer, &ctx,
                 &pos);
    }

    // Record link info.
    bool has_link = !s.link().empty();
    if (has_link) {
      buffer->links_.push_back(
          LinkInfo(s.link(), start_glyph_index, buffer->get_glyph_count()));
    }

    // If the buffer has an ellipsis, won't add text any more.
    if (buffer->HasEllipsis()) {
      break;
    }
  }

  ctx.set_lastline_must_break(true);
  buffer->UpdateLine(param, layout_direction_, &ctx);

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
    auto ret = UpdateUV(parameters.get_glyph_flags(), it->second.get());

    // Increment the reference count if the buffer is ref counting buffer.
    if (parameters.get_ref_count_flag()) {
      ret->set_ref_count(ret->get_ref_count() + 1);
    }
    return ret;
  }
  return nullptr;
}

mathfu::vec2 FontManager::GetStartPosition(
      const FontBufferParameters &parameters) const {
  float x_start = 0;
  if (layout_direction_ == kTextLayoutDirectionRTL) {
    // In RTL layout, the glyph position starts from right.
    x_start = static_cast<float>(parameters.get_size().x);
  }
  return mathfu::vec2(x_start, 0);
}

FontBuffer *FontManager::CreateBuffer(const char *text, uint32_t length,
                                      const FontBufferParameters &parameters,
                                      mathfu::vec2 *text_pos,
                                      ErrorType *error) {
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
  FontBufferContext ctx;
  ctx.SetAttribute(FontBufferAttributes());
  line_width_ = 0;

  if (!FillBuffer(text, length, parameters, buffer.get(), &ctx, text_pos,
                  error)) {
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
                                    FontBufferContext *context,
                                    mathfu::vec2 *text_pos, ErrorType *error) {
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
  auto buffer_length = length ? length + 1 : 0;
  wordbreak_info_.resize(buffer_length);
  if (length) {
    // We tweak the last byte of libunibreak's output rather than always to have
    // LINEBREAK_MUSTBREAK but can be either ALLOWBREAK or MUSTBREAK to work
    // with the appending FontBuffers feature.
    // libUnibreak won't access out of range of 'text' as it's expected 0
    // terminated string.
    set_linebreaks_utf8(reinterpret_cast<const utf8_t *>(text), buffer_length,
                        language_.c_str(), &wordbreak_info_[0]);
    // Dispose the last element and update the last element.
    wordbreak_info_.pop_back();
    if (wordbreak_info_.back() != LINEBREAK_MUSTBREAK) {
      wordbreak_info_.back() = LINEBREAK_ALLOWBREAK;
    }
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
  int32_t max_line_width = 0;
  // Height calculation needs to use ysize before conversion.
  float total_height = ysize;
  bool first_character = true;
  auto line_height = ysize * line_height_scale_;
  FontMetrics initial_metrics;
  int32_t base_line = context->original_base_line();
  if (!base_line) {
    // The context has not been set yet. Initialize metrics.
    base_line = current_font_->GetBaseLine(ysize);
    context->set_original_base_line(base_line);
    initial_metrics =
        FontMetrics(base_line, 0, base_line, base_line - ysize, 0);
  } else {
    // Appending FontBuffers, restore parameters from existing context and
    // buffer.
    initial_metrics = buffer->metrics();
    max_line_width = buffer->get_size().x * kFreeTypeUnit;
    total_height = static_cast<float>(buffer->get_size().y);

    if (context->current_font_size() != ysize) {
      // Adjust glyph positions in current line?
      auto offset = buffer->AdjustCurrentLine(parameters, context);
      if (text_pos != nullptr) {
        text_pos->y += offset;
      }
      context->set_current_font_size(ysize);
    }
  }

  // Set up positions.
  const mathfu::vec2 pos_start = GetStartPosition(parameters);
  mathfu::vec2 pos = pos_start;
  if (text_pos != nullptr) {
    pos = *text_pos;
  }

  // Find words and layout them.
  while (word_enum.Advance()) {
    // Set font face index for current word.
    current_font_->SetCurrentFaceIndex(word_enum.GetCurrentFaceIndex());
    bool layout_success = false;

    auto max_width = size.x * kFreeTypeUnit;
    if (!multi_line) {
      // Single line text.
      // In this mode, it layouts all string into single line.
      auto word_width = static_cast<int32_t>(LayoutText(text, length) * scale);
      max_line_width = word_width + buffer->get_size().x * kFreeTypeUnit;
      layout_success = word_width > 0;

      if (size.x && max_line_width > max_width && !caret_info) {
        // The text size exceeds given size.
        // Rewind the buffers and add an ellipsis if it's speficied.
        const ErrorType buffer_error =
            AppendEllipsis(word_enum, parameters, base_line, buffer, context,
                           &pos, &initial_metrics);
        if (buffer_error != kErrorTypeSuccess) {
          if (error) *error = buffer_error;
          return nullptr;
        }

        // Shrink the max_line_width after rewinding and adding ellipsis.
        if (ellipsis_.length()) {
          if (layout_direction_ == kTextLayoutDirectionRTL) {
            max_line_width = (pos_start.x - pos.x) * kFreeTypeUnit;
          } else {
            max_line_width = pos.x * kFreeTypeUnit;
          }
        }
      }

      if (layout_direction_ == kTextLayoutDirectionRTL && size.x == 0) {
        pos.x = static_cast<float>(max_line_width / kFreeTypeUnit);
      }
    } else {
      // Multi line text.
      // In this mode, it layouts every single word in the order of the text and
      // performs a line break if either current word exceeds the max line
      // width or indicated a line break must happen due to a line break
      // character etc.

      auto rewind = 0;
      auto last_line =
          size.y &&
          static_cast<int32_t>(total_height + line_height) > size.y;
      auto word_width = static_cast<int32_t>(
          LayoutText(text + word_enum.GetCurrentWordIndex(),
                     word_enum.GetCurrentWordLength(), max_width / scale,
                     line_width_ / scale, last_line,
                     parameters.get_enable_hyphenation_flag(), &rewind) *
          scale);
      layout_success = word_width > 0;
      if (rewind) {
        // Force a linebreak and rewind some characters for a next line.
        word_enum.Rewind(rewind);
      }

      if (context->lastline_must_break() ||
          ((line_width_ + word_width) > max_width && size.x) ||
          !layout_success) {
        auto new_pos = vec2(pos_start.x, pos.y + line_height);
        first_character = context->lastline_must_break();
        if (last_line && !caret_info) {
          if (context->lastline_must_break()) {
            // Previous line was a line break and also the last_line, so don't
            // print this text that was supposed to go on the last+1 line.
            hb_buffer_clear_contents(harfbuzz_buf_);
          }
          // Else the text width exceeds given width, so still print the text on
          // the same line and add an ellipsis if it's speficied.
          const ErrorType buffer_error =
              AppendEllipsis(word_enum, parameters, base_line, buffer, context,
                             &pos, &initial_metrics);
          if (buffer_error != kErrorTypeSuccess) {
            if (error) *error = buffer_error;
            return nullptr;
          }
          // Update alignment after an ellipsis is appended.
          context->set_lastline_must_break(true);
          buffer->UpdateLine(parameters, layout_direction_, context);
          hb_buffer_clear_contents(harfbuzz_buf_);
          break;
        }
        // Line break.
        total_height += line_height;
        buffer->UpdateLine(parameters, layout_direction_, context);
        pos = new_pos;

        if (word_width > max_width &&
            !parameters.get_enable_hyphenation_flag()) {
          std::string s = std::string(&text[word_enum.GetCurrentWordIndex()],
                                      word_enum.GetCurrentWordLength());
          LogInfo(
              "A single word '%s' exceeded the given line width setting.\n"
              "Try enabling a hyphenation support.",
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
      context->set_lastline_must_break(word_enum.CurrentWordMustBreak());
    }

    // Update the first caret position.
    if (caret_info && first_character) {
      buffer->AddCaretPosition(pos + vec2(0, base_line));
      first_character = false;
    }

    const ErrorType buffer_error =
        UpdateBuffer(word_enum, parameters, base_line, buffer, context, &pos,
                     &initial_metrics);
    if (buffer_error != kErrorTypeSuccess) {
      if (error) *error = buffer_error;
      return nullptr;
    }

    // Add word boundary information.
    buffer->AddWordBoundary(parameters, context);

    // Cleanup buffer contents.
    hb_buffer_clear_contents(harfbuzz_buf_);
  }

  // Add the last caret.
  if (caret_info) {
    buffer->AddCaretPosition(pos + vec2(0, base_line));
  }

  // Update the last line.
  if (context->appending_buffer() == false) {
    context->set_lastline_must_break(true);
    buffer->UpdateLine(parameters, layout_direction_, context);
  }

  // Set buffer revision using glyph cache revision.
  buffer->set_revision(glyph_cache_->get_revision());

  // Setup size.
  buffer->set_size(vec2i(max_line_width / kFreeTypeUnit, total_height));

  // Setup font metrics.
  buffer->set_metrics(initial_metrics);

  // Return the position.
  if (text_pos != nullptr) {
    *text_pos = pos;
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

void FontManager::RemapBuffers(bool flush_cache) {
  // Acquire cache mutex.
  fplutil::MutexLock lock(*cache_mutex_);

  if (flush_cache) {
    glyph_cache_->Flush();
    atlas_last_flush_revision_ = glyph_cache_->get_last_flush_revision();
  }

  auto it = map_buffers_.begin();
  auto end = map_buffers_.end();
  while (it != end) {
    UpdateUV(it->first.get_glyph_flags(), it->second.get());
    it++;
  }
}

FontManager::ErrorType FontManager::UpdateBuffer(
    const WordEnumerator &word_enum, const FontBufferParameters &parameters,
    int32_t base_line, FontBuffer *buffer, FontBufferContext *context,
    mathfu::vec2 *pos, FontMetrics *metrics) {
  auto ysize = static_cast<int32_t>(parameters.get_font_size());
  auto converted_ysize = ConvertSize(ysize);
  float scale = ysize / static_cast<float>(converted_ysize);

  // Retrieve layout info.
  uint32_t glyph_count;
  auto glyph_info = hb_buffer_get_glyph_infos(harfbuzz_buf_, &glyph_count);
  auto glyph_pos = hb_buffer_get_glyph_positions(harfbuzz_buf_, &glyph_count);

  for (size_t i = 0; i < glyph_count; ++i) {
    auto code_point = glyph_info[i].codepoint;
    if (!code_point) {
      continue;
    }
    ErrorType glyph_error = kErrorTypeSuccess;
    auto cache = GetCachedEntry(code_point, converted_ysize,
                                parameters.get_glyph_flags(), &glyph_error);
    if (cache == nullptr) {
      // Cleanup buffer contents.
      hb_buffer_clear_contents(harfbuzz_buf_);
      return glyph_error;
    }

    mathfu::vec2 pos_advance =
        mathfu::vec2(
            static_cast<float>(glyph_pos[i].x_advance) * kerning_scale_,
            static_cast<float>(-glyph_pos[i].y_advance)) *
        scale / static_cast<float>(kFreeTypeUnit);
    // Advance positions before rendering in RTL.
    if (layout_direction_ == kTextLayoutDirectionRTL) {
      *pos -= pos_advance;
    }

    // Register vertices only when the glyph has a size.
    if (cache->get_size().x && cache->get_size().y) {
      // Add the code point to the buffer. This information is used when
      // re-fetching UV information when the texture atlas is updated.
      buffer->AddGlyphInfo(current_font_->GetCurrentFaceId(), code_point,
                           converted_ysize);

      // Calculate internal/external leading value and expand a buffer if
      // necessary.
      FontMetrics new_metrics;
      if (UpdateMetrics(static_cast<int32_t>(cache->get_offset().y),
                        cache->get_size().y, *metrics, &new_metrics)) {
        *metrics = new_metrics;
      }

      // Expand buffer if necessary.
      auto buffer_idx = buffer->GetBufferIndex(cache->get_pos().z, context);
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
            current_font_->GetUnderline(ysize) + vec2i(pos->y, 0));
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
    bool end_of_line =
        context->lastline_must_break() == true && i == glyph_count - 1;
    if (parameters.get_caret_info_flag() && end_of_line == false) {
      // Is the current glyph a ligature?
      // We are not using hb_ot_layout_get_ligature_carets() as the API barely
      // work with existing fonts.
      // https://bugs.freedesktop.org/show_bug.cgi?id=90962 tracks a request
      // for the issue.
      auto carets = GetCaretPosCount(word_enum, glyph_info,
                                     static_cast<int32_t>(glyph_count),
                                     static_cast<int32_t>(i));

      auto scaled_offset = cache->get_offset().x * scale;
      // Add caret points
      for (auto caret = 1; caret <= carets; ++caret) {
        buffer->AddCaretPosition(
            *pos + vec2(scaled_offset - pos_advance.x +
                        caret * pos_advance.x / carets,
                        base_line));
      }
    }
  }
  return kErrorTypeSuccess;
}

FontManager::ErrorType FontManager::AppendEllipsis(
    const WordEnumerator &word_enum, const FontBufferParameters &parameters,
    int32_t base_line, FontBuffer *buffer, FontBufferContext *context,
    mathfu::vec2 *pos, FontMetrics *metrics) {
  buffer->has_ellipsis_ = true;
  context->set_lastline_must_break(false);

  if (!ellipsis_.length()) {
    return kErrorTypeSuccess;
  }

  auto max_width = parameters.get_size().x * kFreeTypeUnit;
  auto ysize = static_cast<int32_t>(parameters.get_font_size());
  auto converted_ysize = ConvertSize(ysize);
  float scale = ysize / static_cast<float>(converted_ysize);

  // Dump current string to the buffer.
  ErrorType buffer_error = UpdateBuffer(word_enum, parameters, base_line,
                                        buffer, context, pos, metrics);
  if (buffer_error != kErrorTypeSuccess) {
    return buffer_error;
  }

  // Calculate ellipsis string information.
  hb_buffer_clear_contents(harfbuzz_buf_);
  auto ellipsis_width = LayoutText(ellipsis_.c_str(), ellipsis_.length()) *
                        scale;
  if (ellipsis_width > max_width) {
    LogInfo("The ellipsis string width exceeded the given line width.\n");
    ellipsis_width = max_width;
  }

  // Remove some glyph entries from the buffer to have a room for the
  // ellipsis string.
  RemoveEntries(parameters, ellipsis_width, buffer, context, pos);

  // Add ellipsis string to the buffer.
  buffer_error = UpdateBuffer(word_enum, parameters, base_line, buffer, context,
                              pos, metrics);
  if (buffer_error != kErrorTypeSuccess) {
    return buffer_error;
  }

  // Cleanup buffer contents.
  hb_buffer_clear_contents(harfbuzz_buf_);

  return kErrorTypeSuccess;
}

bool FontManager::NeedToRemoveEntries(const FontBufferParameters &parameters,
                                      uint32_t required_width,
                                      const FontBuffer *buffer,
                                      size_t entry_index) const {
  // Only remove entries from the last line.
  if (entry_index <= buffer->line_start_indices_.back()) {
    return false;
  }

  const auto pos_start = GetStartPosition(parameters);
  const auto &vertices = buffer->get_vertices();

  // entry_index starts 1 past the index of the last glyph.
  auto vert_index = 0u;
  // We want to get the farthest vertex from the origin.
  if (layout_direction_ == kTextLayoutDirectionRTL) {
    // Get the last glyph's left edge.
    vert_index = entry_index * kVerticesPerGlyph + kVertexOfLeftEdge;
  } else {
    // Get the last glyph's right edge.
    vert_index = entry_index * kVerticesPerGlyph + kVertexOfRightEdge;
  }

  const auto x = vertices.at(vert_index).position_.data[0];
  auto width = 0.f;
  // Width needs to be (more positive x value - less positive x value).
  if (layout_direction_ == kTextLayoutDirectionRTL) {
    // Width of RTL is counted backwards from the pos_start.
    width = pos_start.x - x;
  } else {
    // Width of LTR is x - 0, or just x.
    width = x;
  }

  const auto max_width = parameters.get_size().x;
  return max_width - width < required_width / kFreeTypeUnit;
}

void FontManager::RemoveEntries(const FontBufferParameters &parameters,
                                uint32_t required_width, FontBuffer *buffer,
                                FontBufferContext *context, mathfu::vec2 *pos) {
  // Determine how many letters to remove.
  const int32_t kLastElementIndex = -2;
  auto &vertices = buffer->get_vertices();
  // entry_index starts 1 past the index of the last glyph.
  auto entry_index = vertices.size() / kVerticesPerGlyph;

  // Remove words when the eliminate mode if per word basis.
  if (ellipsis_mode_ == kEllipsisModeTruncateWord &&
      !context->word_boundary().empty()) {
    auto it = context->word_boundary().rbegin();
    auto end = context->word_boundary().rend();
    do {
      entry_index = *it;
    } while (++it != end &&
        NeedToRemoveEntries(parameters, required_width, buffer, entry_index));
  }

  // Remove characters.
  while (entry_index > 0 &&
         NeedToRemoveEntries(parameters, required_width, buffer, entry_index)) {
    --entry_index;
  }

  auto entries_to_remove =
      static_cast<int32_t>(vertices.size() / kVerticesPerGlyph - entry_index);
  auto removing_index =
      static_cast<int32_t>(vertices.size()) + kLastElementIndex;
  auto latest_attribute = context->attribute_history().back();

  if (removing_index <= 0) {
    return;
  }
  while (entries_to_remove-- > 0) {
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
      context->attribute_history().pop_back();
      latest_attribute = context->attribute_history().back();
      entries_to_remove++;
      continue;
    }

    // Remove codepoint.
    buffer->glyph_info_.pop_back();

    // Keep the x position of removed glyph and use it for a start of the
    // ellipsis string.
    if (layout_direction_ == kTextLayoutDirectionRTL) {
      // Get the right edge of the glyph because that's closest to the
      // remaining glyphs in RTL.
      pos->x = (buffer->vertices_.end() + kVertexOfRightEdge)->position_.data[0];
    } else {
      // In LTR the left edge is closest to the remaining glyphs.
      pos->x = (buffer->vertices_.end() + kVertexOfLeftEdge)->position_.data[0];
    }

    // Remove vertices.
    for (size_t j = 0; j < kVerticesPerGlyph; ++j) {
      buffer->vertices_.pop_back();
    }
  }

  // Position ellipsis a bit closer to the last character.
  if (ellipsis_mode_ == kEllipsisModeTruncateWord) {
    const float kSpacingBeforeEllipsis = 0.5f;
    float last_x = pos->x;
    if (layout_direction_ == kTextLayoutDirectionRTL) {
      // Get the left edge of the last glyph because that's farthest edge in
      // RTL.
      last_x = (buffer->vertices_.end() + kVertexOfLeftEdge)->position_.data[0];
    } else {
      // In LTR the right edge is the farthest.
      last_x = (buffer->vertices_.end() + kVertexOfRightEdge)->position_.data[0];
    }
    pos->x = pos->x - (pos->x - last_x) * kSpacingBeforeEllipsis;
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

FontBuffer *FontManager::UpdateUV(GlyphFlags flags, FontBuffer *buffer) {
  if (GetFontBufferStatus(*buffer) == kFontBufferStatusNeedReconstruct) {
    // Cache revision has been updated.
    // Some referencing glyph cache entries might have been evicted.
    // So we need to check glyph cache entries again while we can still use
    // layout information.

    // Set freetype settings.
    FontBufferContext ctx;
    auto current_font = current_font_;
    auto current_size = current_font_->GetPixelSize();

    // Keep original buffer.
    std::vector<std::vector<uint16_t>> original_indices =
        std::move(buffer->indices_);
    std::vector<FontBufferAttributes> original_slices =
        std::move(buffer->slices_);
    auto &glyph_info = buffer->get_glyph_info();

    auto current_face_id = kNullHash;
    for (size_t j = 0; j < original_indices.size(); ++j) {
      // Set up attributes and fonts.
      auto attr = original_slices[j];
      attr.slice_index_ = kIndexInvalid;
      ctx.SetAttribute(attr);

      auto indices = original_indices[j];
      for (size_t i = 0; i < indices.size(); i += kIndicesPerGlyph) {
        // Reconstruct indices.
        auto index = indices[i] / kVerticesPerGlyph;
        auto &info = glyph_info.at(index);

        if (current_face_id != info.face_id_) {
          current_font_ = HbFont::Open(info.face_id_, &font_cache_);
          if (current_font_ == nullptr) {
            fplbase::LogError("A font in use has been closed! fontID:%d",
                              info.face_id_);
            return buffer;
          }
          current_face_id = info.face_id_;
        }
        current_font_->SetPixelSize(info.size_);

        ErrorType glyph_error = kErrorTypeSuccess;
        auto cache = GetCachedEntry(info.code_point_, info.size_, flags,
                                    &glyph_error);
        if (cache == nullptr) {
          return nullptr;
        }

        // Expand buffer if necessary.
        auto buffer_idx = buffer->GetBufferIndex(cache->get_pos().z, &ctx);
        buffer->AddIndices(buffer_idx, index);

        // Update UV.
        buffer->UpdateUV(static_cast<int32_t>(index), cache->get_uv());
      }
    }
    // Update revision.
    buffer->set_revision(glyph_cache_->get_revision());

    // Restore font.
    current_font_ = current_font;
    current_font_->SetPixelSize(current_size);
  }

  return buffer;
}

bool FontManager::Open(const FontFamily &family) {
  const char *font_name = family.get_name().c_str();
  auto it = map_faces_.find(font_name);
  if (it != map_faces_.end()) {
    // The font has been already opened.
#ifdef FLATUI_VERBOSE_LOGGING
    LogInfo("Specified font '%s' is already opened.", font_name);
#endif  // FLATUI_VERBOSE_LOGGING
    it->second->AddRef();
    return true;
  }

  // Create FaceData and insert the created entry to the hash map.
  auto insert =
      map_faces_.insert(std::pair<std::string, std::unique_ptr<FaceData>>(
          font_name, std::unique_ptr<FaceData>(new FaceData())));
  auto face = insert.first->second.get();
  face->AddRef();

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

  // Register the font face to the cache.
  HbFont::Open(*face, &font_cache_);

  // Set first opened font as a default font.
  if (!face_initialized_) {
    if (!SelectFont(font_name)) {
      face->Release();
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
  if (it->second->Release()) {
    return true;
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

bool FontManager::SelectFont(const FontFamily &family) {
  auto it = map_faces_.find(family.get_name());
  if (it == map_faces_.end()) {
    LogError("SelectFont error: '%s'", family.get_name().c_str());
    return false;
  }

#if defined(FLATUI_SYSTEM_FONT)
  if (it->second->get_font_id() == kSystemFontId) {
    // Select the system font.
    return SelectFont(&family, 1);
  }
#endif  // FLATUI_SYSTEM_FONT

  current_font_ = HbFont::Open(*it->second.get(), &font_cache_);
  return current_font_ != nullptr;
}

bool FontManager::SelectFont(const FontFamily *font_families, int32_t count) {
#if defined(FLATUI_SYSTEM_FONT)
  // Open single font file.
  if (count == 1 && strcmp(font_families[0].get_name().c_str(), kSystemFont)) {
    return SelectFont(font_families[0]);
  }
#endif
  // Normalize the font names and create hash.
  auto id = kInitialHashValue;
  for (auto i = 0; i < count; ++i) {
    id = HashId(font_families[i].get_name().c_str(), id);
  }
  current_font_ = HbFont::Open(id, &font_cache_);

  if (current_font_ == nullptr) {
    std::vector<FaceData *> v;
    for (auto i = 0; i < count; ++i) {
#if defined(FLATUI_SYSTEM_FONT)
      if (!strcmp(font_families[i].get_name().c_str(), kSystemFont)) {
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
        auto it = map_faces_.find(font_families[i].get_name());
        if (it == map_faces_.end()) {
          LogError("SelectFont error: '%s'",
                   font_families[i].get_name().c_str());
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

bool FontManager::SelectFont(const char *font_name) {
  FontFamily family(font_name);
  return SelectFont(family);
}

bool FontManager::SelectFont(const char *font_names[], int32_t count) {
  std::vector<FontFamily> vec_family;
  for (auto i = 0; i < count; ++i) {
    vec_family.push_back(FontFamily(font_names[i]));
  }
  return SelectFont(&vec_family[0], count);
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

  // Resolve glyph cache's dirty rects, but only if we're in the render pass
  // (current_pass_ hasn't been updated yet, so use !start_subpass).
  if (glyph_cache_->get_dirty_state() && !start_subpass) {
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
                                bool last_line, bool enable_hyphenation,
                                int32_t *rewind) {
  // Update language settings.
  SetLanguageSettings();

  // Layout the text.
  hb_buffer_add_utf8(harfbuzz_buf_, text, static_cast<uint32_t>(length), 0,
                     static_cast<int32_t>(length));
  hb_shape(current_font_->GetHbFont(), harfbuzz_buf_, nullptr, 0);
  if (layout_direction_ == kTextLayoutDirectionRTL) {
    hb_buffer_reverse(harfbuzz_buf_);
  }

  // Retrieve layout info.
  uint32_t glyph_count;
  hb_glyph_info_t *glyph_info =
      hb_buffer_get_glyph_infos(harfbuzz_buf_, &glyph_count);
  hb_glyph_position_t *glyph_pos =
      hb_buffer_get_glyph_positions(harfbuzz_buf_, &glyph_count);

  // Retrieve a width of the string.
  float string_width = 0.0f;
  auto available_space = max_width - current_width;
  for (uint32_t i = 0; i < glyph_count; ++i) {
    auto advance = static_cast<float>(glyph_pos[i].x_advance) * kerning_scale_;
    if (max_width && string_width + advance > max_width) {
      // If a single word exceeds the max width, the word is forced to
      // linebreak.

      // Find a string length that fits to an avaialble space AND the available
      // space can have at least one letter.
      while (string_width > available_space &&
             available_space >=
                 static_cast<float>(glyph_pos[0].x_advance) * kerning_scale_) {
        --i;
        string_width -=
            static_cast<float>(glyph_pos[i].x_advance) * kerning_scale_;
      }
      if (i <= 0) {
        // If there is no space for even one letter, don't render anything.
        hb_buffer_set_length(harfbuzz_buf_, 0);
        // But, still progress past that one letter to prevent infinite looping.
        *rewind = static_cast<int32_t>(length - 1);
        return 0;
      }

      // Calculate # of characters to rewind.
      *rewind = static_cast<int32_t>(length - glyph_info[i].cluster);
      hb_buffer_set_length(harfbuzz_buf_, i);
      break;
    }

    // Check hyphenation.
    if (enable_hyphenation && !(last_line && ellipsis_.length()) &&
        string_width + advance > available_space) {
      return Hyphenate(text, length, available_space, rewind);
    }
    string_width += advance;
  }
  return static_cast<int32_t>(string_width);
}

int32_t FontManager::Hyphenate(const char *text, size_t length,
                               int32_t available_space, int32_t *rewind) {
  std::vector<uint8_t> result;
  hyphenator_.Hyphenate(reinterpret_cast<const uint8_t *>(text), length,
                        &result);

  std::string hyphenating_str(text, length);
  auto it = result.rbegin();
  auto end = result.rend();
  while (it != end) {
    // Retrieve the last hyphenation point.
    if (*it) {
      // Generate text with a hyphen and layout it.
      size_t hyphenation_point = result.size() - (it - result.rbegin()) - 1;
      size_t idx = 0;
      for (size_t i = 0; i < hyphenation_point; i++) {
        ub_get_next_char_utf8(reinterpret_cast<const uint8_t *>(text), length,
                              &idx);
      }
      *(hyphenating_str.begin() + idx) = '-';  // Using minus-hyphen here since
                                               // hyphen ("‐") is not laid out
                                               // correctly in some cases.

      // Layout the text with a hyphen.
      hb_buffer_clear_contents(harfbuzz_buf_);
      auto width =
          LayoutText(hyphenating_str.data(), idx + 1, 0, 0, false, rewind);

      if (width < available_space) {
        // Found a right hyphenation point.
        *rewind = length - idx;
        return width;
      }
    }
    // Check with next hyphenation point.
    it++;
  }

  // Didn't find any hyphenation point, restore buffer state and return.
  hb_buffer_clear_contents(harfbuzz_buf_);
  return LayoutText(text, length, 0, 0, false, rewind);
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
    hyphenation_rule_ =
        layout_info->hyphenation ? layout_info->hyphenation : "";
    SetupHyphenationPatternPath(nullptr);
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
                                                   GlyphFlags flags,
                                                   ErrorType *error) {
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
      *error = kErrorTypeMissingGlyph;
      return nullptr;
    }

    // Store the glyph to cache.
    FT_GlyphSlot g = face->glyph;
    GlyphCacheEntry entry;
    entry.set_code_point(code_point);
    bool color_glyph = g->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA;
    float bitmap_left = g->bitmap_left +
                        static_cast<float>(g->lsb_delta) / kFreeTypeUnit;

    // Does not support SDF for color glyphs.
    if (flags & (kGlyphFlagsOuterSDF | kGlyphFlagsInnerSDF) &&
        g->bitmap.width && g->bitmap.rows && !color_glyph) {
      // Adjust a glyph size and an offset with a padding.
      entry.set_offset(vec2(bitmap_left - kGlyphCachePaddingSDF,
                            g->bitmap_top + kGlyphCachePaddingSDF));
      entry.set_size(vec2i(g->bitmap.width + kGlyphCachePaddingSDF * 2,
                           g->bitmap.rows + kGlyphCachePaddingSDF * 2));
      entry.set_advance(vec2i(g->advance.x / kFreeTypeUnit, 0));
      cache = glyph_cache_->Set(nullptr, key, entry);
      if (cache != nullptr) {
        // Generates SDF.
        auto buffer = glyph_cache_->get_monochrome_buffer();
        auto pos = cache->get_pos();
        auto stride = buffer->get_size().x;
        auto p = buffer->get(pos.z) + pos.x + pos.y * stride;
        Grid<uint8_t> src(g->bitmap.buffer,
                          vec2i(g->bitmap.width, g->bitmap.rows),
                          kGlyphCachePaddingSDF, g->bitmap.width);
        Grid<uint8_t> dest(p, cache->get_size(), 0, stride);
        sdf_computer_->Compute(src, &dest, flags);
      }
    } else {
      if (color_glyph) {
        // FreeType returns fixed sized bitmap for color glyphs.
        // Rescale bitmap here for better quality and performance.
        auto glyph_scale = static_cast<float>(ysize) / g->bitmap.rows;
        int32_t new_width = static_cast<int32_t>(g->bitmap.width * glyph_scale);
        int32_t new_height = static_cast<int32_t>(g->bitmap.rows * glyph_scale);
        int32_t new_advance =
            static_cast<int32_t>(g->advance.x * glyph_scale / kFreeTypeUnit);
        std::unique_ptr<uint8_t[]> out_buffer(
            new uint8_t[new_width * new_height * sizeof(uint32_t)]);

        // TODO: Evaluate generating mipmap for a dirty rect and see
        // it would improve performance and produces acceptable quality.
        stbir_resize_uint8(g->bitmap.buffer, g->bitmap.width, g->bitmap.rows, 0,
                           out_buffer.get(), new_width, new_height, 0,
                           sizeof(uint32_t));

        if (glyph_cache_->SupportsColorGlyphs()) {
          entry.set_color_glyph(true);
        } else {
          // Copy the color glyph's alpha into the monochrome buffer.  Otherwise
          // this will cause the entire font buffer to fail.
          std::unique_ptr<uint8_t[]> mono_buffer(
              new uint8_t[new_width * new_height]);
          const uint32_t* src =
              reinterpret_cast<const uint32_t*>(out_buffer.get());
          uint8_t* dst = mono_buffer.get();

          for (int i = 0; i < new_width * new_height; ++i, ++src, ++dst) {
            *dst = static_cast<uint8_t>((*src) >> 24);
          }

          out_buffer = std::move(mono_buffer);
        }

        const float kEmojiBaseLine = 0.85f;
        entry.set_offset(vec2(bitmap_left * glyph_scale,
                              new_height * kEmojiBaseLine));
        entry.set_size(vec2i(new_width, new_height));
        entry.set_advance(vec2i(new_advance, 0));
        cache = glyph_cache_->Set(out_buffer.get(), key, entry);
      } else {
        entry.set_offset(vec2(bitmap_left, g->bitmap_top));
        entry.set_size(vec2i(g->bitmap.width, g->bitmap.rows));
        entry.set_advance(vec2i(g->advance.x / kFreeTypeUnit, 0));
        cache = glyph_cache_->Set(g->bitmap.buffer, key, entry);
      }
    }

    if (cache == nullptr) {
      // Glyph cache need to be flushed.
      // Returning nullptr here for a retry.
      LogInfo("Glyph cache is full. Need to flush and re-create.\n");
      *error = kErrorTypeCacheIsFull;
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
  } else if (font_buffer.get_revision() > current_atlas_revision_) {
    return kFontBufferStatusNeedCacheUpdate;
  }
  return kFontBufferStatusReady;
}

}  // namespace flatui
