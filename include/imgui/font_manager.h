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

#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H

#include "renderer.h"
#include "imgui/internal/glyph_cache.h"

// Forward decls for FreeType & Harfbuzz
typedef struct FT_LibraryRec_ *FT_Library;
typedef struct FT_FaceRec_ *FT_Face;
typedef struct FT_GlyphSlotRec_ *FT_GlyphSlot;
struct hb_font_t;
struct hb_buffer_t;

namespace fpl {

// Forward decl.
class FontTexture;
class FontBuffer;
class FontMetrics;

// Constant to convert FreeType unit to pixel unit.
// In FreeType & Harfbuzz, the position value unit is 1/64 px whereas
// configurable in imgui. The constant is used to convert FreeType unit
// to px.
const int32_t kFreeTypeUnit = 64;

// Default size of glyph cache.
const int32_t kGlyphCacheWidth = 1024;
const int32_t kGlyphCacheHeight = 1024;

// FontManager manages font rendering with OpenGL utilizing freetype
// and harfbuzz as a glyph rendering and layout back end.
//
// It opens speficied OpenType/TrueType font and rasterize to OpenGL texture.
// An application can use the generated texture for a text rendering.
//
// The class is not threadsafe, it's expected to be only used from
// within OpenGL rendering thread.
class FontManager {
 public:
  FontManager();
  // Constructor with a cache size in pixels.
  // The given size is rounded up to nearest power of 2 internally to be used as
  // an OpenGL texture sizes.
  FontManager(const mathfu::vec2i &cache_size);
  ~FontManager();

  // Open font face, TTF, OT fonts are supported.
  // In this version it supports only single face at a time.
  // Return value: false when failing to open font, such as
  // a file open error, an invalid file format etc.
  bool Open(const char *font_name);

  // Discard font face that has been opened via Open().
  bool Close();

  // Retrieve a texture with the text.
  // This API doesn't use the glyph cache, instead it writes the string image
  // directly to the returned texture. The user can use this API when a font
  // texture is used for a long time, such as a string image used in game HUD.
  FontTexture *GetTexture(const char *text, const float ysize);

  // Retrieve a vertex buffer for a font rendering using glyph cache.
  // Returns nullptr if the string does not fit in the glyph cache.
  // When this happens, caller may flush the glyph cache with
  // FlushAndUpdate() call and re-try the GetBuffer() call.
  FontBuffer *GetBuffer(const char *text, const float ysize);

