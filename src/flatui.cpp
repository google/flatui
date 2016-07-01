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

#include <cstring>
#include "flatui/flatui.h"
#include "flatui/internal/flatui_util.h"
#include "flatui/internal/flatui_layout.h"
#include "flatui/internal/hb_complex_font.h"
#include "flatui/internal/micro_edit.h"
#include "fplbase/utilities.h"
#include "motive/engine.h"
#include "motive/init.h"

using fplbase::Button;
using fplbase::InputSystem;
using fplbase::LogError;
using fplbase::LogInfo;
using fplbase::Mesh;
using fplbase::Shader;
using fplbase::Texture;

namespace flatui {

// Enum indicating a type of shaders used during font rendering.
enum FontShaderType {
  kFontShaderTypeDefault = 0,
  kFontShaderTypeSdf,
  kFontShaderTypeColor,
  kFontShaderTypeCount,
};

struct Anim {
  bool called_last_frame;
  motive::Motivator motivator;
};

Direction GetDirection(Layout layout) {
  return static_cast<Direction>(layout & ~(kDirHorizontal - 1));
}

Alignment GetAlignment(Layout layout) {
  return static_cast<Alignment>(layout & (kDirHorizontal - 1));
}

static const float kScrollSpeedDragDefault = 2.0f;
static const float kScrollSpeedWheelDefault = 16.0f;
static const float kScrollSpeedGamepadDefault = 0.1f;
static const int32_t kDragStartThresholdDefault = 8;
static const int32_t kPointerIndexInvalid = -1;
static const int32_t kElementIndexInvalid = -1;
static const vec2i kDragStartPoisitionInvalid = vec2i(-1, -1);
#if !defined(NDEBUG)
static const uint32_t kDefaultGroupHashedId = HashId(kDefaultGroupID);
#endif

// This holds transient state used while a GUI is being laid out / rendered.
// It is intentionally hidden from the interface.
// It is implemented as a singleton that the GUI element functions can access.

class InternalState;
InternalState *state = nullptr;

class InternalState : public LayoutManager {
 public:
  InternalState(fplbase::AssetManager &assetman, FontManager &fontman,
                fplbase::InputSystem &input,
                motive::MotiveEngine *motive_engine)
      : LayoutManager(assetman.renderer().window_size()),
        default_projection_(true),
        depth_test_(false),
        matman_(assetman),
        renderer_(assetman.renderer()),
        input_(input),
        fontman_(fontman),
        motive_engine_(motive_engine),
        clip_position_(mathfu::kZeros2i),
        clip_size_(mathfu::kZeros2i),
        clip_inside_(false),
        text_outer_color_size_(0.0f),
        text_line_height_scale_(kLineHeightDefault),
        text_kerning_scale_(kKerningScaleDefault),
        glyph_flags_(kGlyphFlagsNone),
        sdf_threshold_(kSDFThresholdDefault),
        pointer_max_active_index_(kPointerIndexInvalid),
        gamepad_has_focus_element(false),
        default_focus_element_(kElementIndexInvalid),
        gamepad_event(kEventHover),
        latest_event_(kEventNone),
        latest_event_element_idx_(0),
        version_(&Version()) {
    bool flush_pointer_capture = true;
    // Cache the state of multiple pointers, so we have to do less work per
    // interactive element.
    pointer_max_active_index_ = 0;  // Mouse is always active.
    // TODO: pointer_max_active_index_ should start at kPointerIndexInvalid
    // if on a touchscreen.
    for (int i = 0; i < InputSystem::kMaxSimultanuousPointers; i++) {
      pointer_buttons_[i] = &input.GetPointerButton(i);
      clip_mouse_inside_[i] = true;
      if (pointer_buttons_[i]->is_down() || pointer_buttons_[i]->went_down() ||
          pointer_buttons_[i]->went_up()) {
        pointer_max_active_index_ = std::max(pointer_max_active_index_, i);
        flush_pointer_capture = false;
      }
    }

    for (int i = 0; i <= pointer_max_active_index_; i++) {
      pointer_pos_[i] = input_.get_pointers()[i].mousepos;
      pointer_delta_[i] = input_.get_pointers()[i].mousedelta;
    }

    // If no pointer is active, flush the pointer capture status.
    if (flush_pointer_capture) {
      persistent_.dragging_pointer_ = kPointerIndexInvalid;
      persistent_.mouse_capture_ = kNullHash;
      for (int i = 0; i < InputSystem::kMaxSimultanuousPointers; i++) {
        persistent_.pointer_element[i] = kNullHash;
      }
    }

    // If this assert hits, you likely are trying to created nested GUIs.
    assert(!state);

    state = this;

    // Load shaders ahead.
    image_shader_ = matman_.LoadShader("shaders/textured");
    assert(image_shader_);
    color_shader_ = matman_.LoadShader("shaders/color");
    assert(color_shader_);

    font_shaders_[kFontShaderTypeDefault][0]
        .set(matman_.LoadShader("shaders/font"));
    font_shaders_[kFontShaderTypeDefault][1]
        .set(matman_.LoadShader("shaders/font_clipping"));
    font_shaders_[kFontShaderTypeSdf][0]
        .set(matman_.LoadShader("shaders/font_sdf"));
    font_shaders_[kFontShaderTypeSdf][1]
        .set(matman_.LoadShader("shaders/font_clipping_sdf"));
    font_shaders_[kFontShaderTypeColor][0]
        .set(matman_.LoadShader("shaders/font_color"));
    font_shaders_[kFontShaderTypeColor][1]
        .set(matman_.LoadShader("shaders/font_clipping_color"));

    text_color_ = mathfu::kOnes4f;

    scroll_speed_drag_ = kScrollSpeedDragDefault;
    scroll_speed_wheel_ = kScrollSpeedWheelDefault;
    scroll_speed_gamepad_ = kScrollSpeedGamepadDefault;
    drag_start_threshold_ =
        vec2i(kDragStartThresholdDefault, kDragStartThresholdDefault);
    current_pointer_ = kPointerIndexInvalid;

    fontman_.StartLayoutPass();

    if (!persistent_.initialized) {
      motive::SplineInit::Register();
      persistent_.initialized = true;
    }
  }

  ~InternalState() { state = nullptr; }

  // Override the use of a default projection matrix and canvas size.
  void UseExistingProjection(const vec2i &canvas_size) {
    canvas_size_ = canvas_size;
    default_projection_ = false;
  }

  void ApplyCustomTransform(const mat4 &imvp) {
    if (layout_pass_) {
      for (int i = 0; i <= pointer_max_active_index_; i++) {
        auto clip_pos =
            vec2(pointer_pos_[i]) / vec2(renderer_.window_size()) * 2 - 1;
        clip_pos.y() *= -1;  // Mouse coords are LH, clip space is RH.
        // Get two 3d positions at the pointer position to form a ray
        auto obj_pos1 = imvp * vec4(vec2(clip_pos), vec2(-0.5f, 1.0f));
        auto obj_pos2 = imvp * vec4(vec2(clip_pos), vec2(0.5f, 1.0f));
        // (inverse) perspective divide.
        obj_pos1 /= obj_pos1.w();
        obj_pos2 /= obj_pos2.w();
        auto ray = obj_pos2 - obj_pos1;
        // Find where the ray intersects object-space plane Z=0.
        auto t = (0.0f - obj_pos1.z()) / ray.z();
        auto on_plane = t * ray.xy() + obj_pos1.xy();
        pointer_pos_[i] = vec2i(on_plane + 0.5f);
        // Back into LH UI pixels.
        pointer_pos_[i].y() = canvas_size_.y() - pointer_pos_[i].y();
        // TODO(wvo): transform delta relative to current pointer_pos_ ?
      }
    }
  }

