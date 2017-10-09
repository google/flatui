// Copyright 2015 Google Inc. All rights reserved.
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

#include <memory>
#include <set>
#include <sstream>
#include "fplbase/renderer.h"
#include "fplutil/mutex.h"
#include "flatui/font_buffer.h"
#include "flatui/font_util.h"
#include "flatui/version.h"
#include "flatui/internal/distance_computer.h"
#include "flatui/internal/glyph_cache.h"
#include "flatui/internal/flatui_util.h"
#include "flatui/internal/hb_complex_font.h"
#include "flatui/internal/hyphenator.h"

#if defined(__APPLE__) || defined(__ANDROID__)
#define FLATUI_SYSTEM_FONT (1)
#endif  // defined(__APPLE__) || defined(__ANDROID__)

// Forward decls for FreeType.
typedef struct FT_LibraryRec_ *FT_Library;
typedef struct FT_GlyphSlotRec_ *FT_GlyphSlot;
typedef unsigned long FT_ULong;

// Forward decls for Harfbuzz.
typedef const struct hb_language_impl_t *hb_language_t;
/// @endcond

/// @brief Namespace for FlatUI library.
namespace flatui {

/// @file
/// @addtogroup flatui_font_manager
/// @{

/// @cond FLATUI_INTERNAL
// Forward decl.
class FaceData;
class FontTexture;
class FontBuffer;
class FontBufferContext;
class FontMetrics;
class WordEnumerator;
struct ScriptInfo;
/// @endcond

/// @var kVerticesPerGlyph/kIndicesPerGlyph
///
/// @brief The number of vertices/indices per a glyph entry.
const int32_t kVerticesPerGlyph = 4;
const int32_t kIndicesPerGlyph = 6;

/// @var kIndexOfLeftEdge/kIndexOfRightEdge
///
/// @brief The offset to the index of a vertex for the previous glyph's
/// left or right edge in a font_buffer.
const int32_t kVertexOfLeftEdge = -3;
const int32_t kVertexOfRightEdge = -1;

/// @var kGlyphCacheWidth
///
/// @brief The default size of the glyph cache width.
const int32_t kGlyphCacheWidth = 1024;

/// @var kGlyphCacheHeight
///
/// @brief The default size of the glyph cache height.
const int32_t kGlyphCacheHeight = 1024;

/// @var kGlyphCacheMaxSlices
///
/// @brief The default size of the max glyph cache slices. The number of cache
/// slices grows up to the value.
const int32_t kGlyphCacheMaxSlices = 4;

/// @var kDefaultLanguage
///
/// @brief The default language used for a line break.
const char *const kDefaultLanguage = "en";

#ifdef FLATUI_SYSTEM_FONT
/// @var kSystemFont
///
/// @brief A constant to spefify loading a system font. Used with OpenFont() and
/// SelectFont() API
/// Currently the system font is supported on iOS/macOS and Android only.
const char *const kSystemFont = ".SystemFont";
static const HashedId kSystemFontId = HashId(kSystemFont);
#endif  // FLATUI_SYSTEM_FONT

/// @class FontManager
///
/// @brief FontManager manages font rendering with OpenGL utilizing freetype
/// and harfbuzz as a glyph rendering and layout back end.
///
/// It opens speficied OpenType/TrueType font and rasterize to OpenGL texture.
/// An application can use the generated texture for a text rendering.
///
/// @warning The class is not threadsafe, it's expected to be only used from
/// within OpenGL rendering thread.
class FontManager {
 public:
  /// @brief The default constructor for FontManager.
  FontManager();

  /// @brief Constructor for FontManager with a given cache size.
  ///
  /// @note The given size is rounded up to nearest power of 2 internally to be
  /// used as an OpenGL texture sizes.
  ///
  /// @param[in] cache_size The size of the cache, in pixels as x & y values.
  /// @param[in] max_slices The maximum number of cache slices. When a glyph
  /// slice gets full with glyph entries, the cache allocates another slices for
  /// more cache spaces up to the value.
  FontManager(const mathfu::vec2i &cache_size, int32_t max_slices);