  // Set renderer. Renderer is used to create a texture instance.
  void SetRenderer(Renderer &renderer) {
    renderer_ = &renderer;

    // Initialize the font atlas texture.
    atlas_texture_.reset(new Texture(renderer));
    atlas_texture_.get()->LoadFromMemory(glyph_cache_->get_buffer(),
                                         glyph_cache_->get_size(),
                                         kFormatLuminance, false);

    // Disable mipmap for the atlas texture.
    atlas_texture_.get()->Set(0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  }

  // Returns if a font has been loaded.
  bool FontLoaded() { return face_initialized_; }

  // The user of the font_manager class is expected to use the class in 2
  // passes, a layout pass and a render pass.
  // During the layout pass, the user invokes GetBuffer() calls to fetch all
  // required glyphs into the cache. Once in the cache, the user can retrieve
  // the size of the strings to position them.
  // Once the layout pass finishes, the user invokes StartRenderPass() to upload
  // cache images to
  // the atlas texture, and use the texture in the render pass.
  // This design helps to minimize the frequency of the atlas texture upload.
  //
  // Indicate a start of new layout pass. Call the API each time the user starts
  // a layout pass.
  void StartLayoutPass();

  // Flush existing glyph cache contents and start new layout pass.
  // Call this API while in a layout pass when the glyph cache is fragmented.
  void FlushAndUpdate() { UpdatePass(true); }

  // Indicate a start of new render pass. Call the API each time the user starts
  // a render pass.
  void StartRenderPass() { UpdatePass(false); }

  // Getter of the font atlas texture.
  Texture *GetAtlasTexture() { return atlas_texture_.get(); }

  // The user can supply a size selector function to adjust glyph sizes when
  // storing a glyph cache entry.
  // By doing that, multiple strings with slightly different sizes can share the
  // same glyph cache entry so that the user can reduce a total # of cached
  // entries.
  // The callback argument is a requested glyph size in pixels.
  // The callback should return desired glyph size that should be used for the
  // glyph sizes.
  void SetSizeSelector(std::function<int32_t(const int32_t)> selector) {
    size_selector_.swap(selector);
  }

 private:
  // Pass indicating rendering pass.
  static const int32_t kRenderPass = -1;

  // Initialize static data associated with the class.
  static void Initialize();

  // Clean up static data associated with the class.
  // Note that after the call, an application needs to call Initialize() again
  // to use the class.
  static void Terminate();

  // Expand a texture image buffer when the font metrics is changed.
  // Returns true if the image buffer was reallocated.
  static bool ExpandBuffer(const int32_t width, const int32_t height,
                           const FontMetrics &original_metrics,
                           const FontMetrics &new_metrics,
                           std::unique_ptr<uint8_t[]> *image);

  // Layout text and update harfbuzz_buf_.
  // Returns the width of the text layout in pixels.
  uint32_t LayoutText(const char *text);

  // Calculate internal/external leading value and expand a buffer if
  // necessary.
  // Returns true if the size of metrics has been changed.
  bool UpdateMetrics(const FT_GlyphSlot glyph,
                     const FontMetrics &current_metrics,
                     FontMetrics *new_metrics);

  // Retrieve cached entry from the glyph cache.
  // If an entry is not found in the glyph cache, the API tries to create new
  // cache entry and returns it if succeeded.
  // Returns nullptr if,
  // - The font doesn't have the requested glyph.
  // - The glyph doesn't fit into the cache (even after trying to evict some
  // glyphs in cache based on LRU rule).
  // (e.g. Requested glyph size too large or the cache is highly fragmented.)
  const GlyphCacheEntry *GetCachedEntry(const uint32_t code_point,
                                        const int32_t y_size);

  // Update font manager, check glyph cache if the texture atlas needs to be
  // updated.
  // If start_subpass == true,
  // the API uploads the current atlas texture, flushes cache and starts
  // a sub layout pass. Use the feature when the cache is full and needs to
  // flushed during a rendering pass.
  void UpdatePass(const bool start_subpass);

  // Update UV value in the FontBuffer.
  // Returns nullptr if one of UV values couldn't be updated.
  FontBuffer *UpdateUV(const int32_t ysize, FontBuffer *buffer);

  // Convert requested glyph size using SizeSelector if it's set.
  int32_t ConvertSize(const int32_t size);

  // Renderer instance.
  Renderer *renderer_;

  // freetype's fontface instance. In this version of FontManager, it supports
  // only 1 font opened at a time.
  FT_Face face_;

  // harfbuzz's font information instance.
  hb_font_t *harfbuzz_font_;

  // Opened font file data.
  // The file needs to be kept open until FreeType finishes using the file.
  std::string font_data_;

  // flag indicating if a font file has loaded.
  bool face_initialized_;

  // Texture cache for a rendered string image.
  // Using the std::string & its' vertical size in pixels (int32_t) as keys.
  // The map is used for GetTexture() API.
  std::unordered_map<std::string,
                     std::unordered_map<int32_t, std::unique_ptr<FontTexture>>>
      map_textures_;

  // Cache for a texture atlas + vertex array rendering.
  // Using the std:;string & its' vertical size in pixels (int32_t) as keys.
  // The map is used for GetBuffer() API.
  std::unordered_map<std::string,
                     std::unordered_map<int32_t, std::unique_ptr<FontBuffer>>>
      map_buffers_;

  // Singleton instance of Freetype library.
  static FT_Library *ft_;

  // Harfbuzz buffer
  static hb_buffer_t *harfbuzz_buf_;

  // Unique pointer to a glyph cache.
  std::unique_ptr<GlyphCache<uint8_t>> glyph_cache_;

  // Current atlas texture's contents revision.
  uint32_t current_atlas_revision_;

  // Unique pointer to a font atlas texture.
  std::unique_ptr<Texture> atlas_texture_;

  // Current pass counter.
  // Current implementation only supports up to 2 passes in a rendering cycle.
  int32_t current_pass_;

  // Size selector function object used to adjust a glyph size.
  std::function<int32_t(const int32_t)> size_selector_;
};

// Font texture class inherits Texture publicly.
// The class has additional properties for font metrics.
//
// For details of font metrics, refer http://support.microsoft.com/kb/32667
// In this class, ascender and descender values are retrieved from FreeType
// font property.
// And internal/external leading values are updated based on rendering glyph
// information. When rendering a string, the leading values tracks max (min for
// internal leading) value in the string.
class FontMetrics {
 public:
  // Constructor
  FontMetrics()
      : base_line_(0),
        internal_leading_(0),
        ascender_(0),
        descender_(0),
        external_leading_(0) {}

