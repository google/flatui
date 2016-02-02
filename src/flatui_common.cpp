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
#include "flatui/flatui_common.h"

using fplbase::Texture;
using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec4;

namespace flatui {

vec4 g_hover_color = vec4(0.5f, 0.5f, 0.5f, 0.5f);
vec4 g_click_color = vec4(1.0f, 1.0f, 1.0f, 0.5f);

void SetHoverClickColor(const vec4 &hover_color, const vec4 &click_color) {
  g_hover_color = hover_color;
  g_click_color = click_color;
}

void EventBackground(Event event) {
  if (event & kEventIsDown)
    ColorBackground(g_click_color);
  else if (event & kEventHover)
    ColorBackground(g_hover_color);
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

void ShowTexture(const Texture &texture, const Margin &texture_margin,
                 float size) {
  StartGroup(kLayoutVerticalLeft, size);
  SetMargin(texture_margin);
  Image(texture, size);
  EndGroup();
}

Event TextButton(const Texture &texture, const Margin &texture_margin,
                 const char *text, float size, const Margin &text_margin,
                 const ButtonProperty property) {
  StartGroup(kLayoutHorizontalCenter, 0, text);
  auto event = kEventNone;
  if (!(property & kButtonPropertyDisabled)) {
    event = CheckEvent();
    EventBackground(event);
  }

  if (property & kButtonPropertyImageLeft) {
    ShowTexture(texture, texture_margin, size);
  }

  StartGroup(kLayoutVerticalLeft, size);
  SetMargin(text_margin);
  Label(text, size);
  EndGroup();

  if (property & kButtonPropertyImageRight) {
    ShowTexture(texture, texture_margin, size);
  }

  EndGroup();
  return event;
}

Event CheckBox(const Texture &texture_checked, const Texture &texture_unchecked,
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

Event Slider(const Texture &tex_bar, const Texture &tex_knob, const vec2 &size,
             float bar_height, const char *id, float *slider_value) {
  StartGroup(kLayoutHorizontalBottom, 0, id);
  StartSlider(kDirHorizontal, size.y() * 0.5f, slider_value);
  auto event = CheckEvent();
  // Show focus area when controled by a gamepad.
  if (!IsLastEventPointerType()) EventBackground(event);
  CustomElement(size, id, [&tex_knob, &tex_bar, bar_height, slider_value](
                              const vec2i &pos, const vec2i &size) {
    // Render the slider.
    auto bar_pos = pos;
    auto bar_size = size;
    bar_pos += vec2i(size.y() / 2,
                     static_cast<int>(size.y() * (1.0 - bar_height) / 2.0f));
    bar_size = vec2i(std::max(bar_size.x() - size.y(), 0),
                     static_cast<int>(bar_size.y() * bar_height));

    auto knob_pos = pos;
    vec2i knob_sizes(size.y(), size.y());
    knob_pos.x() += static_cast<int>(*slider_value *
                                     static_cast<float>(size.x() - size.y()));
    RenderTextureNinePatch(tex_bar, vec4(0.5f, 0.5f, 0.5f, 0.5f), bar_pos,
                           bar_size);
    RenderTexture(tex_knob, knob_pos, knob_sizes);
  });
  EndSlider();
  EndGroup();
  return event;
}

Event ScrollBar(const Texture &tex_background, const Texture &tex_foreground,
                const vec2 &size, float bar_size, const char *id,
                float *scroll_value) {
  StartGroup(kLayoutHorizontalBottom, 0, id);
  Direction direction;
  int32_t dimension;
  if (size.y() < size.x()) {
    direction = kDirHorizontal;
    dimension = 0;
  } else {
    direction = kDirVertical;
    dimension = 1;
  }
  float margin = size[dimension] * bar_size * 0.5f;
  StartSlider(direction, margin, scroll_value);

  auto event = CheckEvent();
  // Show focus area when controled by a gamepad.
  if (!IsLastEventPointerType()) EventBackground(event);
  CustomElement(size, id, [&tex_foreground, &tex_background, bar_size,
                           scroll_value, dimension, margin](
                              const vec2i &pos, const vec2i &render_size) {
    // Set up the bar position and size.
    auto bar_render_pos = pos;
    bar_render_pos[dimension] += static_cast<int>(
        *scroll_value * (render_size[dimension] - margin * 2.0f * GetScale()));

    vec2i bar_render_size(render_size.x(), render_size.y());
    bar_render_size[dimension] =
        static_cast<int>(bar_render_size[dimension] * bar_size);

    RenderTextureNinePatch(tex_background, vec4(0.5f, 0.5f, 0.5f, 0.5f), pos,
                           render_size);
    RenderTextureNinePatch(tex_foreground, vec4(0.5f, 0.5f, 0.5f, 0.5f),
                           bar_render_pos, bar_render_size);
  });
  EndSlider();
  EndGroup();
  return event;
}

}  // namespace flatui
