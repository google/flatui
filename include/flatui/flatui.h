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

#ifndef FPL_FLATUI_H
#define FPL_FLATUI_H

#include <functional>
#include <string>

#include "fplbase/asset_manager.h"
#include "fplbase/input.h"
#include "mathfu/constants.h"
#include "font_manager.h"

namespace fpl {
namespace gui {

// The core function that drives the GUI.
// assetman: the AssetManager you want to use textures from.
// gui_definition: a function that defines all GUI elements using the GUI
// element construction functions.
// It will be run twice, once for layout, once for rendering & events.
//
// While an initialization of FlatUI, it implicitly loads shaders used in the
// API below using AssetManager.
// shaders/color.glslv & .glslf, shaders/font.glslv & .glslf
// shaders/textured.glslv & .glslf
void Run(AssetManager &assetman, FontManager &fontman, InputSystem &input,
         const std::function<void()> &gui_definition);

// Event types returned by most interactive elements. These are flags because
// multiple may occur during one frame, and thus should be tested using &.
// For example, it is not uncommon for the value to be
// kEventWentDown | kEventWentUp if the click/touch was quicker than the
// current frametime.
enum Event {
  // No event this frame. Also returned by all elememts during layout pass.
  kEventNone = 0,
  // Pointing device (or button) was released this frame while over this
  // element. Only fires if this element was also the one to receive
  // the corresponding kEventWentDown.
  kEventWentUp = 1,
  // Pointing device went down this frame while over this element. You're not
  // guaranteed to also receive a kEventWentUp, the pointing device may move
  // to another element (or no element) before that happens.
  kEventWentDown = 2,
  // Pointing device is currently being held down on top of this element.
  // You're not guaranteed to receive this event between an kEventWentDown
  // and a kEventWentUp, it occurs only if it spans multiple frames.
  // Only fires for the element the corresponding kEventWentDown fired on.
  kEventIsDown = 4,
  // Pointing device started dragging this frame while over this element.
  // The element is expected to call CapturePointer() API to recieve drag event
  // continuously even the pointer goes out of the element.
  kEventStartDrag = 8,
  // Pointing device finished dragging this frame.
  kEventEndDrag = 16,
  // Pointing device is currently in dragging mode.
  kEventIsDragging = 32,
  // Pointing device is currently over the element but not pressed.
  // This event does NOT occur on touch screen devices and only occurs for
  // devices that use a mouse, or a gamepad (that emulates a mouse with a
  // selection). As such it is good to show a subtle form of highlighting
  // upon this event, but the UI should not rely on it to function.
  kEventHover = 64,

