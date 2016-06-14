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
    stride_ = size.x();
    if (p_ == nullptr) {
      p_ = std::unique_ptr<std::vector<T>>(new std::vector<T>());
    } else {
      p_->clear();
    }
    p_->resize(size.x() * size.y(), initial_value);
    data_ = &(*p_)[0];
    return;
  }

  int32_t GetWidth() const { return size_.x() + padding_ * 2; }
  int32_t GetHeight() const { return size_.y() + padding_ * 2; }
  const vec2i GetSize() const {
    return size_ + vec2i(padding_ * 2, padding_ * 2);
  }
  const vec2i GetOriginalSize() const { return size_; }

  // Setter to the buffer.
  void Set(const vec2i& pos, T value) {
    auto p = pos - vec2i(padding_, padding_);
    if (!mathfu::InRange2D(p, mathfu::kZeros2i, size_)) {
      return;
    }
    data_[p.x() + p.y() * stride_] = value;
  }

  // Getter to the buffer.
  T Get(const vec2i& pos) const {
    auto p = pos - vec2i(padding_, padding_);
    if (inverted_) {
      if (!mathfu::InRange2D(p, mathfu::kZeros2i, size_)) {
        return T(std::numeric_limits<FundamentalType>::max());
      }
      return T(std::numeric_limits<FundamentalType>::max()) -
             data_[p.x() + p.y() * stride_];
    } else {
      if (!mathfu::InRange2D(p, mathfu::kZeros2i, size_)) {
        return T(std::numeric_limits<FundamentalType>::min());
      }
      return data_[p.x() + p.y() * stride_];
    }
  }

  // Change an inversion setting in the grid when retrieving a data.
  void invert(bool b, const T& invert_reference) {
    inverted_ = b;
    invert_reference_ = invert_reference;
  }
  bool get_invert() const { return inverted_; }

  std::unique_ptr<std::vector<T>> get_buffer() { return p_; };

 protected:
  T* data_;
  vec2i size_;
  int32_t padding_;
  int32_t stride_;
  bool inverted_;
  T invert_reference_;
  std::unique_ptr<std::vector<T>> p_;
};

/// @cond FLATUI_INTERNAL
//
// The DistanceComputer class implements the main functions to compute signed
// distance fields.
//
// This code is an implementation of the algorithm described at
// <http://contourtextures.wikidot.com>. The advantage of this algorithm over
// other SDF generators is that it uses an antialiased rendered font instead of
// a bitmapped font. A bitmapped font would have to be rendered at much higher
// resolution to achieve the same quality as provided here.
//
template <typename T, typename FundamentalType = T>
class DistanceComputer {
 public:
  DistanceComputer() : any_distance_changed_(false) {}
  ~DistanceComputer() {}

  // Computes and returns a Grid containing signed distances for a Grid
  // representing a grayscale image.  Each pixel's value ("signed distance") is
  // the distance from the center of that pixel to the nearest boundary/edge,
  // signed so that pixels inside the boundary are negative and those outside
  // the boundary are positive.
  void Compute(const Grid<T, FundamentalType>& image,
               Grid<T, FundamentalType>* dest, GlyphFlags flag) {
    auto original_size = image.GetOriginalSize();
    if (original_size.x() == 0 || original_size.y() == 0) {
      return;
    }
    auto genearate_inner_distance = flag & kGlyphFlagsInnerSDF;
    auto size = image.GetSize();
    auto width = size.x();
    auto height = size.y();
    // Compute the local gradients in both dimensions.
    gradients_.SetSize(size, mathfu::kZeros2f);
    distances_to_edges_.SetSize(size, mathfu::kZeros2i);
    outer_distances_.SetSize(size, 0.0f);
    distances_ = &outer_distances_;
    image_ = &image;
    destination_ = dest;
    ComputeGradients();

    // Store everything in a struct to pass to the main computation function.
    InitializeDistanceGrid();
    ComputeDistances();

    if (genearate_inner_distance) {
      // Flip the image & gradient, and recalculate a distance field.
      Grid<T, FundamentalType>& inverted_image =
          const_cast<Grid<T, FundamentalType>&>(image);
      auto invert = inverted_image.get_invert();
      inverted_image.invert(true, 0xff);
      gradients_.invert(true, mathfu::kZeros2f);

      distances_ = &inner_distances_;
      inner_distances_.SetSize(size, 0.0f);
      distances_to_edges_.SetSize(size, mathfu::kZeros2i);
      InitializeDistanceGrid();
      ComputeDistances();
      inverted_image.invert(invert, 0xff);
    }

    // Copy the value to the destination buffer.
    for (auto y = 0; y < height; ++y) {
      for (auto x = 0; x < width; ++x) {
        auto pos = vec2i(x, y);
        // Don't return negative distances.
        auto mid =
            static_cast<float>((std::numeric_limits<FundamentalType>::max() +
                                std::numeric_limits<FundamentalType>::min()) /
                               2);
        auto value = distances_->Get(pos);
        if (genearate_inner_distance) {
          value = outer_distances_.Get(pos) - value;
        }
        const float kSDFMultiplier = -16.0f;
        value = mathfu::Clamp(
            value * kSDFMultiplier + mid,
            static_cast<float>(std::numeric_limits<FundamentalType>::min()),
            static_cast<float>(std::numeric_limits<FundamentalType>::max()));
        destination_->Set(pos, T(static_cast<FundamentalType>(value)));
      }
    }
    return;
  }

