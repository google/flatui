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

#include "precompiled.h"
#include "flatui/internal/micro_edit.h"
#include "linebreak.h"

namespace fpl {

void MicroEdit::Initialize(const char *id, std::string *text) {
  Reset();
  id_ = id;
  text_ = text;

  // Keep a copy of initial string (Used when canceling an edit).
  initial_string_ = *text;

  // Retrieve word breaking information using libunibreak.
  UpdateWordBreakInfo();

  // Initial caret position is at the end of the string.
  SetCaret(num_characters_);
}

void MicroEdit::UpdateWordBreakInfo() {
  if (!text_->length()) {
    num_characters_ = 0;
    wordbreak_index_ = 0;
    return;
  }

  wordbreak_info_.resize(text_->length());
  set_linebreaks_utf8(reinterpret_cast<const utf8_t *>(text_->c_str()),
                      text_->length(), language_.c_str(), &wordbreak_info_[0]);

  // Count number of characters.
  auto num_characters = 1;
  for (size_t i = 1; i < wordbreak_info_.size(); ++i) {
    if (wordbreak_info_[i - 1] != LINEBREAK_INSIDEACHAR) {
      num_characters++;
    }
  }
  num_characters_ = num_characters;

  UpdateWordBreakIndex();
}

void MicroEdit::UpdateWordBreakIndex() {
  // TODO: Update for a better look up rather than a linear search.
  // Check for the last caret position.
  if (caret_pos_ == num_characters_) {
    wordbreak_index_ = wordbreak_info_.size();
    return;
  }

  wordbreak_index_ = 0;
  auto current_pos = 1;
  for (size_t i = 1; i < wordbreak_info_.size(); ++i) {
    if (wordbreak_info_[i - 1] != LINEBREAK_INSIDEACHAR) {
      if (caret_pos_ == current_pos) {
        // Update an index of wordbreak info for the current caret position.
        wordbreak_index_ = i;
        break;
      }
      current_pos++;
    }
  }
}

bool MicroEdit::MoveCaret(bool forward) {
  auto moved_caret = false;
  auto delta = forward ? 1 : -1;
  if (!in_text_input_) {
    // Move a caret position right.
    if (caret_pos_ + delta >= 0 && caret_pos_ + delta <= num_characters_) {
      SetCaret(caret_pos_ + delta);
      moved_caret = true;
    }
  } else {
    // Move a caret in IME input.
    if (!input_text_selection_length_) {
      if (input_text_caret_offset_ - delta >= 0 &&
          input_text_caret_offset_ - delta <= input_text_characters_) {
        SetCaret(caret_pos_ + delta);
        moved_caret = true;
      }
    }
  }
  return moved_caret;
}

bool MicroEdit::SetCaret(int32_t position) {
  if (!in_text_input_) {
    position = std::min(position, num_characters_);
    position = std::max(position, 0);
    caret_pos_ = position;
    UpdateWordBreakIndex();
  } else {
    // Move a caret in IME input.
    if (!input_text_selection_length_) {
      // Move a caret only when there is no focused string.
      input_text_caret_offset_ =
          caret_pos_ + input_text_caret_offset_ - position;
      input_text_caret_offset_ = std::max(input_text_caret_offset_, 0);
      input_text_caret_offset_ =
          std::min(input_text_caret_offset_, input_text_characters_);
    }
  }
  return true;
}

void MicroEdit::InsertText(const std::string &text) {
  text_->insert(wordbreak_index_, text);
  caret_pos_ += GetNumCharacters(text);
  UpdateWordBreakInfo();
}

void MicroEdit::RemoveText(int32_t num_remove) {
  for (auto i = 0; i < num_remove; ++i) {
    // Remove 1 character from the buffer.
    auto num_to_erase = 1;
    auto erase_index = wordbreak_index_;
    while (erase_index < static_cast<int32_t>(text_->size()) &&
           wordbreak_info_[erase_index++] == LINEBREAK_INSIDEACHAR) {
      num_to_erase++;
    };
    text_->erase(wordbreak_index_, num_to_erase);
    wordbreak_info_.erase(
        wordbreak_info_.begin() + wordbreak_index_,
        wordbreak_info_.begin() + wordbreak_index_ + num_to_erase);
  }
  UpdateWordBreakInfo();
}

int32_t MicroEdit::GetNumCharacters(const std::string &text) {
  // Retrieve # of characters in the text.
  if (!text.length()) {
    return 0;
  }

  std::vector<char> v(text.length());
  set_linebreaks_utf8(reinterpret_cast<const utf8_t *>(text.c_str()),
                      text.length(),
                      language_.c_str(), &v[0]);
  auto characters = 0;
  for (auto i : v) {
    if (i != LINEBREAK_INSIDEACHAR) {
      characters++;
    }
  }
  return characters;
}

void MicroEdit::ResetEditingText() {
  input_text_.clear();
  editing_text_.clear();
  in_text_input_ = false;
  input_text_caret_offset_ = 0;
  return;
}

void MicroEdit::UpdateEditingText(const std::string &input) {
  if (input.length() == 0) {
    ResetEditingText();
    return;
  }
  in_text_input_ = true;
  input_text_ = std::move(input);
  input_text_characters_ = GetNumCharacters(input);
  editing_text_ = *text_;
  editing_text_.insert(wordbreak_index_, input_text_.c_str());
}

bool MicroEdit::GetInputRegions(int32_t *input_region_start,
                                int32_t *input_region_length,
                                int32_t *focus_region_start,
                                int32_t *focus_region_length) {
  if (!in_text_input_) return false;

  if (input_region_start) {
    *input_region_start = caret_pos_;
  }
  if (input_region_length) {
    *input_region_length = input_text_characters_;
  }
  if (focus_region_start) {
    *focus_region_start = caret_pos_ + input_text_selection_start_;
  }
  if (focus_region_length) {
    *focus_region_length = input_text_selection_length_;
  }
  return true;
}

// Get a caret position in current text.
int32_t MicroEdit::GetCaretPosition() {
  if (!in_text_input_) return caret_pos_;

  // Is there a focus region in IME input texts?
  if (input_text_selection_length_) {
    return caret_pos_ + input_text_selection_start_ +
           input_text_selection_length_;
  }
  return caret_pos_ + input_text_characters_ - input_text_caret_offset_;
}

bool MicroEdit::HandleInputEvents(const std::vector<TextInputEvent> *events) {
  bool ret = false;
  auto event = events->begin();
  while (event != events->end()) {
    switch (event->type) {
      case kTextInputEventTypeKey:
        // Do nothing when a key is released.
        if (event->key.state == SDL_RELEASED)
          break;
        switch (event->key.symbol) {
          case SDLK_RETURN:
          case SDLK_RETURN2:
            // Finish the input session if IME is not active.
            if (!in_text_input_) ret = true;
            break;
          case SDLK_LEFT:
            MoveCaret(false);
            break;
          case SDLK_RIGHT:
            MoveCaret(true);
            break;
          case SDLK_BACKSPACE:
            // Delete a character before the caret position.
            if (!in_text_input_) {
              if (MoveCaret(false)) {
                RemoveText(1);
              }
            }
            break;
          case SDLK_DELETE:
            // Delete a character at the caret position.
            if (!in_text_input_) {
              if (num_characters_ && caret_pos_ < num_characters_) {
                RemoveText(1);
              }
            }
            break;
          case SDLK_ESCAPE:
            // Reset the edit session.
            if (!in_text_input_) {
              *text_ = initial_string_;
              UpdateWordBreakInfo();
              SetCaret(num_characters_);
            } else {
              ResetEditingText();
            }
            break;
          case SDLK_HOME:
            SetCaret(0);
            break;
          case SDLK_END:
            SetCaret(num_characters_ + input_text_characters_);
            break;
        }
        break;
      case kTextInputEventTypeEdit:
        // Note: Due to SDL bug #3006,
        // https://bugzilla.libsdl.org/show_bug.cgi?id=3006
        // we can only recieve an input string up to32 bytes from IME now.
        UpdateEditingText(event->text);
        input_text_selection_start_ = event->edit.start;
        input_text_selection_length_ = event->edit.length;
        break;
      case kTextInputEventTypeText:
        InsertText(event->text);
        ResetEditingText();
        break;
    }
    event++;
  }
  return ret;
}

}  // namespace fpl