  void SetDepthTest(bool enable) { depth_test_ = enable; }

  // Set up an ortho camera for all 2D elements, with (0, 0) in the top left,
  // and the bottom right the windows size in pixels.
  // This is currently hardcoded to use overlay on top of the entire GL window.
  // If that ever changes, we also need to change our use of glScissor below.
  void SetOrtho() {
    auto ortho_mat = mathfu::mat4::Ortho(
        0.0f, static_cast<float>(canvas_size_.x()),
        static_cast<float>(canvas_size_.y()), 0.0f, -1.0f, 1.0f);
    renderer_.set_model_view_projection(ortho_mat);
  }

  // Switch from the layout pass to the render pass.
  void StartRenderPass() {
    // Do nothing if there is no elements.
    if (!StartSecondPass()) return;

    // Update font manager if they need to upload font atlas texture.
    fontman_.StartRenderPass();

    CheckGamePadNavigation();

    if (default_projection_) SetOrtho();
  }

  void RenderQuad(Shader *sh, const vec4 &color, const vec2i &pos,
                  const vec2i &size, const vec4 &uv) {
    renderer_.set_color(color);
    sh->Set(renderer_);
    Mesh::RenderAAQuadAlongX(vec3(vec2(pos), 0), vec3(vec2(pos + size), 0),
                             uv.xy(), uv.zw());
  }

  void RenderQuad(Shader *sh, const vec4 &color, const vec2i &pos,
                  const vec2i &size) {
    RenderQuad(sh, color, pos, size, vec4(0, 0, 1, 1));
  }

  // An image element.
  void Image(const Texture &texture, float ysize) {
    auto hash = HashPointer(&texture);
    if (layout_pass_) {
      auto virtual_image_size = vec2(
          texture.original_size().x() * ysize / texture.original_size().y(),
          ysize);
      // Map the size to real screen pixels, rounding to the nearest int
      // for pixel-aligned rendering.
      auto size = VirtualToPhysical(virtual_image_size);
      NewElement(size, hash);
      Extend(size);
    } else {
      auto element = NextElement(hash);
      if (element) {
        texture.Set(0);
        RenderQuad(image_shader_, mathfu::kOnes4f, Position(*element),
                   element->size);
        Advance(element->size);
      }
    }
  }

  Event Edit(float ysize, const mathfu::vec2 &edit_size,
             TextAlignment alignment, const char *id, EditStatus *status,
             std::string *text) {
    auto hash = HashId(id);
    StartGroup(GetDirection(kLayoutHorizontalBottom),
               GetAlignment(kLayoutHorizontalBottom), 0, hash);
    EditStatus edit_status = kEditStatusNone;
    if (EqualId(persistent_.input_focus_, hash)) {
      // The widget is in edit.
      edit_status = kEditStatusInEdit;
    }

    // Check event, this marks this element as an interactive element.
    auto event = CheckEvent(false);

    // Set text color
    renderer_.set_color(text_color_);

    auto physical_label_size = VirtualToPhysical(edit_size);
    auto size = VirtualToPhysical(vec2(0, ysize));
    auto ui_text = text;
    auto edit_mode = kMultipleLines;
    // Check if the editbox is a single line editbox.
    if (physical_label_size.y() == 0 || physical_label_size.y() == size.y()) {
      physical_label_size.y() = size.y();
      edit_mode = kSingleLine;
    }
    if (edit_status && persistent_.text_edit_.GetEditingText()) {
      // Get a text from the micro editor when it's editing.
      ui_text = persistent_.text_edit_.GetEditingText();
    }
    auto parameter = FontBufferParameters(
        fontman_.GetCurrentFont()->GetFontId(), HashId(ui_text->c_str()),
        static_cast<float>(size.y()), physical_label_size, alignment,
        glyph_flags_, edit_status == kEditStatusInEdit, false,
        text_kerning_scale_, text_line_height_scale_);
    auto buffer =
        fontman_.GetBuffer(ui_text->c_str(), ui_text->length(), parameter);
    assert(buffer);

    // Check if the editbox is an auto expanding edit box.
    if (physical_label_size.x() == 0) {
      physical_label_size.x() = buffer->get_size().x();
      edit_mode = kSingleLine;
    }

    persistent_.text_edit_.SetBuffer(buffer);
    persistent_.text_edit_.SetWindowSize(physical_label_size);

    auto window = vec4i(vec2i(0, 0), physical_label_size);
    if (edit_status) {
      window = persistent_.text_edit_.GetWindow();
    }
    auto pos = Label(*buffer, parameter, window);
    if (!layout_pass_) {
      bool pick_caret = (event & kEventWentDown) != 0;
      if (EqualId(persistent_.input_focus_, hash)) {
        // The edit box is in focus. Now we can start text input.
        if (persistent_.input_capture_ != hash) {
          // Initialize the editor.
          persistent_.text_edit_.Initialize(text, edit_mode);
          persistent_.text_edit_.SetLanguage(fontman_.GetLanguage());
          persistent_.text_edit_.SetDirection(fontman_.GetLayoutDirection());
          persistent_.text_edit_.SetBuffer(buffer);
          pick_caret = true;
          CaptureInput(hash, true);
        }
        edit_status = kEditStatusInEdit;
      } else {
        edit_status = kEditStatusNone;
      }
      if (pick_caret) {
        auto caret_pos =
            persistent_.text_edit_.Pick(GetPointerPosition() - pos, 0);
        persistent_.text_edit_.SetCaret(caret_pos);
      }

      int32_t input_region_start, input_region_length;
      int32_t focus_region_start, focus_region_length;
      if (persistent_.text_edit_.GetInputRegions(
              &input_region_start, &input_region_length, &focus_region_start,
              &focus_region_length)) {
        // IME is active in the editor.
        // Show some input region indicators.
        if (edit_status && input_region_length) {
          const float kInputLineWidth = 1.0f;
          const float kFocusLineWidth = 3.0f;

          // Calculate and render an input text region.
          DrawUnderline(*buffer, input_region_start, input_region_length, pos,
                        static_cast<float>(size.y()), kInputLineWidth);

          // Calculate and render a focus text region inside the input text.
          if (focus_region_length) {
            DrawUnderline(*buffer, focus_region_start, focus_region_length, pos,
                          static_cast<float>(size.y()), kFocusLineWidth);
          }

          // Specify IME rect to input system.
          auto ime_rect = pos + buffer->GetCaretPosition(input_region_start);
          auto ime_size = pos + buffer->GetCaretPosition(input_region_start +
                                                         input_region_length) -
                          ime_rect;
          if (focus_region_length) {
            ime_rect = pos + buffer->GetCaretPosition(focus_region_start);
            ime_size = pos + buffer->GetCaretPosition(focus_region_start +
                                                      focus_region_length) -
                       ime_rect;
          }
          vec4 rect;
          rect.x() = static_cast<float>(ime_rect.x());
          rect.y() = static_cast<float>(ime_rect.y());
          rect.z() = static_cast<float>(ime_size.x());
          rect.w() = static_cast<float>(ime_size.y());
          input_.SetTextInputRect(rect);
        }
      }

      if (edit_status) {
        // Render caret.
        const float kCaretPositionSizeFactor = 0.8f;
        const float kCaretWidth = 4.0f;
        auto caret_pos =
            buffer->GetCaretPosition(persistent_.text_edit_.GetCaretPosition());
        auto caret_height = size.y() * kCaretPositionSizeFactor;
        if (caret_pos.x() >= window.x() - kCaretWidth &&
            caret_pos.x() <= window.x() + window.z() + kCaretWidth &&
            caret_pos.y() >= window.y() &&
            caret_pos.y() - caret_height <= window.y() + window.w()) {
          caret_pos += pos;
          // Caret Y position is at the base line, add some offset.
          caret_pos.y() -= static_cast<int>(caret_height);

          auto caret_size = VirtualToPhysical(vec2(kCaretWidth, ysize));
          RenderCaret(caret_pos, caret_size);
        }

        // Handle text input events only after the rendering for the pass is
        // finished.
        edit_status = persistent_.text_edit_.HandleInputEvents(
            input_.GetTextInputEvents());
        input_.ClearTextInputEvents();
        if (edit_status == kEditStatusFinished ||
            edit_status == kEditStatusCanceled) {
          CaptureInput(kNullHash, true);
        }
      }
    }
    EndGroup();
    if (status) {
      *status = edit_status;
    }
    return event;
  }

