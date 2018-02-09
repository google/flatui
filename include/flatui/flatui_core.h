// Copyright 2017 Google Inc. All rights reserved.
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

#ifndef FPL_FLATUI_CORE_H
#define FPL_FLATUI_CORE_H

#include "mathfu/constants.h"
#include "mathfu/glsl_mappings.h"

namespace flatui {

// clang-format off
/// @enum Alignment
///
/// @brief Alignment of the groups.
///
/// @note: `kAlignTop` and `kAlignLeft`(as well as `kAlignBottom` and
/// `kAlignRight`) are intended to be aliases of one another, as they both
/// express the same thing on their respective axis.
///
/// **Enumerations**:
///
/// * `kAlignTop` or `kAlignLeft` (`1`) - Align along the top (or left,
///                                       depending on the axis).
/// * `kAlignCenter` (`2`) - Align along the center of the axis.
/// * `kAlignBottom` or `kAlignRight` (`3`) - Align along the bottom (or right,
///                                           depending on the axis).
enum Alignment {
  kAlignTop = 1,
  kAlignLeft = 1,
  kAlignCenter = 2,
  kAlignBottom = 3,
  kAlignRight = 3
};

/// @enum Direction
///
/// @brief Direction of the groups.
///
/// **Enumerations**:
///
/// * `kDirHorizontal` (`4`) - The direction of the group is horizontal
///                            (x-axis).
/// * `kDirVertical` (`8`) - The direction of the group is vertical (y-axis).
/// * `kDirOverlay` (`12`) - The group of elements are placed on top of one
///                          another (along the z-axis).
enum Direction {
  kDirHorizontal = 4,
  kDirVertical = 8,
  kDirOverlay = 12
};

/// @enum Layout
///
/// @brief Specify how to layout a group.
///
/// Elements can be positioned either horizontally or vertically. The elements
/// can be aligned on either side, or centered.
///
/// For example, `kLayoutHorizontalTop` indicates that the elements are layed
/// out from left-to-right, with items of uneven height being aligned from the
/// top.
///
/// In this example, we have three elements: `A` with a height of 3, `B` with
/// a height of 1, and `C` with a height of 2. So we lay the elements out from
/// left to right in the order `A`->`B`->`C`, and we align them along the top.
///
/// That layout would look like this:
///
/// A @htmlonly &nbsp @endhtmlonly  B @htmlonly &nbsp @endhtmlonly  C
///
/// A @htmlonly &nbsp&nbsp&nbsp&nbsp&nbsp&nbsp @endhtmlonly C
///
/// A
///
/// **Enumerations**:
///
/// * `kLayoutHorizontalTop` (`5`) - Lay out the elements horizontally, aligning
///                                  elements of uneven height along the top.
/// * `kLayoutHorizontalCenter` (`6`) - Lay out the elements horizontally,
///                                     aligning elements of uneven height along
///                                     the center.
/// * `kLayoutHotizontalBottom` (`7`) - Lay out the elements horizontally,
///                                     aligning elements of uneven height along
///                                     the bottom.
/// * `kLayoutVerticalLeft` (`9`) - Lay out the elements vertically, aligning
///                                 elements of uneven width along the left.
/// * `kLayoutVerticalCenter` (`10`) - Lay out the elements vertically, aligning
///                                   elements of uneven width along the center.
/// * `kLayoutVerticalRight` (`11`) - Lay out the elements vertically, aligning
///                                  the elements of uneven width along the
///                                  right.
/// * `kLayoutOverlay` (`14`) - Lay out the elements on top of one another, from
///                             the center.
enum Layout {
  kLayoutHorizontalTop =    kDirHorizontal| kAlignTop,
  kLayoutHorizontalCenter = kDirHorizontal| kAlignCenter,
  kLayoutHorizontalBottom = kDirHorizontal| kAlignBottom,
  kLayoutVerticalLeft =     kDirVertical  | kAlignLeft,
  kLayoutVerticalCenter =   kDirVertical  | kAlignCenter,
  kLayoutVerticalRight =    kDirVertical  | kAlignRight,
  kLayoutOverlay =          kDirOverlay   | kAlignCenter,
};
// clang-format on

/// @var FLATUI_DEFAULT_VIRTUAL_RESOLUTION
///
/// @brief The default virtual resolution, if none is set.
const float FLATUI_DEFAULT_VIRTUAL_RESOLUTION = 1000.0f;

/// @var kDefaultGroupID
///
/// @brief A sentinel value for group IDs.
const char *const kDefaultGroupID = "__group_id__";

/// @var kDefaultImageID
///
/// @brief A sentinel value for image IDs.
const char *const kDefaultImageID = "__image_id__";

/// @struct Margin
///
/// @brief Specifies the margins for a group, in units of virtual
/// resolution.
struct Margin {
  /// @brief Create a Margin with all four sides of equal size.
  ///
  /// @param[in] m A float size to be used as the margin on all sides.
  Margin(float m) : borders(mathfu::vec4(m)) {}

  /// @brief Create a Margin with the left and right sizes of `x`, and top
  /// and bottom sizes of `y`.
  ///
  /// @param[in] x A float size to be used as the margin for the left and
  /// right sides.
  /// @param[in] y A float size to be used as the margin for the right and
  /// left sides.
  Margin(float x, float y) : borders(mathfu::vec4(x, y, x, y)) {}

  // Create a margin specifying all 4 sides individually.
  /// @brief Creates a margin specifying all four sides individually.
  ///
  /// @param[in] left A float size to be used as the margin for the left side.
  /// @param[in] top A float size to be used as the margin for the top side.
  /// @param[in] right A float size to be used as the margin for the right side.
  /// @param[in] bottom A float size to be used as the margin for the bottom
  /// side.
  Margin(float left, float top, float right, float bottom)
      : borders(mathfu::vec4(left, top, right, bottom)) {}

  /// @var borders
  ///
  /// @brief A vector of four floats containing the values for the
  /// four sides of the margin.
  ///
  /// The internal layout of the margin is: `left`, `top`, `right`, `bottom`.
  mathfu::vec4_packed borders;
};

}  // namespace flatui

#endif  // FPL_FLATUI_CORE_H
