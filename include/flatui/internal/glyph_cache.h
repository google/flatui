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

#ifndef GLYPH_CACH_H
#define GLYPH_CACH_H

#include <list>
#include <map>
#include <set>
#include <unordered_map>

#include "flatui_util.h"
#include "fplbase/texture.h"
#include "fplbase/utilities.h"
#include "mathfu/constants.h"

using fplbase::LogInfo;

namespace flatui {

/// @cond FLATUI_INTERNAL

// The glyph cache maintains a list of GlyphCacheRow. Each row has a fixed sizes
// of height, which is determined at a row creation time. A row can include
// multiple GlyphCacheEntry with a same or smaller height and they can have
// variable width. In a row,
// GlyphCacheEntry are stored from left to right in the order of registration
// and won't be evicted per entry, but entire row is flushed when necessary to
// make a room for new GlyphCacheEntry.
// The purpose of this design is to cache as many glyphs and to achieve high
// caching perfomance estimating same size of glphys tends to be stored in a
// cache at same time. (e.g. Caching a string in a same size.)
//
// When looking up a cached entry, the API looks up unordered_map which is O(1)
// operation.
// If there is no cached entry for given code point, the caller needs to invoke
// Set() API to fill in a cache.
// Set() operation takes
// O(log N (N=# of rows)) when there is a room in the cache for the request,
// + O(N (N=# of rows)) to look up and evict least recently used row with
// sufficient height.

// Enable tracking stats in Debug build.
#ifdef _DEBUG
#define GLYPH_CACHE_STATS (1)
#endif

// Forward decl.
class FontBuffer;
class GlyphCache;
class GlyphCacheRow;
class GlyphCacheEntry;
class GlyphKey;
class GlyphCacheBufferBase;
template <typename T>
class GlyphCacheBuffer;

// Constants for a cache entry size rounding up and padding between glyphs.
// Adding a padding between cached glyph images to avoid sampling artifacts of
// texture fetches.
const int32_t kGlyphCacheHeightRound = 4;
const int32_t kGlyphCachePaddingX = 1;
const int32_t kGlyphCachePaddingY = 1;
const int32_t kGlyphCachePaddingSDF = 4;

// Flags that controls glyph image generation.
enum GlyphFlags {
  kGlyphFlagsNone = 0,      // Normal glyph.
  kGlyphFlagsOuterSDF = 1,  // Glyph image with an outer SDF information.
  kGlyphFlagsInnerSDF = 2,  // Glyph image with an inner SDF information.
};

// Flags that indicates glyph cache image format.
enum GlyphFormats {
  kGlyphFormatsMono = 0,            // Single channel monochrome glyph.
  kGlyphFormatsColor = 0x80000000,  // Color glyph.
};

const float kSDFThresholdDefault = 16.0f / 255.0f;

// Bitwise OR operator for GlyphFlags enumeration.
inline GlyphFlags operator|(GlyphFlags a, GlyphFlags b) {
  return static_cast<GlyphFlags>(static_cast<int>(a) | static_cast<int>(b));
}

// Struct tracking glyph cache usage stats for a debug support.
struct GlyphCacheStats {
#ifdef GLYPH_CACHE_STATS
  // Variables to track usage stats.
  int32_t lookup_;     // Number of cache look-up happened.
  int32_t hit_;        // Number of cache hit happened during cache look-up.
  int32_t row_flush_;  // Number of cache row flush happened.
  int32_t set_fail_;   // Occurance of cache look up failure.
                       // This would involve full cache flush.
#endif                 // GLYPH_CACHE_STATS
};

// Class that includes glyph parameters.
class GlyphKey {
 public:
  // Constructors.
  GlyphKey()
      : font_id_(kNullHash),
        code_point_(0),
        glyph_size_(0),
        flags_(kGlyphFlagsNone) {}
  GlyphKey(const HashedId font_id, uint32_t code_point, uint32_t glyph_size,
           GlyphFlags flags) {
    font_id_ = font_id;
    code_point_ = code_point;
    glyph_size_ = glyph_size;
    flags_ = flags;
  }

  // Compare operator.
  bool operator==(const GlyphKey& other) const {
    return (code_point_ == other.code_point_ && font_id_ == other.font_id_ &&
            glyph_size_ == other.glyph_size_ && flags_ == other.flags_);
  }