  // Constructor with initialization parameters.
  FontMetrics(const int32_t base_line, const int32_t internal_leading,
              const int32_t ascender, const int32_t descender,
              const int32_t external_leading)
      : base_line_(base_line),
        internal_leading_(internal_leading),
        ascender_(ascender),
        descender_(descender),
        external_leading_(external_leading) {
    assert(internal_leading >= 0);
    assert(ascender >= 0);
    assert(descender <= 0);
    assert(external_leading <= 0);
  }
  ~FontMetrics() {}

  // Setter/Getter of the baseline value.
  int32_t base_line() const { return base_line_; }
  void set_base_line(const int32_t base_line) { base_line_ = base_line; }

  // Setter/Getter of the internal leading parameter.
  int32_t internal_leading() const { return internal_leading_; }
  void set_internal_leading(const int32_t internal_leading) {
    assert(internal_leading >= 0);
    internal_leading_ = internal_leading;
  }

  // Setter/Getter of the ascender value.
  int32_t ascender() const { return ascender_; }
  void set_ascender(const int32_t ascender) {
    assert(ascender >= 0);
    ascender_ = ascender;
  }

  // Setter/Getter of the descender value.
  int32_t descender() const { return descender_; }
  void set_descender(const int32_t descender) {
    assert(descender <= 0);
    descender_ = descender;
  }

  // Setter/Getter of the external leading value.
  int32_t external_leading() const { return external_leading_; }
  void set_external_leading(const int32_t external_leading) {
    assert(external_leading <= 0);
    external_leading_ = external_leading;
  }

  // Returns total height of the glyph.
  int32_t total() const {
    return internal_leading_ + ascender_ - descender_ - external_leading_;
  }

 private:
  // Baseline: Baseline of the glpyhs.
  // When rendering multiple glyphs in the same horizontal group,
  // baselines need be aligned.
  // Most of glyphs fit within the area of ascender + descender.
  // However some glyphs may render in internal/external leading area.
  // (e.g. Å, underlines)
  int32_t base_line_;

  // Internal leading: Positive value that describes the space above the
  // ascender.
  int32_t internal_leading_;

  // Ascender: Positive value that describes the size of the ascender above
  // the baseline.
  int32_t ascender_;

  // Descender: Negative value that describes the size of the descender below
  // the baseline.
  int32_t descender_;

  // External leading : Negative value that describes the space below
  // the descender.
  int32_t external_leading_;
};

// Font texture class
class FontTexture : public Texture {
 public:
  FontTexture(Renderer &renderer) : Texture(renderer) {}
  ~FontTexture() {}

  // Setter/Getter of the metrics parameter of the font texture.
  const FontMetrics &metrics() const { return metrics_; }
  void set_metrics(const FontMetrics &metrics) { metrics_ = metrics; }