  // Helper for Edit widget to draw an underline.
  void DrawUnderline(const FontBuffer &buffer, int32_t start, int32_t length,
                     const vec2i &pos, float font_size, float line_width) {
    const float kUnderlineOffsetFactor = 0.2f;

    auto startpos = buffer.GetCaretPosition(start);
    auto size = buffer.GetCaretPosition(start + length) - startpos;
    startpos.y() += static_cast<int>(font_size * kUnderlineOffsetFactor);
    size.y() += static_cast<int>(line_width);

    RenderQuad(color_shader_, mathfu::kOnes4f, pos + startpos, size);
  }

  // Helper for Edit widget to render a caret.
  void RenderCaret(const vec2i &caret_pos, const vec2i &caret_size) {
    // TODO: Make the caret rendering configurable.

    // Caret blink duration.
    // 10.0 indicates the counter value increased by 10 for each seconds,
    // so the caret blink cycle becomes 10 / (2 * M_PI) second.
    const double kCareteBlinkDuration = 10.0;
    auto t = input_.Time();
    if (sin(t * kCareteBlinkDuration) > 0.0) {
      RenderQuad(color_shader_, mathfu::kOnes4f, caret_pos, caret_size);
    }
  }

  void Label(const char *text, float ysize, const vec2 &label_size,
             TextAlignment alignment) {
    // Set text color.
    renderer_.set_color(text_color_);

    auto physical_label_size = VirtualToPhysical(label_size);
    auto size = VirtualToPhysical(vec2(0, ysize));
    auto parameter = FontBufferParameters(
        fontman_.GetCurrentFont()->GetFontId(), HashId(text),
        static_cast<float>(size.y()), physical_label_size, alignment,
        glyph_flags_, false, false, text_kerning_scale_,
        text_line_height_scale_);
    auto buffer = fontman_.GetBuffer(text, strlen(text), parameter);
    assert(buffer);
    Label(*buffer, parameter, vec4i(vec2i(0, 0), buffer->get_size()));
  }

  void DrawFontBuffer(const FontBuffer &buffer, const vec2 &pos,
                      const mathfu::vec4 &clip_rect, bool use_sdf,
                      bool render_outer_color) {
    auto slices = buffer.get_slices();
    auto current_format = fplbase::kFormatAuto;
    bool clipping = clip_rect.z() != 0.0f && clip_rect.w() != 0.0f;

    for (size_t i = 0; i < slices->size(); ++i) {
      auto texture = fontman_.GetAtlasTexture(slices->at(i));
      texture->Set(0);

      if (current_format != texture->format()) {
        current_format = texture->format();

        // Switch shaders based on drawing conditions.
        FontShaderType shader_type =
            current_format == fplbase::kFormat8888
                ? kFontShaderTypeColor
                : use_sdf ? kFontShaderTypeSdf : kFontShaderTypeDefault;

        // Color glyph doesn't support outer_color.
        if (shader_type == kFontShaderTypeColor && render_outer_color) continue;

        auto current_shader = &font_shaders_[shader_type][clipping ? 1 : 0];
        current_shader->set_renderer(renderer_);

        // Set shader specific parameters.
        auto *color = &text_color_;
        current_shader->set_position_offset(vec3(pos, 0.0f));
        if (use_sdf) {
          if (render_outer_color) {
            color = &text_outer_color_;
            current_shader->set_threshold(text_outer_color_size_);
          } else {
            current_shader->set_threshold(sdf_threshold_);
          }
        }
        if (clipping) {
          current_shader->set_clipping(clip_rect);
        }
        if (current_shader->color_handle() >= 0) {
          current_shader->set_color(*color);
        }
      }

      const fplbase::Attribute kFormat[] = {
          fplbase::kPosition3f, fplbase::kTexCoord2f, fplbase::kEND};
      auto indices = buffer.get_indices(static_cast<int32_t>(i));
      Mesh::RenderArray(
          Mesh::kTriangles, static_cast<int>(indices->size()), kFormat,
          sizeof(FontVertex),
          reinterpret_cast<const char *>(buffer.get_vertices()->data()),
          indices->data());
    }
  }

  vec2i Label(const FontBuffer &buffer, const FontBufferParameters &parameter,
              const vec4i &window) {
    vec2i pos = mathfu::kZeros2i;
    auto hash = parameter.get_text_id();
    if (layout_pass_) {
      auto size = window.zw();
      NewElement(size, hash);
      Extend(size);
    } else {
      // Check if texture atlas needs to be updated.
      if (buffer.get_pass() > 0) {
        fontman_.StartRenderPass();
      }

      auto element = NextElement(hash);
      if (element) {
        pos = Position(*element);

        bool clipping = false;
        if (window.z() && window.w()) {
          clipping = window.x() || window.y() ||
                     (buffer.get_size().x() > window.z()) ||
                     (buffer.get_size().y() > window.w());
        }

        if (clipping) {
          // Font rendering with a clipping.
          pos -= window.xy();

          // Set a window to show a part of the label.
          auto start = vec2(position_ - pos);
          auto end = start + vec2(window.zw());
          start.y() -= buffer.metrics().internal_leading();
          end.y() -= buffer.metrics().external_leading();

          if (parameter.get_glyph_flags() &
              (kGlyphFlagsInnerSDF | kGlyphFlagsOuterSDF)) {
            if (text_outer_color_size_ != 0.0f) {
              // Render shadow.
              DrawFontBuffer(buffer, vec2(pos) + text_outer_color_offset,
                             vec4(start, end), true, true);
            }
            DrawFontBuffer(buffer, vec2(pos), vec4(start, end), true, false);
          } else {
            DrawFontBuffer(buffer, vec2(pos), vec4(start, end), false, false);
          }
        } else {
          // Regular font rendering.
          if (parameter.get_glyph_flags() &
              (kGlyphFlagsInnerSDF | kGlyphFlagsOuterSDF)) {
            if (text_outer_color_size_ != 0.0f) {
              // Render shadow.
              DrawFontBuffer(buffer, vec2(pos) + text_outer_color_offset,
                             mathfu::kZeros4f, true, true);
            }
            DrawFontBuffer(buffer, vec2(pos), mathfu::kZeros4f, true, false);
          } else {
            DrawFontBuffer(buffer, vec2(pos), mathfu::kZeros4f, false, false);
          }
        }
        Advance(element->size);
      }
    }
    return pos;
  }