  // Hash function.
  size_t operator()(const GlyphKey& key) const {
    // Note that font_id_ is an already hashed value.
    auto value =
        ((std::hash<uint32_t>()(key.code_point_) ^ (key.font_id_ << 1)) >> 1) ^
        (std::hash<uint32_t>()(key.glyph_size_) << 1);
    value = value ^ (std::hash<int32_t>()(key.flags_) << 1) >> 1;
    return value;
  }

 private:
  HashedId font_id_;
  uint32_t code_point_;
  uint32_t glyph_size_;
  GlyphFlags flags_;
};

// Cache entry for a glyph.
class GlyphCacheEntry {
 public:
  // Typedef for cache entry map's iterator.
  typedef std::unordered_map<GlyphKey, std::unique_ptr<GlyphCacheEntry>,
                             GlyphKey>::iterator iterator;
  typedef std::list<GlyphCacheRow>::iterator iterator_row;

  GlyphCacheEntry()
      : code_point_(0), size_(0, 0), offset_(0, 0), color_glyph_(false) {}

  // Setter/Getter of code point.
  // Code point is an entry in a font file, not a direct transform of Unicode.
  uint32_t get_code_point() const { return code_point_; }
  void set_code_point(uint32_t code_point) { code_point_ = code_point; }

  // Setter/Getter of cache entry size.
  mathfu::vec2i get_size() const { return size_; }
  void set_size(const mathfu::vec2i& size) { size_ = size; }

  // Setter/Getter of cache entry offset.
  mathfu::vec2i get_offset() const { return offset_; }
  void set_offset(const mathfu::vec2i& offset) { offset_ = offset; }

  // Setter/Getter of the cache entry position.
  mathfu::vec3i get_pos() const { return pos_; }
  void set_pos(const mathfu::vec3i& pos) { pos_ = pos; }

  // Setter/Getter of UV
  mathfu::vec4 get_uv() const { return uv_; }
  void set_uv(const mathfu::vec4& uv) { uv_ = uv; }

  MATHFU_DEFINE_CLASS_SIMD_AWARE_NEW_DELETE

  // Setter/Getter of a glyph cache row holding the entry.
  GlyphCacheEntry::iterator_row get_row() const { return it_row_; }
  void set_row(GlyphCacheEntry::iterator_row row) { it_row_ = row; }

  // Setter/Getter of color glyph information.
  bool get_color_glyph() const { return color_glyph_; }
  void set_color_glyph(bool b) { color_glyph_ = b; }

 private:
  // Friend class, GlyphCache needs an access to internal variables of the
  // class.
  friend class GlyphCache;

  // Code point of the glyph.
  uint32_t code_point_;

  // Cache entry sizes.
  mathfu::vec2i size_;

  // Glyph image's offset value relative to font metrics origin.
  mathfu::vec2i offset_;

  // Glyph image's UV in the texture atlas.
  mathfu::vec4 uv_;

  // Glyph image's position value in the buffer.
  mathfu::vec3i pos_;

  // Iterator to the row entry.
  GlyphCacheEntry::iterator_row it_row_;

  // Iterator to the row LRU entry.
  std::list<GlyphCacheEntry::iterator_row>::iterator it_lru_row_;

  // Pointer to the GlyphCacheBufferBase.
  GlyphCacheBufferBase* buffer_;

  // Flag indicating if the glyph is color glyph.
  bool color_glyph_;
};

// Single row in a cache. A row correspond to a horizontal slice of a texture.
// (e.g if a texture has a 256x256 of size, and a row has a max glyph height of
// 16, the row corresponds to 256x16 pixels of the overall texture.)
//
// One cache row contains multiple GlyphCacheEntry with a same or smaller
// height. GlyphCacheEntry entries are stored from left to right and not evicted
// per glyph, but entire row at once for a performance reason.
// GlyphCacheRow is an internal class for GlyphCache.
class GlyphCacheRow {
 public:
  GlyphCacheRow() { Initialize(0, 0, mathfu::vec2i(0, 0)); }
  // Constructor with an arguments.
  // slice : an index indicating a glyph cache slice.
  // y_pos : vertical position of the row in the buffer.
  // witdh : width of the row. Typically same value of the buffer width.
  // height : height of the row.
  GlyphCacheRow(int32_t slice, int32_t y_pos, const mathfu::vec2i& size) {
    Initialize(slice, y_pos, size);
  }
  ~GlyphCacheRow() {}

