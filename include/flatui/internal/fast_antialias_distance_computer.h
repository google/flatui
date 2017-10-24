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

#ifndef FLATUI_INTERNAL_FAST_ANTIALIAS_DISTANCE_COMPUTER_H
#define FLATUI_INTERNAL_FAST_ANTIALIAS_DISTANCE_COMPUTER_H

#include "flatui/internal/distance_computer.h"

namespace flatui {

using mathfu::vec2;
using mathfu::vec2i;

// The FastAntialiasDistanceComputer is a tweak of flatui's
// AntialiasDistanceComputer that makes a few concessions to quality
// for speed.
//
// Document describing tradeoffs:
// https://docs.google.com/document/d/1H09JZsB4OyJf7uqJ8q4hkr6SvsEIVn7lP6LOvIhZMts/edit#
template <typename T, typename FundamentalType = T>
class FastAntialiasDistanceComputer :
    public flatui::DistanceComputer<T, FundamentalType> {
 public:
  FastAntialiasDistanceComputer() : any_distance_changed_(false) {}
  ~FastAntialiasDistanceComputer() {}

  // Computes and returns a Grid containing signed distances for a Grid
  // representing a grayscale image.  Each pixel's value ("signed distance") is
  // the distance from the center of that pixel to the nearest boundary/edge,
  // signed so that pixels inside the boundary are negative and those outside
  // the boundary are positive.
  void Compute(const Grid<T, FundamentalType>& image,
               Grid<T, FundamentalType>* dest,
               GlyphFlags flag) override {
    auto original_size = image.GetOriginalSize();
    if (original_size.x == 0 || original_size.y == 0) {
      return;
    }
    auto genearate_inner_distance = flag & kGlyphFlagsInnerSDF;
    auto size = image.GetSize();
    auto width = size.x;
    auto height = size.y;
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
      gradients_.invert(false, mathfu::kZeros2f);
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
      vec2 g = vec2(fabsf(gradient[0]), fabsf(gradient[1])).Normalized();
      // The following isnan checks are needed because if the gradients_ Grid
      // is inverted then gradients_->Get will do a FLT_MAX - value, which will
      // mean |gradient| will contain FLT_MAX, which will cause Normalized()
      // to return NaNs on some platforms (iOS being one).  This is a very
      // common case, not a rare case, and happens for pixels near the glyph
      // edge.  If we hit this case, then we approximate linearly as above.
      if (isnan(g.x) || isnan(g.y)) {
        return 0.5f - value;
      }
      if (g.x < g.y) std::swap(g.x, g.y);
      const auto gradient_value = static_cast<float>(0.5f * g.y / g.x);
      float dist;
      if (value < gradient_value) {
        // 0 <= value < gradient_value.
        dist = 0.5f * (g.x + g.y) - sqrtf(2.0f * g.x * g.y * value);
      } else if (value < 1.0f - gradient_value) {
        // gradient_value <= value <= 1 - gradient_value.
        dist = (0.5f - value) * g.x;
      } else {
        // 1 - gradient_value < value <= 1.
        dist = -0.5f * (g.x + g.y) + sqrtf(2.0f * g.x * g.y * (1.0f - value));
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
        for (auto x = 1; x < width; ++x) {
          auto pos = vec2i(x, y);
          auto dist = distances_->Get(pos);
          if (dist > 0.0f) {
            UpdateDistance(pos, vec2i(0, -1), &dist);
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
    } while (any_distance_changed_);

    do {
      any_distance_changed_ = false;
      // Propagate from bottom up, starting with the second row from the bottom.
      for (auto y = height - 2; y >= 0; --y) {
        // Propagate distances to the left.
        for (auto x = width - 1; x >= 0; --x) {
          auto pos = vec2i(x, y);
          auto dist = distances_->Get(pos);
          if (dist > 0.0) {
            UpdateDistance(pos, vec2i(0, 1), &dist);
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
    const auto new_dist = ComputeDistanceToEdge(edge_pixel, new_xy_dist);
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
                              const vec2i& vec_to_edge_pixel) {
    auto pixel_value = image_->Get(pixel);

    // If the pixel value is negative or 0, return kLargeDistance so that
    // processing will continue.
    if (pixel_value == 0) return kLargeDistance;

    // The returned value is expected in range of 0.0 - 1.0.
    const auto normalized_value =
        static_cast<float>(pixel_value) / std::numeric_limits<T>::max();

    vec2 vec_to_edge_pixel_f = vec2(vec_to_edge_pixel);

    // Use the length of the vector to the edge pixel to estimate the real
    // distance to the edge.
    // Optimization: This code was reworked to avoid fcmp since it is expensive.
    float length;
    bool should_estimate_based_on_direction_to_edge;
    if (vec_to_edge_pixel.x == 0) {
      length = abs(vec_to_edge_pixel.y);
      should_estimate_based_on_direction_to_edge = vec_to_edge_pixel.y != 0;
    } else if (vec_to_edge_pixel.y == 0) {
      length = abs(vec_to_edge_pixel.x);
      should_estimate_based_on_direction_to_edge = vec_to_edge_pixel.x != 0;
    } else {
      length = vec_to_edge_pixel_f.Length();
      should_estimate_based_on_direction_to_edge = length > 0.0f;
    }

    const auto dist =
        should_estimate_based_on_direction_to_edge ?
            // Estimate based on direction to edge (accurate for large vectors).
            ApproximateDistanceToEdge(normalized_value, vec_to_edge_pixel_f)
                      :
                      // Estimate based on local gradient only.
            ApproximateDistanceToEdge(normalized_value, gradients_.Get(pixel));
    return length + dist;
  }

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

}  // namespace vr

#endif  // FLATUI_INTERNAL_FAST_ANTIALIAS_DISTANCE_COMPUTER_H