  /// @brief The destructor for FontManager.
  ~FontManager();

  /// @brief Open a font face, TTF, OT font.
  ///
  /// In this version it supports only single face at a time.
  ///
  /// @param[in] font_name A C-string in UTF-8 format representing
  /// the name of the font.
  /// @return Returns `false` when failing to open font, such as
  /// a file open error, an invalid file format etc. Returns `true`
  /// if the font is opened successfully.
  bool Open(const char *font_name);

  /// @brief Open a font face, TTF, OT font by FontFamily structure.
  /// Use this version of API when opening a font with a family name, with a
  /// font collection index etc.
  ///
  /// @param[in] family A FontFamily structure indicating font parameters.
  /// @return Returns `false` when failing to open font, such as
  /// a file open error, an invalid file format etc. Returns `true`
  /// if the font is opened successfully.
  bool Open(const FontFamily &family);

  /// @brief Discard a font face that has been opened via `Open()`.
  ///
  /// @param[in] font_name A C-string in UTF-8 format representing
  /// the name of the font.
  ///
  /// @return Returns `true` if the font was closed successfully. Otherwise
  /// it returns `false`.
  bool Close(const char *font_name);

  /// @brief Discard a font face that has been opened via `Open()`.
  /// Use this version of API when closeing a font opened with FontFamily.
  ///
  /// @param[in] family A FontFamily structure indicating font parameters.
  ///
  /// @return Returns `true` if the font was closed successfully. Otherwise
  /// it returns `false`.
  bool Close(const FontFamily &family);

  /// @brief Select the current font face. The font face will be used by a glyph
  /// rendering.
  ///
  /// @note The font face needs to have been opened by `Open()`.
  ///
  /// @param[in] font_name A C-string in UTF-8 format representing
  /// the name of the font.
  ///
  /// @return Returns `true` if the font was selected successfully. Otherwise it
  /// returns false.
  bool SelectFont(const char *font_name);

  /// @brief Select the current font face with a FontFamily.
  bool SelectFont(const FontFamily &family);

  /// @brief Select the current font faces with a fallback priority.
  ///
  /// @param[in] font_names An array of C-string corresponding to the name of
  /// the font. Font names in the array are stored in a priority order.
  /// @param[in] count A count of font names in the array.
  ///
  /// @return Returns `true` if the fonts were selected successfully. Otherwise
  /// it returns false.
  bool SelectFont(const char *font_names[], int32_t count);

  /// @brief Select the current font faces with a fallback priority.
  ///
  /// @param[in] family_names An array of FontFamily structure that holds font
  /// family information.
  /// @param[in] count A count of font family in the array.
  ///
  /// @return Returns `true` if the fonts were selected successfully. Otherwise
  /// it returns false.
  bool SelectFont(const FontFamily family_names[], int32_t count);

  /// @brief Retrieve a vertex buffer for a font rendering using glyph cache.
  ///
  /// @param[in] text A C-string in UTF-8 format with the text for the
  /// FontBuffer.
  /// @param[in] length The length of the text string.
  /// @param[in] parameters The FontBufferParameters specifying the parameters
  /// for the FontBuffer.
  ///
  /// @return Returns `nullptr` if the string does not fit in the glyph cache.
  ///  When this happens, caller may flush the glyph cache with
  /// `FlushAndUpdate()` call and re-try the `GetBuffer()` call.
  FontBuffer *GetBuffer(const char *text, size_t length,
                        const FontBufferParameters &parameters);

  /// @brief Retrieve a vertex buffer for basic HTML rendering.
  ///
  /// @param[in] html A C-string in UTF-8 format with the HTML to be rendered.
  /// Note that we support only a limited subset of HTML at the moment,
  /// including anchors, paragraphs, and breaks.
  /// @param[in] parameters The FontBufferParameters specifying the parameters
  /// for the FontBuffer.
  ///
  /// @note call get_links() on the returned FontBuffer to receive information
  /// on where the anchor links are located, and the linked-to addresses.
  ///
  /// @return Returns `nullptr` if the HTML does not fit in the glyph cache.
  ///  When this happens, caller may flush the glyph cache with
  /// `FlushAndUpdate()` call and re-try the `GetBuffer()` call.
  FontBuffer *GetHtmlBuffer(const char *html,
                            const FontBufferParameters &parameters);

