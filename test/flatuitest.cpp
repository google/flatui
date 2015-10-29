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

using namespace fpl;

extern "C" int FPL_main() {
  Renderer renderer;
  InputSystem input;
  FontManager fontman;
  AssetManager assetman(renderer);

  // Initialize stuff.
  renderer.Initialize();
  renderer.SetCulling(Renderer::kCullBack);
  input.Initialize();

  // Open OpenType font.
  fontman.Open("fonts/NotoSansCJKjp-Bold.otf");
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
    static bool show_about = false;
    static vec2 scroll_offset(mathfu::kZeros2f);
    static bool checkbox1_checked;
    static float slider_value;
    static std::string str("Edit box.");
    static std::string str2("More Edit box.");
    static std::string str3("The\nquick brown fox jumps over the lazy dog.\n"
                            "The quick brown fox jumps over the lazy dog. "
                            "The quick brown fox jumps over the lazy dog. "
                            "The quick brown fox jumps over the lazy dog. ");

    auto click_about_example = [&](const char *id, bool about_on) {
      if (gui::ImageButton(*tex_about, 50, gui::Margin(10), id) ==
          gui::kEventWentUp) {
        LogInfo("You clicked: %s", id);
        show_about = about_on;
      }
    };

    gui::Run(assetman, fontman, input, [&]() {
      gui::SetVirtualResolution(1000);
      gui::StartGroup(gui::kLayoutOverlay, 0);
        gui::StartGroup(gui::kLayoutHorizontalTop, 10);
          gui::PositionGroup(gui::kAlignCenter, gui::kAlignCenter,
                             mathfu::kZeros2f);
          gui::StartGroup(gui::kLayoutVerticalLeft, 20);
            click_about_example("my_id1", true);
            gui::Edit(30, vec2(400, 30), "edit2", &str2);
            gui::StartGroup(gui::kLayoutHorizontalTop, 0);
              gui::Edit(30, vec2(0, 30), "edit", &str);
              gui::Label(">Tail", 30);
            gui::EndGroup();
            gui::Slider(*tex_circle, *tex_bar,
                        vec2(300, 25), 0.5f, "slider", &slider_value);
            gui::CheckBox(*tex_check_on, *tex_check_off, "CheckBox", 30,
                          gui::Margin(6, 0), &checkbox1_checked);
            gui::StartGroup(gui::kLayoutHorizontalTop, 0);
              gui::Label("Property T", 30);
              gui::SetTextColor(mathfu::vec4(1.0f, 0.0f, 0.0f, 1.0f));
              gui::Label("Test ", 30);
              gui::SetTextColor(mathfu::kOnes4f);
              gui::Label("ffWAWÄテスト", 30);
            gui::EndGroup();
            if (gui::TextButton("text button test", 20, gui::Margin(10)) ==
                gui::kEventWentUp) {
              LogInfo("You clicked: text button");
            }
            gui::StartGroup(gui::kLayoutVerticalLeft, 20, "scroll");
              gui::StartScroll(vec2(200, 100), &scroll_offset);
                gui::ImageBackgroundNinePatch(*tex_about,
                                              vec4(0.2f, 0.2f, 0.8f, 0.8f));
                click_about_example("my_id4", true);
                gui::Label("The quick brown fox jumps over the lazy dog", 24);
                gui::Label("The quick brown fox jumps over the lazy dog", 20);
              gui::EndScroll();
            gui::EndGroup();
          gui::EndGroup();
          gui::StartGroup(gui::kLayoutVerticalCenter, 40);
            click_about_example("my_id2", true);
            gui::Image(*tex_about, 40);
            gui::Image(*tex_about, 30);
          gui::EndGroup();
          gui::StartGroup(gui::kLayoutVerticalRight, 0);
            gui::Edit(24, vec2(400, 400), "edit3", &str3);
          gui::EndGroup();
        gui::EndGroup();
        if (show_about) {
          gui::StartGroup(gui::kLayoutVerticalLeft, 20, "about_overlay");
            gui::ModalGroup();
            gui::PositionGroup(gui::kAlignRight, gui::kAlignTop, vec2(-30, 30));
            gui::SetMargin(gui::Margin(10));
            gui::ColorBackground(vec4(0.5f, 0.5f, 0.0f, 1.0f));
            click_about_example("my_id3", false);
            gui::Label("This is the about window! すし!", 32);
            gui::Label("You should only be able to click on the", 24);
            gui::Label("about button above, not anywhere else", 20);
          gui::EndGroup();
        }
      gui::EndGroup();
    });
  }

  // Shut down the renderer.
  renderer.ShutDown();

  return 0;
}
