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

#include "font_manager.h"
#include "fplbase/utilities.h"

#ifdef IMGUI_USE_LIBUNIBREAK
#include "linebreak.h"
#endif

namespace fpl {

// Constants
const char *kLineBreakDefaultLaunguage = "en";

// Singleton object of FreeType&Harfbuzz.
FT_Library *FontManager::ft_;
hb_buffer_t *FontManager::harfbuzz_buf_;

// Enumerate words in a specified buffer using line break information generated
// by libunibreak.
class WordEnumerator {
public:
  // Delete default constructor.
  WordEnumerator() = delete;

  // buffer: a buffer contains a linebreak information.
  // Note that the class accesses the given buffer while it's lifetime.
  WordEnumerator(const std::vector<char> &buffer)
  : current_index_(0), current_length_(0), finished_(false) {
    buffer_ = &buffer;
  }

  // Advance the current word index
  // return: true if the buffer has next word. false if the buffer finishes.
  bool Advance() {
    assert(buffer_);
    if (buffer_->size() == 0 && finished_ == false) {
      // If there is no word breaking information, allow one iteration.
      finished_ = true;
      return true;
    }

    current_index_ += current_length_;
    if (current_index_ >= buffer_->size()) {
      return false;
    }

    size_t index = current_index_;
    while (index < buffer_->size()) {
      auto word_info = (*buffer_)[index];
      if (word_info == LINEBREAK_MUSTBREAK || word_info == LINEBREAK_ALLOWBREAK) {
        break;
      }
      index++;
    }
    current_length_ = index - current_index_ + 1;
    return true;
  }

  // Get an index of the current word in the given text buffer.
  uint32_t GetCurrentWordIndex() {
    assert(buffer_);
    return current_index_;
  }

  // Get a length of the current word in the given text buffer.
  size_t GetCurrentWordLength() {
    assert(buffer_);
    return current_length_;
  }

  // Retrieve if current word must break a line.
  bool CurrentWordMustBreak() {
    assert(buffer_);
    // Check if the first advance is made.
    if (current_index_ + current_length_ == 0) return false;
    return (*buffer_)[current_index_ + current_length_ - 1] ==
    LINEBREAK_MUSTBREAK;
  }

private:
  size_t current_index_;
  size_t current_length_;
  const std::vector<char> *buffer_;
  bool finished_;
};

FontManager::FontManager() {
  // Initialize variables and libraries.
  Initialize();

  // Initialize glyph cache.
  glyph_cache_.reset(new GlyphCache<uint8_t>(
      mathfu::vec2i(kGlyphCacheWidth, kGlyphCacheHeight)));
}

FontManager::FontManager(const mathfu::vec2i &cache_size) {
  // Initialize variables and libraries.
  Initialize();

  // Initialize glyph cache.
  glyph_cache_.reset(new GlyphCache<uint8_t>(cache_size));
}

FontManager::~FontManager() { Close(); }

void FontManager::Initialize() {
  assert(ft_ == nullptr);

  // Initialize variables.
  renderer_ = nullptr;
  face_initialized_ = false;
  current_atlas_revision_ = 0;
  current_pass_ = 0;
  language_ = kLineBreakDefaultLaunguage;
  line_height_ = kLineHeightDefault;

  ft_ = new FT_Library;
  FT_Error err;
  if ((err = FT_Init_FreeType(ft_))) {
    // Error! Please fix me.
    SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                 "Can't initialize freetype. FT_Error:%d\n", err);
    assert(0);
  }

  // Create a buffer for harfbuzz.
  harfbuzz_buf_ = hb_buffer_create();

#ifdef IMGUI_USE_LIBUNIBREAK
  // Initialize libunibreak
  init_linebreak();
#else
  #error libunibreak is required for multiline label support!
#endif
}

void FontManager::Terminate() {
  assert(ft_ != nullptr);
  hb_buffer_destroy(harfbuzz_buf_);
  harfbuzz_buf_ = nullptr;

  FT_Done_FreeType(*ft_);
  ft_ = nullptr;
}

