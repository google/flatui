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

#ifndef FPL_IMGUI_H
#define FPL_IMGUI_H

#include <functional>
#include <string>
#include <vector>
#include <map>

#include "fplbase/input.h"
#include "fplbase/material_manager.h"
#include "mathfu/constants.h"
#include "font_manager.h"

namespace fpl {
namespace gui {

// The core function that drives the GUI.
// matman: the MaterialManager you want to use textures from.
// gui_definition: a function that defines all GUI elements using the GUI
// element construction functions.
// It will be run twice, once for layout, once for rendering & events.
//
// While an initialization of Imgui, it implicitly loads shaders used in the
// API below using MaterialManager.
// shaders/color.glslv & .glslf, shaders/font.glslv & .glslf
// shaders/textured.glslv & .glslf
void Run(MaterialManager &matman, FontManager &fontman, InputSystem &input,
         const std::function<void()> &gui_definition);

// Event types returned by most interactive elements. These are flags because
// multiple may occur during one frame, and thus should be tested using &.
// For example, it is not uncommon for the value to be
// EVENT_WENT_DOWN | EVENT_WENT_UP if the click/touch was quicker than the
// current frametime.
enum Event {
  // No event this frame. Also returned by all elememts during layout pass.
  EVENT_NONE = 0,
  // Pointing device (or button) was released this frame while over this
  // element. Only fires if this element was also the one to receive
  // the corresponding EVENT_WENT_DOWN.
  EVENT_WENT_UP = 1,
  // Pointing device went down this frame while over this element. You're not
  // guaranteed to also receive a EVENT_WENT_UP, the pointing device may move
  // to another element (or no element) before that happens.
  EVENT_WENT_DOWN = 2,
  // Pointing device is currently being held down on top of this element.
  // You're not guaranteed to receive this event between an EVENT_WENT_DOWN
  // and a EVENT_WENT_UP, it occurs only if it spans multiple frames.
  // Only fires for the element the corresponding EVENT_WENT_DOWN fired on.
  EVENT_IS_DOWN = 4,
  // Pointing device is currently over the element but not pressed.
  // This event does NOT occur on touch screen devices and only occurs for
  // devices that use a mouse, or a gamepad (that emulates a mouse with a
  // selection). As such it is good to show a subtle form of highlighting
  // upon this event, but the UI should not rely on it to function.
  EVENT_HOVER = 8,
};

// Alignment and Direction of groups. Instead of using these directly, use
// the Layout enum below.
enum Alignment { ALIGN_TOPLEFT = 1, ALIGN_CENTER = 2, ALIGN_BOTTOMRIGHT = 3 };
enum Direction { DIR_HORIZONTAL = 4, DIR_VERTICAL = 8, DIR_OVERLAY = 12 };

// Specify how to layout a group. Elements can be positioned either
// horizontally next to eachother or vertically, with elements aligned
// to either side or centered.
// e.g. LAYOUT_HORIZONTAL_TOP looks like:
//
// A B C
// A   C
// A
enum Layout {
  LAYOUT_HORIZONTAL_TOP    = DIR_HORIZONTAL | ALIGN_TOPLEFT,
  LAYOUT_HORIZONTAL_CENTER = DIR_HORIZONTAL | ALIGN_CENTER,
  LAYOUT_HORIZONTAL_BOTTOM = DIR_HORIZONTAL | ALIGN_BOTTOMRIGHT,

  LAYOUT_VERTICAL_LEFT     = DIR_VERTICAL | ALIGN_TOPLEFT,
  LAYOUT_VERTICAL_CENTER   = DIR_VERTICAL | ALIGN_CENTER,
  LAYOUT_VERTICAL_RIGHT    = DIR_VERTICAL | ALIGN_BOTTOMRIGHT,

  LAYOUT_OVERLAY_CENTER    = DIR_OVERLAY | ALIGN_CENTER,
};

// Specify margins for a group, in units of virtual resolution.
struct Margin {
  // Create a margin with all 4 sides equal size.
  Margin(float m) : borders(m) {}
  // Create a margin with left/right set to x, and top/bottom to y.
  Margin(float x, float y) : borders(x, x, y, y) {}
  // Create a margin specifying all 4 sides individually.
  Margin(float left, float top, float right, float bottom)
      : borders(left, top, right, bottom) {}

  vec4 borders;
};

// Convert virtual screen coordinate to physical value.
mathfu::vec2i VirtualToPhysical(const mathfu::vec2 &v);

// Retrieve the scaling factor for the virtual resolution.
float GetScale();

// Render an image as a GUI element.
// texture_name: filename of the image, must have been loaded with
// the material manager before.
// ysize: vertical size in virtual resolution. xsize will be derived
// automatically based on the image dimensions.
void Image(const char *texture_name, float ysize);

// Render an label as a GUI element.
// text: label string in UTF8
// ysize: vertical size in virtual resolution. xsize will be derived
// automatically based on the text length.
void Label(const char *text, float ysize);

// Set Label's text color.
void SetTextColor(const vec4 &color);

// Create a group of elements with the given layout and intra-element spacing.
// Start/end calls must be matched and may be nested to create more complex
// layouts.
void StartGroup(Layout layout, float spacing = 0,
                const char *id = "__group_id__");
void EndGroup();

// The following functions are specific to a group and should be called
// after StartGroup (and before any elements):

// Sets the margin for the current group.
void SetMargin(const Margin &margin);

// Check for events on the current group.
Event CheckEvent();

// Set the background for the group. May use alpha.
void ColorBackground(const vec4 &color);

// Set the background texture for the group.
void ImageBackground(const Texture &tex);

// Set the background texture for the group with nine patch settings.
// In the patch_info, the user can define nine patch settings
// as vec4(x0, y0, x1, y1) where
// (x0,y0): top-left corner of stretchable area in UV coordinate.
// (x1,y1): bottom-right corner of stretchable area in UV coordinate.
// The coordinates are in UV value in the texture (0.0 ~ 1.0).
// For more information for nine patch, refer
//http://developer.android.com/guide/topics/graphics/2d-graphics.html#nine-patch
void ImageBackgroundNinePatch(const Texture &tex, const vec4 &patch_info);

// Make the current group into a scrolling group that can display arbitrary
// sized elements inside a window of "size", scrolled to the current "offset"
// (which the caller should store somewhere that survives the current frame).
// Call StartScroll right after StartGroup, and EndScroll right before EndGroup.
void StartScroll(const vec2 &size, vec2i *offset);
void EndScroll();

// Put a custom element with given size.
// Renderer function is invoked while render pass to render the element.
void CustomElement(const vec2 &virtual_size,
                   const char *id,
                   const std::function<void(const vec2i &pos,
                                            const vec2i &size)>renderer);
void RenderTexture(const Texture &tex, const vec2i &pos, const vec2i &size);

// The default virtual resolution used if none is set.
const float IMGUI_DEFAULT_VIRTUAL_RESOLUTION = 1000.0f;

// Position the GUI within the screen as a whole, call this as first thing
// in your gui_definition.
// virtual_resolution: the virtual resolution of the smallest
// dimension of your screen (the Y size in landscape mode, or X in portrait).
// All dimension specified below are relative to this.
// If this function is not called, it defaults to virtual resolution set to
// IMGUI_DEFAULT_VIRTUAL_RESOLUTION, and top/left placement.
void PositionUI(float virtual_resolution, Layout horizontal, Layout vertical);

// Use glyph cache for a font rendering
#define USE_GLYPHCACHE (1)

}  // namespace gui
}  // namespace fpl

#endif  // FPL_IMGUI_H