  /// @brief Release the FonBuffer instance.
  /// If the FontBuffer is a reference counting buffer, the API decrements the
  /// reference count of the buffer and when it reaches 0, the buffer becomes
  /// invalid.
  /// If the buffer is non-reference counting buffer, the buffer is invalidated
  /// immediately and all references to the FontBuffer with same parameters
  /// become invalid. It is not a problem for those non-reference counting
  /// buffers because they are expected to create/fetch a buffer each render
  /// cycle.
  void ReleaseBuffer(FontBuffer *buffer);

  /// @brief Optionally flush the glyph cache and update existing FontBuffer's
  /// UV information.
  /// The API doesn't change a reference count of buffers.
  /// Note that current implementation references font instances and it is the
  /// caller's responsibility not to close font files in use.
  ///
  /// @param[in] flush_cache set true to flush existing glyph cache contents.
  void RemapBuffers(bool flush_cache);

  /// @return Returns `true` if a font has been loaded. Otherwise it returns
  /// `false`.
  bool FontLoaded() { return face_initialized_; }

  /// @brief Indicate a start of new layout pass.
  ///
  /// Call the API each time the user starts a layout pass.
  ///
  /// @note The user of the font_manager class is expected to use the class in
  /// 2 passes: a layout pass and a render pass.
  ///
  /// During the layout pass, the user invokes `GetBuffer()` calls to fetch all
  /// required glyphs into the cache. Once in the cache, the user can retrieve
  /// the size of the strings to position them.
  ///
  /// Once the layout pass finishes, the user invokes `StartRenderPass()` to
  /// upload cache images to the atlas texture, and use the texture in the
  /// render pass. This design helps to minimize the frequency of the atlas
  /// texture upload.
  void StartLayoutPass();

  /// @brief Flush the existing glyph cache contents and start new layout pass.
  ///
  /// Call this API while in a layout pass when the glyph cache is fragmented.
  // Returns false if the API failed to acquire a mutex when it's running in
  // multithreaded mode. In that case, make sure the caller invokes the API
  // repeatedly.
  bool FlushAndUpdate() { return UpdatePass(true); }

  /// @brief Flush the existing FontBuffer in the cache.
  ///
  /// Call this API when FontBuffers are not used anymore.
  void FlushLayout() {
    for (auto it = map_buffers_.begin(); it != map_buffers_.end(); it++) {
      // Erase only non-ref-counted buffers.
      if (!it->first.get_ref_count_flag()) {
        map_buffers_.erase(it);
      }
    }
  }

  /// @brief Indicates a start of new render pass.
  ///
  /// Call the API each time the user starts a render pass.
  // Returns false if the API failed to acquire a mutex when it's running in
  // multithreaded mode. In that case, make sure the caller invokes the API
  // repeatedly.
  bool StartRenderPass() { return UpdatePass(false); }

  /// @return Returns font atlas texture.
  ///
  /// @param[in] slice an index indicating an atlas texture.
  /// An index with kGlyphFormatsColor indicates that the slice is a multi
  /// channel buffer for color glyphs.
  fplbase::Texture *GetAtlasTexture(int32_t slice) {
    // Note: We don't need to lock cache_mutex_ because we're just returning
    //       a GL texture in an array. No need to lock the GL thread.
    if (!(slice & kGlyphFormatsColor)) {
      return glyph_cache_->get_monochrome_buffer()->get_texture(slice);
    } else {
      return glyph_cache_->get_color_buffer()->get_texture(slice &
                                                           ~kGlyphFormatsColor);
    }
  }

  /// @brief The user can supply a size selector function to adjust glyph sizes
  /// when storing a glyph cache entry. By doing that, multiple strings with
  /// slightly different sizes can share the same glyph cache entry, so that the
  /// user can reduce a total # of cached entries.
  ///
  /// @param[in] selector The callback argument is a request for the glyph
  /// size in pixels. The callback should return the desired glyph size that
  /// should be used for the glyph sizes.
  void SetSizeSelector(std::function<int32_t(const int32_t)> selector) {
    size_selector_.swap(selector);
  }

