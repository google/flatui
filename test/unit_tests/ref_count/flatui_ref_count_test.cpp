// Copyright 2016 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <thread>
#include "src/flatui_serialization.cpp"
#include "flatui/flatui_generated.h"
#include "fplutil/main.h"
#include "gtest/gtest.h"
#include "mocks/flatui_common_mocks.h"
#include "mocks/flatui_mocks.h"
#include "mocks/fplbase_mocks.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

// Note: You cannot easily extact `va_list` arguments from the
// `fplbase::LogError` mock. Therefore, I simply check that the correct
// error message was called, without checking the format string conversion
// specifiers.

class FlatUIRefCountTest : public ::testing::Test {
 protected:
  static void SetUpTestCase() {       // Called once, before any tests.
    flatui::SetErrorOutputCount(-1);  // Disable error count limit.
  }

  virtual void SetUp() {
    // Initialize stuff.
    renderer_.Initialize(mathfu::vec2i(800, 600), "FlatUI test");
    renderer_.SetCulling(fplbase::kCullingModeBack);

    // Instantiate a FontManager.
    int32_t width = 256;
    int32_t height = 256;
    int32_t slices = 1;
    font_manager_ =
        new flatui::FontManager(mathfu::vec2i(width, height), slices);

    // Set the local directory to the assets folder for this sample.
    bool result = fplbase::ChangeToUpstreamDir("../", "assets");
    assert(result);

    // Open OpenType font.
    font_manager_->Open("fonts/NotoSansCJKjp-Bold.otf");
    font_manager_->SetRenderer(renderer_);
  }

  virtual void TearDown() {
    delete font_manager_;
    renderer_.ShutDown();
  }

  fplbase::Renderer renderer_;
  flatui::FontManager *font_manager_;
};

TEST_F(FlatUIRefCountTest, TestCreateBuffer) {
  // Create a FontBuffer.
  auto string = "Test string";
  auto parameter = flatui::FontBufferParameters(
      font_manager_->GetCurrentFont()->GetFontId(), flatui::HashId(string),
      static_cast<float>(48), mathfu::vec2i(0, 0), flatui::kTextAlignmentLeft,
      flatui::kGlyphFlagsNone, true, true);
  auto buffer = font_manager_->GetBuffer(string, strlen(string), parameter);
  ASSERT_EQ(1, static_cast<int32_t>(buffer->get_ref_count()));
  font_manager_->ReleaseBuffer(buffer);
}

TEST_F(FlatUIRefCountTest, TestCheckRefCounts) {
  // multiple ref counts test.
  auto string = "Test string";
  flatui::FontBuffer *buffer;
  auto parameter = flatui::FontBufferParameters(
      font_manager_->GetCurrentFont()->GetFontId(), flatui::HashId(string),
      static_cast<float>(48), mathfu::vec2i(0, 0), flatui::kTextAlignmentLeft,
      flatui::kGlyphFlagsNone, true, true);
  int32_t k = 32;
  for (int32_t i = 0; i < k; ++i) {
    buffer = font_manager_->GetBuffer(string, strlen(string), parameter);
    ASSERT_EQ(i + 1, static_cast<int32_t>(buffer->get_ref_count()));
  }
  for (int32_t i = k; i > 0; --i) {
    font_manager_->ReleaseBuffer(buffer);
    ASSERT_EQ(i - 1, static_cast<int32_t>(buffer->get_ref_count()));
  }
}

TEST_F(FlatUIRefCountTest, TestCacheEviction) {
  // Test cache eviction.
  char string2[] = "!";
  char c = '!';
  const int32_t num = 96;
  flatui::FontBuffer *buffers[num];
  for (int32_t i = 0; i < num; ++i) {
    auto parameter = flatui::FontBufferParameters(
        font_manager_->GetCurrentFont()->GetFontId(), flatui::HashId(string2),
        static_cast<float>(48), mathfu::vec2i(0, 0), flatui::kTextAlignmentLeft,
        flatui::kGlyphFlagsNone, true, true);
    buffers[i] = font_manager_->GetBuffer(string2, 2, parameter);
    snprintf(string2, 2, "%c", c);
    c++;
    // Update counters.
    font_manager_->StartLayoutPass();
    font_manager_->StartRenderPass();
  }

  int32_t invalid = 0;
  for (int32_t i = 0; i < num; ++i) {
    if (!buffers[i]->Verify()) {
      invalid++;
    }
  }
  LogInfo("%d/%d buffers are invalid", invalid, num);
}

