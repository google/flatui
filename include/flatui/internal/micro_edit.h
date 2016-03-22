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

#ifndef FPL_MICRO_EDIT_H
#define FPL_MICRO_EDIT_H

#include <vector>

#include "fplbase/input.h"
#include "mathfu/constants.h"
#include "flatui/font_manager.h"

namespace flatui {

/// @cond FLATUI_INTERNAL

const int32_t kCaretPosInvalid = -1;

enum CaretPosition {
  kHeadOfLine,
  kTailOfLine,
};

enum EditorMode {
  kSingleLine,
  kMultipleLines,
};

// Micro editor for FlatUI's text input widget.
// Grab input events from SDL's IME (Input Method Editor) support API and handle
// them accordingly.
class MicroEdit {
 public:
  MicroEdit() { Reset(); }

  // Initialize an edit session with a given string.
  void Initialize(std::string *text, EditorMode mode);

  // Handle input events provided by SDL.
  // Returns true if the input session is finished by pressing a return.
  // In FlatUI, the library will call this function in the render pass.
  // Since SDL can generates multiple input events at a time, we accepts events
  // as a vector.
  EditStatus HandleInputEvents(
      const std::vector<fplbase::TextInputEvent> *events);

  // Get a caret position in current text.
  int32_t GetCaretPosition();

  // Get an editing text and selection postition in IME.
  std::string *GetEditingText() {
    return in_text_input_ ? &editing_text_ : text_;
  }

  // Retrieve input region parameters as a caret position index.
  // The input region is a region that is IME is in edit in the editing string
  // (retrieved via GetEditingText()) and the focus region is also a region in
  // IME control but needs to be shown as it's 'in focus' in UI.
  // Returns false if the IME is not active.
  bool GetInputRegions(int32_t *input_region_start,
                       int32_t *input_region_length,
                       int32_t *focus_region_start,
                       int32_t *focus_region_length);

  // Set a langauge used to determine line breaking.
  // language: ISO 639 based language code. Default setting is 'en'(English).
  // The value is passed to libunibreak.
  // As of libunibreak version 3.0, a list of supported languages is,
  // "en", "de", "es", "fr", "ru", "zh", "ja", "ko"
  void SetLanguage(const std::string &language) { language_ = language; }

  // Set a layout direction in the editor.
  // This setting affects forward/backward direction of a cursor control.
  void SetDirection(const TextLayoutDirection direction) {
    direction_ = direction;
  }

  // Set a FontBuffer to the editor.
  // The FontBuffer data is used to move a caret, pick a letter etc.
  void SetBuffer(const FontBuffer *buffer) { buffer_ = buffer; }

  // Get a window of the editor, the value is used in external UI rendering to
  // show a part of the editting string.
  // Returns mathfu::kZeros4i if the FontBuffer size is smaller than the given
  // window size.
  const mathfu::vec4i &GetWindow();

  // Set a window size of the editor in UI in pixels.
  void SetWindowSize(const mathfu::vec2i &size) {
    window_.z() = size.x();
    window_.w() = size.y();
  }

  // Pick a caret position from the pointer position.
  // Pointer position should be a relative offset value from the top left corner
  // of the edit window.
  int32_t Pick(const mathfu::vec2i &pointer_position, float offset);

  // Set caret position as a character index. The charactor index can be
  // retrieved Pick() or other APIs.
  bool SetCaret(int32_t position);

 private:
  void Reset() {
    initial_string_.clear();
    wordbreak_info_.clear();
    editing_text_.clear();
    input_text_selection_start_ = 0;
    input_text_selection_length_ = 0;
    input_text_caret_offset_ = 0;
    caret_pos_ = 0;
    wordbreak_index_ = 0;
    num_characters_ = 0;
    text_ = nullptr;
    in_text_input_ = false;
    window_ = mathfu::kZeros4i;
    window_offset_ = mathfu::kZeros2i;
    buffer_ = nullptr;
    expected_caret_x_position_ = kCaretPosInvalid;
    single_line_ = true;
    language_ = kDefaultLanguage;
    direction_ = kTextLayoutDirectionLTR;
  }

  // Helper to count a number of characters in a text.
  int32_t GetNumCharacters(const std::string &text);

  // Update a character index information in the UTF8 buffer.
  void UpdateWordBreakInfo();

  // Update an index in the wordbreak info buffer to corresponding caret
  // position.
  void UpdateWordBreakIndex();

  // Update an editing text.
  void UpdateEditingText(const std::string &input);

  // Reset current editing text.
  void ResetEditingText();

  // Caret control APIs.
  // Returns true if the caret has been moved by the API call.
  bool MoveCaret(bool forward);
  bool MoveCaretVertical(int32_t offset);
  bool MoveCaretInLine(CaretPosition position);
  bool MoveCaretToWordBoundary(bool forward);

  // Insert a text before the caret position and update caret position.
  void InsertText(const std::string &text);

  // Remove the specified number of text after the caret position.
  void RemoveText(int32_t num_remove);

  // Helper for Pick() API.
  int32_t PickColumn(const mathfu::vec2i &pointer_position,
                     std::vector<mathfu::vec2i>::const_iterator start_it,
                     std::vector<mathfu::vec2i>::const_iterator end_it);
  void PickRow(const mathfu::vec2i &pointer_position,
               std::vector<mathfu::vec2i>::const_iterator *start_it,
               std::vector<mathfu::vec2i>::const_iterator *end_it);

  int32_t caret_pos_;
  int32_t wordbreak_index_;
  int32_t num_characters_;

  // A pointer to a currently editting text.
  std::string *text_;

  // A copy of initial string. Used to reset the edit.
  std::string initial_string_;

  // Language setting used for line break operation.
  std::string language_;

  // Text layout direciton.
  TextLayoutDirection direction_;

  // Word breaking info retrieved by libUnibreak.
  std::vector<char> wordbreak_info_;

  // Editing text in IME.
  bool in_text_input_;
  std::string input_text_;
  int32_t input_text_characters_;
  int32_t input_text_caret_offset_;
  int32_t input_text_selection_start_;
  int32_t input_text_selection_length_;

  // Editing text shown in UI (Combination of edited text + IME text).
  std::string editing_text_;

  // Window offset and size of the editor in UI.
  mathfu::vec4i window_;
  mathfu::vec2i window_offset_;

  // Expected caret position, when moving a caret upward/downward, the editor
  // will pick a caret positon closest to this x position. This position is
  // updated when a caret is moved explicitly.
  int32_t expected_caret_x_position_;

  // A flag indicating the editor is for single line edit.
  bool single_line_;

  // A pointer to the FontBuffer.
  const FontBuffer *buffer_;
};
/// @endcond

}  // namespace flatui

#endif  // FPL_MICRO_EDIT_H