 private:
  // Computes the local gradients of an image in the X and Y dimensions and
  // returns them as an Array2<Vector2d>.
  void ComputeGradients() {
    const auto w = image_->GetWidth();
    const auto h = image_->GetHeight();

    // This computes the local gradients at pixels near black/white boundaries
    // in the image using convolution filters. The gradient is not needed at
    // other pixels, where it's mostly zero anyway.

    // The 3x3 kernel does not work at the edges, so skip those pixels.
    // TODO: Use a subset of the filter kernels for edge pixels.
    for (auto y = 1; y < h - 1; ++y) {
      for (auto x = 1; x < w - 1; ++x) {
        auto pos = vec2i(x, y);
        auto value = image_->Get(pos);
        // If the pixel is fully on or off, leave the gradient as (0,0).
        // Otherwise, compute it.
        if (value > std::numeric_limits<T>::min() &&
            value < std::numeric_limits<T>::max()) {
          gradients_.Set(pos, FilterPixel(pos));
        }
      }
    }
    return;
  }

  // Applies a 3x3 filter kernel to an image pixel to get the gradients.
  const vec2 FilterPixel(const vec2i& pos) {
    // 3x3 filter kernel. The X gradient uses the array as is and the Y gradient
    // uses the transpose.
    static const float kSqrt2 = sqrtf(2.0f);
    static const float kFilter[3][3] = {
        {-1.0f, 0.0f, 1.0f}, {-kSqrt2, 0.0f, kSqrt2}, {-1.0f, 0.0f, 1.0f}};

    vec2 filtered(0.0f, 0.0f);
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        float val = image_->Get(pos + vec2i(j - 1, i - 1));
        filtered[0] += kFilter[i][j] * val;
        filtered[1] += kFilter[j][i] * val;
      }
    }
    return filtered.Normalized();
  }

  // Creates and initializes a grid containing the distances.
  void InitializeDistanceGrid() {
    const auto w = image_->GetWidth();
    const auto h = image_->GetHeight();
    for (auto y = 0; y < h; ++y) {
      for (auto x = 0; x < w; ++x) {
        auto pos = vec2i(x, y);
        auto v = static_cast<float>(image_->Get(pos)) /
                 std::numeric_limits<T>::max();
        auto dist = v <= 0.0f ? kLargeDistance
                              : v >= 1.0f ? 0.0f : ApproximateDistanceToEdge(
                                                       v, gradients_.Get(pos));
        distances_->Set(pos, dist);
      }
    }
  }

  // Approximates the distance to an image edge from a pixel using the pixel
  // value and the local gradient.
  float ApproximateDistanceToEdge(float value, const vec2& gradient) {
    if (gradient[0] == 0.0f || gradient[1] == 0.0f) {
      // Approximate the gradient linearly using the middle of the range.
      return 0.5f - value;
    } else {
      // Since the gradients are symmetric with respect to both sign and X/Y
      // transposition, do the work in the first octant (positive gradients, x
      // gradient >= y gradient) for simplicity.
      vec2 g = vec2(fabs(gradient[0]), fabs(gradient[1])).Normalized();
      if (g.x() < g.y()) std::swap(g.x(), g.y());
      const auto gradient_value = static_cast<float>(0.5f * g.y() / g.x());
      float dist;
      if (value < gradient_value) {
        // 0 <= value < gradient_value.
        dist = 0.5f * (g.x() + g.y()) - sqrtf(2.0f * g.x() * g.y() * value);
      } else if (value < 1.0f - gradient_value) {
        // gradient_value <= value <= 1 - gradient_value.
        dist = (0.5f - value) * g.x();
      } else {
        // 1 - gradient_value < value <= 1.
        dist = -0.5f * (g.x() + g.y()) +
               sqrtf(2.0f * g.x() * g.y() * (1.0f - value));
      }
      return dist;
    }
  }

  // Computes and returns the distances.
  void ComputeDistances() {
    const auto width = image_->GetWidth();
    const auto height = image_->GetHeight();

    // Keep processing while distances are being modified.
    do {
      any_distance_changed_ = false;

      // Propagate from top down, starting with the second row.
      for (auto y = 1; y < height; ++y) {
        // Propagate distances to the right.
        for (auto x = 0; x < width; ++x) {
          auto pos = vec2i(x, y);
          auto dist = distances_->Get(pos);
          if (dist > 0.0f) {
            UpdateDistance(pos, vec2i(0, -1), &dist);
            if (x > 0) {
              UpdateDistance(pos, vec2i(-1, 0), &dist);
              UpdateDistance(pos, vec2i(-1, -1), &dist);
            }
            if (x < width - 1) {
              UpdateDistance(pos, vec2i(1, -1), &dist);
            }
          }
        }

        // Propagate distances to the left (skip the rightmost pixel).
        for (auto x = width - 2; x >= 0; --x) {
          auto pos = vec2i(x, y);
          auto dist = distances_->Get(pos);
          if (dist > 0.0f) {
            UpdateDistance(pos, vec2i(1, 0), &dist);
          }
        }
      }

      // Propagate from bottom up, starting with the second row from the bottom.
      for (auto y = height - 2; y >= 0; --y) {
        // Propagate distances to the left.
        for (auto x = width - 1; x >= 0; --x) {
          auto pos = vec2i(x, y);
          auto dist = distances_->Get(pos);
          if (dist > 0.0) {
            UpdateDistance(pos, vec2i(0, 1), &dist);
            if (x > 0) {
              UpdateDistance(pos, vec2i(-1, 1), &dist);
            }
            if (x < width - 1) {
              UpdateDistance(pos, vec2i(1, 0), &dist);
              UpdateDistance(pos, vec2i(1, 1), &dist);
            }
          }
        }

        // Propagate distances to the right (skip the leftmost pixel).
        for (auto x = 1; x < width; ++x) {
          auto pos = vec2i(x, y);
          auto dist = distances_->Get(pos);
          if (dist > 0.0) {
            UpdateDistance(pos, vec2i(-1, 0), &dist);
          }
        }
      }
    } while (any_distance_changed_);
  }

  // Computes the distance from data->cur_pixel to an edge pixel based on the
  // information at the pixel at (data->cur_pixel + offset). If the new
  // distance is smaller than the current distance (dist), this modifies dist
  // and sets data->any_distance_changed to true.
  void UpdateDistance(const vec2i& pos, const vec2i& offset, float* dist) {
    const vec2i test_pixel = pos + offset;
    const vec2i xy_dist = distances_to_edges_.Get(test_pixel);
    const vec2i edge_pixel = test_pixel - xy_dist;
    const vec2i new_xy_dist = xy_dist - offset;
    const auto new_dist = ComputeDistanceToEdge(edge_pixel, vec2(new_xy_dist));
    static const float kEpsilon = 1e-3f;
    if (new_dist < *dist - kEpsilon) {
      distances_->Set(pos, new_dist);
      distances_to_edges_.Set(pos, new_xy_dist);
      *dist = new_dist;
      any_distance_changed_ = true;
    }
  }

  // Computes the new distance from a pixel to an edge pixel based on previous
  // information.
  float ComputeDistanceToEdge(const vec2i& pixel,
                              const vec2& vec_to_edge_pixel) {
    // The returned value is expected in range of 0.0 - 1.0.
    const auto value =
        static_cast<float>(image_->Get(pixel)) / std::numeric_limits<T>::max();

    // If the pixel value is negative or 0, return kLargeDistance so that
    // processing will continue.
    if (value == 0.0f) return kLargeDistance;

    // Use the length of the vector to the edge pixel to estimate the real
    // distance to the edge.
    const auto length = vec_to_edge_pixel.Length();
    const auto dist =
        length > 0.0f
            ?
            // Estimate based on direction to edge (accurate for large vectors).
            ApproximateDistanceToEdge(value, vec_to_edge_pixel)
            :
            // Estimate based on local gradient only.
            ApproximateDistanceToEdge(value, gradients_.Get(pixel));
    return length + dist;
  }

  // Represents a large distance during computation.
  const float kLargeDistance = 1e6;

  // The original monochrome image data, as floats (0 - 1).
  const Grid<T>* image_;
  // Destination buffer.
  Grid<T>* destination_;
  // Local gradients in X and Y.
  Grid<vec2, float> gradients_;
  // Current pixel distances in X and Y to edges.
  Grid<vec2i, int> distances_to_edges_;
  // Final distance values.
  Grid<float>* distances_;
  Grid<float> inner_distances_;
  Grid<float> outer_distances_;

  // This is set to true when a value in the distances grid is modified.
  bool any_distance_changed_;
};

}  // namespace flatui

#endif  // FLATUI_INTERNAL_DISTANCE_COMPUTER_H
