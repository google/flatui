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
#include "precompiled.h"

#include "fplbase/fpl_common.h"
#include "fplbase/utilities.h"
#include "internal/flatui_util.h"
#include "internal/glyph_cache.h"

using fplbase::LogInfo;
using fplbase::LogError;

namespace flatui {

GlyphCache::GlyphCache(const mathfu::vec2i& size, int32_t max_slices)
    : counter_(0), revision_(0) {
  // Round up cache sizes to power of 2.
  size_ = mathfu::RoundUpToPowerOf2(size);
  buffers_.Initialize(this, size_, max_slices);
  max_slices_ = max_slices;
  // Create new cache buffer slice.
  buffers_.InsertNewBuffer();

#ifdef GLYPH_CACHE_STATS
  ResetStats();
#endif // GLYPH_CACHE_STATS
}

const GlyphCacheEntry* GlyphCache::Find(const GlyphKey& key) {
#ifdef GLYPH_CACHE_STATS
  // Update debug variable.
  stats_.lookup_++;
#endif // GLYPH_CACHE_STATS
  auto it = map_entries_.find(key);
  if (it != map_entries_.end()) {
    // Found an entry!

    // Mark the row as being used in current cycle.
    it->second->it_row_->set_last_used_counter(counter_);

    // Update row LRU entry. The row is now most recently used.
    it->second->buffer_->UpdateRowLRU(it->second->it_lru_row_);

#ifdef GLYPH_CACHE_STATS
    // Update debug variable.
    stats_.hit_++;
#endif // GLYPH_CACHE_STATS
    return it->second.get();
  }

  // Didn't find a cached entry. A caller may call Store() function to store
  // new entriy to the cache.
  return nullptr;
}

const GlyphCacheEntry* GlyphCache::Set(const void* const image,
                                       const GlyphKey& key,
                                       const GlyphCacheEntry& entry) {
  // Lookup entries if the entry is already stored in the cache.
  auto p = Find(key);
#ifdef GLYPH_CACHE_STATS
  // Adjust debug variable.
  stats_.lookup_--;
#endif // GLYPH_CACHE_STATS
  if (p) {
    // Make sure cached entry has same properties.
    // The cache only support one entry per a glyph code point for now.
    assert(p->get_size().x() == entry.get_size().x());
    assert(p->get_size().y() == entry.get_size().y());
#ifdef GLYPH_CACHE_STATS
    // Adjust debug variable.
    stats_.hit_--;
#endif // GLYPH_CACHE_STATS
    return p;
  }

  // Adjust requested height & width.
  // Height is rounded up to multiple of kGlyphCacheHeightRound.
  // Expecting kGlyphCacheHeightRound is base 2.
  int32_t req_width = entry.get_size().x() + kGlyphCachePaddingX;
  int32_t req_height = ((entry.get_size().y() + kGlyphCachePaddingY +
                         (kGlyphCacheHeightRound - 1)) &
                        ~(kGlyphCacheHeightRound - 1));
  // Find sufficient space in the buffer.
  GlyphCacheEntry::iterator_row it_row;
  GlyphCacheEntry* ret;
  GlyphCacheBufferBase* buffer =
      entry.color_glyph_
          ? reinterpret_cast<GlyphCacheBufferBase*>(&color_buffers_)
          : reinterpret_cast<GlyphCacheBufferBase*>(&buffers_);

  if (!buffer->FindRow(req_width, req_height, &it_row)) {
    // Couldn't find sufficient row entry nor free space to create new row.
    if (buffers_.PurgeCache(req_height)) {
      // Call the function recursively.
      return Set(image, key, entry);
    }
#ifdef GLYPH_CACHE_STATS
    stats_.set_fail_++;
#endif // GLYPH_CACHE_STATS
    // TODO: Try to flush multiple rows and merge them to free up space.
    // Now we don't have any space in the cache.
    // It's caller's responsivility to recover from the situation.
    // Possible work arounds are:
    // - Draw glyphs with current glyph cache contents and then flush them,
    // start new caching.
    // - Just increase cache size.
    return nullptr;
  }

  // Create new entry in the look-up map.
  auto pair =
      map_entries_.insert(std::pair<GlyphKey, std::unique_ptr<GlyphCacheEntry>>(
          key, std::unique_ptr<GlyphCacheEntry>(new GlyphCacheEntry(entry))));
  auto it_entry = pair.first;
  ret = it_entry->second.get();

  // Reserve a region in the row.
  auto pos = mathfu::vec3i(
      it_row->Reserve(it_entry, mathfu::vec2i(req_width, req_height)),
      it_row->get_y_pos(), it_row->get_slice() & ~kGlyphFormatsColor);

  // Store given image into the buffer.
  if (image != nullptr) {
    buffer->CopyImage(pos, reinterpret_cast<const uint8_t*>(image), ret);
  }
  buffer->UpdateDirtyRect(pos.z(),
                          mathfu::vec4i(pos.xy(), pos.xy() + ret->get_size()));

  // Update UV of the entry.
  mathfu::vec4 uv(
      mathfu::vec2(pos.xy()) / mathfu::vec2(size_),
      mathfu::vec2(pos.xy() + entry.get_size()) / mathfu::vec2(size_));
  ret->set_uv(uv);

  pos.z() = it_row->get_slice();
  ret->set_pos(pos);

  // Establish links.
  ret->set_row(it_row);
  ret->it_lru_row_ = it_row->get_it_lru_row();
  ret->buffer_ = buffer;

  // Update row LRU entry.
  buffer->UpdateRowLRU(it_row->get_it_lru_row());
  it_row->set_last_used_counter(counter_);

  return ret;
}

bool GlyphCache::Flush() {
#ifdef GLYPH_CACHE_STATS
  ResetStats();
#endif // GLYPH_CACHE_STATS
  map_entries_.clear();

  // Clear buffers.
  buffers_.Reset();
  color_buffers_.Reset();

  // Update cache revision.
  revision_ = counter_;
  return true;
}

const GlyphCacheStats& GlyphCache::Status() {
#ifdef GLYPH_CACHE_STATS
  LogInfo("Cache size: %dx%d", size_.x(), size_.y());
  LogInfo("Cache slices: %d", get_num_slices());
  LogInfo("Cached glyphs: %d", map_entries_.size());
  LogInfo("Cache hit: %d / %d", stats_.hit_, stats_.lookup_);
  LogInfo("Row flush: %d", stats_.row_flush_);
  LogInfo("Set fail: %d", stats_.set_fail_);
#endif // GLYPH_CACHE_STATS
  return stats_;
}

void GlyphCache::EnableColorGlyph() {
  if (color_buffers_.get_size().x() == 0) {
    // Initialize color buffer.
    color_buffers_.Initialize(this, size_, max_slices_);
    color_buffers_.InsertNewBuffer();
  }
}

void GlyphCache::FlushCachedEntries(
    std::vector<GlyphCacheEntry::iterator>& entries) {
  // Erase cached glyphs from look-up map.
  for (auto entry = entries.begin(); entry != entries.end(); ++entry) {
    map_entries_.erase(*entry);
  }

  // Update cache revision.
  // It's setting revision equal to the current counter value so that it just
  // change the revision once a rendering cycle even multiple cache flush
  // happens in a cycle.
  revision_ = counter_;

#ifdef GLYPH_CACHE_STATS
  stats_.row_flush_++;
#endif // GLYPH_CACHE_STATS
}

#ifdef GLYPH_CACHE_STATS
void GlyphCache::ResetStats() {
  // Initialize debug variables.
  stats_.hit_ = 0;
  stats_.lookup_ = 0;
  stats_.row_flush_ = 0;
  stats_.set_fail_ = 0;
}
#endif // GLYPH_CACHE_STATS

void GlyphCacheBufferBase::Initialize(GlyphCache* cache,
                                      const mathfu::vec2i& size,
                                      int32_t max_slices) {
  cache_ = cache;
  size_ = size;
  max_slices_ = max_slices;
}

bool GlyphCacheBufferBase::FindRow(int32_t req_width, int32_t req_height,
                                   GlyphCacheEntry::iterator_row* it_found) {
  // Look up the row map to retrieve a row iterator to start with.
  bool ret = false;
  auto it = map_row_.lower_bound(req_height);
  while (it != map_row_.end()) {
    if (it->second->DoesFit(mathfu::vec2i(req_width, req_height))) {
      break;
    }
    it++;
  }

  if (it != map_row_.end()) {
    // Found sufficient space in the buffer.
    auto it_row = it->second;

    if (it_row->get_num_glyphs() == 0) {
      // Putting first entry to the row.
      // In this case, we create new empty row to track rest of free space.
      auto original_height = it_row->get_size().y();
      auto original_y_pos = it_row->get_y_pos();

      if (original_height >= req_height + kGlyphCacheHeightRound) {
        // Create new row in free space.
        it_row->set_size(mathfu::vec2i(size_.x(), req_height));

        // Update row height map key as well.
        map_row_.erase(it_row->get_it_row_height_map());
        auto it_map =
            map_row_.insert(std::pair<int32_t, GlyphCacheEntry::iterator_row>(
                req_height, it_row));
        it_row->set_it_row_height_map(it_map);

        InsertNewRow(it_row->get_slice(), original_y_pos + req_height,
                     mathfu::vec2i(size_.x(), original_height - req_height),
                     list_row_.end());
      }
    }
    *it_found = it_row;
    ret = true;
  }
  return ret;
}

void GlyphCacheBufferBase::InsertNewRow(
    int32_t slice, int32_t y_pos, const mathfu::vec2i& size,
    const GlyphCacheEntry::iterator_row pos) {
  // First, check if we can merge the requested row with next row to free up
  // more spaces.
  // New row is always inserted right after valid row entry. So we don't have
  // to check previous row entry to merge.
  if (pos != list_row_.end()) {
    auto next_entry = std::next(pos);
    if (next_entry->get_num_glyphs() == 0 && next_entry->get_slice() == slice) {
      // We can merge them.
      mathfu::vec2i next_size = next_entry->get_size();
      next_size.y() += size.y();
      next_entry->set_y_pos(next_entry->get_y_pos() - size.y());
      next_entry->set_size(next_size);
      next_entry->set_last_used_counter(cache_->get_counter());
      return;
    }
  }

  // Insert new row.
  auto it = list_row_.insert(pos, GlyphCacheRow(slice, y_pos, size));
  auto it_lru_row = lru_row_.insert(lru_row_.end(), it);
  auto it_map = map_row_.insert(
      std::pair<int32_t, GlyphCacheEntry::iterator_row>(size.y(), it));

  // Update a link.
  it->set_it_lru_row(it_lru_row);
  it->set_it_row_height_map(it_map);
}

void GlyphCacheBufferBase::UpdateRowLRU(
    std::list<GlyphCacheEntry::iterator_row>::iterator it) {
  lru_row_.splice(lru_row_.end(), lru_row_, it);
}

void GlyphCacheBufferBase::UpdateDirtyRect(int32_t slice,
                                           const mathfu::vec4i& rect) {
  if (!dirty_) {
    // Initialize dirty rects.
    for (size_t i = 0; i < dirty_rects_.size(); ++i) {
      dirty_rects_[i] = mathfu::vec4i(size_, mathfu::kZeros2i);
    }
  }

  dirty_ = true;
  dirty_rects_[slice] =
      mathfu::vec4i(mathfu::vec2i::Min(dirty_rects_[slice].xy(), rect.xy()),
                    mathfu::vec2i::Max(dirty_rects_[slice].zw(), rect.zw()));
}
}