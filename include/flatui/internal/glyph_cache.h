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
#include <unordered_map>

#include "flatui_util.h"
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
template <typename T>
class GlyphCache;
class GlyphCacheRow;
class GlyphCacheEntry;
class GlyphKey;

// Constants for a cache entry size rounding up and padding between glyphs.
// Adding a padding between cached glyph images to avoid sampling artifacts of
// texture fetches.
const int32_t kGlyphCacheHeightRound = 4;
const int32_t kGlyphCachePaddingX = 1;
const int32_t kGlyphCachePaddingY = 1;
const int32_t kGlyphCachePaddingSDF = 4;

// TODO: Provide proper int specialization in mathfu.
static inline int32_t RoundUpToPowerOf2(int32_t x) {
  return static_cast<int32_t>(mathfu::RoundUpToPowerOf2(static_cast<float>(x)));
}

// Flags that controls glyph image generation.
enum GlyphFlags {
  kGlyphFlagsNone = 0,      // Normal glyph.
  kGlyphFlagsOuterSDF = 1,  // Glyph image with an outer SDF information.
  kGlyphFlagsInnerSDF = 2,  // Glyph image with an inner SDF information.
};

const float kSDFThresholdDefault = 16.0f / 255.0f;

// Bitwise OR operator for GlyphFlags enumeration.
inline GlyphFlags operator|(GlyphFlags a, GlyphFlags b) {
  return static_cast<GlyphFlags>(static_cast<int>(a) | static_cast<int>(b));
}

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

  GlyphCacheEntry() : code_point_(0), size_(0, 0), offset_(0, 0) {}

  // Setter/Getter of code point.
  // Code point is an entry in a font file, not a direct transform of Unicode.
  uint32_t get_code_point() const { return code_point_; }
  void set_code_point(const uint32_t code_point) { code_point_ = code_point; }

  // Setter/Getter of cache entry size.
  mathfu::vec2i get_size() const { return size_; }
  void set_size(const mathfu::vec2i& size) { size_ = size; }

  // Setter/Getter of cache entry offset.
  mathfu::vec2i get_offset() const { return offset_; }
  void set_offset(const mathfu::vec2i& offset) { offset_ = offset; }

  // Setter/Getter of the cache entry position.
  mathfu::vec2i get_pos() const { return pos_; }
  void set_pos(const mathfu::vec2i& pos) { pos_ = pos; }

  // Setter/Getter of UV
  mathfu::vec4 get_uv() const { return uv_; }
  void set_uv(const mathfu::vec4& uv) { uv_ = uv; }

  MATHFU_DEFINE_CLASS_SIMD_AWARE_NEW_DELETE

 private:
  // Friend class, GlyphCache needs an access to internal variables of the
  // class.
  template <typename T>
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
  mathfu::vec2i pos_;

  // Iterator to the row entry.
  GlyphCacheEntry::iterator_row it_row;

  // Iterator to the row LRU entry.
  std::list<GlyphCacheEntry::iterator_row>::iterator it_lru_row_;
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
  GlyphCacheRow() { Initialize(0, mathfu::vec2i(0, 0)); }
  // Constructor with an arguments.
  // y_pos : vertical position of the row in the buffer.
  // witdh : width of the row. Typically same value of the buffer width.
  // height : height of the row.
  GlyphCacheRow(const int32_t y_pos, const mathfu::vec2i& size) {
    Initialize(y_pos, size);
  }
  ~GlyphCacheRow() {}

  // Initialize the row width and height.
  void Initialize(const int32_t y_pos, const mathfu::vec2i& size) {
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
  void set_last_used_counter(const uint32_t counter) {
    last_used_counter_ = counter;
  }

  // Setter/Getter of row size.
  mathfu::vec2i get_size() const { return size_; }
  void set_size(const mathfu::vec2i size) { size_ = size; }

  // Setter/Getter of row y pos.
  int32_t get_y_pos() const { return y_pos_; }
  void set_y_pos(const int32_t y_pos) { y_pos_ = y_pos; }

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
  void set_it_row_height_map(const std::multimap<
      int32_t, GlyphCacheEntry::iterator_row>::iterator it_row_height_map) {
    it_row_height_map_ = it_row_height_map;
  }

  // Getter of cached glyph entries.
  std::vector<GlyphCacheEntry::iterator>& get_cached_entries() {
    return cached_entries_;
  }

 private:
  // Last used counter value of the entry. The value is used to determine
  // if the entry can be evicted from the cache.
  uint32_t last_used_counter_;

  // Remaining width of the row.
  // As new contents are added to the row, remaining width decreases.
  int32_t remaining_width_;

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
};

