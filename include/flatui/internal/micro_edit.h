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

namespace fpl {

// Micro editor for FlatUI's text input widget.
// Grab input events from SDL's IME (Input Method Editor) support API and handle
// them accordingly.
class MicroEdit {
 public:
  MicroEdit() {
    language_ = kLineBreakDefaultLanguage;
    Reset();
  }

  // Initialize an edit session with a given string.
  void Initialize(const char *id, std::string *text);

  // Handle input events provided by SDL.
  // Returns true if the input session is finished by pressing a return.
  // In FlatUI, the library will call this function in the render pass.
  // Since SDL can generates multiple input events at a time, we accepts events
  // as a vector.
  bool HandleInputEvents(const std::vector<TextInputEvent> *events);

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
  // language: ISO 639-1 based language code. Default setting is 'en'(English).
  // The value is passed to libunibreak.
  // As of libunibreak version 3.0, a list of supported languages is,
  // "en", "de", "es", "fr", "ru", "zh", "ja", "ko"
  void SetLanguage(const std::string &language) { language_ = language; }

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
    id_ = nullptr;
    in_text_input_ = false;
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
  bool SetCaret(int32_t position);

  // Insert a text before the caret position and update caret position.
  void InsertText(const std::string &text);

  // Remove the specified number of text after the caret position.
  void RemoveText(int32_t num_remove);

  int32_t caret_pos_;
  int32_t wordbreak_index_;
  int32_t num_characters_;

  // A pointer to a currently editting text.
  std::string *text_;

  // A copy of initial string. Used to reset the edit.
  std::string initial_string_;

  // Language setting used for line break operation.
  std::string language_;

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

  // ID of the editting widget.
  const char *id_;
};

}  // namespace fpl

#endif  // FPL_MICRO_EDIT_H