  // Render texture on the screen.
  void RenderTexture(const Texture &tex, const vec2i &pos, const vec2i &size) {
    RenderTexture(tex, pos, size, mathfu::kOnes4f);
  }

  // Render texture on the screen with color.
  void RenderTexture(const Texture &tex, const vec2i &pos, const vec2i &size,
                     const vec4 &color) {
    if (!layout_pass_) {
      tex.Set(0);
      RenderQuad(image_shader_, color, pos, size);
    }
  }

  void RenderTextureNinePatch(const Texture &tex, const vec4 &patch_info,
                              const vec2i &pos, const vec2i &size) {
    if (!layout_pass_) {
      tex.Set(0);
      renderer_.set_color(mathfu::kOnes4f);
      image_shader_->Set(renderer_);
      Mesh::RenderAAQuadAlongXNinePatch(vec3(vec2(pos), 0),
                                        vec3(vec2(pos + size), 0), tex.size(),
                                        patch_info);
    }
  }

  void ModalGroup() {
    if (group_stack_.back().direction_ == kDirOverlay) {
      // Simply mark all elements before this last group as non-interactive.
      for (size_t i = 0; i < element_idx_; i++)
        elements_[i].interactive = false;
    }
  }

  void StartScroll(const vec2 &size, vec2 *virtual_offset) {
    auto psize = VirtualToPhysical(size);
    auto offset = VirtualToPhysical(*virtual_offset);

    if (layout_pass_) {
      // If you hit this assert, you are nesting scrolling areas, which is
      // not supported.
      assert(!clip_inside_);
      clip_inside_ = true;
      // Pass this size to EndScroll.
      clip_size_ = psize;
      clip_position_ = mathfu::kZeros2i;
    } else {
      // This currently assumes an ortho camera that corresponds to all pixels
      // of the GL screen, which is exactly what Run() sets up.
      // If that ever changes, this call will have to get more complicated,
      // translating to whereever the GUI is placed, or in the case of 3D
      // placement use another technique alltogether (render to texture,
      // glClipPlane, or stencil buffer).
      assert(default_projection_);
      renderer_.ScissorOn(
          vec2i(position_.x(), canvas_size_.y() - position_.y() - psize.y()),
          psize);

      vec2i pointer_delta = mathfu::kZeros2i;
      int32_t scroll_speed = static_cast<int32_t>(scroll_speed_drag_);

      // Check drag event only.
      auto &element = elements_[element_idx_];
      size_ = psize;

      // Set the interactive flag, check event and restore the flag.
      auto interactive = element.interactive;
      element.interactive = true;
      auto event = CheckEvent(true);
      element.interactive = interactive;

      if (event & kEventStartDrag) {
        // Start drag.
        CapturePointer(element.hash);
      }

      if (IsPointerCaptured(element.hash)) {
        if (event & kEventEndDrag) {
          // Finish dragging and release the pointer.
          ReleasePointer();
        }
        pointer_delta = pointer_delta_[0];
      } else {
        // Wheel scroll
        if (mathfu::InRange2D(pointer_pos_[0], position_, position_ + psize)) {
          pointer_delta = input_.mousewheel_delta();
          scroll_speed = static_cast<int32_t>(-scroll_speed_wheel_);
        }
      }

      if (!IsLastEventPointerType()) {
        // Gamepad/keyboard control.
        // Note that a scroll group is not an interactive group by default.
        // If the user want to nagivate a scroll group with a game pad,
        // the user need to make a group interactive by invoking CheckEvent()
        // right after StartGroup().
        if (event & kEventWentUp) {
          // Toggle capture.
          if (!IsInputCaptured(element.hash)) {
            CaptureInput(element.hash, false);
          } else {
            CaptureInput(kNullHash, false);
          }
        }

        if (IsInputCaptured(element.hash)) {
          // Gamepad navigation.
          pointer_delta = GetNavigationDirection2D();
          pointer_delta *=
              vec2i(vec2(element.extra_size) * scroll_speed_gamepad_);
          scroll_speed = 1;
        }
      }

      // Scroll the pane on user input.
      offset = vec2i::Max(mathfu::kZeros2i,
                          vec2i::Min(elements_[element_idx_].extra_size,
                                     offset - pointer_delta * scroll_speed));

      // See if the mouse is outside the clip area, so we can avoid events
      // being triggered by elements that are not visible.
      for (int i = 0; i <= pointer_max_active_index_; i++) {
        if (!mathfu::InRange2D(pointer_pos_[i], position_, position_ + psize)) {
          clip_mouse_inside_[i] = false;
        }
      }
      // Store size/position, so expensive rendering commands can choose to
      // clip against the viewport.
      // TODO: add culling code where appropriate.
      clip_size_ = psize;
      clip_position_ = position_;
      // Start the rendering of this group at the offset before the start of
      // the window to clip against. Also makes events work correctly.
      position_ -= offset;
    }

    *virtual_offset = PhysicalToVirtual(offset);
  }

  void EndScroll() {
    if (layout_pass_) {
      // Track original size.
      elements_[element_idx_].extra_size = size_ - clip_size_;
      // Overwrite what was computed for the elements.
      size_ = clip_size_;
      clip_inside_ = false;
    } else {
      for (int i = 0; i <= pointer_max_active_index_; i++) {
        clip_mouse_inside_[i] = true;
      }
      renderer_.ScissorOff();
    }
  }

  void StartSlider(Direction direction, float scroll_margin, float *value) {
    auto event = CheckEvent(false);
    if (!layout_pass_) {
      auto hash = elements_[element_idx_].hash;
      if (event & kEventStartDrag) {
        CapturePointer(hash);
      } else if (event & kEventEndDrag) {
        ReleasePointer();
      }

      if (IsLastEventPointerType()) {
        // Update the knob position.
        if (event & kEventIsDragging || event & kEventWentDown ||
            event & kEventIsDown) {
          switch (direction) {
            case kDirHorizontal:
              *value = static_cast<float>(GetPointerPosition().x() -
                                          position_.x() - scroll_margin) /
                       static_cast<float>(size_.x() - scroll_margin * 2.0f);
              break;
            case kDirVertical:
              *value = static_cast<float>(GetPointerPosition().y() -
                                          position_.y() - scroll_margin) /
                       static_cast<float>(size_.y() - scroll_margin * 2.0f);
              break;
            default:
              assert(0);
          }
          // Clamp the slider value.
          *value = mathfu::Clamp(*value, 0.0f, 1.0f);
        }
      } else {
        // Gamepad/keyboard control.
        if (event & kEventWentUp) {
          // Toggle capture.
          if (!IsInputCaptured(hash)) {
            CaptureInput(hash, false);
          } else {
            CaptureInput(kNullHash, false);
          }
        }

        if (IsInputCaptured(hash)) {
          // Accept gamepad navigation.
          int dir = GetNavigationDirection();
          if (dir) {
            *value += dir * scroll_speed_gamepad_;
            *value = mathfu::Clamp(*value, 0.0f, 1.0f);
          }
        }
      }
    }
  }