 private:
  FontMetrics metrics_;
};

// Font vertex data.
struct FontVertex {
  FontVertex(const float x, const float y, const float z, const float u,
             const float v) {
    position_.data[0] = x;
    position_.data[1] = y;
    position_.data[2] = z;
    uv_.data[0] = u;
    uv_.data[1] = v;
  }
  mathfu::vec3_packed position_;
  mathfu::vec2_packed uv_;
};

// Font buffer class
// Used with the texture atlas rendering.
class FontBuffer {
 public:
  // Constants
  static const int32_t kIndiciesPerCodePoint = 6;
  static const int32_t kVerticesPerCodePoint = 4;

  FontBuffer() : revision_(0) {}

  // Constructor with a buffer sizse.
  FontBuffer(uint32_t size) : revision_(0) {
    indices_.reserve(size * kIndiciesPerCodePoint);
    vertices_.reserve(size * kVerticesPerCodePoint);
    code_points_.reserve(size);
  }
  ~FontBuffer() {}

  // Setter/Getter of the metrics parameter of the font texture.
  const FontMetrics &metrics() const { return metrics_; }
  void set_metrics(const FontMetrics &metrics) { metrics_ = metrics; }

  // Getter of the indices array.
  std::vector<uint32_t> *get_indices() { return &indices_; }
  const std::vector<uint32_t> *get_indices() const { return &indices_; }

  // Getter of the vertices array.
  std::vector<FontVertex> *get_vertices() { return &vertices_; }
  const std::vector<FontVertex> *get_vertices() const { return &vertices_; }

  // Getter of the code points array.
  std::vector<uint32_t> *get_code_points() { return &code_points_; }
  const std::vector<uint32_t> *get_code_points() const { return &code_points_; }

  // Getter/Setter of the size of the string.
  const vec2i &get_size() const { return size_; }
  void set_size(const vec2i &size) { size_ = size; }

  // Getter/Setter of the glyph cache revision counter.
  // Each time a contents of the glyph cache is updated, the revision of the
  // cache is updated.
  // If the cache revision and the buffer revision is different, the
  // font_manager try to re-construct the buffer.
  uint32_t get_revision() const { return revision_; }
  void set_revision(const uint32_t revision) { revision_ = revision; }

  // Getter/Setter of the pass counter.
  // In the render pass, the value is used if the user of the class needs to
  // call StartRenderPass() call to upload the atlas texture.
  int32_t get_pass() const { return pass_; }
  void set_pass(const int32_t pass) { pass_ = pass; }

  // Add 4 vertices used for a glyph rendering to the vertex array.
  void AddVertices(const vec2 &pos, const int32_t base_line, const float scale,
                   const GlyphCacheEntry &entry);

  // Update UV information of a glyph entry.
  // uv vector should include top left corner of UV value as xy, and
  // bottom right of UV value s wz component of the vector.
  void UpdateUV(const int32_t index, const vec4 &uv);

  // Verify sizes of arrays used in the buffer are correct.
  bool Verify() {
    assert(vertices_.size() == code_points_.size() * kVerticesPerCodePoint);
    assert(indices_.size() == code_points_.size() * kIndiciesPerCodePoint);
    return true;
  }

 private:
  // Font metrics information.
  FontMetrics metrics_;

  // Arrays for font indices, vertices and code points.
  // They are hold as a separate vector because OpenGL draw call needs them to
  // be a separate array.

  // Indices of the font buffer.
  std::vector<uint32_t> indices_;

  // Vertices data of the font buffer.
  std::vector<FontVertex> vertices_;

  // Code points used in the buffer. This array is used to fetch and update UV
  // entries when the glyph cache is flushed.
  std::vector<uint32_t> code_points_;

  // Size of the string in pixels.
  vec2i size_;

  // Revision of the FontBuffer corresponding glyph cache revision.
  // Caller needs to check the revision value if glyph texture has referencing
  // entries by checking the revision.
  uint32_t revision_;

  // Pass id. Each pass should have it's own texture atlas contents.
  int32_t pass_;
};

}  // namespace fpl

#endif  // FONT_MANAGER_H
