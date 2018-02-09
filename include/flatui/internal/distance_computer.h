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

#ifndef FLATUI_INTERNAL_DISTANCE_COMPUTER_H
#define FLATUI_INTERNAL_DISTANCE_COMPUTER_H

#include "flatui/internal/glyph_cache.h"

namespace flatui {

using mathfu::vec2;
using mathfu::vec2i;

/// @cond FLATUI_INTERNAL
// A helper template class to access buffer data with padding.
template <typename T, typename FundamentalType = T>
class Grid {
 public:
  // Constructors.
  Grid()
      : data_(nullptr),
        size_(mathfu::kZeros2i),
        padding_(0),
        stride_(0),
        inverted_(false),
        p_(nullptr) {}
  Grid(T* data, const vec2i& size, int32_t padding, size_t stride)
      : data_(data),
        size_(size),
        padding_(padding),
        stride_(static_cast<int32_t>(stride)),
        inverted_(false),
        p_(nullptr) {}

  // Set up the grid with a buffer allocation.
  void SetSize(const vec2i& size, T initial_value) {
    size_ = size;
    padding_ = 0;
    stride_ = size.x;
    if (p_ == nullptr) {
      p_ = std::unique_ptr<std::vector<T>>(new std::vector<T>());
    } else {
      p_->clear();
    }
    p_->resize(size.x * size.y, initial_value);
    data_ = &(*p_)[0];
    return;
  }

  int32_t GetWidth() const { return size_.x + padding_ * 2; }
  int32_t GetHeight() const { return size_.y + padding_ * 2; }
  const vec2i GetSize() const {
    return size_ + vec2i(padding_ * 2, padding_ * 2);
  }
  const vec2i GetOriginalSize() const { return size_; }

  // Setter to the buffer.
  void Set(const vec2i& pos, T value) {
    vec2i p = pos - vec2i(padding_, padding_);
    if (!mathfu::InRange2D(p, mathfu::kZeros2i, size_)) {
      return;
    }
    data_[p.x + p.y * stride_] = value;
  }

  // Getter to the buffer.
  T Get(const vec2i& pos) const {
    vec2i p = pos - vec2i(padding_, padding_);
    if (inverted_) {
      if (!mathfu::InRange2D(p, mathfu::kZeros2i, size_)) {
        return T(std::numeric_limits<FundamentalType>::max());
      }
      return T(std::numeric_limits<FundamentalType>::max()) -
             data_[p.x + p.y * stride_];
    } else {
      if (!mathfu::InRange2D(p, mathfu::kZeros2i, size_)) {
        return T(std::numeric_limits<FundamentalType>::min());
      }
      return data_[p.x + p.y * stride_];
    }
  }

  // Change an inversion setting in the grid when retrieving a data.
  void invert(bool b, const T& invert_reference) {
    inverted_ = b;
    invert_reference_ = invert_reference;
  }
  bool get_invert() const { return inverted_; }

  std::unique_ptr<std::vector<T>> get_buffer() {
    return p_;
  };

 protected:
  T* data_;
  vec2i size_;
  int32_t padding_;
  int32_t stride_;
  bool inverted_;
  T invert_reference_;
  std::unique_ptr<std::vector<T>> p_;
};

// Represents a large distance during computation.
static const float kLargeDistance = 1e6;

/// @cond FLATUI_INTERNAL
//
// The DistanceComputer class implements the main functions to compute signed
// distance fields.
//
template <typename T, typename FundamentalType = T>
class DistanceComputer {
 public:
  virtual ~DistanceComputer() {}

  // Computes and returns a Grid containing signed distances for a Grid
  // representing a grayscale image.  Each pixel's value ("signed distance") is
  // the distance from the center of that pixel to the nearest boundary/edge,
  // signed so that pixels inside the boundary are negative and those outside
  // the boundary are positive.
  virtual void Compute(const Grid<T, FundamentalType>& image,
                       Grid<T, FundamentalType>* dest,
                       GlyphFlags flag) = 0;

};

}  // namespace flatui

#endif  // FLATUI_INTERNAL_DISTANCE_COMPUTER_H