  void EndSlider() {}

  // The hashmap stores the internal animation state for the API
  // call to `Animatable(id)`. To conserve memory space, the
  // internal state for `id` will be removed if `Animatable(id)`
  // was not called in the previous frame.
  void Clean() {
    for (auto it = persistent_.animations.begin();
         it != persistent_.animations.end();) {
      if (it->second.called_last_frame) {
        it->second.called_last_frame = false;
        ++it;
      } else {
        it = persistent_.animations.erase(it);
      }
    }
  }

  // Return the internal animation state for the API call to
  // `Animatable(id)`. This state persists from frame to
  // frame as long as `Animatable(id)` is called every frame.
  // If no state currently exists for `id` because
  // `Animatable(id)` was not called the previous frame,
  // return nullptr.
  static Anim *FindAnim(const std::string &id) {
    auto it = persistent_.animations.find(id);
    return it != persistent_.animations.end() ? &(it->second) : nullptr;
  }

  // Initialize an array of Motive Targets with the values of targets.
  static void CreateMotiveTargets(const float *values, int dimensions,
                                  motive::MotiveTime target_time,
                                  motive::MotiveTarget1f *targets) {
    for (int i = 0; i < dimensions; ++i) {
      targets[i] = motive::MotiveTarget1f(
          motive::MotiveNode1f(values[i], 0, target_time));
    }
  }

  // Create a new Motivator if it isn't found in our hashmap and initialize it
  // with starting values. Return the current value of the motivator.
  const float *Animatable(const std::string id, const float *starting_values,
                          int dimensions) {
    assert(motive_engine_);
    Anim *current = FindAnim(id);
    if (!current) {
      motive::MotiveTarget1f targets[kMaxDimensions];
      CreateMotiveTargets(starting_values, dimensions, 0, targets);
      motive::MotivatorNf motivator(motive::SplineInit(), motive_engine_,
                                    dimensions, targets);
      Anim animation;
      animation.motivator = motivator;
      current = &(persistent_.animations[id] = animation);
    }
    current->called_last_frame = true;
    return reinterpret_cast<motive::MotivatorNf *>(&current->motivator)
        ->Values();
  }

  // Set the target value to which the motivator animates.
  void StartAnimation(const std::string id, double target_time,
                      const float *target_values, int dimensions) {
    Anim *current = FindAnim(id);
    if (current) {
      const motive::MotiveTime motive_target_time =
          static_cast<motive::MotiveTime>(target_time * kSecondsToMotiveTime);
      motive::MotiveTarget1f targets[kMaxDimensions];
      CreateMotiveTargets(target_values, dimensions, motive_target_time,
                          targets);
      reinterpret_cast<motive::MotivatorNf *>(&current->motivator)
          ->SetTargets(targets);
    }
  }

  // Set scroll speed of the scroll group.
  // scroll_speed_drag: Scroll speed with a pointer drag operation.
  // scroll_speed_wheel: Scroll speed with a mouse wheel operation.
  // scroll_speed_gamepad: Scroll speed with a gamepad operation.
  void SetScrollSpeed(float scroll_speed_drag, float scroll_speed_wheel,
                      float scroll_speed_gamepad) {
    scroll_speed_drag_ = scroll_speed_drag;
    scroll_speed_wheel_ = scroll_speed_wheel;
    scroll_speed_gamepad_ = scroll_speed_gamepad;
  }

  // Set drag start threshold.
  // The value is used to determine if the drag operation should start after a
  // pointer WENT_DOWN event happened.
  void SetDragStartThreshold(int drag_start_threshold) {
    drag_start_threshold_ = vec2i(drag_start_threshold, drag_start_threshold);
  }

  // Capture the input to an element.
  void CaptureInput(HashedId hash, bool control_text_input) {
    persistent_.input_capture_ = hash;
    if (!EqualId(hash, kNullHash)) {
      persistent_.input_focus_ = hash;

      if (control_text_input) {
        // Start recording input events.
        if (!input_.IsRecordingTextInput()) {
          input_.RecordTextInput(true);
        }

        // Enable IME.
        input_.StartTextInput();
      }
    } else {
      // The element releases keyboard focus as well.
      persistent_.input_focus_ = kNullHash;

      if (control_text_input) {
        // Stop recording input events.
        if (input_.IsRecordingTextInput()) {
          input_.RecordTextInput(false);
        }
        // Disable IME
        input_.StopTextInput();
      }
    }
  }

  // Check if the element is capturing input events.
  bool IsInputCaptured(HashedId hash) {
    return EqualId(persistent_.input_capture_, hash);
  }

  // Capture the pointer to an element.
  // The element with element_id will continue to recieve pointer events
  // exclusively until CapturePointer() with kNullHash API is called.
  //
  // hash: an element hash that captures the pointer.
  void CapturePointer(HashedId hash) {
    persistent_.mouse_capture_ = hash;
    RecordId(hash, current_pointer_);
  }

  // Check if the element can recieve pointer events.
  // Return false if the pointer is captured by another element.
  bool CanReceivePointerEvent(HashedId hash) {
    return persistent_.mouse_capture_ == kNullHash ||
           EqualId(persistent_.mouse_capture_, hash);
  }

  // Check if the element is capturing pointer events.
  bool IsPointerCaptured(HashedId hash) {
    return EqualId(persistent_.mouse_capture_, hash);
  }

  void RecordId(HashedId hash, int i) { persistent_.pointer_element[i] = hash; }
  bool SameId(HashedId hash, int i) {
    return EqualId(hash, persistent_.pointer_element[i]);
  }

  Event FireEvent(size_t element_idx, Event e) {
    latest_event_ = e;
    latest_event_element_idx_ = element_idx;
    if (global_listener_) global_listener_(elements_[element_idx].hash, e);
    return e;
  }