  // Initialize the row width and height.
  void Initialize(int32_t slice, int32_t y_pos, const mathfu::vec2i& size) {
    slice_ = slice;
    last_used_counter_ = 0;
    y_pos_ = y_pos;
    remaining_width_ = size.x();
    size_ = size;
    cached_entries_.clear();
  }

  // Check if the row has a room for a requested width and height.
  bool DoesFit(const mathfu::vec2i& size) const {
    return !(size.x() > remaining_width_ || size.y() > size_.y());
  }

  // Reserve an area in the row.
  int32_t Reserve(const GlyphCacheEntry::iterator it,
                  const mathfu::vec2i& size) {
    assert(DoesFit(size));

    // Update row info.
    int32_t pos = size_.x() - remaining_width_;
    remaining_width_ -= size.x();
    cached_entries_.push_back(it);
    return pos;
  }

  // Setter/Getter of last used counter.
  uint32_t get_last_used_counter() const { return last_used_counter_; }
  void set_last_used_counter(uint32_t counter) { last_used_counter_ = counter; }

  // Setter/Getter of row size.
  mathfu::vec2i get_size() const { return size_; }
  void set_size(const mathfu::vec2i size) { size_ = size; }

  // Setter/Getter of row y pos.
  int32_t get_y_pos() const { return y_pos_; }
  void set_y_pos(int32_t y_pos) { y_pos_ = y_pos; }

  // Setter/Getter of row's slice.
  int32_t get_slice() const { return slice_; }
  void set_slice(int32_t slice) { slice_ = slice; }

  // Getter of cached glyphs.
  size_t get_num_glyphs() const { return cached_entries_.size(); }

  // Setter/Getter of iterator to row LRU.
  const std::list<GlyphCacheEntry::iterator_row>::iterator get_it_lru_row()
      const {
    return it_lru_row_;
  }
  void set_it_lru_row(
      const std::list<GlyphCacheEntry::iterator_row>::iterator it_lru_row) {
    it_lru_row_ = it_lru_row;
  }

  // Setter/Getter of iterator to row height map.
  std::multimap<int32_t, GlyphCacheEntry::iterator_row>::iterator
  get_it_row_height_map() const {
    return it_row_height_map_;
  }
  void set_it_row_height_map(
      const std::multimap<int32_t, GlyphCacheEntry::iterator_row>::iterator
          it_row_height_map) {
    it_row_height_map_ = it_row_height_map;
  }

  // Getter of cached glyph entries.
  std::vector<GlyphCacheEntry::iterator>& get_cached_entries() {
    return cached_entries_;
  }

  // Add a reference to the glyph cache row.
  void AddRef(FontBuffer* p) { ref_.insert(p); }

  // Release a reference from the glyph cache row.
  void Release(FontBuffer* p) { ref_.erase(p); }

  // Invalidate FontBuffers that are referencing the cache row when the row
  // becomes invalid.
  void InvalidateReferencingBuffers();

 private:
  // Last used counter value of the entry. The value is used to determine
  // if the entry can be evicted from the cache.
  uint32_t last_used_counter_;

  // Remaining width of the row.
  // As new contents are added to the row, remaining width decreases.
  int32_t remaining_width_;

  // Index indicating a glyph cache slice.
  int32_t slice_;

  // Size of the row.
  mathfu::vec2i size_;

  // Vertical position of the row in the entire cache buffer.
  uint32_t y_pos_;

  // Iterator to the row LRU list.
  std::list<GlyphCacheEntry::iterator_row>::iterator it_lru_row_;

  // Iterator to the row height map.
  std::multimap<int32_t, GlyphCacheEntry::iterator_row>::iterator
      it_row_height_map_;

  // Tracking cached entries in the row.
  // When flushing the row, entries in the map is removed using the vector.
  std::vector<GlyphCacheEntry::iterator> cached_entries_;

  // Set holding pointers of FontBuffer that is referencing the cache row.
  std::set<FontBuffer*> ref_;
};

// The base class of GlyphCacheBuffer.
// The class maintains cache rows.
class GlyphCacheBufferBase {
 public:
  GlyphCacheBufferBase() : max_slices_(0), dirty_(false) {
    size_ = mathfu::kZeros2i;
  }

