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
#include "flatui/flatui.h"
#include "flatui/internal/micro_edit.h"
#include "linebreak.h"

using mathfu::vec2i;
using mathfu::vec4i;

namespace flatui {

void MicroEdit::Initialize(std::string *text, EditorMode mode) {
  Reset();
  text_ = text;
  if (mode == kMultipleLines) {
    single_line_ = false;
  }

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
    wordbreak_index_ = static_cast<int32_t>(wordbreak_info_.size());
    return;
  }

  wordbreak_index_ = 0;
  auto current_pos = 1;
  for (size_t i = 1; i < wordbreak_info_.size(); ++i) {
    if (wordbreak_info_[i - 1] != LINEBREAK_INSIDEACHAR) {
      if (caret_pos_ == current_pos) {
        // Update an index of wordbreak info for the current caret position.
        wordbreak_index_ = static_cast<int32_t>(i);
        break;
      }
      current_pos++;
    }
  }
}

bool MicroEdit::MoveCaretVertical(int32_t offset) {
  if (buffer_ == nullptr) return false;
  auto pos = buffer_->GetCaretPosition(caret_pos_);
  if (expected_caret_x_position_ != kCaretPosInvalid) {
    pos.x() = expected_caret_x_position_;
  }

  auto position = Pick(pos, static_cast<float>(offset));
  if (position == kCaretPosInvalid) {
    return false;
  }
  auto ret = SetCaret(position);
  // Restore caret x position which is updated inside SetCaret().
  expected_caret_x_position_ = pos.x();
  return ret;
}

bool MicroEdit::MoveCaretInLine(CaretPosition position) {
  // Pick current row.
  auto start_of_line = buffer_->GetCaretPositions().begin();
  auto end_of_line = buffer_->GetCaretPositions().end();
  auto pos = buffer_->GetCaretPositions()[GetCaretPosition()];
  PickRow(pos, &start_of_line, &end_of_line);

  ptrdiff_t index = 0;
  if (position == kTailOfLine) {
    index = std::distance(buffer_->GetCaretPositions().begin(), end_of_line);
  } else if (position == kHeadOfLine) {
    index = std::distance(buffer_->GetCaretPositions().begin(), start_of_line);
  }
  return SetCaret(static_cast<int32_t>(index));
}

bool MicroEdit::MoveCaretToWordBoundary(bool forward) {
  bool ret = false;
  while (MoveCaret(forward)) {
    ret = true;
    if (wordbreak_info_[wordbreak_index_] != LINEBREAK_NOBREAK) {
      break;
    }
  }
  return ret;
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
        SetCaret(caret_pos_ + input_text_characters_ -
                 input_text_caret_offset_ + delta);
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
      input_text_caret_offset_ = caret_pos_ + input_text_characters_ - position;
      input_text_caret_offset_ = std::max(input_text_caret_offset_, 0);
      input_text_caret_offset_ =
          std::min(input_text_caret_offset_, input_text_characters_);
    }
  }

  if (buffer_ && buffer_->HasCaretPositions()) {
    expected_caret_x_position_ = buffer_->GetCaretPosition(position).x();
  }
  return true;
}