  Event CheckEvent(bool check_dragevent_only) {
    if (latest_event_element_idx_ == element_idx_) return latest_event_;

    auto &element = elements_[element_idx_];
    if (layout_pass_) {
      element.interactive = true;
#if !defined(NDEBUG)
      // Sanity check for not to have default ID in an interactive element.
      // Also each interactive elements should have unique IDs
      // (not checking here for a performance reason).
      // When some elements has same ID, a game pad navigation wouldn't work
      // as expected.
      if (element.hash == kDefaultGroupHashedId) {
        LogInfo("An interactive element %d shouldn't have a default group ID.",
                element_idx_);
      }
#endif
    } else {
      // We only fire events after the layout pass.
      // Check if this is an inactive part of an overlay.
      if (element.interactive) {
        auto hash = element.hash;
        // pointer_max_active_index_ is typically 0, so loop not expensive.
        for (int i = 0; i <= pointer_max_active_index_; i++) {
          if ((CanReceivePointerEvent(hash) && clip_mouse_inside_[i] &&
               mathfu::InRange2D(pointer_pos_[i], position_,
                                 position_ + size_)) ||
              IsPointerCaptured(hash)) {
            auto &button = *pointer_buttons_[i];
            int event = 0;

            if (persistent_.dragging_pointer_ == i && !button.went_down()) {
              // The pointer is in drag operation.
              if (button.went_up()) {
                event |= kEventEndDrag;
                persistent_.dragging_pointer_ = kPointerIndexInvalid;
                persistent_.drag_start_position_ = kDragStartPoisitionInvalid;
                if (SameId(hash, i)) {
                  event |= kEventWentUp;
                }
              } else if (button.is_down()) {
                event |= kEventIsDragging;
                if (SameId(hash, i)) {
                  event |= kEventIsDown;
                }
              } else {
                persistent_.dragging_pointer_ = kPointerIndexInvalid;
              }
            } else {
              if (!check_dragevent_only) {
                // Regular pointer event handlings.
                if (button.went_down()) {
                  RecordId(hash, i);
                  event |= kEventWentDown;
                }

                if (button.went_up() && SameId(hash, i)) {
                  event |= kEventWentUp;
                } else if (button.is_down() && SameId(hash, i)) {
                  event |= kEventIsDown;

                  if (persistent_.input_focus_ != hash) {
                    // Stop input handling.
                    CaptureInput(kNullHash, true);
                    // Record the last element we received an up on, as the
                    // target for keyboard input.
                    persistent_.input_focus_ = hash;
                  }
                }
              }

              // Check for drag events.
              if (button.went_down()) {
                persistent_.drag_start_position_ = pointer_pos_[i];
              }
              if (button.is_down() &&
                  mathfu::InRange2D(persistent_.drag_start_position_, position_,
                                    position_ + size_) &&
                  !mathfu::InRange2D(
                       pointer_pos_[i],
                       persistent_.drag_start_position_ - drag_start_threshold_,
                       persistent_.drag_start_position_ +
                           drag_start_threshold_)) {
                // Start drag event.
                // Note that any element the event can recieve the drag start
                // event, so that parent layer can start a dragging operation
                // regardless if it's sub-layer is checking event.
                event |= kEventStartDrag;
                persistent_.drag_start_position_ = pointer_pos_[i];
                persistent_.dragging_pointer_ = i;
              }
            }

            if (event) persistent_.is_last_event_pointer_type = true;

            if (!event) event = kEventHover;

            gamepad_has_focus_element = true;
            current_pointer_ = i;
            // We only report an event for the first finger to touch an element.
            // This is intentional.

            return FireEvent(element_idx_, static_cast<Event>(event));
          }
        }
        // Generate events for the current element the gamepad/keyboard
        // is focused on, but only if the gamepad/keyboard is active.
        if (!persistent_.is_last_event_pointer_type &&
            EqualId(persistent_.input_focus_, hash)) {
          gamepad_has_focus_element = true;
          return FireEvent(element_idx_, gamepad_event);
        }
      }
    }
    return kEventNone;
  }

  ssize_t GetCapturedPointerIndex() { return persistent_.dragging_pointer_; }

  bool IsLastEventPointerType() {
    return persistent_.is_last_event_pointer_type;
  }

  void SetDefaultFocus() {
    // Need to keep an index rather than the hash to check if the element is
    // an interactive element later.
    default_focus_element_ = static_cast<int32_t>(element_idx_);
  }

  bool depth_test() { return depth_test_; }

  void CheckGamePadFocus() {
    if (!gamepad_has_focus_element && persistent_.input_capture_ == kNullHash) {
      // This may happen when a GUI first appears or when elements get removed.
      // TODO: only do this when there's an actual gamepad connected.
      if (default_focus_element_ != kElementIndexInvalid &&
          elements_[default_focus_element_].interactive) {
        persistent_.input_focus_ = elements_[default_focus_element_].hash;
      } else {
        persistent_.input_focus_ = NextInteractiveElement(-1, 1);
      }
    }
  }

  void CheckGamePadNavigation() {
    // Update state.
    int dir = GetNavigationDirection();

    // Gamepad/keyboard navigation only happens when the keyboard is not
    // captured.
    if (persistent_.input_capture_ != kNullHash) {
      return;
    }

    // If "back" is pressed, clear the current focus.
    if (BackPressed()) {
      CaptureInput(kNullHash, true);
    }

    // Now find the current element, and move to the next.
    if (dir) {
      for (auto it = elements_.begin(); it != elements_.end(); ++it) {
        if (EqualId(it->hash, persistent_.input_focus_)) {
          persistent_.input_focus_ = NextInteractiveElement(
              static_cast<int>(&*it - &elements_[0]), dir);
          break;
        }
      }
    }
  }

  // Returns true if the "back" (leave this level of focus) button is pressed.
  bool BackPressed() {
    bool back_pressed = false;
#ifdef ANDROID_GAMEPAD
    auto &gamepads = input_.GamepadMap();
    for (auto &gamepad : gamepads) {
      back_pressed |=
          gamepad.second.GetButton(fplbase::Gamepad::kButtonBack).is_down();
    }
#endif  // ANDROID_GAMEPAD
    // Map escape to back by default.
    back_pressed |= input_.GetButton(fplbase::FPLK_ESCAPE).is_down();
    return back_pressed;
  }

  vec2i GetNavigationDirection2D() {
    vec2i dir = mathfu::kZeros2i;
// FIXME: this should work on other platforms too.
#ifdef ANDROID_GAMEPAD
    auto &gamepads = input_.GamepadMap();
    for (auto &gamepad : gamepads) {
      dir = CheckButtons(gamepad.second.GetButton(fplbase::Gamepad::kLeft),
                         gamepad.second.GetButton(fplbase::Gamepad::kRight),
                         gamepad.second.GetButton(fplbase::Gamepad::kUp),
                         gamepad.second.GetButton(fplbase::Gamepad::kDown),
                         gamepad.second.GetButton(fplbase::Gamepad::kButtonA));
    }
#endif
    // For testing, also support keyboard:
    if (!dir.x() && !dir.y()) {
      dir = CheckButtons(input_.GetButton(fplbase::FPLK_LEFT),
                         input_.GetButton(fplbase::FPLK_RIGHT),
                         input_.GetButton(fplbase::FPLK_UP),
                         input_.GetButton(fplbase::FPLK_DOWN),
                         input_.GetButton(fplbase::FPLK_RETURN));
    }
    return dir;
  }

  int GetNavigationDirection() {
    auto dir = GetNavigationDirection2D();
    if (dir.y()) return dir.y();
    return dir.x();
  }

  vec2i CheckButtons(const Button &left, const Button &right, const Button &up,
                     const Button &down, const Button &action) {
    vec2i dir = mathfu::kZeros2i;
    if (left.went_up()) dir.x() = -1;
    if (right.went_up()) dir.x() = 1;
    if (up.went_up()) dir.y() = -1;
    if (down.went_up()) dir.y() = 1;
    if (action.went_up()) gamepad_event = kEventWentUp;
    if (action.went_down()) gamepad_event = kEventWentDown;
    if (action.is_down()) gamepad_event = kEventIsDown;
    if (dir.x() || dir.y() || gamepad_event != kEventHover)
      persistent_.is_last_event_pointer_type = false;
    return dir;
  }

