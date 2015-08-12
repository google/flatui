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

#include "precompiled.h"
#include "fplbase/renderer.h"
#include "flatui/flatui.h"

namespace fpl {
namespace gui {

vec4 g_hover_color = vec4(0.5f, 0.5f, 0.5f, 0.5f);
vec4 g_click_color = vec4(1.0f, 1.0f, 1.0f, 0.5f);

void SetHoverClickColor(const vec4 &hover_color, const vec4 &click_color) {
  g_hover_color = hover_color;
  g_click_color = click_color;
}

void EventBackground(Event event) {
  if (event & kEventIsDown) ColorBackground(g_click_color);
  else if (event & kEventHover) ColorBackground(g_hover_color);
}

Event ImageButton(const Texture &texture, float size, const Margin &margin,
                  const char *id) {
  StartGroup(kLayoutVerticalLeft, size, id);
    SetMargin(margin);
    auto event = CheckEvent();
    EventBackground(event);
    Image(texture, size);
  EndGroup();
  return event;
}

Event TextButton(const char *text, float size, const Margin &margin) {
  StartGroup(kLayoutVerticalLeft, size, text);
    SetMargin(margin);
    auto event = CheckEvent();
    EventBackground(event);
    Label(text, size);
  EndGroup();
  return event;
}

Event CheckBox(const Texture &texture_checked,
               const Texture &texture_unchecked,
               const char *label, float size, const Margin &margin,
               bool *is_checked) {
  StartGroup(kLayoutHorizontalBottom, 0, label);
    auto event = CheckEvent();
    Image(*is_checked ? texture_checked : texture_unchecked, size);
    SetMargin(margin);
    Label(label, size);
    // Change the state here, to ensure it changes after we've already rendered.
    if (event & kEventWentUp) {
      *is_checked = !*is_checked;
    }
  EndGroup();
  return event;
}

Event Slider(const Texture &tex_bar, const Texture &tex_knob,
             const vec2 &size, float bar_height, const char *id,
             float *slider_value) {
  StartGroup(kLayoutHorizontalBottom, 0, id);
    StartSlider(kDirHorizontal, slider_value);
      auto event = CheckEvent();
      CustomElement(size, id, [&tex_knob, &tex_bar, bar_height,
                               slider_value](const vec2i &pos,
                                             const vec2i &size){
        // Render the slider.
        auto bar_pos = pos;
        auto bar_size = size;
        bar_pos += vec2i(size.y() * 0.5, size.y() * (1.0 - bar_height) * 0.5);
        bar_size = vec2i(std::max(bar_size.x() - size.y(), 0),
                         bar_size.y() * bar_height);

        auto knob_pos = pos;
        auto knob_sizes = vec2i(size.y(), size.y());
        knob_pos.x() += *slider_value * static_cast<float>(size.x() - size.y());
        RenderTextureNinePatch(tex_bar, vec4(0.5f, 0.5f, 0.5f, 0.5f),
                      bar_pos, bar_size);
        RenderTexture(tex_knob, knob_pos, knob_sizes);
      });
    EndSlider();
  EndGroup();
  return event;
}

}  // gui
}  // fpl