FontBuffer *FontManager::GetBuffer(const char *text, const uint32_t length,
                                   const float ysize,
                                   const mathfu::vec2i &size) {
  // Adjust y size if the size selector is set.
  int32_t converted_ysize = ConvertSize(ysize);
  float scale = ysize / static_cast<float>(converted_ysize);
  bool multi_line = size.x();

  // Check cache if we already have a FontBuffer generated.
  auto it = map_buffers_.find(text);
  if (it != map_buffers_.end()) {
    auto t = it->second.find(ysize);
    if (t != it->second.end()) {
      // Update current pass.
      if (current_pass_ != kRenderPass) {
        t->second->set_pass(current_pass_);
      }

      // Update UV of the buffer
      auto ret = UpdateUV(converted_ysize, t->second.get());
      return ret;
    }
  }

  // Otherwise, create new FontBuffer.

  // Set freetype settings.
  FT_Set_Pixel_Sizes(face_, 0, converted_ysize);

  // Create FontBuffer with derived string length.
  std::unique_ptr<FontBuffer> buffer(new FontBuffer(length));

  // Retrieve word breaking information using libunibreak.
  if (multi_line) {
    wordbreak_info_.resize(length);
    set_linebreaks_utf8(reinterpret_cast<const utf8_t *>(text), length,
                        language_.c_str(), &wordbreak_info_[0]);
  } else {
    wordbreak_info_.resize(0);
  }

  // Initialize font metrics parameters.
  int32_t base_line = ysize * face_->ascender / face_->units_per_EM;
  FontMetrics initial_metrics(base_line, 0, base_line, base_line - ysize, 0);
  mathfu::vec2 pos(mathfu::kZeros2f);
  FT_GlyphSlot glyph = face_->glyph;

  uint32_t line_width = 0;
  uint32_t max_line_width = 0;
  uint32_t total_glyph_count = 0;
  uint32_t total_height = ysize;
  bool lastline_must_break = false;
  WordEnumerator word_enum(wordbreak_info_);

  // Find words and layout them.
  while (word_enum.Advance()) {
    if (!multi_line) {
      // Single line text.
      // In this mode, it layouts all string into single line.
      max_line_width = LayoutText(text, length) * scale;
    } else {
      // Multi line text.
      // In this mode, it layouts every single word in the order of the text and
      // performs a line break if either current word exceeds the max line
      // width or indicated a line break must happen due to a line break
      // character etc.
      uint32_t word_width = LayoutText(text + word_enum.GetCurrentWordIndex(),
                                       word_enum.GetCurrentWordLength()) *
                            scale;
      if (lastline_must_break || (line_width + word_width) / kFreeTypeUnit >
                                     static_cast<uint32_t>(size.x())) {
        // Line break.
        auto line_height = face_->height * line_height_ * scale / kFreeTypeUnit;
        pos = vec2(0.f, pos.y() + line_height);
        total_height += line_height;
        if (size.y() && total_height > static_cast<uint32_t>(size.y())) {
          // The text size exceeds given size.
          // For now, we just don't render the rest of strings.

          // Cleanup buffer contents.
          hb_buffer_clear_contents(harfbuzz_buf_);
          break;
        }

        line_width = word_width;
        if (line_width > static_cast<uint32_t>(size.x())) {
          std::string s = std::string(&text[word_enum.GetCurrentWordIndex()],
                                      word_enum.GetCurrentWordLength());
          fpl::LogInfo(
              "A single word '%s' exceeded the given line width setting.\n"
              "Currently multiline label doesn't support a hyphenation",
              s.c_str());
        }
      } else {
        line_width += word_width;
      }
      max_line_width = std::max(max_line_width, line_width);
      lastline_must_break = word_enum.CurrentWordMustBreak();
    }

    // Retrieve layout info.
    uint32_t glyph_count;
    auto glyph_info = hb_buffer_get_glyph_infos(harfbuzz_buf_, &glyph_count);
    auto glyph_pos = hb_buffer_get_glyph_positions(harfbuzz_buf_, &glyph_count);

    for (size_t i = 0; i < glyph_count; ++i) {
      auto code_point = glyph_info[i].codepoint;
      auto cache = GetCachedEntry(code_point, converted_ysize);
      if (cache == nullptr) {
        // Cleanup buffer contents.
        hb_buffer_clear_contents(harfbuzz_buf_);
        return nullptr;
      }

      // Add the code point to the buffer. This information is used when
      // re-fetching UV information when the texture atlas is updated.
      buffer->get_code_points()->push_back(code_point);

      // Calculate internal/external leading value and expand a buffer if
      // necessary.
      FontMetrics new_metrics;
      if (UpdateMetrics(glyph, initial_metrics, &new_metrics)) {
        initial_metrics = new_metrics;
      }

      // Construct indices array.
      const uint16_t kIndices[] = {0, 1, 2, 1, 2, 3};
      for (auto index : kIndices) {
        buffer->get_indices()->push_back(index + (total_glyph_count + i) * 4);
      }

      // Construct intermediate vertices array.
      // The vertices array is update in the render pass with correct
      // glyph size & glyph cache entry information.

      // Update vertices.
      buffer->AddVertices(pos, base_line, scale, *cache);

      // Update UV.
      buffer->UpdateUV(total_glyph_count + i, cache->get_uv());

      // Set buffer revision using glyph cache revision.
      buffer->set_revision(glyph_cache_->get_revision());

      // Advance positions.
      pos += mathfu::vec2(glyph_pos[i].x_advance, -glyph_pos[i].y_advance) *
             scale / kFreeTypeUnit;
    }

    // Update total number of glyphs.
    total_glyph_count += glyph_count;

    // Cleanup buffer contents.
    hb_buffer_clear_contents(harfbuzz_buf_);
  }

  // Setup size.
  buffer->set_size(vec2i(max_line_width / kFreeTypeUnit, total_height));

  // Setup font metrics.
  buffer->set_metrics(initial_metrics);

  // Set current pass.
  if (current_pass_ != kRenderPass) {
    buffer->set_pass(current_pass_);
  }

  // Verify the buffer.
  assert(buffer->Verify());

  // Insert the created entry to the hash map.
  auto insert =
      map_buffers_[text].insert(std::pair<int32_t, std::unique_ptr<FontBuffer>>(
          ysize, std::move(buffer)));
  return insert.first->second.get();
}