void MicroEdit::InsertText(const std::string &text) {
  text_->insert(wordbreak_index_, text);
  caret_pos_ += GetNumCharacters(text);
  expected_caret_x_position_ = kCaretPosInvalid;
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
                      text.length(), language_.c_str(), &v[0]);
  auto characters = 0;
  for (auto it = v.begin(); it != v.end(); ++it) {
    auto i = *it;
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

const vec4i &MicroEdit::GetWindow() {
  if (buffer_ == nullptr) {
    return mathfu::kZeros4i;
  }

  auto buffer_size = buffer_->get_size();
  if (buffer_size.x() > window_.z() || buffer_size.y() > window_.w()) {
    // Check if we need to scroll inside the edit box.
    const float kWindowThresholdFactor = 0.15f;
    auto caret_pos = buffer_->GetCaretPosition(GetCaretPosition());
    if (direction_ == kTextLayoutDirectionRTL) {
      caret_pos.x() = window_.z() - caret_pos.x();
    }
    auto threshold = window_.z() * kWindowThresholdFactor;
    auto window_start = caret_pos - window_offset_;

    // Update offset if the caret position is outside of the threshold area.
    if (window_start.x() < threshold) {
      window_offset_.x() -= static_cast<int>(threshold);
    } else if (window_start.x() > window_.z() - threshold) {
      window_offset_.x() += static_cast<int>(threshold);
    }

    if (window_start.y() < 0) {
      window_offset_.y() -= buffer_->metrics().total();
    } else if (window_start.y() > window_.w()) {
      window_offset_.y() += buffer_->metrics().total();
    }

    window_.x() = std::max(
        std::min(window_offset_.x(), buffer_size.x() - window_.z()), 0);
    window_.y() = std::max(
        std::min(window_offset_.y(), buffer_size.y() - window_.w()), 0);
    if (direction_ == kTextLayoutDirectionRTL) {
      window_.x() = -window_.x();
    }
  } else {
    window_.x() = 0;
    window_.y() = 0;
  }
  return window_;
}

int32_t MicroEdit::Pick(const vec2i &pointer_position, float offset) {
  if (buffer_ == nullptr || !buffer_->HasCaretPositions()) {
    return kCaretPosInvalid;
  }

  // Pick a row first.
  auto start_it = buffer_->GetCaretPositions().begin();
  auto end_it = buffer_->GetCaretPositions().end();
  auto buffer_begin = start_it;
  auto buffer_end = end_it;
  PickRow(pointer_position, &start_it, &end_it);

  // Pick previous/next row based on the offset value.
  if (offset < 0) {
    if (start_it == buffer_begin) {
      return kCaretPosInvalid;
    }
    end_it = start_it - 1;
    start_it = buffer_begin;
    PickRow(*end_it, &start_it, &end_it);
  } else if (offset > 0) {
    if (end_it == buffer_end - 1) {
      return kCaretPosInvalid;
    }
    start_it = end_it + 1;
    end_it = buffer_end - 1;
    PickRow(*start_it, &start_it, &end_it);
  }

  // And pick a column.
  return PickColumn(pointer_position, start_it, end_it);
}

void MicroEdit::PickRow(const vec2i &pointer_position,
                        std::vector<vec2i>::const_iterator *start_it,
                        std::vector<vec2i>::const_iterator *end_it) {
  // Perform a binary search in the caret position buffer.
  auto compare = [](const vec2i &lhs,
                    const vec2i &rhs) { return lhs.y() < rhs.y(); };
  *start_it = std::lower_bound(*start_it, *end_it, pointer_position, compare);
  if (*start_it < *end_it)
    *end_it = std::upper_bound(*start_it, *end_it, **start_it, compare) - 1;
}

int32_t MicroEdit::PickColumn(const vec2i &pointer_position,
                              std::vector<vec2i>::const_iterator start_it,
                              std::vector<vec2i>::const_iterator end_it) {
  auto compare = [this](const vec2i &lhs, const vec2i &rhs) {
    if (direction_ == kTextLayoutDirectionRTL) {
      return lhs.x() >= rhs.x();
    } else {
      return lhs.x() <= rhs.x();
    }
  };
  const auto it = std::upper_bound(start_it, end_it, pointer_position, compare);

  int32_t index = static_cast<int32_t>(
      std::distance(buffer_->GetCaretPositions().begin(), it));
  return index;
}

EditStatus MicroEdit::HandleInputEvents(
    const std::vector<fplbase::TextInputEvent> *events) {
  EditStatus ret = kEditStatusInEdit;
  auto event = events->begin();
  while (event != events->end()) {
    bool forward = true;
    if (direction_ == kTextLayoutDirectionRTL) {
      forward = false;
    }
    switch (event->type) {
      case fplbase::kTextInputEventTypeKey:
        // Handle release event only for return key release.
        // Releasing focus via return key need to be handled
        // when releasing the key to align with other widgets's input handling.
        if (!event->key.state) {
          if (event->key.symbol == fplbase::FPLK_RETURN ||
              event->key.symbol == fplbase::FPLK_RETURN2) {
            if (single_line_ || !(event->key.modifier & FPL_KMOD_SHIFT)) {
              // Finish the input session if IME is not active.
              if (!in_text_input_) ret = kEditStatusFinished;
            }
          }
          break;
        }

        switch (event->key.symbol) {
          case fplbase::FPLK_RETURN:
          case fplbase::FPLK_RETURN2:
            if (!single_line_ && (event->key.modifier & FPL_KMOD_SHIFT)) {
              InsertText("\n");
            }
            break;
          case fplbase::FPLK_LEFT:
            if (event->key.modifier & FPL_KMOD_GUI) {
              if (direction_ == kTextLayoutDirectionRTL) {
                MoveCaretInLine(kTailOfLine);
              } else {
                MoveCaretInLine(kHeadOfLine);
              }
            } else if (event->key.modifier & FPL_KMOD_ALT) {
              MoveCaretToWordBoundary(!forward);
            } else {
              MoveCaret(!forward);
            }
            break;
          case fplbase::FPLK_RIGHT:
            if (event->key.modifier & FPL_KMOD_GUI) {
              if (direction_ == kTextLayoutDirectionRTL) {
                MoveCaretInLine(kHeadOfLine);
              } else {
                MoveCaretInLine(kTailOfLine);
              }
            } else if (event->key.modifier & FPL_KMOD_ALT) {
              MoveCaretToWordBoundary(forward);
            } else {
              MoveCaret(forward);
            }
            break;
          case fplbase::FPLK_UP:
            if (event->key.modifier & FPL_KMOD_GUI) {
              SetCaret(0);
            } else {
              MoveCaretVertical(-buffer_->metrics().total());
            }
            break;
          case fplbase::FPLK_DOWN:
            if (event->key.modifier & FPL_KMOD_GUI) {
              SetCaret(num_characters_ + input_text_characters_);
            } else {
              MoveCaretVertical(buffer_->metrics().total());
            }
            break;
          case fplbase::FPLK_BACKSPACE:
            // Delete a character before the caret position.
            if (!in_text_input_) {
              if (MoveCaret(false)) {
                RemoveText(1);
                ret = kEditStatusUpdated;
              }
            }
            break;
          case fplbase::FPLK_DELETE:
            // Delete a character at the caret position.
            if (!in_text_input_) {
              if (num_characters_ && caret_pos_ < num_characters_) {
                RemoveText(1);
                ret = kEditStatusUpdated;
              }
            }
            break;
          case fplbase::FPLK_ESCAPE:
            // Reset the edit session.
            if (!in_text_input_) {
              if (!text_->compare(initial_string_)) {
                // Finish the edit when escape is pressed without any text edit.
                ret = kEditStatusCanceled;
              } else {
                *text_ = initial_string_;
                UpdateWordBreakInfo();
                SetCaret(num_characters_);
                ret = kEditStatusUpdated;
              }
            } else {
              ResetEditingText();
            }
            break;
          case fplbase::FPLK_HOME:
            SetCaret(0);
            break;
          case fplbase::FPLK_END:
            SetCaret(num_characters_ + input_text_characters_);
            break;
        }
        break;
      case fplbase::kTextInputEventTypeEdit:
        // Note: Due to SDL bug #3006,
        // https://bugzilla.libsdl.org/show_bug.cgi?id=3006
        // we can only recieve an input string up to32 bytes from IME now.
        UpdateEditingText(event->text);
        input_text_selection_start_ = event->edit.start;
        input_text_selection_length_ = event->edit.length;
        ret = kEditStatusUpdated;
        break;
      case fplbase::kTextInputEventTypeText:
        InsertText(event->text);
        ResetEditingText();
        ret = kEditStatusUpdated;
        break;
    }
    event++;
  }
  return ret;
}

}  // namespace flatui
