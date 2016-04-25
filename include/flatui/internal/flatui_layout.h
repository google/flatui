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

// This file contains the implementation of the layout algorithm of FlatUI.
// It is mostly used internally by FlatUI, but can possibly also be useful
// standalone if you need to layout something independent of the rendering &
// input portions of FlatUI, e.g.:
//
// LayoutManager lm;
// lm.Run([&]() {
//   lm.StartGroup(..);
//     lm.Element(vec2(100, 20), "myid",
//       [](const vec2i &pos, const vec2i &size) { /* render rect */ });
//   lm.EndGroup(..);
// });
//
// See FlatUI documentation for more information on these functions.

#ifndef FPL_FLATUI_LAYOUT_H
#define FPL_FLATUI_LAYOUT_H

#include "flatui/flatui.h"
#include "flatui/internal/flatui_util.h"

namespace flatui {

using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec3;
using mathfu::vec3i;
using mathfu::vec4;
using mathfu::vec4i;
using mathfu::mat4;

// This holds the transient state of a group while its layout is being
// calculated / rendered.
class Group {
 public:
  Group()
      : direction_(kDirHorizontal),
        align_(kAlignTop),
        spacing_(0),
        size_(mathfu::kZeros2i),
        position_(mathfu::kZeros2i),
        element_idx_(0),
        margin_(mathfu::kZeros4i) {}

  Group(Direction direction, Alignment align, int spacing, size_t element_idx)
      : direction_(direction),
        align_(align),
        spacing_(spacing),
        size_(mathfu::kZeros2i),
        position_(mathfu::kZeros2i),
        element_idx_(element_idx),
        margin_(mathfu::kZeros4i) {}

  // Extend this group with the size of a new element, and possibly spacing
  // if it wasn't the first element.
  void Extend(const vec2i &extension) {
    switch (direction_) {
      case kDirHorizontal:
        size_ = vec2i(size_.x() + extension.x() + (size_.x() ? spacing_ : 0),
                      std::max(size_.y(), extension.y()));
        break;
      case kDirVertical:
        size_ = vec2i(std::max(size_.x(), extension.x()),
                      size_.y() + extension.y() + (size_.y() ? spacing_ : 0));
        break;
      case kDirOverlay:
        size_ = vec2i(std::max(size_.x(), extension.x()),
                      std::max(size_.y(), extension.y()));
        break;
    }
  }

  Direction direction_;
  Alignment align_;
  int spacing_;
  vec2i size_;
  vec2i position_;
  size_t element_idx_;
  vec4i margin_;
};

// We create one of these per GUI element, so new fields should only be
// added when absolutely necessary.
struct UIElement {
  UIElement(const vec2i &_size, HashedId _hash)
      : size(_size),
        extra_size(mathfu::kZeros2i),
        hash(_hash),
        interactive(false) {}
  vec2i size;        // Minimum on-screen size computed by layout pass.
  vec2i extra_size;  // Additional size in a scrolling area (TODO: remove?)
  HashedId hash;     // From id specified by the user.
  bool interactive;  // Wants to respond to user input.
};

// This holds the transient state while a layout is being performed.
// Call Run() on an instance to layout a definition.
// See the FlatUI documentation (or flatui.h) for a more extensive explanation
// of how this works.
class LayoutManager : public Group {
 public:
  LayoutManager(const vec2i &canvas_size)
      : Group(kDirVertical, kAlignLeft, 0, 0),
        layout_pass_(true),
        canvas_size_(canvas_size),
        virtual_resolution_(FLATUI_DEFAULT_VIRTUAL_RESOLUTION) {
    SetScale();
  }

  // Changes the virtual resolution (defaults to
  // FLATUI_DEFAULT_VIRTUAL_RESOLUTION).
  // All floating point sizes below for elements are in terms of this
  // resolution, which will then be converted to physical (pixel) based integer
  // coordinates during layout.
  void SetVirtualResolution(float virtual_resolution) {
    if (layout_pass_) {
      virtual_resolution_ = virtual_resolution;
      SetScale();
    }
  }

  vec2 GetVirtualResolution() const {
    return vec2(canvas_size_) / pixel_scale_;
  }