FontBuffer *FontManager::UpdateUV(const int32_t ysize, FontBuffer *buffer) {
  if (buffer->get_revision() != current_atlas_revision_) {
    // Cache revision has been updated.
    // Some referencing glyph cache entries might have been evicted.
    // So we need to check glyph cache entries again while we can still use
    // layout information.

    // Set freetype settings.
    FT_Set_Pixel_Sizes(face_, 0, ysize);

    auto code_points = buffer->get_code_points();
    for (size_t i = 0; i < code_points->size(); ++i) {
      auto code_point = code_points->at(i);
      auto cache = GetCachedEntry(code_point, ysize);
      if (cache == nullptr) {
        return nullptr;
      }

      // Update UV.
      buffer->UpdateUV(i, cache->get_uv());

      // Update revision.
      buffer->set_revision(glyph_cache_->get_revision());
    }
  }
  return buffer;
}

FontTexture *FontManager::GetTexture(const char *text, const uint32_t length,
                                     const float original_ysize) {
  // Round up y size if the size selector is set.
  int32_t ysize = ConvertSize(original_ysize);

  // Check cache if we already have a texture.
  auto it = map_textures_.find(text);
  if (it != map_textures_.end()) {
    auto t = it->second.find(ysize);
    if (t != it->second.end()) return t->second.get();
  }

  // Otherwise, create new texture.

  // Set freetype settings.
  FT_Set_Pixel_Sizes(face_, 0, ysize);

  // Layout text.
  auto string_width = LayoutText(text, length);

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
  int32_t base_line = ysize * face_->ascender / face_->units_per_EM;
  FontMetrics initial_metrics(base_line, 0, base_line, base_line - ysize, 0);

  // rasterized image format in FreeType is 8 bit gray scale format.
  std::unique_ptr<uint8_t[]> image(new uint8_t[width * height]);
  memset(image.get(), 0, width * height);

  // TODO: make padding values configurable.
  uint32_t kGlyphPadding = 0;
  mathfu::vec2 pos(kGlyphPadding, kGlyphPadding);
  FT_GlyphSlot glyph = face_->glyph;

  for (size_t i = 0; i < glyph_count; ++i) {
    FT_Error err;

    // Load glyph using harfbuzz layout information.
    // Note that harfbuzz takes care of ligatures.
    if ((err = FT_Load_Glyph(face_, glyph_info[i].codepoint, FT_LOAD_RENDER))) {
      // Error. This could happen typically the loaded font does not support
      // particular glyph.
      SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Can't load glyph %c FT_Error:%d\n",
                   text[i], err);
      return nullptr;
    }

    // Calculate internal/external leading value and expand a buffer if
    // necessary.
    FontMetrics new_metrics;
    if (UpdateMetrics(glyph, initial_metrics, &new_metrics)) {
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
      pos.x() = -glyph->bitmap_left;
    }

    // Copy the texture
    uint32_t y_offset = initial_metrics.base_line() - glyph->bitmap_top;
    for (uint32_t y = 0; y < glyph->bitmap.rows; ++y) {
      memcpy(&image[(pos.y() + y + y_offset) * width + pos.x() +
                    glyph->bitmap_left],
             &glyph->bitmap.buffer[y * glyph->bitmap.pitch],
             glyph->bitmap.width);
    }

    // Advance positions.
    pos += mathfu::vec2(glyph_pos[i].x_advance, -glyph_pos[i].y_advance) /
           kFreeTypeUnit;
  }

  // Create new texture.
  FontTexture *tex = new FontTexture(*renderer_);
  tex->LoadFromMemory(image.get(), vec2i(width, height), kFormatLuminance,
                      false);

  // Setup UV.
  tex->set_uv(vec4(0.0f, 0.0f,
                   static_cast<float>(string_width) / static_cast<float>(width),
                   static_cast<float>(initial_metrics.total()) /
                       static_cast<float>(height)));

  // Setup font metrics.
  tex->set_metrics(initial_metrics);

  // Cleanup buffer contents.
  hb_buffer_clear_contents(harfbuzz_buf_);

  // Put to the dic.
  map_textures_[text][ysize].reset(tex);

  return tex;
}

