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
using flatui::Edit;
using flatui::EditStatus;
using flatui::EndGroup;
using flatui::Image;
using flatui::Label;
using flatui::Margin;
using flatui::PositionGroup;
using flatui::SetVirtualResolution;
using flatui::SetTextFont;
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
  // Open OpenType font including the color emoji font.
  const char* fonts[] = {
    "fonts/NotoSansCJKjp-Bold.otf",
    "fonts/NotoColorEmoji.ttf"
  };
  for (size_t i = 0; i < FPL_ARRAYSIZE(fonts); ++i) {
    fontman.Open(fonts[i]);
  }

  // Enable color glyph support.
  fontman.EnableColorGlyph();

  while (!(input.exit_requested() ||
           input.GetButton(fplbase::FPLK_AC_BACK).went_down())) {
    input.AdvanceFrame(&renderer.window_size());
    renderer.AdvanceFrame(input.minimized(), input.Time());
    const float kColorGray = 0.5f;
    renderer.ClearFrameBuffer(vec4(kColorGray, kColorGray, kColorGray, 1.0f));

    // Show test GUI using flatui API.
    // In this sample, it shows basic UI elements of a label with emoji.
    // Define flatui block. Note that the block is executed multiple times,
    // One for a layout pass and another for a rendering pass.
    Run(assetman, fontman, input, [&]() {
      // Set a virtual resolution all coordinates will use. 1000 is now the
      // size of the smallest dimension (Y in landscape mode).
      SetVirtualResolution(1000);

      // Activate 2 fonts.
      SetTextFont(fonts, 2);

      // Start our root group.
      StartGroup(flatui::kLayoutVerticalCenter);
      {
        // Position group in the center of the screen.
        PositionGroup(flatui::kAlignCenter, flatui::kAlignCenter,
                      mathfu::kZeros2f);

        StartGroup(flatui::kLayoutVerticalLeft, 20);
        {
          const int32_t kSize = 48;
          auto string =
          "Emoji in label. I\xE2\x9D\xA4\xF0\x9F\x8D\xA3\xF0\x9F\x98\x81";
          Label(string, kSize, vec2(500, kSize), flatui::kTextAlignmentLeft);

          static std::string edit =
          "In edit, too.\xF0\x9F\x91\xBD\xE2\x9D\xA4\xF0\x9F\x8D\xA3";
          EditStatus status;
          Edit(kSize, vec2(500, kSize), "id", &status, &edit);
        }
        EndGroup();
      }
      EndGroup();
    });
  }

  return 0;
}
