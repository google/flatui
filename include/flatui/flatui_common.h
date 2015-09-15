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

#ifndef FPL_FLATUI_COMMON_H
#define FPL_FLATUI_COMMON_H

// This file contains some "typical" widgets that will likely be widely
// useful.
// FlatUI is structured to be very compositional, meaning that no one widget
// will always be universally useful for everybody.
// That means these widgets have a special status: they are what you want to
// get an average UI going, but if you have special needs that go beyond
// these widgets, they can instead serve as building blocks, or as examples
// to build your own special purpose widgets easily.

#include "fplbase/material.h"

namespace fpl {
namespace gui {

// Definitions to specify button properties used in common widget.
enum ButtonProperty {
  kButtonPropertyDisabled = 1,
  kButtonPropertyImageLeft = 2,
  kButtonPropertyImageRight = 4,
};
inline ButtonProperty operator|(ButtonProperty a, ButtonProperty b) {
  return static_cast<ButtonProperty>(static_cast<int>(a) | static_cast<int>(b));
}
inline ButtonProperty operator|=(ButtonProperty a, ButtonProperty b) {
  return a | b;
}

// Some of the widgets below give user feedback by rendering a transparent
// background to signal user hovering over (or selected widgets in case of
// gamepad/keyboard navigation) and clicking/touching/interacting with the
// widget. By default, these are a dark grey and a middle grey, respectively,
// both semi-transparent.
void SetHoverClickColor(const vec4 &hover_color, const vec4 &click_color);

// A simple button showing a text to click on, with `size` indicating
// vertical height. Uses the colors set above.
Event TextButton(const char *text, float size, const Margin &margin);

// A simple button showing an image to click on, with `size` indicating
// vertical height. Uses the colors set above.
Event ImageButton(const Texture &texture, float size, const Margin &margin,
                  const char *id);

// A text button showing an image shown on the left.
Event TextButton(const Texture &texture, const Margin &texure_margin,
                 const char *text, float size,
                 const Margin &margin, const ButtonProperty property);

// A checkbox with a label next to it. Pass textures for the two states,
// the label, the height (for both image & label).
Event CheckBox(const Texture &texture_checked,
               const Texture &texture_unchecked,
               const char *label, float size, const Margin &margin,
               bool *is_checked);

// A slider to change a numeric value. Pass textures for the background
// (usually a bar of some kind, which should be a ninepatch texture) and a knob
// to move on top of it.
// Specify the size of the whole, at what Y ratio the bar sits (usually 0.5).
// The slider value is between 0.0 and 1.0 inclusive, which should be easy
// to scale to any float/int range.
Event Slider(const Texture &tex_bar, const Texture &tex_knob,
             const vec2 &size, float bar_height, const char *id,
             float *slider_value);

// A scrollbar to indicate a position of a scroll view.
// Pass textures for the background and the foreground both must be a ninepatch
// texture.
// Specify the size of the whole and a relative size of the scroll bar inside.
// The scroll value is between 0.0 and 1.0 inclusive, which should be easy
// to scale to any float/int range.
Event ScrollBar(const Texture &tex_background, const Texture &tex_foreground,
                const vec2 &size, float bar_size, const char *id,
                float *scroll_value);

}  // namespace gui
}  // namespace fpl

#endif  // FPL_FLATUI_COMMON_H