// Create unique instances using FontBufferParameters::cache_id.
// By setting the ID, FontManager is expected to create unique FontBuffer
// instances.
TEST_F(FlatUIRefCountTest, TestUniqueInstance) {
  // Flush fontmanager.
  font_manager_->FlushAndUpdate();

  // Test no cached behavior.
  const char string2[] = "!";
  const int32_t num = 8;
  flatui::FontBuffer *buffers[num];
  std::set<flatui::FontBuffer *> buffer_set;
  flatui::HashedId id = flatui::kNullHash;
  for (int32_t i = 0; i < num; ++i) {
    auto parameter = flatui::FontBufferParameters(
        font_manager_->GetCurrentFont()->GetFontId(), flatui::HashId(string2),
        static_cast<float>(48), mathfu::vec2i(0, 0), flatui::kTextAlignmentLeft,
        flatui::kGlyphFlagsNone, true, true, flatui::kKerningScaleDefault,
        flatui::kLineHeightDefault, id);
    buffers[i] = font_manager_->GetBuffer(string2, 2, parameter);
    // Update counters.
    font_manager_->StartLayoutPass();
    font_manager_->StartRenderPass();

    // Verify all intances are unique.
    ASSERT_EQ(buffer_set.end(), buffer_set.find(buffers[i]));
    buffer_set.insert(buffers[i]);

    // Update the ID.
    id++;
  }

  int32_t invalid = 0;
  for (int32_t i = 0; i < num; ++i) {
    if (!buffers[i]->Verify()) {
      invalid++;
    }
  }
  LogInfo("%d/%d buffers are invalid", invalid, num);

  for (int32_t i = 0; i < num; ++i) {
    font_manager_->ReleaseBuffer(buffers[i]);
  }
}

TEST_F(FlatUIRefCountTest, TestMultiThreadBufferCreation) {
  auto buffergen_test = [this] {
    char string2[] = "!";
    char c = '!';
    const int32_t num = 96;
    flatui::FontBuffer *buffers[num];
    for (int32_t i = 0; i < num; ++i) {
      auto parameter = flatui::FontBufferParameters(
          font_manager_->GetCurrentFont()->GetFontId(), flatui::HashId(string2),
          static_cast<float>(48), mathfu::vec2i(0, 0),
          flatui::kTextAlignmentLeft, flatui::kGlyphFlagsNone, true, true);
      buffers[i] = font_manager_->GetBuffer(string2, 2, parameter);
      snprintf(string2, 2, "%c", c);
      c++;
    }

    // Verify the generated buffers.
    for (int32_t i = 0; i < num; ++i) {
      ASSERT_EQ(true, buffers[i]->Verify());
    }
  };

  // Invoke StartRenderPass() API while generating FontBuffers.
  for (auto i = 0; i < 10; i++) {
    // Flush fontmanager.
    font_manager_->FlushAndUpdate();

    std::thread fontmanager_thread(buffergen_test);
    for (auto j = 0; j < 10; j++) {
      font_manager_->StartLayoutPass();
      font_manager_->StartRenderPass();
    }
    fontmanager_thread.join();
  }

  // TODO: Add a verification of generated cache bitmap.
}

TEST_F(FlatUIRefCountTest, TestEmptyString) {
  // Create a FontBuffer.
  auto string = "Test string";
  auto empty_string = "";
  auto parameter = flatui::FontBufferParameters(
      font_manager_->GetCurrentFont()->GetFontId(), flatui::HashId(string),
      static_cast<float>(48), mathfu::vec2i(300, 100),
      flatui::kTextAlignmentCenter, flatui::kGlyphFlagsNone, true, false);
  auto buffer = font_manager_->GetBuffer(string, strlen(string), parameter);
  (void)buffer;

  // Create another FontBuffer with an empty string and check if it's an empty
  // buffer.
  auto parameter_empty = flatui::FontBufferParameters(
      font_manager_->GetCurrentFont()->GetFontId(),
      flatui::HashId(empty_string), static_cast<float>(48),
      mathfu::vec2i(300, 100), flatui::kTextAlignmentCenter,
      flatui::kGlyphFlagsNone, true, false);
  auto buffer_empty = font_manager_->GetBuffer(
      empty_string, strlen(empty_string), parameter_empty);
  ASSERT_EQ(0, static_cast<int32_t>(buffer_empty->get_vertices()->size()));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

// Dummy entry to link the test in Android successfully.
extern "C" int FPL_main(int /*argc*/, char ** /*argv*/) { return 0; }