  // For example, when performing a drag operation, the user recieves events in
  // the sequence below.
  // kEventWentDown, kEventIsDown (Until a pointer motion exceeds a threshold)
  // kEventStartDrag, kEventIsDragging and kEventEndDrag.
};

// Alignment and Direction of groups.
// Note that Top & Left (as well as Bottom & Right) are intended to be aliases
// of eachother, as they both express the same thing on their respective axis.
enum Alignment {
  kAlignTop = 1,
  kAlignLeft = 1,
  kAlignCenter = 2,
  kAlignBottom = 3,
  kAlignRight = 3
};
enum Direction {
  kDirHorizontal = 4,
  kDirVertical = 8,
  kDirOverlay = 12
};

// Specify how to layout a group. Elements can be positioned either
// horizontally next to eachother or vertically, with elements aligned
// to either side or centered.
// e.g. kLayoutHorizontalTop looks like:
//
// A B C
// A   C
// A
enum Layout {
  kLayoutHorizontalTop    = kDirHorizontal | kAlignTop,
  kLayoutHorizontalCenter = kDirHorizontal | kAlignCenter,
  kLayoutHorizontalBottom = kDirHorizontal | kAlignBottom,
  kLayoutVerticalLeft     = kDirVertical | kAlignLeft,
  kLayoutVerticalCenter   = kDirVertical | kAlignCenter,
  kLayoutVerticalRight    = kDirVertical | kAlignRight,
  kLayoutOverlay          = kDirOverlay | kAlignCenter,
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

// Multi line version of label.
// text: label string in UTF8
// ysize: vertical size in virtual resolution.
// size: max size of the label in virtual resolution.
//       0 for size.y indicates no height restriction.
//       The API renders whole texts in the label in this case.
void Label(const char *text, float ysize, const mathfu::vec2 &size);

// Set Label's text color.
void SetTextColor(const vec4 &color);

// Render an edit box as a GUI element.
// ysize: vertical size in virtual resolution.
// size: a size of the editbox in virtual resolution.
//       0 for size.x indicates an auto expanding edit box.
//       0 for size.y indicates a single line label.
// string: label string in UTF8
// returns true if the widget is in edit.
bool Edit(float ysize, const mathfu::vec2 &size, const char* id,
          std::string* string);

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

// Check for events on the current group.
// check_dragevent_only : Check only a drag event and ignore button events.
//                        If an element is not interested in a button event,
//                        The caller should set this flag for other elements
//                        correctly can recieve WENT_UP event, because WENT_UP
//                        event will be only recieved a same element that
//                        recieved corresponding WENT_DOWN event.
Event CheckEvent(bool check_dragevent_only);

// Call inside of a group that is meant to be like a popup inside of a
// kLayoutOverlay. It will cause all interactive elements in all groups that
// precede it to not respond to input.
void ModalGroup();

// Capture a pointer event.
// After the API call, the element with element_id will recieve pointer event
// exclusively until ReleasePointer() is called. This API is used mainly for a
// drag operation that an element wants to recieve events continuously.
void CapturePointer(const char *element_id);
// Release a pointer capture.
void ReleasePointer();

// Set scroll speeds of drag and mouse wheel operations.
// default: kScrollSpeedDragDefault & kScrollSpeedWheelgDefault
void SetScrollSpeed(float scroll_speed_drag, float scroll_speed_wheel);

// Set a threshold value of a drag operation start.
// default: kDragStartThresholdDefault
void SetDragStartThreshold(float drag_start_threshold);

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
// http://developer.android.com/guide/topics/graphics/2d-graphics.html#nine-patch
void ImageBackgroundNinePatch(const Texture &tex, const vec4 &patch_info);

// Make the current group into a scrolling group that can display arbitrary
// sized elements inside a window of "size", scrolled to the current "offset"
// (which the caller should store somewhere that survives the current frame).
// Call StartScroll right after StartGroup, and EndScroll right before EndGroup.
void StartScroll(const vec2 &size, vec2i *offset);
void EndScroll();

// Make the current group into a slider group that can handle a basic slider
// behavior. The group captures/releases the pointer as necessary.
// Call StartSlider right after StartGroup, and EndSlider right before EndGroup.
void StartSlider(Direction direction, float *value);
void EndSlider();

// Put a custom element with given size.
// Renderer function is invoked while render pass to render the element.
void CustomElement(
    const vec2 &virtual_size, const char *id,
    const std::function<void(const vec2i &pos, const vec2i &size)> renderer);

// Render a texture to specific position & size. Usually used in CustomElement's
// Render callback.
void RenderTexture(const Texture &tex, const vec2i &pos, const vec2i &size);

// Render a nine-patch texture to specific position & size. Usually used in
// CustomElement's Render callback.
void RenderTextureNinePatch(const Texture &tex, const vec4 &patch_info,
                            const vec2i &pos, const vec2i &size);

// The default virtual resolution used if none is set.
const float FLATUI_DEFAULT_VIRTUAL_RESOLUTION = 1000.0f;

// Set the virtual resolution of the smallest dimension of your screen
// (the Y size in landscape mode, or X in portrait).
// All dimension specified elsewhere in floats are relative to this.
// If this function is not called, it defaults to
// FLATUI_DEFAULT_VIRTUAL_RESOLUTION.
// If you wish to use native pixels, call this with min(screenx, screeny).
// Call this as first thing in your gui definition.
void SetVirtualResolution(float virtual_resolution);

// Position a group within the screen as a whole using 9 possible alignments.
// Call this as first thing in any top level groups (either your root group,
// or the children of your root if the root is a kLayoutOverlay.
// "offset" allows you to displace from the given alignment.
// If this function is not called, it defaults to top/left placement.
void PositionGroup(Alignment horizontal, Alignment vertical,
                   const vec2 &offset);

// By default, FlatUI sets up a projection matrix for all rendering that
// covers the entire screen (as given by Renderer::window_size().
// Call this function to instead use whatever projection is in place before
// Run() is called (which may be a 2D or 3D projection).
// Specify the new canvas size for the UI to live inside of.
void UseExistingProjection(const vec2i &canvas_size);

}  // namespace gui
}  // namespace fpl

#endif  // FPL_FLATUI_H