bool FontManager::ExpandBuffer(const int32_t width, const int32_t height,
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

bool FontManager::Open(const char *font_name) {
  assert(!face_initialized_);

  // Load the font file of assets.
  if (!LoadFile(font_name, &font_data_)) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Can't load font reource: %s\n",
                 font_name);
    return false;
  }

  // Open the font.
  FT_Error err;
  if ((err = FT_New_Memory_Face(
           *ft_, reinterpret_cast<const unsigned char *>(&font_data_[0]),
           font_data_.size(), 0, &face_))) {
    // Failed to open font.
    SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                 "Failed to initialize font:%s FT_Error:%d\n", font_name, err);
    return false;
  }

  // Create harfbuzz font information from freetype face.
  harfbuzz_font_ = hb_ft_font_create(face_, NULL);
  if (!harfbuzz_font_) {
    // Failed to open font.
    SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                 "Failed to initialize harfbuzz layout information:%s\n",
                 font_name);
    font_data_.clear();
    FT_Done_Face(face_);
    return false;
  }

  face_initialized_ = true;
  return true;
}

bool FontManager::Close() {
  if (!face_initialized_) return false;

  map_textures_.clear();

  map_buffers_.clear();

  hb_font_destroy(harfbuzz_font_);

  FT_Done_Face(face_);

  font_data_.clear();

  face_initialized_ = false;

  return true;
}

void FontManager::StartLayoutPass() {
  // Reset pass.
  current_pass_ = 0;
}

void FontManager::UpdatePass(const bool start_subpass) {
  // Increment a cycle counter in glyph cache.
  glyph_cache_->Update();

  if (glyph_cache_->get_dirty_state() && current_pass_ <= 0) {
    auto rect = glyph_cache_->get_dirty_rect();
    atlas_texture_.get()->Set(0);

    // In OpenGL ES2.0, width and pitch of the src buffer needs to match. So
    // that we are updating entire row at once.
    // TODO: Optimize glTexSubImage2D call in ES3.0 capable platform.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, rect.y(),
                    glyph_cache_.get()->get_size().x(), rect.w() - rect.y(),
                    GL_LUMINANCE, GL_UNSIGNED_BYTE,
                    glyph_cache_.get()->get_buffer() +
                        glyph_cache_.get()->get_size().x() * rect.y());
    current_atlas_revision_ = glyph_cache_->get_revision();
    glyph_cache_->set_dirty_state(false);
  }

  if (start_subpass) {
    if (current_pass_ > 0) {
      SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                   "Multiple subpasses in one rendering pass is not supported. "
                   "When this happens, increase the glyph cache size not to "
                   "flush the atlas texture multiple times in one rendering "
                   "pass.");
    }
    glyph_cache_->Flush();
    current_atlas_revision_ = glyph_cache_->get_revision();
    current_pass_++;
  } else {
    // Reset pass.
    current_pass_ = kRenderPass;
  }
}