template <typename T>
class GlyphCache {
 public:
  // Constructor with parameters.
  // width: width of the glyph cache texture. Rounded up to power of 2.
  // height: height of the glyph cache texture. Rounded up to power of 2.
  GlyphCache(const mathfu::vec2i& size)
      : counter_(0), revision_(0), dirty_(false) {
    // Round up cache sizes to power of 2.
    size_.x() = RoundUpToPowerOf2(size.x());
    size_.y() = RoundUpToPowerOf2(size.y());

    // Allocate the glyph cache buffer.
    // A buffer format can be 8/32 bpp (32 bpp is mostly used for Emoji).
    buffer_.reset(new T[size_.x() * size_.y()]);

    // Clearing allocated buffer.
    const int32_t kCacheClearValue = 0x0;
    memset(buffer_.get(), kCacheClearValue, size_.x() * size_.y() * sizeof(T));

    // Create first (empty) row entry.
    InsertNewRow(0, size_, list_row_.end());

#ifdef GLYPH_CACHE_STATS
    ResetStats();
#endif
  }
  ~GlyphCache() {};

  // Look up a cached entries.
  // Return value: A pointer to a cached glyph entry.
  // nullptr if not found.
  const GlyphCacheEntry* Find(const GlyphKey& key) {
#ifdef GLYPH_CACHE_STATS
    // Update debug variable.
    stats_lookup_++;
#endif
    auto it = map_entries_.find(key);
    if (it != map_entries_.end()) {
      // Found an entry!

      // Mark the row as being used in current cycle.
      it->second->it_row->set_last_used_counter(counter_);

      // Update row LRU entry. The row is now most recently used.
      lru_row_.splice(lru_row_.end(), lru_row_, it->second->it_lru_row_);

#ifdef GLYPH_CACHE_STATS
      // Update debug variable.
      stats_hit_++;
#endif
      return it->second.get();
    }

    // Didn't find a cached entry. A caller may call Store() function to store
    // new entriy to the cache.
    return nullptr;
  }

  // Set an entry to the cache.
  // Return value: true if caching succeeded. false if there is no room in the
  // cache for a requested entry.
  // Returns a pointer to inserted entry.
  const GlyphCacheEntry* Set(const T* const image, const GlyphKey& key,
                             const GlyphCacheEntry& entry) {
    // Lookup entries if the entry is already stored in the cache.
    auto p = Find(key);
#ifdef GLYPH_CACHE_STATS
    // Adjust debug variable.
    stats_lookup_--;
#endif
    if (p) {
      // Make sure cached entry has same properties.
      // The cache only support one entry per a glyph code point for now.
      assert(p->get_size().x() == entry.get_size().x());
      assert(p->get_size().y() == entry.get_size().y());
#ifdef GLYPH_CACHE_STATS
      // Adjust debug variable.
      stats_hit_--;
#endif
      return p;
    }

    // Adjust requested height & width.
    // Height is rounded up to multiple of kGlyphCacheHeightRound.
    // Expecting kGlyphCacheHeightRound is base 2.
    int32_t req_width = entry.get_size().x() + kGlyphCachePaddingX;
    int32_t req_height = ((entry.get_size().y() + kGlyphCachePaddingY +
                           (kGlyphCacheHeightRound - 1)) &
                          ~(kGlyphCacheHeightRound - 1));

    // Look up the row map to retrieve a row iterator to start with.
    auto it = map_row_.lower_bound(req_height);
    while (it != map_row_.end()) {
      if (it->second->DoesFit(mathfu::vec2i(req_width, req_height))) {
        break;
      }
      it++;
    }

    GlyphCacheEntry* ret;
    if (it != map_row_.end()) {
      // Found sufficient space in the buffer.
      auto it_row = it->second;

      if (it_row->get_num_glyphs() == 0) {
        // Putting first entry to the row.
        // In this case, we create new empty row to track rest of free space.
        auto original_height = it_row->get_size().y();
        auto original_y_pos = it_row->get_y_pos();

        if (original_height - req_height >= kGlyphCacheHeightRound) {
          // Create new row for free space.
          it_row->set_size(mathfu::vec2i(size_.x(), req_height));

          // Update row height map key as well.
          map_row_.erase(it_row->get_it_row_height_map());
          auto it_map =
              map_row_.insert(std::pair<int32_t, GlyphCacheEntry::iterator_row>(
                  req_height, it_row));
          it_row->set_it_row_height_map(it_map);

          InsertNewRow(original_y_pos + req_height,
                       mathfu::vec2i(size_.x(), original_height - req_height),
                       list_row_.end());
        }
      }

      // Create new entry in the look-up map.
      auto pair = map_entries_.insert(
          std::pair<GlyphKey, std::unique_ptr<GlyphCacheEntry>>(
              key,
              std::unique_ptr<GlyphCacheEntry>(new GlyphCacheEntry(entry))));
      auto it_entry = pair.first;
      ret = it_entry->second.get();

      // Reserve a region in the row.
      auto pos = mathfu::vec2i(
          it_row->Reserve(it_entry, mathfu::vec2i(req_width, req_height)),
          it_row->get_y_pos());

      // Store given image into the buffer.
      if (image != nullptr) {
        CopyImage(pos, image, ret);
      } else {
        UpdateDirtyRect(mathfu::vec4i(pos, pos + ret->get_size()));
      }

      // Update UV of the entry.
      mathfu::vec4 uv(
          mathfu::vec2(pos) / mathfu::vec2(size_),
          mathfu::vec2(pos + entry.get_size()) / mathfu::vec2(size_));
      ret->set_uv(uv);
      ret->set_pos(pos);

      // Establish links.
      ret->it_row = it_row;
      ret->it_lru_row_ = it_row->get_it_lru_row();

      // Update row LRU entry.
      lru_row_.splice(lru_row_.end(), lru_row_, it_row->get_it_lru_row());
      it_row->set_last_used_counter(counter_);
    } else {
      // Couldn't find sufficient row entry nor free space to create new row.

      // Try to find a row that is not used in current cycle and has enough
      // height from LRU list.
      for (auto row_it = lru_row_.begin(); row_it != lru_row_.end(); ++row_it) {
        auto& row = *row_it;
        if (row->get_last_used_counter() == counter_) {
          // The row is being used in current rendering cycle.
          // We can not evict the row.
          continue;
        }
        if (row->get_size().y() >= req_height) {
          // Now flush & initialize the row.
          FlushRow(row);
          row->Initialize(row->get_y_pos(), row->get_size());

          // Call the function recursively.
          return Set(image, key, entry);
        }
      }
#ifdef GLYPH_CACHE_STATS
      stats_set_fail_++;
#endif
      // TODO: Try to flush multiple rows and merge them to free up space.
      // TODO: Evaluate re-allocate solution.
      // Now we don't have any space in the cache.
      // It's caller's responsivility to recover from the situation.
      // Possible work arounds are:
      // - Draw glyphs with current glyph cache contents and then flush them,
      // start new caching.
      // - Just increase cache size.
      return nullptr;
    }

    return ret;
  }