  HashedId NextInteractiveElement(int start, int direction) {
    auto range = static_cast<int>(elements_.size());
    auto count = range;
    for (auto i = start; count > 0; --count) {
      i = (i + direction + range) % range;
      if (elements_[i].interactive) return elements_[i].hash;
    }
    return kNullHash;
  }

  void ColorBackground(const vec4 &color) {
    if (!layout_pass_) {
      RenderQuad(color_shader_, color, position_, GroupSize());
    }
  }

  void ImageBackground(const Texture &tex) {
    if (!layout_pass_) {
      tex.Set(0);
      RenderQuad(image_shader_, mathfu::kOnes4f, position_, GroupSize());
    }
  }

  void ImageBackgroundNinePatch(const Texture &tex, const vec4 &patch_info) {
    RenderTextureNinePatch(tex, patch_info, position_, GroupSize());
  }

  // Enable/Disable SDF generation.
  void EnableTextSDF(bool inner_sdf, bool outer_sdf, float threshold) {
    glyph_flags_ = (inner_sdf ? kGlyphFlagsInnerSDF : kGlyphFlagsNone) |
                   (outer_sdf ? kGlyphFlagsOuterSDF : kGlyphFlagsNone);
    sdf_threshold_ = threshold;
  }

  void SetTextOuterColor(const mathfu::vec4 &color, float size,
                         const mathfu::vec2 &offset) {
    // Outer SDF need to be enabled to use the feature.
    assert(glyph_flags_ | kGlyphFlagsOuterSDF);
    text_outer_color_ = color;
    text_outer_color_size_ = size;
    text_outer_color_offset = offset;
  }

  // Set Label's text color.
  void SetTextColor(const vec4 &color) { text_color_ = color; }

  // Set Label's font.
  bool SetTextFont(const char *font_name) {
    return fontman_.SelectFont(font_name);
  }
  bool SetTextFont(const char *font_names[], int32_t count) {
    return fontman_.SelectFont(font_names, count);
  }

  // Set a locale used for the text rendering.
  void SetTextLocale(const char *locale) { fontman_.SetLocale(locale); }

  // Override text layout direction that is set by SetTextLanguage() API.
  void SetTextDirection(TextLayoutDirection direction) {
    fontman_.SetLayoutDirection(direction);
  }

  // Set a line height scaling used in the text rendering.
  void SetTextLineHeightScale(float scale) { text_line_height_scale_ = scale; }

  // Set a kerning scaling used in the text rendering.
  void SetTextKerningScale(float scale) { text_kerning_scale_ = scale; }

  // Set ellipsis characters used in the text rendering.
  void SetTextEllipsis(const char* ellipsis) {
    fontman_.SetTextEllipsis(ellipsis);
  }

  void SetGlobalListener(
      const std::function<void(HashedId id, Event event)> &callback) {
    global_listener_ = callback;
  }

  // Return the version of the FlatUI Library
  const FlatUiVersion *GetFlatUiVersion() const { return version_; }

 private:
  vec2i GetPointerDelta() { return pointer_delta_[0]; }

  vec2i GetPointerPosition() { return pointer_pos_[0]; }

  bool default_projection_;

  bool depth_test_;

  fplbase::AssetManager &matman_;
  fplbase::Renderer &renderer_;
  InputSystem &input_;
  FontManager &fontman_;
  Shader *image_shader_;
  Shader *color_shader_;
  FontShader font_shaders_[kFontShaderTypeCount][2];
  motive::MotiveEngine *motive_engine_;

  // Expensive rendering commands can check if they're inside this rect to
  // cull themselves inside a scrolling group.
  vec2i clip_position_;
  vec2i clip_size_;
  bool clip_mouse_inside_[InputSystem::kMaxSimultanuousPointers];
  bool clip_inside_;

  // Widget properties.
  mathfu::vec4 text_color_;
  // Text's outer color setting
  mathfu::vec4 text_outer_color_;
  float text_outer_color_size_;
  mathfu::vec2 text_outer_color_offset;

  // Text metrices.
  float text_line_height_scale_;
  float text_kerning_scale_;

  // Flags that indicates glyph generation parameters, such as SDF.
  GlyphFlags glyph_flags_;
  // Threshold value for SDF rendering.
  float sdf_threshold_;

  int pointer_max_active_index_;
  const Button *pointer_buttons_[InputSystem::kMaxSimultanuousPointers];
  vec2i pointer_pos_[InputSystem::kMaxSimultanuousPointers];
  vec2i pointer_delta_[InputSystem::kMaxSimultanuousPointers];
  bool gamepad_has_focus_element;
  int32_t default_focus_element_;
  Event gamepad_event;

  // Drag operations.
  float scroll_speed_drag_;
  float scroll_speed_wheel_;
  float scroll_speed_gamepad_;
  vec2i drag_start_threshold_;

  // The latest pointer that returned an event.
  int32_t current_pointer_;

  // Cache the latest event so that multiple call to CheckEvent() can be safe.
  Event latest_event_;
  size_t latest_event_element_idx_;

  std::function<void(HashedId id, Event event)> global_listener_;

  // Intra-frame persistent state.
  static struct PersistentState {
    PersistentState() : is_last_event_pointer_type(true), initialized(false) {
      // This is effectively a global, so no memory allocation or other
      // complex initialization here.
      for (int i = 0; i < InputSystem::kMaxSimultanuousPointers; i++) {
        pointer_element[i] = kNullHash;
      }
      input_focus_ = input_capture_ = mouse_capture_ = kNullHash;
      dragging_pointer_ = kPointerIndexInvalid;
    }

    // For each pointer, the element id that last received a down event.
    HashedId pointer_element[InputSystem::kMaxSimultanuousPointers];
    // The element the gamepad is currently "over", simulates the mouse
    // hovering over an element.
    HashedId input_focus_;

    // The element that capturing the keyboard.
    HashedId input_capture_;

    // The element that capturing the pointer.
    // The element continues to recieve mouse events until it releases the
    // capture.
    HashedId mouse_capture_;

    // Simple text edit handler for an edit box.
    MicroEdit text_edit_;

    // Keep tracking a pointer position of a drag start.
    vec2i drag_start_position_;
    int32_t dragging_pointer_;

    // If true, then touch/mouse, else gamepad/keyboard.
    bool is_last_event_pointer_type;

    // If true, then InternalState has been initialized.
    bool initialized;

    // HashMap for storing animations.
    std::unordered_map<std::string, Anim> animations;
  } persistent_;

  const FlatUiVersion *version_;