uint32_t FontManager::LayoutText(const char *text, const size_t length) {
  // TODO: make harfbuzz settings (and other font settings) configurable.
  // Set harfbuzz settings.
  hb_buffer_set_direction(harfbuzz_buf_, HB_DIRECTION_LTR);
  hb_buffer_set_script(harfbuzz_buf_, HB_SCRIPT_LATIN);
  hb_buffer_set_language(harfbuzz_buf_, hb_language_from_string(text, length));

  // Layout the text.
  hb_buffer_add_utf8(harfbuzz_buf_, text, length, 0, length);
  hb_shape(harfbuzz_font_, harfbuzz_buf_, nullptr, 0);

  // Retrieve layout info.
  uint32_t glyph_count;
  hb_glyph_position_t *glyph_pos =
      hb_buffer_get_glyph_positions(harfbuzz_buf_, &glyph_count);

  // Retrieve a width of the string.
  uint32_t string_width = 0;
  for (uint32_t i = 0; i < glyph_count; ++i) {
    string_width += glyph_pos[i].x_advance;
  }
  return string_width;
}

bool FontManager::UpdateMetrics(const FT_GlyphSlot g,
                                const FontMetrics &current_metrics,
                                FontMetrics *new_metrics) {
  // Calculate internal/external leading value and expand a buffer if
  // necessary.
  if (g->bitmap_top > current_metrics.ascender() ||
      static_cast<int32_t>(g->bitmap_top - g->bitmap.rows) <
          current_metrics.descender()) {
    *new_metrics = current_metrics;
    new_metrics->set_internal_leading(
        std::max(current_metrics.internal_leading(),
                 g->bitmap_top - current_metrics.ascender()));
    new_metrics->set_external_leading(
        std::min(current_metrics.external_leading(),
                 static_cast<int32_t>(g->bitmap_top - g->bitmap.rows -
                                      current_metrics.descender())));
    new_metrics->set_base_line(new_metrics->internal_leading() +
                               new_metrics->ascender());

    return true;
  }
  return false;
}

const GlyphCacheEntry *FontManager::GetCachedEntry(const uint32_t code_point,
                                                   const int32_t ysize) {
  auto cache = glyph_cache_->Find(code_point, ysize);

  if (cache == nullptr) {
    // Load glyph using harfbuzz layout information.
    // Note that harfbuzz takes care of ligatures.
    FT_Error err;
    if ((err = FT_Load_Glyph(face_, code_point, FT_LOAD_RENDER))) {
      // Error. This could happen typically the loaded font does not support
      // particular glyph.
      SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Can't load glyph %c FT_Error:%d\n",
                   code_point, err);
      return nullptr;
    }

    // Store the glyph to cache.
    FT_GlyphSlot g = face_->glyph;
    GlyphCacheEntry entry;
    entry.set_code_point(code_point);
    entry.set_size(vec2i(g->bitmap.width, g->bitmap.rows));
    entry.set_offset(vec2i(g->bitmap_left, g->bitmap_top));
    cache = glyph_cache_->Set(g->bitmap.buffer, ysize, entry);

    if (cache == nullptr) {
      // Glyph cache need to be flushed.
      // Returning nullptr here for a retry.
      SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                  "Glyph cache is full. Need to flush and re-create.\n");
      return nullptr;
    }
  }
  return cache;
}

int32_t FontManager::ConvertSize(const int32_t original_ysize) {
  if (size_selector_ != nullptr) {
    return size_selector_(original_ysize);
  } else {
    return original_ysize;
  }
}

void FontBuffer::AddVertices(const vec2 &pos, const int32_t base_line,
                             const float scale, const GlyphCacheEntry &entry) {
  mathfu::vec2i rounded_pos = mathfu::vec2i(pos);
  auto scaled_offset = mathfu::vec2(entry.get_offset()) * scale;
  auto scaled_size = mathfu::vec2(entry.get_size()) * scale;
  float scaled_base_line = base_line * scale;

  auto x = rounded_pos.x() + scaled_offset.x();
  auto y = rounded_pos.y() + scaled_base_line - scaled_offset.y();
  vertices_.emplace_back(x, y, 0.0f, 0.0f, 0.0f);

  vertices_.emplace_back(x, y + scaled_size.y(), 0.0f, 0.0f, 0.0f);

  vertices_.emplace_back(x + scaled_size.x(), y, 0.0f, 0.0f, 0.0f);

  vertices_.emplace_back(x + scaled_size.x(), y + scaled_size.y(), 0.0f, 0.0f,
                         0.0f);
}

void FontBuffer::UpdateUV(const int32_t index, const vec4 &uv) {
  vertices_[index * 4].uv_ = uv.xy();
  vertices_[index * 4 + 1].uv_ = mathfu::vec2(uv.x(), uv.w());
  vertices_[index * 4 + 2].uv_ = mathfu::vec2(uv.z(), uv.y());
  vertices_[index * 4 + 3].uv_ = uv.zw();
}

}  // namespace fpl
