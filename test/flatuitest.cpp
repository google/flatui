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

#include "fplbase/renderer.h"
#include "fplbase/input.h"
#include "fplbase/utilities.h"
#include "flatui/flatui.h"
#include "flatui/flatui_common.h"
#include <cassert>

using namespace flatui;
using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec4;

extern "C" int FPL_main(int /*argc*/, char **argv) {
  fplbase::Renderer renderer;
  fplbase::InputSystem input;
  FontManager fontman;
  fplbase::AssetManager assetman(renderer);

  // Set the local directory to the assets for this test.
  bool result = fplbase::ChangeToUpstreamDir(argv[0], "test/assets");
  assert(result);

  // Initialize stuff.
  renderer.Initialize(vec2i(800, 600), "FlatUI test");
  renderer.SetCulling(fplbase::kCullingModeBack);
  input.Initialize();

  // Open OpenType font including the color emoji font.
  const char* fonts[] = {
    "fonts/NotoSansCJKjp-Bold.otf",
    "fonts/NotoNaskhArabic-Regular.ttf",
    "fonts/NotoColorEmoji.ttf"
  };
  fontman.EnableColorGlyph();

  for (size_t i = 0; i < FPL_ARRAYSIZE(fonts); ++i) {
    fontman.Open(fonts[i]);
  }
  fontman.SetRenderer(renderer);

  // Load textures.
  auto tex_about = assetman.LoadTexture("textures/text_about.webp");
  auto tex_check_on = assetman.LoadTexture("textures/btn_check_on.webp");
  auto tex_check_off = assetman.LoadTexture("textures/btn_check_off.webp");
  auto tex_circle = assetman.LoadTexture("textures/white_circle.webp");
  auto tex_bar = assetman.LoadTexture("textures/gray_bar.webp");
  assetman.StartLoadingTextures();

  // Wait for everything to finish loading...
  while (assetman.TryFinalize() == false) {
    renderer.AdvanceFrame(input.minimized(), input.Time());
  }

  // Main loop.
  while (!input.exit_requested()) {
    input.AdvanceFrame(&renderer.window_size());
    renderer.AdvanceFrame(input.minimized(), input.Time());

    const float kColorGray = 0.25f;
    renderer.ClearFrameBuffer(
        vec4(kColorGray, kColorGray, kColorGray, 1.0f));

    // Draw GUI test
    static float f = 0.0f;
    f += 0.04f;
    const TextAlignment alignments[] = {TextAlignment::kTextAlignmentLeft,
      TextAlignment::kTextAlignmentRight, TextAlignment::kTextAlignmentCenter,
      TextAlignment::kTextAlignmentJustify,
      TextAlignment::kTextAlignmentRightJustify,
      TextAlignment::kTextAlignmentCenterJustify};
    static int32_t alignment_idx = 0;

    static bool show_about = false;
    static vec2 scroll_offset(mathfu::kZeros2f);
    static bool checkbox1_checked;
    static bool test_rtl;
    static float slider_value;
    static std::string str("Edit box.");
    static std::string str2("More Edit box.");
    static std::string str3("The\nquick brown fox jumps over the lazy dog.\n"
                            "The quick brown fox jumps over the lazy dog. "
                            "The quick brown fox jumps over the lazy dog. "
                            "The quick brown fox jumps over the lazy dog. ");
    // Sample Arabic string encoded Unicode entities.
    auto rtl_string =
    "\xd9\x8a\xd9\x8e\xd8\xac\xd9\x90\xd8\xa8\xd9\x8f\x20\x3a\x20\xd9\x85\xd9"
    "\x90\xd9\x86\x20\xd9\x88\xd9\x8e\xd8\xac\xd9\x8e\xd8\xa8\xd9\x8e\x20\x28"
    "\xd9\x85\xd9\x90\xd8\xab\xd9\x8e\xd8\xa7\xd9\x84\x20\x29\x20\x2c\x20\xd9"
    "\x81\xd9\x90\xd8\xb9\xd9\x92\xd9\x84\xd9\x8c\x20\xd9\x85\xd9\x8f\xd8\xb6"
    "\xd9\x8e\xd8\xa7\xd8\xb1\xd9\x90\xd8\xb9\xd9\x8c\x20\xd9\x85\xd9\x8e\xd8"
    "\xb1\xd9\x92\xd9\x81\xd9\x8f\xd9\x88\xd8\xb9\xd9\x8c\x20\xd8\xa8\xd9\x90"
    "\xd8\xb6\xd9\x8e\xd9\x85\xd9\x91\xd9\x8e\xd8\xa9\xd9\x8d .It is necessary";

    auto click_about_example = [&](const char *id, bool about_on) {
      if (ImageButton(*tex_about, 50, Margin(10), id) ==
          kEventWentUp) {
        fplbase::LogInfo("You clicked: %s", id);
        show_about = about_on;
      }
    };

    Run(assetman, fontman, input, [&]() {
      SetGlobalListener([](HashedId id, Event event) {
        // Example global event listener for logging / debugging / analytics.
        // Don't use this for normal event handling, see examples below instead.
        if (event & (kEventWentUp | kEventWentDown))
          fplbase::LogInfo("Event Listener: %x -> %d", id, event);
      });
      SetVirtualResolution(1000);
      //ApplyCustomTransform(mathfu::mat4::Identity());
      if (test_rtl) {
        SetTextLocale("ar");
      } else {
        SetTextLocale("en");
      }
      SetTextFont(fonts, 3);

      SetTextEllipsis("...");

      StartGroup(kLayoutOverlay, 0);
        StartGroup(kLayoutHorizontalTop, 10);
          PositionGroup(kAlignCenter, kAlignCenter,
                        mathfu::kZeros2f);
          StartGroup(kLayoutVerticalLeft, 20);
           CheckBox(*tex_check_on, *tex_check_off, "Test RTL", 30,
               Margin(6, 0), &test_rtl);
            click_about_example("my_id1", true);
            Edit(30, vec2(400, 30), "edit2", nullptr, &str2);
            StartGroup(kLayoutHorizontalTop, 0);
              Edit(30, vec2(0, 30), "edit", nullptr, &str);
              Label(">Tail", 30);
            EndGroup();
            Slider(*tex_circle, *tex_bar,
                   vec2(300, 25), 0.5f, "slider", &slider_value);
            CheckBox(*tex_check_on, *tex_check_off, "CheckBox", 30,
                          Margin(6, 0), &checkbox1_checked);
            StartGroup(kLayoutHorizontalTop, 0);
              Label("Property T", 30);
              SetTextColor(vec4(1.0f, 0.0f, 0.0f, 1.0f));
              Label("Test ", 30);
              SetTextColor(mathfu::kOnes4f);
              Label("ffWAWÄテスト", 30);
            EndGroup();
            if (TextButton("text button test", 20, Margin(10)) ==
                kEventWentUp) {
              fplbase::LogInfo("You clicked: text button");
            }
            StartGroup(kLayoutVerticalLeft, 20, "scroll");
            StartScroll(vec2(300, 200), &scroll_offset);
                ImageBackgroundNinePatch(*tex_about,
                                         vec4(0.2f, 0.2f, 0.8f, 0.8f));
                click_about_example("my_id4", true);
                Label("The quick brown fox jumps over the lazy dog", 24);
                Label("The quick brown fox jumps over the lazy dog", 20);
              EndScroll();
            EndGroup();
          EndGroup();
          StartGroup(kLayoutVerticalCenter, 40);
            click_about_example("my_id2", true);
            Image(*tex_about, 40);
            Image(*tex_about, 30);
          EndGroup();
          StartGroup(kLayoutVerticalRight, 0);
            if (TextButton("Change layout", 20, Margin(10)) ==
                kEventWentUp) {
              alignment_idx = (alignment_idx + 1) % FPL_ARRAYSIZE(alignments);
            }
            Edit(24, vec2(400, 400), alignments[alignment_idx], "edit3",
                 nullptr, &str3);
            // Some arabic labels.
            SetTextLocale("ar");
            Label(rtl_string, 40, vec2(400,0));
            SetTextLocale("en");
          EndGroup();
        EndGroup();
        if (show_about) {
          StartGroup(kLayoutVerticalLeft, 20, "about_overlay");
            ModalGroup();
            PositionGroup(kAlignRight,
                          kAlignTop, vec2(-30, 30));
            SetMargin(Margin(10));
            ColorBackground(vec4(0.5f, 0.5f, 0.0f, 1.0f));
            click_about_example("my_id3", false);
            Label("This is the about window! すし!", 32);
            Label("You should only be able to click on the", 24);
            Label("about button above, not anywhere else", 20);

            // Layout test with ellipsis.
            flatui::SetTextEllipsis("...");
            Label("The quick brown fox jumps over the lazy dog", 40,
                  vec2(400, 100), flatui::kTextAlignmentCenter);
            Label("US NATIONAL PARKS AND HISTORY", 40, vec2(400, 100),
                  flatui::kTextAlignmentCenter);
            Label("PARKS CANADA, PARCS CANADA", 40, vec2(400, 100),
                  flatui::kTextAlignmentCenter);
            Label("ONE", 40, vec2(400, 100),
                  flatui::kTextAlignmentCenter);
          EndGroup();
        }
      EndGroup();
    });
  }

  return 0;
}