  /// @param[in] locale  A C-string corresponding to the of the
  /// language defined in ISO 639 and the country code difined in ISO 3166
  /// separated
  /// by '-'. (e.g. 'en-US').
  ///
  /// @note The API sets language, script and layout direction used for
  /// following text renderings based on the locale.
  void SetLocale(const char *locale);

  /// @return Returns the current language setting as a std::string.
  const char *GetLanguage() { return language_.c_str(); }

  /// @brief Set a script used for a script layout.
  ///
  /// @param[in] language ISO 15924 based script code. Default setting is
  /// 'Latn' (Latin).
  /// For more detail, refer http://unicode.org/iso15924/iso15924-codes.html
  ///
  void SetScript(const char *script);

  /// @brief Set a script layout direction.
  ///
  /// @param[in] direction Text layout direction.
  ///            TextLayoutDirectionLTR & TextLayoutDirectionRTL are supported.
  ///
  void SetLayoutDirection(const TextLayoutDirection direction) {
    if (direction == kTextLayoutDirectionTTB) {
      fplbase::LogError("TextLayoutDirectionTTB is not supported yet.");
      return;
    }

    layout_direction_ = direction;
  }

  /// @brief Set up hyphenation pattern path.
  /// Since the hyphenation pattern is different per locale, current locale
  /// needs to be set properly for the hyphnation to work.
  ///
  /// @param[in] hyb_path A file path to search the hyb (dictionary files
  /// required for the hyphenation process. On Android,
  /// "/system/usr/hyphen-data" is the default search path.
  ///
  void SetupHyphenationPatternPath(const char *hyb_path) {
    if (hyb_path != nullptr) {
      hyb_path_ = hyb_path;
    }
    if (!hyphenation_rule_.empty() && !hyb_path_.empty()) {
      std::string pattern_file =
          hyb_path_ + "/hyph-" + hyphenation_rule_ + ".hyb";
      hyphenator_.Open(pattern_file.c_str());
    }
  }

  /// @return Returns the current layout direciton.
  TextLayoutDirection GetLayoutDirection() { return layout_direction_; }

  /// @return Returns the current font.
  HbFont *GetCurrentFont() { return current_font_; }

  /// @brief Enable an use of color glyphs in supporting OTF/TTF font.
  /// The API will initialize internal glyph cache for color glyphs.
  void EnableColorGlyph();

  /// @brief Set an ellipsis string used in label/edit widgets.
  ///
  /// @param[in] ellipsis A C-string specifying characters used as an ellipsis.
  /// Can be multiple characters, typically '...'. When a string in a widget
  /// doesn't fit to the given size, the string is truncated to fit the ellipsis
  /// string appended at the end.
  /// @param[in] mode A flag controlling appending behavior of the ellipsis.
  /// Default is kEllipsisModeTruncateCharacter.
  ///
  /// Note: FontManager doesn't support dynamic change of the ellipsis string
  /// in current version. FontBuffer contents that has been created are not
  /// updated when the ellipsis string is changed.
  void SetTextEllipsis(const char *ellipsis,
                       EllipsisMode mode = kEllipsisModeTruncateCharacter) {
    ellipsis_ = ellipsis;
    ellipsis_mode_ = mode;
  }

  /// @brief Check a status of the font buffer.
  /// @param[in] font_buffer font buffer to check.
  ///
  /// @return Returns a status of the FontBuffer if it's ready to use with
  /// current texture atlas contents.
  FontBufferStatus GetFontBufferStatus(const FontBuffer &font_buffer) const;

  /// @brief Factory function for the DistanceComputer.
  static DistanceComputer<uint8_t>*(*DistanceComputerFactory)(void);

 private:
  // Pass indicating rendering pass.
  static const int32_t kRenderPass = -1;