  // Flush all cache entries.
  bool Flush() {
#ifdef GLYPH_CACHE_STATS
    ResetStats();
#endif
    map_entries_.clear();
    lru_row_.clear();
    list_row_.clear();
    map_row_.clear();

    // Update cache revision.
    revision_ = counter_;

    // Create first (empty) row entry.
    InsertNewRow(0, size_, list_row_.end());

    dirty_ = false;

    return true;
  }

  // Increment a cycle counter of the cache.
  // Invoke this API for each rendering cycle.
  // The counter is used to determine which cache entries can be evicted when
  // cache entries are full.
  void Update() { counter_++; }

  // Debug API to show cache statistics.
  void Status() {
#ifdef GLYPH_CACHE_STATS
    LogInfo("Cache size: %dx%d", size_.x(), size_.y());
    LogInfo("Cache hit: %d / %d", stats_hit_, stats_lookup_);

    auto total_glyph = 0;
    for (auto row : list_row_) {
      LogInfo("Row start:%d height:%d glyphs:%d counter:%d", row.get_y_pos(),
              row.get_size().y(), row.get_num_glyphs(),
              row.get_last_used_counter());
      total_glyph += row.get_num_glyphs();
    }
    LogInfo("Cached glyphs: %d", total_glyph);
    LogInfo("Row flush: %d", stats_row_flush_);
    LogInfo("Set fail: %d", stats_set_fail_);
#endif
  }

  // Getter/Setter of the counter.
  uint32_t get_revision() const { return revision_; }
  void set_revision(const uint32_t revision) { revision_ = revision; }

  // Getter/Setter of dirty state.
  bool get_dirty_state() const {
    return dirty_;
  };
  void set_dirty_state(const bool dirty) { dirty_ = dirty; }

  // Getter of dirty rect.
  const mathfu::vec4i& get_dirty_rect() const { return dirty_rect_; }

  // Getter of allocated glyph cache buffer.
  T* get_buffer() const { return buffer_.get(); }

  // Getter of the cache size.
  const mathfu::vec2i& get_size() const { return size_; }