  void Initialize(GlyphCache* cache, const mathfu::vec2i& size,
                  int32_t max_slices);
  const mathfu::vec2i& get_size() { return size_; }

  bool FindRow(int32_t req_width, int32_t req_height,
               GlyphCacheEntry::iterator_row* it_found);

  // Insert new row to the row list with a given size.
  // It tries to merge 2 rows if next row is also empty one.
  void InsertNewRow(int32_t slice, int32_t y_pos, const mathfu::vec2i& size,
                    const GlyphCacheEntry::iterator_row pos);

  // Update the row LRU list.
  void UpdateRowLRU(std::list<GlyphCacheEntry::iterator_row>::iterator it);

  // Update dirty rect.
  void UpdateDirtyRect(int32_t slice, const mathfu::vec4i& rect);

  // Getter of dirty rect.
  const mathfu::vec4i& get_dirty_rect(int32_t slice) const {
    return dirty_rects_[slice];
  }

  // Getter/Setter of dirty state.
  bool get_dirty_state() const { return dirty_; };
  void set_dirty_state(bool dirty) { dirty_ = dirty; }

  // Virtual functions to retrieve buffer parameters.
  virtual fplbase::TextureFormat get_texture_format() = 0;
  virtual int32_t get_num_slices() const = 0;
  virtual uint8_t* get(int32_t slice) const = 0;
  virtual int32_t get_element_size() const = 0;
  virtual void CopyImage(const mathfu::vec3i& pos, const uint8_t* const image,
                         const GlyphCacheEntry* entry) = 0;
  virtual void ResolveDirtyRect() = 0;

 protected:
  // list of rows in the cache.
  std::list<GlyphCacheRow> list_row_;

  // LRU entries of the row. Tracks iterator to list_row_.
  std::list<GlyphCacheEntry::iterator_row> lru_row_;

  // Map to row entries to have O(log N) access to a row entry.
  // Tracks iterator to list_row_.
  // Using multimap because multiple rows can have same row height.
  // Key: height of the row. With the map, an API can have quick access to a row
  // with a given height.
  std::multimap<int32_t, GlyphCacheEntry::iterator_row> map_row_;

  // Size of the buffer.
  mathfu::vec2i size_;

  // # of slices in the buffer.
  int32_t max_slices_;

  // Flag indicates if the cache is dirty. If it's dirty, corresponding font
  // atlas texture needs to be uploaded.
  bool dirty_;

  // Dirty region in the buffer.
  std::vector<mathfu::vec4i> dirty_rects_;

  // Pointer to the cache class to retrieve cache system parameters.
  GlyphCache* cache_;
};

template <typename T>
class GlyphCacheBuffer : public GlyphCacheBufferBase {
 public:
  // Copy glyph image into the buffer.
  void CopyImage(const mathfu::vec3i& pos, const uint8_t* const src,
                 const GlyphCacheEntry* entry) {
    auto dest = buffers_[pos.z()].get();
    auto src_stride = entry->get_size().x() * sizeof(T);
    assert(pos.x() < size_.x());
    assert(pos.y() < size_.y());
    for (int32_t y = 0; y < entry->get_size().y(); ++y) {
      memcpy(dest + pos.x() + (pos.y() + y) * size_.x(), src + y * src_stride,
             src_stride);
    }
  }

  // Insert new buffer in the buffer list.
  void InsertNewBuffer() {
    // Increase the buffer size.
    int32_t new_index = static_cast<int32_t>(buffers_.size());
    buffers_.resize(new_index + 1);
    dirty_rects_.resize(new_index + 1, mathfu::kZeros4i);

    // Allocate the glyph cache buffer.
    // A buffer format can be 8/32 bpp (32 bpp is mostly used for Emoji).
    buffers_[new_index].reset(new T[size_.x() * size_.y()]);

    // Defining a cache clear value for debugging use (e.g. changing it to other
    // values and check if the entrie is cleared correctly.)
    const int32_t kCacheClearValue = 0x0;
    // Clearing allocated buffer.
    memset(buffers_[new_index].get(), kCacheClearValue,
           size_.x() * size_.y() * sizeof(T));

    // Create first (empty) row entry.
    auto index = new_index | buffer_format();
    InsertNewRow(index, 0, size_, list_row_.end());

    // Allocate new texture.
    textures_.emplace_back(nullptr, get_texture_format(),
                           fplbase::kTextureFlagsNone);
#ifdef GLYPH_CACHE_STATS
    LogInfo("Cached glyphs: new buffer is allocated.\nCurrent buffer size:%d",
            new_index + 1);
#endif  // GLYPH_CACHE_STATS
  }