  // Error identifiers.
  enum ErrorType {
    kErrorTypeSuccess = 0,
    kErrorTypeMissingGlyph,
    kErrorTypeCacheIsFull,
  };

  // Initialize static data associated with the class.
  void Initialize();

  // Clean up static data associated with the class.
  // Note that after the call, an application needs to call Initialize() again
  // to use the class.
  static void Terminate();

  // Layout text and update harfbuzz_buf_.
  // Returns the width of the text layout in pixels.
  int32_t LayoutText(const char *text, size_t length, int32_t max_width = 0,
                     int32_t current_width = 0, bool last_line = false,
                     bool enable_hyphenation = false,
                     int32_t *rewind = nullptr);

  // Helper function to add string information to the buffer.
  ErrorType UpdateBuffer(const WordEnumerator &word_enum,
                         const FontBufferParameters &parameters,
                         int32_t base_line, FontBuffer *buffer,
                         FontBufferContext *context, mathfu::vec2 *pos,
                         FontMetrics *metrics);

  // Helper function to determine how many entries to remove from the buffer.
  bool NeedToRemoveEntries(const FontBufferParameters &parameters,
                           uint32_t required_width, const FontBuffer *buffer,
                           size_t entry_index) const;

  // Helper function to remove entries from the buffer for specified width.
  void RemoveEntries(const FontBufferParameters &parameters,
                     uint32_t required_width, FontBuffer *buffer,
                     FontBufferContext *context, mathfu::vec2 *pos);

  // Helper function to append ellipsis to the buffer.
  ErrorType AppendEllipsis(const WordEnumerator &word_enum,
                           const FontBufferParameters &parameters,
                           int32_t base_line, FontBuffer *buffer,
                           FontBufferContext *context, mathfu::vec2 *pos,
                           FontMetrics *metrics);

  // Calculate internal/external leading value and expand a buffer if
  // necessary.
  // Returns true if the size of metrics has been changed.
  bool UpdateMetrics(int32_t top, int32_t height,
                     const FontMetrics &current_metrics,
                     FontMetrics *new_metrics);

  // Retrieve cached entry from the glyph cache.
  // If an entry is not found in the glyph cache, the API tries to create new
  // cache entry and returns it if succeeded.
  // Returns nullptr and sets *error if,
  // - The font doesn't have the requested glyph.
  // - The glyph doesn't fit into the cache (even after trying to evict some
  // glyphs in cache based on LRU rule).
  // (e.g. Requested glyph size too large or the cache is highly fragmented.)
  const GlyphCacheEntry *GetCachedEntry(uint32_t code_point, uint32_t y_size,
                                        GlyphFlags flags, ErrorType *error);

  // Update font manager, check glyph cache if the texture atlas needs to be
  // updated.
  // If start_subpass == true,
  // the API uploads the current atlas texture, flushes cache and starts
  // a sub layout pass. Use the feature when the cache is full and needs to
  // flushed during a rendering pass.
  // Returns false if the API failed to acquire a mutex when it's running in
  // multithreaded mode. In that case, make sure the caller invokes the API
  // repeatedly.
  bool UpdatePass(bool start_subpass);

  // Update UV value in the FontBuffer.
  // Returns nullptr if one of UV values couldn't be updated.
  FontBuffer *UpdateUV(GlyphFlags flags, FontBuffer *buffer);

  // Convert requested glyph size using SizeSelector if it's set.
  int32_t ConvertSize(int32_t size);

  // Retrieve a caret count in a specific glyph from linebreak and halfbuzz
  // glyph information.
  int32_t GetCaretPosCount(const WordEnumerator &enumerator,
                           const hb_glyph_info_t *info, int32_t glyph_count,
                           int32_t index);

  // The start position is different depending on LTR or RTL
  // TextLayoutDirection.
  mathfu::vec2 GetStartPosition(const FontBufferParameters &parameters) const;

  // Create FontBuffer with requested parameters.
  // The function may return nullptr if the glyph cache is full or a glyph fails
  // to load.
  FontBuffer *CreateBuffer(const char *text, uint32_t length,
                           const FontBufferParameters &parameters,
                           mathfu::vec2 *text_pos = nullptr,
                           ErrorType *error = nullptr);