  // Disable copy constructor.
  InternalState(const InternalState &);
  InternalState &operator=(const InternalState &);
};

InternalState::PersistentState InternalState::persistent_;

void Run(fplbase::AssetManager &assetman, FontManager &fontman,
         fplbase::InputSystem &input, motive::MotiveEngine *motive_engine,
         const std::function<void()> &gui_definition) {
  // Create our new temporary state.
  InternalState internal_state(assetman, fontman, input, motive_engine);

  // run through persistent_.animations, removing animation that has
  // called_last_frame as false and set all "called_last_frame" to be false
  state->Clean();

  // Run two passes, one for layout, one for rendering.
  // First pass:
  gui_definition();

  // Second pass:
  internal_state.StartRenderPass();

  auto &renderer = assetman.renderer();
  renderer.SetBlendMode(fplbase::kBlendModeAlpha);
  renderer.DepthTest(internal_state.depth_test());

  gui_definition();

  internal_state.CheckGamePadFocus();
}

void Run(fplbase::AssetManager &assetman, FontManager &fontman,
         fplbase::InputSystem &input,
         const std::function<void()> &gui_definition) {
  Run(assetman, fontman, input, NULL, gui_definition);
}

InternalState *Gui() {
  assert(state);
  return state;
}

void Image(const Texture &texture, float size) { Gui()->Image(texture, size); }

void Label(const char *text, float font_size) {
  auto size = vec2(0, font_size);
  Gui()->Label(text, font_size, size, kTextAlignmentLeft);
}

void Label(const char *text, float font_size, const vec2 &size) {
  Gui()->Label(text, font_size, size, kTextAlignmentLeft);
}

void Label(const char *text, float font_size, const vec2 &size,
           TextAlignment alignment) {
  Gui()->Label(text, font_size, size, alignment);
}

Event Edit(float ysize, const mathfu::vec2 &size, const char *id,
           EditStatus *status, std::string *string) {
  return Gui()->Edit(ysize, size, kTextAlignmentLeft, id, status, string);
}

Event Edit(float ysize, const mathfu::vec2 &size, TextAlignment alignment,
           const char *id, EditStatus *status, std::string *string) {
  return Gui()->Edit(ysize, size, alignment, id, status, string);
}

void StartGroup(Layout layout, float spacing, const char *id) {
  Gui()->StartGroup(GetDirection(layout), GetAlignment(layout), spacing,
                    HashId(id));
}

void EndGroup() { Gui()->EndGroup(); }

void SetMargin(const Margin &margin) { Gui()->SetMargin(margin); }

void StartScroll(const vec2 &size, vec2 *offset) {
  Gui()->StartScroll(size, offset);
}

void EndScroll() { Gui()->EndScroll(); }

void StartSlider(Direction direction, float scroll_margin, float *value) {
  Gui()->StartSlider(direction, scroll_margin, value);
}

void EndSlider() { Gui()->EndSlider(); }

void CustomElement(
    const vec2 &virtual_size, const char *id,
    const std::function<void(const vec2i &pos, const vec2i &size)> renderer) {
  Gui()->Element(virtual_size, id, renderer);
}

void RenderTexture(const Texture &tex, const vec2i &pos, const vec2i &size) {
  Gui()->RenderTexture(tex, pos, size);
}

void RenderTexture(const Texture &tex, const vec2i &pos, const vec2i &size,
                   const vec4 &color) {
  Gui()->RenderTexture(tex, pos, size, color);
}

void RenderTextureNinePatch(const Texture &tex, const vec4 &patch_info,
                            const vec2i &pos, const vec2i &size) {
  Gui()->RenderTextureNinePatch(tex, patch_info, pos, size);
}

void EnableTextSDF(bool inner_sdf, bool outer_sdf, float threshold) {
  Gui()->EnableTextSDF(inner_sdf, outer_sdf, threshold);
}

void SetTextOuterColor(const mathfu::vec4 &color, float size,
                       const mathfu::vec2 &offset) {
  Gui()->SetTextOuterColor(color, size, offset);
}

void SetTextColor(const mathfu::vec4 &color) { Gui()->SetTextColor(color); }

bool SetTextFont(const char *font_name) {
  return Gui()->SetTextFont(font_name);
}
bool SetTextFont(const char *font_names[], int32_t count) {
  return Gui()->SetTextFont(font_names, count);
}

void SetTextLocale(const char *locale) { Gui()->SetTextLocale(locale); }
void SetTextDirection(const TextLayoutDirection direction) {
  Gui()->SetTextDirection(direction);
}

void SetTextLineHeightScale(float scale) {
  Gui()->SetTextLineHeightScale(scale);
}

void SetTextKerningScale(float scale) { Gui()->SetTextKerningScale(scale); }

void SetTextEllipsis(const char* ellipsis) {
  Gui()->SetTextEllipsis(ellipsis);
}

Event CheckEvent() { return Gui()->CheckEvent(false); }
Event CheckEvent(bool check_dragevent_only) {
  return Gui()->CheckEvent(check_dragevent_only);
}

void SetDefaultFocus() { Gui()->SetDefaultFocus(); }

ssize_t GetCapturedPointerIndex() { return Gui()->GetCapturedPointerIndex(); }

void ModalGroup() { Gui()->ModalGroup(); }

void ColorBackground(const vec4 &color) { Gui()->ColorBackground(color); }

void ImageBackground(const Texture &tex) { Gui()->ImageBackground(tex); }

void ImageBackgroundNinePatch(const Texture &tex, const vec4 &patch_info) {
  Gui()->ImageBackgroundNinePatch(tex, patch_info);
}

void SetVirtualResolution(float virtual_resolution) {
  Gui()->SetVirtualResolution(virtual_resolution);
}

vec2 GetVirtualResolution() { return Gui()->GetVirtualResolution(); }

void PositionGroup(Alignment horizontal, Alignment vertical,
                   const vec2 &offset) {
  Gui()->PositionGroup(horizontal, vertical, offset);
}

void UseExistingProjection(const vec2i &canvas_size) {
  Gui()->UseExistingProjection(canvas_size);
}

void SetDepthTest(bool enable) { Gui()->SetDepthTest(enable); }

mathfu::vec2i VirtualToPhysical(const mathfu::vec2 &v) {
  return Gui()->VirtualToPhysical(v);
}

mathfu::vec2 PhysicalToVirtual(const mathfu::vec2i &v) {
  return Gui()->PhysicalToVirtual(v);
}

float GetScale() { return Gui()->GetScale(); }

void CapturePointer(const char *element_id) {
  Gui()->CapturePointer(HashId(element_id));
}

void ReleasePointer() { Gui()->CapturePointer(kNullHash); }

void SetScrollSpeed(float scroll_speed_drag, float scroll_speed_wheel,
                    float scroll_speed_gamepad) {
  Gui()->SetScrollSpeed(scroll_speed_drag, scroll_speed_wheel,
                        scroll_speed_gamepad);
}

void SetDragStartThreshold(int drag_start_threshold) {
  Gui()->SetDragStartThreshold(drag_start_threshold);
}

vec2 GroupPosition() {
  return Gui()->PhysicalToVirtual(Gui()->GroupPosition());
}
vec2 GroupSize() { return Gui()->PhysicalToVirtual(Gui()->GroupSize()); }

bool IsLastEventPointerType() { return Gui()->IsLastEventPointerType(); }

void ApplyCustomTransform(const mat4 &imvp) {
  Gui()->ApplyCustomTransform(imvp);
}

void SetGlobalListener(
    const std::function<void(HashedId id, Event event)> &callback) {
  Gui()->SetGlobalListener(callback);
}

const FlatUiVersion *GetFlatUiVersion() { return Gui()->GetFlatUiVersion(); }

namespace details {

const float *Animatable(const std::string &id, const float *starting_values,
                        int dimensions) {
  return Gui()->Animatable(id, starting_values, dimensions);
}

void StartAnimation(const std::string &id, double target_time,
                    const float *target_values, int dimensions) {
  Gui()->StartAnimation(id, target_time, target_values, dimensions);
}

}  // namespace details

}  // namespace flatui
