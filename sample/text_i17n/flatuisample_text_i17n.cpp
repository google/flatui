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

#include <cassert>
#include "flatui/flatui.h"
#include "flatui/flatui_common.h"
#include "fplbase/input.h"
#include "fplbase/renderer.h"
#include "fplbase/utilities.h"

using flatui::Run;
using flatui::EndGroup;
using flatui::Image;
using flatui::Label;
using flatui::Margin;
using flatui::PositionGroup;
using flatui::SetVirtualResolution;
using flatui::StartGroup;
using flatui::TextAlignment;
using flatui::SetTextDirection;
using flatui::SetTextEllipsis;
using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec4;

extern "C" int FPL_main(int /*argc*/, char **argv) {
  fplbase::Renderer renderer;
  renderer.Initialize(vec2i(800, 600), "FlatUI sample Text layout");

  fplbase::InputSystem input;
  input.Initialize();

  // Set the local directory to the assets folder for this sample.
  bool result = fplbase::ChangeToUpstreamDir(argv[0], "assets");
  assert(result);
  (void)result;

  fplbase::AssetManager assetman(renderer);

  flatui::FontManager fontman;
  // Open OpenType font.
  fontman.Open("fonts/NotoSansCJKjp-Bold.otf");

  assetman.StartLoadingTextures();

  // While an initialization of flatui, it implicitly loads shaders used in the
  // API below using AssetManager.
  // shaders/color.glslv & .glslf, shaders/font.glslv & .glslf
  // shaders/textured.glslv & .glslf
  // Wait for everything to finish loading...
  while (!assetman.TryFinalize()) {
    renderer.AdvanceFrame(input.minimized(), input.Time());
  }

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

        // Show a label with Unicode BiDi.
        SetTextEllipsis("...");
        SetTextDirection(flatui::kTextLayoutDirectionTTB);
        Label(
            "吾輩は猫である。名前はまだ無い。\nどこで生れたかとんと見当がつかぬ"
            "。何でも薄暗いじめじめした所でニャーニャー泣いていた事だけは記憶し"
            "ている。吾輩はここで始めて人間というものを見た。",
            40, vec2(400, 600), TextAlignment::kTextAlignmentLeftJustify);
      }
      EndGroup();
    });
  }

  return 0;
}