  // Check if the requested buffer already exist in the cache.
  FontBuffer *FindBuffer(const FontBufferParameters &parameters);

  // Fill in the FontBuffer with the given text.
  FontBuffer *FillBuffer(const char *text, uint32_t length,
                         const FontBufferParameters &parameters,
                         FontBuffer *buffer, FontBufferContext *context,
                         mathfu::vec2 *text_pos = nullptr,
                         ErrorType *error = nullptr);

  // Update language related settings.
  void SetLanguageSettings();

  // Hyphenate given string and layout it.
  int32_t Hyphenate(const char *text, size_t length, int32_t available_space,
                    int32_t *rewind);

  // Apply HtmlFontSection settings while parsing HTML text.
  void SetFontProperties(const HtmlSection &font_section,
                         FontBufferParameters *param, FontBufferContext *ctx);

  // Look up a supported locale.
  // Returns nullptr if the API doesn't find the specified locale.
  const ScriptInfo *FindLocale(const char *locale);

  // Check if speficied language is supported in the font manager engine.
  bool IsLanguageSupported(const char *language);

  /// @brief Set a line height for a multi-line text.
  ///
  /// @param[in] line_height A float representing the line height for a
  /// multi-line text.
  void SetLineHeightScale(float line_height_scale) {
    line_height_scale_ = line_height_scale;
  }

  /// @brief Set a kerning scale value.
  ///
  /// @param[in] kerning_scale A float representing the kerning scale value
  /// applied to the kerning values retrieved from Harfbuzz used in the text
  /// rendering.
  void SetKerningScale(float kerning_scale) { kerning_scale_ = kerning_scale; }

  /// @brief  Retrieve the system's font fallback list and all fonts in the
  /// list.
  /// The implementation is platform specific.
  /// @return true if the system font is successfully opened.
  bool OpenSystemFont();

  /// @brief  Helper function to check font coverage.
  /// @param[in] face FreeType face to check font coverage.
  /// @param[out] font_coverage A set updated for the coverage map of the face.
  /// @return true if the specified font has a new glyph entry.
  bool UpdateFontCoverage(FT_Face face, std::set<FT_ULong> *font_coverage);

/// @brief Platform specific implementation of a system font access.
#ifdef __APPLE__
  bool OpenSystemFontApple();
  bool CloseSystemFontApple();
#endif  // __APPLE__
#ifdef __ANDROID__
  bool OpenSystemFontAndroid();
  bool CloseSystemFontAndroid();
  void ReorderSystemFonts(std::vector<FontFamily> *font_list) const;
#endif  // __ANDROID__

  /// @brief  Close all fonts in the system's font fallback list opened by
  /// OpenSystemFont().
  bool CloseSystemFont();

  // flag indicating if a font file has loaded.
  bool face_initialized_;

  // Map that keeps opened face data instances.
  std::unordered_map<std::string, std::unique_ptr<FaceData>> map_faces_;

  // Used to cache font instances. Refers to memory owned by map_faces_.
  HbFontCache font_cache_;

  // Pointer to active font face instance.
  HbFont *current_font_;

  // Cache for a texture atlas + vertex array rendering.
  // Using the FontBufferParameters as keys.
  // The map is used for GetBuffer() API.
  std::map<FontBufferParameters, std::unique_ptr<FontBuffer>,
           FontBufferParameters> map_buffers_;

  // Instance of Freetype library.
  std::unique_ptr<FT_Library> ft_;

  // Harfbuzz buffer.
  hb_buffer_t *harfbuzz_buf_;

  // Unique pointer to a glyph cache.
  std::unique_ptr<GlyphCache> glyph_cache_;

  // Current atlas texture's contents revision.
  int32_t current_atlas_revision_;
  // A revision that the atlas texture is flushed last time.
  int32_t atlas_last_flush_revision_;

  // Current pass counter.
  // Current implementation only supports up to 2 passes in a rendering cycle.
  int32_t current_pass_;