  // Resolve the dirty state of the cache. If the cache has any dirty rect,
  // the API copies the rect to the texture using FPLBase API.
  void ResolveDirtyRect() {
    auto slices = get_num_slices();
    for (auto i = 0; i < slices; ++i) {
      auto rect = get_dirty_rect(i);
      if (!textures_[i].id()) {
        // Give a texture size but don't have to clear the texture here.
        textures_[i].LoadFromMemory(nullptr, get_size(), get_texture_format());
      }
      if (rect.z() - rect.x() && rect.w() - rect.y()) {
        textures_[i].Set(0);
        fplbase::Texture::UpdateTexture(
            get_texture_format(), 0, rect.y(), get_size().x(),
            rect.w() - rect.y(),
            get(i) + get_element_size() * get_size().x() * rect.y());
      }
    }
    set_dirty_state(false);
  }

  bool PurgeCache(int32_t req_height);

  void Reset() {
    buffers_.clear();
    lru_row_.clear();
    list_row_.clear();
    map_row_.clear();
    dirty_ = false;
    textures_.clear();

    // Create first (empty) row entry.
    InsertNewBuffer();
    auto index = buffer_format();
    InsertNewRow(index, 0, size_, list_row_.end());
  }

  int32_t get_num_slices() const {
    return static_cast<int32_t>(buffers_.size());
  }
  uint8_t* get(int32_t slice) const {
    return reinterpret_cast<uint8_t*>(buffers_[slice].get());
  }
  int32_t get_element_size() const { return sizeof(T); }
  fplbase::Texture* get_texture(int32_t slice) { return &textures_[slice]; }

  fplbase::TextureFormat get_texture_format() {
    if (sizeof(T) == 4) {
      return fplbase::kFormat8888;
    } else if (sizeof(T) == 3) {
      return fplbase::kFormat888;
    }
    // Single channel cache.
    return fplbase::kFormatLuminance;
  }

  // Retrieve the buffer format.
  GlyphFormats buffer_format() {
    if (sizeof(T) > 1) {
      // 8 bit buffers are treated as a monochrome buffer (used for regular and
      // SDF glyphs).
      return kGlyphFormatsColor;
    }
    return kGlyphFormatsMono;
  }

 private:
  std::vector<std::unique_ptr<T[]>> buffers_;
  std::vector<fplbase::Texture> textures_;
};

class GlyphCache {
 public:
  // Constructor with parameters.
  // width: width of the glyph cache texture. Rounded up to power of 2.
  // height: height of the glyph cache texture. Rounded up to power of 2.
  // max_slices: max number of slices in the cache.
  GlyphCache(const mathfu::vec2i& size, int32_t max_slices);
  ~GlyphCache(){};

  // Look up a cached entries.
  // Return value: A pointer to a cached glyph entry.
  // nullptr if not found.
  const GlyphCacheEntry* Find(const GlyphKey& key);

  // Set an entry to the cache.
  // Return value: true if caching succeeded. false if there is no room in the
  // cache for a requested entry.
  // Returns a pointer to inserted entry.
  const GlyphCacheEntry* Set(const void* const image, const GlyphKey& key,
                             const GlyphCacheEntry& entry);

  // Flush all cache entries.
  bool Flush();

  // Increment a cycle counter of the cache.
  // Invoke this API for each rendering cycle.
  // The counter is used to determine which cache entries can be evicted when
  // cache entries are full.
  void Update() { counter_++; }

  // Resolve dirty rects in the glyph cache. The API invoke FPLBase API to
  // update textures.
  void ResolveDirtyRect() {
    buffers_.ResolveDirtyRect();
    color_buffers_.ResolveDirtyRect();
  }

  // Return dirty state of the glyph cache.
  bool get_dirty_state() const {
    return buffers_.get_dirty_state() || color_buffers_.get_dirty_state();
  }

  // Return number of cache slices in the cache.
  int32_t get_num_slices() const {
    return buffers_.get_num_slices() + color_buffers_.get_num_slices();
  }

