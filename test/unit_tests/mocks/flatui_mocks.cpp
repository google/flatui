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

#include "mocks/flatui_mocks.h"

namespace flatui {

void Label(const char* text, float ysize) {
  FlatUIMocks::get_mocks().Label(text, ysize);
}

void Label(const char* text, float ysize, const mathfu::vec2& size) {
  FlatUIMocks::get_mocks().Label(text, ysize, size);
}

void Label(const char* text, float ysize, const mathfu::vec2& label_size,
           TextAlignment alignment) {
  FlatUIMocks::get_mocks().Label(text, ysize, label_size, alignment);
}

Event Edit(float ysize, const mathfu::vec2& size, const char* id,
           EditStatus* status, std::string* string) {
  return FlatUIMocks::get_mocks().Edit(ysize, size, id, status, string);
}

Event Edit(float ysize, const mathfu::vec2& size, TextAlignment alignment,
           const char* id, EditStatus* status, std::string* string) {
  return FlatUIMocks::get_mocks().Edit(ysize, size, alignment, id, status,
                                       string);
}

void Image(const fplbase::Texture& texture, float ysize) {
  FlatUIMocks::get_mocks().Image(texture, ysize);
}

void StartGroup(Layout layout, float spacing, const char* id) {
  FlatUIMocks::get_mocks().StartGroup(layout, spacing, id);
}

void EndGroup() { FlatUIMocks::get_mocks().EndGroup(); }

void ModalGroup() { FlatUIMocks::get_mocks().ModalGroup(); }

void StartScroll(const mathfu::vec2& size, mathfu::vec2* offset) {
  FlatUIMocks::get_mocks().StartScroll(size, offset);
}

void EndScroll() { FlatUIMocks::get_mocks().EndScroll(); }

void StartSlider(Direction direction, float scroll_margin, float* value) {
  FlatUIMocks::get_mocks().StartSlider(direction, scroll_margin, value);
}

void EndSlider() { FlatUIMocks::get_mocks().EndSlider(); }

void SetVirtualResolution(float virtual_resolution) {
  FlatUIMocks::get_mocks().SetVirtualResolution(virtual_resolution);
}

void PositionGroup(Alignment horizontal, Alignment vertical,
                   const mathfu::vec2& offset) {
  FlatUIMocks::get_mocks().PositionGroup(horizontal, vertical, offset);
}

// These functions are not yet mocked, and are only here to satisfy the linker.
mathfu::vec2i VirtualToPhysical(const mathfu::vec2) {
  return mathfu::vec2i(0, 0);
}

mathfu::vec2 PhysicalToVirtual(const mathfu::vec2i) {
  return mathfu::vec2(0.0f, 0.0f);
}

float GetScale() { return 0.0f; }

void SetTextColor(const mathfu::vec4&) {}

bool SetTextFont(const char*) { return false; }

bool SetTextFont(const char*, int32_t) { return false; }

void SetTextLocale(const char*) {}

void SetTextDirection(const TextLayoutDirection) {}

void SetMargin(const Margin&) {}

Event CheckEvent() { return kEventNone; }

Event CheckEvent(bool) { return kEventNone; }

void SetDefaultFocus();

void CapturePointer(const char*) {}

void ReleasePointer() {}

ssize_t GetCapturedPointerIndex() { return 0; }

void SetScrollSpeed(float, float, float) {}

void SetDragThreshold(int) {}

void ColorBackground(const mathfu::vec4&) {}

void ImageBackground(const fplbase::Texture) {}

void ImageBackgroundNinePatch(const fplbase::Texture&, const mathfu::vec4&) {}

void CustomElement(
    const mathfu::vec2&, const char*,
    const std::function<void(const mathfu::vec2i&, const mathfu::vec2i)>) {}

void RenderTexture(const fplbase::Texture&, const mathfu::vec2i,
                   const mathfu::vec2i) {}

void RenderTextureNinePatch(const fplbase::Texture&, const mathfu::vec4&,
                            const mathfu::vec2i&, const mathfu::vec2i&);

mathfu::vec2 GetVirtualResolution() { return mathfu::vec2(0.0f, 0.0f); }

void UseExistingProjection(const mathfu::vec2i&) {}

mathfu::vec2 GroupPosition() { return mathfu::vec2(0.0f, 0.0f); }

mathfu::vec2 GroupSize() { return mathfu::vec2(0.0f, 0.0f); }

bool IsLastEventPointerType() { return false; }

void SetGlobalListener(const std::function<void(HashedId, Event)>&) {}

FlatUiVersion fake_version;
const FlatUiVersion* GetFlatUiVersion() { return &fake_version; }

void SetDepthTest(bool) {}

}  // flatui