  // Size selector function object used to adjust a glyph size.
  std::function<int32_t(const int32_t)> size_selector_;

  // Language of input strings.
  // Used to determine line breaking depending on a language.
  uint32_t script_;
  std::string language_;
  std::string locale_;
  TextLayoutDirection layout_direction_;
  static const ScriptInfo script_table_[];
  static const char *language_table_[];
  hb_language_t hb_language_;

  // Line height for a multi line text.
  float line_height_scale_;
  float kerning_scale_;

  // Current line width. Needs to be persistent while appending buffers.
  int32_t line_width_;

  // Ellipsis settings.
  std::string ellipsis_;
  EllipsisMode ellipsis_mode_;

  // Hyphenation settings.
  std::string hyb_path_;
  std::string hyphenation_rule_;
  Hyphenator hyphenator_;

  // Line break info buffer used in libunibreak.
  std::vector<char> wordbreak_info_;

  // A buffer includes font face index of the current font's faces.
  std::vector<int32_t> fontface_index_;

  // An instance of signed distance field generator.
  // To avoid redundant initializations, the FontManager holds an instnce of the
  // class.
  std::unique_ptr<DistanceComputer<uint8_t>> sdf_computer_;

  // Mutex guarding glyph cache's buffer access.
  fplutil::Mutex *cache_mutex_;

  // A font fallback list retrieved from the current system.
  std::vector<FontFamily> system_fallback_list_;

  const FlatUIVersion *version_;
};

/// @struct ScriptInfo
///
/// @brief This struct holds the script information used for a text layout.
///
struct ScriptInfo {
  /// @var locale
  /// @brief A C-string corresponding to the of the
  /// language defined in ISO 639 and the country code difined in ISO 3166
  /// separated
  /// by '-'. (e.g. 'en-US').
  const char *locale;

  /// @var script
  /// @brief ISO 15924 Script code.
  const char *script;

  /// @var hyphenation
  /// @brief Hyphenation rule.
  const char *hyphenation;

  /// @var direction
  /// @brief Script layout direciton.
  TextLayoutDirection direction;
};
/// @}

/// @class FontShader
///
/// @brief Helper class to handle shaders for a font rendering.
/// The class keeps a reference to a shader, and a location of uniforms with a
/// fixed names.
/// A caller is responsive not to call set_* APIs that specified shader doesn't
/// support.
class FontShader {
 public:
  void set(fplbase::Shader *shader) {
    assert(shader);
    shader_ = shader;
    pos_offset_ = shader->FindUniform("pos_offset");
    color_ = shader->FindUniform("color");
    clipping_ = shader->FindUniform("clipping");
    threshold_ = shader->FindUniform("threshold");
  }
  void set_renderer(fplbase::Renderer *renderer) {
    renderer->SetShader(shader_);
  }

  void set_position_offset(const mathfu::vec3 &vec) {
    assert(fplbase::ValidUniformHandle(pos_offset_));
    shader_->SetUniform(pos_offset_, vec);
  }
  void set_color(const mathfu::vec4 &vec) {
    assert(fplbase::ValidUniformHandle(color_));
    shader_->SetUniform(color_, vec);
  }
  void set_clipping(const mathfu::vec4 &vec) {
    assert(fplbase::ValidUniformHandle(clipping_));
    shader_->SetUniform(clipping_, vec);
  }
  void set_threshold(float f) {
    assert(fplbase::ValidUniformHandle(threshold_));
    shader_->SetUniform(threshold_, &f, 1);
  }

  fplbase::UniformHandle clipping_handle() { return clipping_; }
  fplbase::UniformHandle color_handle() { return color_; }
  fplbase::UniformHandle position_offset_handle() { return pos_offset_; }
  fplbase::UniformHandle threshold_handle() { return threshold_; }

 private:
  fplbase::Shader *shader_;
  fplbase::UniformHandle pos_offset_;
  fplbase::UniformHandle color_;
  fplbase::UniformHandle clipping_;
  fplbase::UniformHandle threshold_;
};
/// @}

}  // namespace flatui

#endif  // FONT_MANAGER_H