  // Helpers to convert entire vectors between virtual and physical coordinates.
  template <int D>
  mathfu::Vector<int, D> VirtualToPhysical(const mathfu::Vector<float, D> &v)
    const {
    return mathfu::Vector<int, D>(v * pixel_scale_ + 0.5f);
  }

  template <int D>
  mathfu::Vector<float, D> PhysicalToVirtual(const mathfu::Vector<int, D> &v)
    const {
    return mathfu::Vector<float, D>(v) / pixel_scale_;
  }

  // Retrieve the scaling factor for the virtual resolution.
  float GetScale() const { return pixel_scale_; }

  // Determines placement for the UI as a whole inside the available space
  // (screen).
  void PositionGroup(Alignment horizontal, Alignment vertical,
                     const vec2 &offset) {
    if (!layout_pass_) {
      auto space = canvas_size_ - size_;
      position_ = AlignDimension(horizontal, 0, space) +
                  AlignDimension(vertical, 1, space) +
                  VirtualToPhysical(offset);
    }
  }

  // Switch from the layout pass to the second pass (for positioning and
  // rendering etc).
  // Layout happens in two passes, where the first computes the sizes of
  // things, and the second assigns final positions based on that. As such,
  // you define your layout using a function (where you call StartGroup /
  // EndGroup / ELement etc.) which you call once before and once
  // after this function.
  // See the implementation of Run() below.
  bool StartSecondPass() {
    // If you hit this assert, you are missing an EndGroup().
    assert(!group_stack_.size());

    // Do nothing if there is no elements.
    if (elements_.size() == 0) return false;

    // Put in a sentinel element. We'll use this element to point to
    // when a group didn't exist during layout but it does during rendering.
    NewElement(mathfu::kZeros2i, kNullHash);

    position_ = mathfu::kZeros2i;
    size_ = elements_[0].size;

    layout_pass_ = false;
    element_it_ = elements_.begin();

    return true;
  }

  // Set the margin for the current group.
  void SetMargin(const Margin &margin) {
    margin_ = VirtualToPhysical(margin.borders);
  }

  // Generic element with user supplied renderer.
  void Element(
      const vec2 &virtual_size, const char *id,
      const std::function<void(const vec2i &pos, const vec2i &size)> renderer) {
    Element(virtual_size, HashId(id), renderer);
  }

  // Same, but using an already computed hash.
  void Element(
      const vec2 &virtual_size, HashedId hash,
      const std::function<void(const vec2i &pos, const vec2i &size)> renderer) {
    if (layout_pass_) {
      auto size = VirtualToPhysical(virtual_size);
      NewElement(size, hash);
      Extend(size);
    } else {
      auto element = NextElement(hash);
      if (element) {
        const vec2i pos = Position(*element);
        if (renderer) {
          renderer(pos, element->size);
        }
        Advance(element->size);
      }
    }
  }


  // An element that has sub-elements.
  void StartGroup(const Group &group, HashedId hash) {
    StartGroup(group.direction_, group.align_,
               static_cast<float>(group.spacing_), hash);
  }

  // An element that has sub-elements. Tracks its state in an instance of
  // Layout, that is pushed/popped from the stack as needed.
  void StartGroup(Direction direction, Alignment align, float spacing,
                  HashedId hash) {
    Group layout(direction, align, static_cast<int>(spacing), elements_.size());
    group_stack_.push_back(*this);
    if (layout_pass_) {
      NewElement(mathfu::kZeros2i, hash);
    } else {
      auto element = NextElement(hash);
      if (element) {
        layout.position_ = Position(*element);
        layout.size_ = element->size;
        // Make layout refer to element it originates from, iterator points
        // to next element after the current one.
        layout.element_idx_ = element_it_ - elements_.begin() - 1;
      } else {
        // This group did not exist during layout, but since all code inside
        // this group will run, it is important to have a valid element_idx_
        // to refer to, so we point it to our (empty) sentinel element:
        layout.element_idx_ = elements_.size() - 1;
      }
    }
    *static_cast<Group *>(this) = layout;
  }

