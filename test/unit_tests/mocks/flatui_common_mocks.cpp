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

#include "mocks/flatui_common_mocks.h"

namespace flatui {

void SetHoverClickColor(const mathfu::vec4 &hover_color,
                        const mathfu::vec4 &click_color) {
  FlatUICommonMocks::get_mocks().SetHoverClickColor(hover_color, click_color);
}

Event TextButton(const char *text, float size, const Margin &margin) {
  return FlatUICommonMocks::get_mocks().TextButton(text, size, margin);
}

Event ImageButton(const fplbase::Texture &texture, float size,
                  const Margin &margin, const char *id) {
  return FlatUICommonMocks::get_mocks().ImageButton(texture, size, margin, id);
}

Event TextButton(const fplbase::Texture &texture, const Margin &texture_margin,
                 const char *text, float size, const Margin &margin,
                 const ButtonProperty property) {
  return FlatUICommonMocks::get_mocks().TextButton(
      texture, texture_margin, text, size, margin, property);
}

Event CheckBox(const fplbase::Texture &texture_checked,
               const fplbase::Texture &texture_unchecked, const char *label,
               float size, const Margin &margin, bool *is_checked) {
  return FlatUICommonMocks::get_mocks().CheckBox(
      texture_checked, texture_unchecked, label, size, margin, is_checked);
}

Event Slider(const fplbase::Texture &tex_bar, const fplbase::Texture &tex_knob,
             const mathfu::vec2 &size, float bar_height, const char *id,
             float *slider_value) {
  return FlatUICommonMocks::get_mocks().Slider(tex_bar, tex_knob, size,
                                               bar_height, id, slider_value);
}

Event ScrollBar(const fplbase::Texture &tex_background,
                const fplbase::Texture &tex_foreground,
                const mathfu::vec2 &size, float bar_size, const char *id,
                float *scroll_value) {
  return FlatUICommonMocks::get_mocks().ScrollBar(
      tex_background, tex_foreground, size, bar_size, id, scroll_value);
}

void EventBackground(Event event) {
  FlatUICommonMocks::get_mocks().EventBackground(event);
}

}  // flatui
