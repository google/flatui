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
using flatui::EnableTextSDF;
using flatui::EndGroup;
using flatui::Image;
using flatui::Label;
using flatui::Margin;
using flatui::PositionGroup;
using flatui::SetVirtualResolution;
using flatui::Slider;
using flatui::StartGroup;
using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec4;

extern "C" int FPL_main(int /*argc*/, char **argv) {
  fplbase::Renderer renderer;
  renderer.Initialize(vec2i(800, 600), "FlatUI sample SDF");

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

  // Use a same font size all glyph to demonstrate SDF.
  const int32_t kMinGlyphSize = 36;
  const int32_t kMaxGlyphSize = 70;
  fontman.SetSizeSelector([](const int32_t){ return kMinGlyphSize; });

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

  const float threshould_min = 6.0f/255.0f;
  const float threshould_max = 20.0f/255.0f;
  const float scale_min = 0.5;
  const float scale_max = 2.0;
  static float threshold = 0.5;
  static float scale = 0.5;
  while (!(input.exit_requested() ||
           input.GetButton(fplbase::FPLK_AC_BACK).went_down())) {
    input.AdvanceFrame(&renderer.window_size());
    renderer.AdvanceFrame(input.minimized(), input.Time());
    const float kColorGray = 0.5f;
    renderer.ClearFrameBuffer(
        vec4(kColorGray, kColorGray, kColorGray, 1.0f));

    // Show test GUI using flatui API.
    // In this sample, it shows basic UI elements of a label and an image.
    // Define flatui block. Note that the block is executed multiple times,
    // One for a layout pass and another for a rendering pass.
    Run(assetman, fontman, input, [&]() {
      // Set a virtual resolution all coordinates will use. 1000 is now the
      // size of the smallest dimension (Y in landscape mode).
      SetVirtualResolution(1000);

      // Enable SDF generation. For this sample it doesn't need inner SDF.
      EnableTextSDF(false, true, threshold * (threshould_max - threshould_min) +
                    threshould_min);
      // Set outer color (e.g. drop shadow).
      flatui::SetTextOuterColor(vec4(0.0f, 0.0f, 0.0f, 0.2f),
                                64.0f / 255.0f, vec2(2,2));

      // Start our root group.
      StartGroup(flatui::kLayoutVerticalLeft);

      // Position group in the center of the screen.
      PositionGroup(flatui::kAlignLeft, flatui::kAlignCenter,
                    mathfu::kZeros2f);

      for (int32_t i = kMinGlyphSize; i < kMaxGlyphSize; i += 2) {
        StartGroup(flatui::kLayoutVerticalLeft, 0);
        SetMargin(Margin(8));
        // Show labels with variable sizes.
        Label("The quick brown fox jumps over the lazy dog.",
              i * (scale * (scale_max - scale_min) + scale_min));
        EndGroup();
      }

      // Show controls to change parameters.
      StartGroup(flatui::kLayoutHorizontalCenter, 0);
      flatui::PositionGroup(flatui::kAlignCenter, flatui::kAlignBottom,
                            mathfu::vec2(0, -100));
      SetMargin(Margin(32));
      EnableTextSDF(false, false, 0);
      SetMargin(Margin(0, 25, 0, 0));
      Label("Thickness", 30);
      SetMargin(Margin(0));
      Slider(*tex_circle, *tex_bar,
             vec2(300, 30), 0.5f, "slider", &threshold);
      SetMargin(Margin(0, 25, 0, 0));
      Label("Scale", 30);
      SetMargin(Margin(0));
      Slider(*tex_circle, *tex_bar,
             vec2(300, 30), 0.5f, "slider2", &scale);
      EndGroup();

      EndGroup();
    });
  }

  return 0;
}
