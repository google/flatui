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

#ifndef FPL_FLATUI_COMMON_H
#define FPL_FLATUI_COMMON_H

#include "fplbase/material.h"

namespace flatui {

/// @file
/// @addtogroup flatui_common_widgets
/// @{
///
/// @brief This file contains some common widgets that will be widely
/// useful.
///
/// FlatUI is structured to be very compositional, meaning that no one widget
/// will always be universally useful for every UI. This means that these
/// widgets are intended for the average UI, but may not be sufficient for
/// custom use cases that extend beyond the scope of these widgets. Therefore
/// these widgets may also serve as building blocks, or as examples, to easily
/// build your own specialized widgets.

/// @enum ButtonProperty
///
/// @brief Specifies the button properties used in common widgets.
///
/// `kButtonPropertyDisabled` disables buttons (`CheckEvent()` calls will be
/// skipped).
/// `kButtonPropertyImageLeft` shows the specified image to the left of the text
/// label in a TextButton.
/// `kButtonPropertyImageRight` shows the specified image to the right of the
/// text label in a TextButton.
enum ButtonProperty {
  kButtonPropertyDisabled = 1,
  kButtonPropertyImageLeft = 2,
  kButtonPropertyImageRight = 4,
};

/// @brief The bitwise OR operator for ButtonProperties.
///
/// E.g. `c = a | b;`
///
/// @param[in] a The first ButtonProperty whose `int` value should be used in
/// the bitwise OR.
/// @param[in] b The second ButtonProperty whose `int` value should be used in
/// the bitwise OR.
///
/// @return Returns a new ButtonProperty that is formed from the result of the
/// bitwise OR of the two input ButtonProperties' `int` values.
inline ButtonProperty operator|(ButtonProperty a, ButtonProperty b) {
  return static_cast<ButtonProperty>(static_cast<int>(a) | static_cast<int>(b));
}

/// @brief The bitwise OR assignment operator for ButtonProperties.
///
/// E.g. `a |= b;`
///
/// @param[in] a The modifiable ButtonProperty lvalue whose `int` value should
/// be used in the bitwise OR. It also captures the return value of the
/// function.
/// @param[in] b The second ButtonProperty whose `int` value is used in the
/// bitwise OR.
///
/// @return Returns a new ButtonProperty that is formed from the result of the
/// bitwise OR of the two input ButtonProperties' `int` values.
inline ButtonProperty operator|=(ButtonProperty &a, const ButtonProperty &b) {
  a = a | b;
  return a;
}

/// @brief Some of the widgets provide user feedback by rendering a transparent
/// background color to signal that the user is hovering over or
/// clicking/touching/interacting with the widget.
///
/// Be default, these are a dark grey and a medium grey, respectively. Both
/// are semi-transparent.
///
/// @param[in] hover_color A const vec4 reference to the RGBA color values to
/// use when hovering over a widget.
/// @param[in] click_color A const vec4 reference to the RGBA color values to
/// use when clicking on a widget.
void SetHoverClickColor(const mathfu::vec4 &hover_color,
                        const mathfu::vec4 &click_color);

/// @brief A simple button showing clickable text.
///
/// @note Uses the colors that are set via `SetHoverClickColor`.
///
/// @param[in] text A C-string of text to display on the button.
/// @param[in] size A float indicating the vertical height.
/// @param[in] margin A Margin that should be placed around the text.
///
/// @return Returns the Event type for the button.
Event TextButton(const char *text, float size, const Margin &margin);

/// @brief A simple button showing a clickable image.
///
/// @note Uses the colors that are set via `SetHoverClickColor`.
///
/// @param[in] texture The Texture of the image to display.
/// @param[in] size A float indicating the vertical height.
/// @param[in] margin A Margin around the `texture`.
/// @param[in] id A C-string to uniquely identify the button.
///
/// @return Returns the Event type for the button.
Event ImageButton(const fplbase::Texture &texture, float size,
                  const Margin &margin, const char *id);

/// @brief A simple button showing clickable text with an image shown beside it.
///
/// @note Uses the colors that are set via `SetHoverClickColor`.
///
/// @param[in] texture The Texture of the image to display beside the button.
/// @param[in] texture_margin The Margin around the `texture`.
/// @param[in] text A C-string of text to display on the button.
/// @param[in] size A float indicating the vertical height.
/// @param[in] margin A Margin that should be placed around the text.
/// @param[in] property A ButtonProperty enum corresponding to where the
/// image should be placed, relative to the button.
///
/// @return Returns the Event type for the button.
Event TextButton(const fplbase::Texture &texture, const Margin &texture_margin,
                 const char *text, float size, const Margin &margin,
                 const ButtonProperty property);

/// @brief A checkbox with a label next to it.
///
/// @param[in] texture_checked A const reference to the Texture for when the
/// box is checked.
/// @param[in] texture_unchecked A const reference to the Texture for when the
/// box is unchecked.
/// @param[in] label A C-string to be used as the label that appears next to the
/// checkbox.
/// @param[in] size A float corresponding to the size of the checkbox and label.
/// @param[in] margin A Margin around the checkbox.
/// @param[in,out] is_checked A pointer to a bool determining if the checkbox is
/// checked or not. It will capture the output of the `kEventWentUp` as well.
///
/// @return Returns the Event type for the checkbox.
Event CheckBox(const fplbase::Texture &texture_checked,
               const fplbase::Texture &texture_unchecked, const char *label,
               float size, const Margin &margin, bool *is_checked);

/// @brief A clider to change a numeric value.
///
/// @param[in] tex_bar The Texture for the slider. Typically this is a bar of
/// some kind as a ninepatch texture.
/// @param[in] tex_knob The Texture for the knob to move on top of the
/// `tex_bar`.
/// @param[in] size A const vec2 reference to specify the whole size, including
/// the margin, and relative size of the slider.
/// @param[in] bar_height A float corresponding the the Y ratio of the bar
/// (usually 0.5).
/// @param[in] id A C-string to uniquely identify the slider.
/// @param[out] slider_value A pointer to a float between 0.0 and 1.0 inclusive,
/// which contains the position of the slider.
Event Slider(const fplbase::Texture &tex_bar, const fplbase::Texture &tex_knob,
             const mathfu::vec2 &size, float bar_height, const char *id,
             float *slider_value);

/// @brief A scrollbar to indicate position in a scroll view.
///
/// @note The background and foreground Textures must be a ninepatch texture.
///
/// @param[in] tex_background A const Texture reference for the background.
/// @param[in] tex_foreground A const Texture reference for the foreground.
/// @param[in] size A const vec2 reference to specify the whole size, including
/// the margin, and the relative size of the scroll bar.
/// @param[in] bar_size A float corresponding to the size of the scroll bar.
/// @param[in] id A C-string to uniquely identify the scroll bar.
/// @param[out] scroll_value A pointer to a float between 0.0 and 1.0 inclusive,
/// which contains the position of the slider.
///
/// @return Returns the Event type for the scroll bar.
Event ScrollBar(const fplbase::Texture &tex_background,
                const fplbase::Texture &tex_foreground,
                const mathfu::vec2 &size, float bar_size, const char *id,
                float *scroll_value);

/// @brief Sets a background color of the widget based on the event status.
///
/// If the event is `kEventIsDown`, the background color used will be the
/// `click_color`. If the event is `kEventHover`, the background color used
/// will be `hover_color`.
///
/// @param[in] event The Event type used to determine the background color.
void EventBackground(Event event);
/// @}

}  // namespace flatui

#endif  // FPL_FLATUI_COMMON_H