 private:
  // Insert new row to the row list with a given size.
  // It tries to merge 2 rows if next row is also empty one.
  void InsertNewRow(const int32_t y_pos, const mathfu::vec2i& size,
                    const GlyphCacheEntry::iterator_row pos) {
    // First, check if we can merge the requested row with next row to free up
    // more spaces.
    // New row is always inserted right after valid row entry. So we don't have
    // to check previous row entry to merge.
    if (pos != list_row_.end()) {
      auto next_entry = std::next(pos);
      if (next_entry->get_num_glyphs() == 0) {
        // We can merge them.
        mathfu::vec2i next_size = next_entry->get_size();
        next_size.y() += size.y();
        next_entry->set_y_pos(next_entry->get_y_pos() - size.y());
        next_entry->set_size(next_size);
        next_entry->set_last_used_counter(counter_);
        return;
      }
    }

    // Insert new row.
    auto it = list_row_.insert(pos, GlyphCacheRow(y_pos, size));
    auto it_lru_row = lru_row_.insert(lru_row_.end(), it);
    auto it_map = map_row_.insert(
        std::pair<int32_t, GlyphCacheEntry::iterator_row>(size.y(), it));

    // Update a link.
    it->set_it_lru_row(it_lru_row);
    it->set_it_row_height_map(it_map);
  }

  void FlushRow(const GlyphCacheEntry::iterator_row row) {
    // Erase cached glyphs from look-up map.
    auto& entries = row->get_cached_entries();
    for (auto entry = entries.begin(); entry != entries.end(); ++entry) {
      map_entries_.erase(*entry);
    }

    // Update cache revision.
    // It's setting revision equal to the current counter value so that it just
    // change the revision once a rendering cycle even multiple cache flush
    // happens in a cycle.
    revision_ = counter_;

#ifdef GLYPH_CACHE_STATS
    stats_row_flush_++;
#endif
  }

  // Copy glyph image into the buffer.
  void CopyImage(const mathfu::vec2i& pos, const T* const image,
                 const GlyphCacheEntry* entry) {
    auto buffer = buffer_.get();
    auto size = entry->get_size().x() * sizeof(T);
    for (int32_t y = 0; y < entry->get_size().y(); ++y) {
      memcpy(buffer + pos.x() + (pos.y() + y) * size_.x(),
             image + y * entry->get_size().x(), size);
    }
    UpdateDirtyRect(mathfu::vec4i(pos, pos + entry->get_size()));
  }

  // Update dirty rect.
  void UpdateDirtyRect(const mathfu::vec4i& rect) {
    if (!dirty_) {
      // Initialize dirty rect.
      dirty_rect_ = mathfu::vec4i(size_, mathfu::kZeros2i);
    }

    dirty_ = true;
    dirty_rect_ =
        mathfu::vec4i(mathfu::vec2i::Min(dirty_rect_.xy(), rect.xy()),
                      mathfu::vec2i::Max(dirty_rect_.zw(), rect.zw()));
  }

#ifdef GLYPH_CACHE_STATS
  void ResetStats() {
    // Initialize debug variables.
    stats_hit_ = 0;
    stats_lookup_ = 0;
    stats_row_flush_ = 0;
    stats_set_fail_ = 0;
  }
#endif

  // A time counter of the cache.
  // In each rendering cycle, the counter is incremented.
  // The counter is used if some cache entry can be evicted in current rendering
  // cycle.
  uint32_t counter_;

  // Size of the glyph cache. Rounded to power of 2.
  mathfu::vec2i size_;

  // Cache buffer;
  std::unique_ptr<T> buffer_;

  // Hash map to the cache entries
  // This map is the primary place to look up the cache entries.
  // Key: a structure that contains glyph parameters such as a code point, font
  // id, glyph size etc.
  // Note that the code point is an index in the
  // font file and not a Unicode value.
  std::unordered_map<GlyphKey, std::unique_ptr<GlyphCacheEntry>, GlyphKey>
      map_entries_;

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

  // Revision of the buffer.
  // Each time one or more cache entry is evicted, a revision of the cache is
  // updated.
  // The revision  is used to determine if caller can make sure referencing
  // glyph cache entries are still in the cache.
  // Note that the revision is not changed when new glyph entries are added
  // because existing entries are still valid in that case.
  uint32_t revision_;

  // Flag indicates if the cache is dirty. If it's dirty, corresponding font
  // atlas texture needs to be uploaded.
  bool dirty_;

  // Dirty region in the buffer.
  mathfu::vec4i dirty_rect_;

#ifdef GLYPH_CACHE_STATS
  // Variables to track usage stats.
  int32_t stats_lookup_;
  int32_t stats_hit_;
  int32_t stats_row_flush_;
  int32_t stats_set_fail_;
#endif
};
/// @endcond

}  // namespace flatui

#endif  // GLYPH_CACH_H