  // Retrieve a cycle counter of the cache.
  uint32_t get_counter() const { return counter_; }

  // Debug API to show cache statistics.
  const GlyphCacheStats& Status();

  // Enable color glyph cache in the cache.
  void EnableColorGlyph();

  // Getter of allocated glyph cache.
  GlyphCacheBuffer<uint8_t>* get_monochrome_buffer() { return &buffers_; }
  GlyphCacheBuffer<uint32_t>* get_color_buffer() { return &color_buffers_; }

  // Getter/Setter of the counter.
  uint32_t get_revision() const { return revision_; }
  void set_revision(uint32_t revision) { revision_ = revision; }

  // Getter of the cache size.
  const mathfu::vec2i& get_size() const { return size_; }

 private:
  // Friend class, GlyphCacheBuffer needs an access to internal variables of the
  // class.
  template <typename T>
  friend class GlyphCacheBuffer;

  // Flush cached entries in the buffer.
  void FlushCachedEntries(std::vector<GlyphCacheEntry::iterator>& entries);

#ifdef GLYPH_CACHE_STATS
  void ResetStats();
#endif  // GLYPH_CACHE_STATS

  // A time counter of the cache.
  // In each rendering cycle, the counter is incremented.
  // The counter is used if some cache entry can be evicted in current rendering
  // cycle.
  uint32_t counter_;

  // Size of the glyph cache. Rounded to power of 2.
  mathfu::vec2i size_;

  // Cache buffers for monochrome (1 channel) glyphs.
  GlyphCacheBuffer<uint8_t> buffers_;

  // Ceche buffer for color glyphs. The buffer is initialized when
  // EnableColorGlyphs() API is invoked.
  GlyphCacheBuffer<uint32_t> color_buffers_;

  // Max number of cache buffers. As a buffer gets full, the glyph cache tries
  // to increase number of cache buffers up to the number;
  int32_t max_slices_;

  // Hash map to the cache entries
  // This map is the primary place to look up the cache entries.
  // Key: a structure that contains glyph parameters such as a code point, font
  // id, glyph size etc.
  // Note that the code point is an index in the font file and not a Unicode
  // value.
  std::unordered_map<GlyphKey, std::unique_ptr<GlyphCacheEntry>, GlyphKey>
      map_entries_;

  // Revision of the buffer.
  // Each time one or more cache entry is evicted, a revision of the cache is
  // updated.
  // The revision  is used to determine if caller can make sure referencing
  // glyph cache entries are still in the cache.
  // Note that the revision is not changed when new glyph entries are added
  // because existing entries are still valid in that case.
  uint32_t revision_;

  // Variables to track usage stats.
  GlyphCacheStats stats_;
};

// The API tries to increase a buffer size up to specified slice first and if
// threre is no room, then try to evict a row entry with or larger than
// 'req_height' which is not used in current rendering cycle.
// (Note that rows used by 'reference counting' FontBuffers are never evicted)
// Rows are stored in the queue in the order of it's size (and LRU if there are
// multiple rows in the same size).
// API returns true if it allocates new buffer or the purge operation was
// successful. Otherwise the API returns false which would initiate multi pass
// glyph rendering in FlatUI (or the user may needs to increase cache size in
// other system).
template <typename T>
bool GlyphCacheBuffer<T>::PurgeCache(int32_t req_height) {
  // Try to expand the cache if we can allocate more slices.
  if (get_num_slices() < max_slices_) {
    InsertNewBuffer();
    return true;
  }

  // Try to find a row that is not used in current cycle and has enough
  // height from LRU list.
  for (auto row_it = lru_row_.begin(); row_it != lru_row_.end(); ++row_it) {
    auto& row = *row_it;
    if (row->get_last_used_counter() == cache_->get_counter()) {
      // The row is being used in current rendering cycle.
      // We can not evict the row.
      continue;
    }
    if (row->get_size().y() >= req_height) {
      // Now flush & initialize the row.
      // Invalidate FontBuffers that are referencing the row.
      row->InvalidateReferencingBuffers();
      cache_->FlushCachedEntries(row->get_cached_entries());
      row->Initialize(row->get_slice(), row->get_y_pos(), row->get_size());

      // Try to call the function recursively.
      return true;
    }
  }
  return false;
}

/// @endcond

}  // namespace flatui

#endif  // GLYPH_CACH_H
