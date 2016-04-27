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

#include "fplbase/renderer.h"
#include "fplbase/input.h"
#include "fplbase/utilities.h"
#include "flatui/flatui.h"
#include "flatui/flatui_common.h"
#include <cassert>

using flatui::Run;
using flatui::EndGroup;
using flatui::Image;
using flatui::Label;
using flatui::Margin;
using flatui::PositionGroup;
using flatui::SetTextKerningScale;
using flatui::SetTextLineHeightScale;
using flatui::SetVirtualResolution;
using flatui::Slider;
using flatui::StartGroup;
using flatui::TextAlignment;
using flatui::TextButton;
using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec4;

extern "C" int FPL_main(int /*argc*/, char **argv) {
  fplbase::Renderer renderer;
  renderer.Initialize(vec2i(800, 600), "FlatUI sample Text layout");

  fplbase::InputSystem input;
  input.Initialize();

  // Set the local directory to the assets folder for this sample.
  bool result = fplbase::ChangeToUpstreamDir(argv[0], "sample/assets");
  assert(result);

  fplbase::AssetManager assetman(renderer);

  flatui::FontManager fontman;
  fontman.SetRenderer(renderer);
  // Open OpenType font.
  fontman.Open("fonts/NotoSansCJKjp-Bold.otf");

  auto tex_circle = assetman.LoadTexture("textures/white_circle.webp");
  auto tex_bar = assetman.LoadTexture("textures/gray_bar.webp");
  assetman.StartLoadingTextures();

  // While an initialization of flatui, it implicitly loads shaders used in the
  // API below using AssetManager.
  // shaders/color.glslv & .glslf, shaders/font.glslv & .glslf
  // shaders/textured.glslv & .glslf
  // Wait for everything to finish loading...
  while (assetman.TryFinalize() == false) {
    renderer.AdvanceFrame(input.minimized(), input.Time());
  }

  static float kerning = 0.5;
  static float line_height_scale = 0.5;
  const float kLineHeightMin = 0.9f;
  const float kLineHeightMax = 1.5f;
  const float kKerningScaleMin = 0.9f;
  const float kKerningScaleMax = 1.1f;

  struct alignment_info {
    const char *alignment_name;
    TextAlignment alignment;
  };

  alignment_info alignments[] = {
      {"Left", TextAlignment::kTextAlignmentLeft},
      {"Right", TextAlignment::kTextAlignmentRight},
      {"Center", TextAlignment::kTextAlignmentCenter},
      {"LeftJustified", TextAlignment::kTextAlignmentLeftJustify},
      {"RightJustified", TextAlignment::kTextAlignmentRightJustify},
      {"CenterJustified", TextAlignment::kTextAlignmentCenterJustify}};
  static auto current_alignment = 3;

  while (!(input.exit_requested() ||
           input.GetButton(fplbase::FPLK_AC_BACK).went_down())) {
    input.AdvanceFrame(&renderer.window_size());
    renderer.AdvanceFrame(input.minimized(), input.Time());
    const float kColorGray = 0.5f;
    renderer.ClearFrameBuffer(vec4(kColorGray, kColorGray, kColorGray, 1.0f));

    // Show test GUI using flatui API.
    // In this sample, it shows basic UI elements of a label and an image.
    // Define flatui block. Note that the block is executed multiple times,
    // One for a layout pass and another for a rendering pass.
    Run(assetman, fontman, input, [&]() {
      // Set a virtual resolution all coordinates will use. 1000 is now the
      // size of the smallest dimension (Y in landscape mode).
      SetVirtualResolution(1000);

      // Start our root group.
      StartGroup(flatui::kLayoutVerticalCenter);
      {
        // Position group in the center of the screen.
        PositionGroup(flatui::kAlignCenter, flatui::kAlignCenter,
                      mathfu::kZeros2f);

        auto line_height =
            kLineHeightMin +
            line_height_scale * (kLineHeightMax - kLineHeightMin);
        auto kerning_scale =
            kKerningScaleMin + kerning * (kKerningScaleMax - kKerningScaleMin);
        SetTextLineHeightScale(line_height);
        SetTextKerningScale(kerning_scale);

        auto string =
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
            "eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut "
            "enim ad minim veniam, quis nostrud exercitation ullamco laboris "
            "nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in "
            "reprehenderit in voluptate velit esse cillum dolore eu fugiat "
            "nulla pariatur. Excepteur sint occaecat cupidatat non proident, "
            "sunt in culpa qui officia deserunt mollit anim id est laborum.";
        Label(string, 35, vec2(800, 800),
              alignments[current_alignment].alignment);

        // Show controls to change parameters.
        StartGroup(flatui::kLayoutVerticalCenter, 0);
        {
          flatui::PositionGroup(flatui::kAlignCenter, flatui::kAlignBottom,
                                mathfu::vec2(0, -100));
          StartGroup(flatui::kLayoutHorizontalCenter, 0);
          {
            SetTextKerningScale(1.0f);
            SetMargin(Margin(0, 0, 0, 0));
            Label("Kerning", 30);
            Slider(*tex_circle, *tex_bar, vec2(300, 30), 0.5f, "slider",
                   &kerning);
            Label("Line height", 30);
            Slider(*tex_circle, *tex_bar, vec2(300, 30), 0.5f, "slider2",
                   &line_height_scale);
          }
          EndGroup();

          SetMargin(Margin(0, 10, 0, 0));
          if (TextButton(alignments[current_alignment].alignment_name, 30,
                         Margin(0)) == flatui::kEventWentUp) {
            current_alignment =
                (current_alignment + 1) % FPL_ARRAYSIZE(alignments);
          }
        }
        EndGroup();
      }
      EndGroup();
    });
  }

  return 0;
}