  // Clean up the Group element started by StartGroup()
  void EndGroup() {
    // If you hit this assert, you have one too many EndGroup().
    assert(group_stack_.size());

    auto size = size_;
    auto margin = margin_.xy() + margin_.zw();
    auto element_idx = element_idx_;
    *static_cast<Group *>(this) = group_stack_.back();
    group_stack_.pop_back();
    if (layout_pass_) {
      size += margin;
      // Contribute the size of this group to its parent.
      Extend(size);
      // Set the size of this group as the size of the element tracking it.
      elements_[element_idx].size = size;
    } else {
      Advance(elements_[element_idx].size);
    }
  }

  // Return the position for the current enclosing StartGroup/EndGroup.
  const vec2i &GroupPosition() const { return position_; }

  // Return the size for the current enclosing StartGroup/EndGroup.
  vec2i GroupSize() const { return size_ + elements_[element_idx_].extra_size; }

  // Run two passes of layout.
  void Run(const std::function<void()> &layout_definition) {
    layout_definition();
    StartSecondPass();
    layout_definition();
  }

 protected:
  // (second pass): retrieve the next corresponding cached element we
  // created in the layout pass. This is slightly more tricky than a straight
  // lookup because event handlers may insert/remove elements.
  UIElement *NextElement(HashedId hash) {
    assert(!layout_pass_);
    auto backup = element_it_;
    while (element_it_ != elements_.end()) {
      // This loop usually returns on the first iteration, the only time it
      // doesn't is if an event handler caused an element to removed.
      auto &element = *element_it_;
      ++element_it_;
      if (EqualId(element.hash, hash)) return &element;
    }
    // Didn't find this id at all, which means an event handler just caused
    // this element to be added, so we skip it.
    element_it_ = backup;
    return nullptr;
  }

  // (layout pass): create a new element.
  void NewElement(const vec2i &size, HashedId hash) {
    assert(layout_pass_);
    elements_.push_back(UIElement(size, hash));
  }

  // (second pass): move the group's current position past an element of
  // the given size.
  void Advance(const vec2i &size) {
    assert(!layout_pass_);
    switch (direction_) {
      case kDirHorizontal:
        position_ += vec2i(size.x() + spacing_, 0);
        break;
      case kDirVertical:
        position_ += vec2i(0, size.y() + spacing_);
        break;
      case kDirOverlay:
        // Keep at starting position.
        break;
    }
  }

  // (second pass): return the top-left position of the current element, as a
  // function of the group's current position and the alignment.
  vec2i Position(const UIElement &element) const {
    assert(!layout_pass_);
    auto pos = position_ + margin_.xy();
    auto space = size_ - element.size - margin_.xy() - margin_.zw();
    switch (direction_) {
      case kDirHorizontal:
        pos += AlignDimension(align_, 1, space);
        break;
      case kDirVertical:
        pos += AlignDimension(align_, 0, space);
        break;
      case kDirOverlay:
        pos += AlignDimension(align_, 0, space);
        pos += AlignDimension(align_, 1, space);
        break;
    }
    return pos;
  }

 private:
  // Compute a space offset for a particular alignment for just the x or y
  // dimension.
  static vec2i AlignDimension(Alignment align, int dim, const vec2i &space) {
    vec2i dest(0, 0);
    switch (align) {
      case kAlignTop:  // Same as kAlignLeft.
        break;
      case kAlignCenter:
        dest[dim] += space[dim] / 2;
        break;
      case kAlignBottom:  // Same as kAlignRight.
        dest[dim] += space[dim];
        break;
    }
    return dest;
  }

  // Initialize the scaling factor for the virtual resolution.
  void SetScale() {
    auto scale = vec2(canvas_size_) / virtual_resolution_;
    pixel_scale_ = std::min(scale.x(), scale.y());
  }

 protected:
  bool layout_pass_;
  std::vector<UIElement> elements_;
  std::vector<UIElement>::iterator element_it_;
  std::vector<Group> group_stack_;
  vec2i canvas_size_;
  float virtual_resolution_;
  float pixel_scale_;
};

}  // namespace flatui

#endif  // FPL_FLATUI_LAYOUT_H
