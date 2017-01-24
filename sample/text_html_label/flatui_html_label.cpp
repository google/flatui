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
#include "fplbase/input.h"
#include "fplbase/renderer.h"
#include "fplbase/utilities.h"

using flatui::Run;
using flatui::EndGroup;
using flatui::Image;
using flatui::Label;
using flatui::SetVirtualResolution;
using flatui::StartGroup;
using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec4;

extern "C" int FPL_main(int /*argc*/, char **argv) {
  fplbase::Renderer renderer;
  renderer.Initialize(vec2i(800, 600), "FlatUI sample");

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
  fontman.Open("fonts/Roboto-Regular.ttf");
  fontman.SelectFont("fonts/NotoSansCJKjp-Bold.otf");

  // Load textures.
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
      StartGroup(flatui::kLayoutVerticalLeft);

      // Position group in the center of the screen.
      PositionGroup(flatui::kAlignCenter, flatui::kAlignCenter,
                    mathfu::kZeros2f);

      // Show a label with a font size of 40px in a virtual resotuion.
      HtmlLabel(
          "<!DOCTYPE html>\n"
          "<html>\n"
          "<body>\n"
          "\n"
          "<h1>My First Heading</h1>\n"
          "<p>My first paragraph.</p>\n"
          "<a href=\"http://address\">Link text</a>\n"
          "plenty\n of \n\nnewlines\n  \nand\nmore  newlines.\n"
          "\n"
          "More text with<br><a href=\"http://address\">Link</a>\n"
          "with <a href=\"http://address\">more Link</a><br/>\n"
          "<font face=\"fonts/Roboto-Regular.ttf\" size=\"30\" "
          "color=\"#800000\">Roboto Regular</font>.<br/>\n"
          "<font face=\"fonts/Roboto-Regular.ttf\" size=\"30\" "
          "color=\"#800000\">font <font "
          "face=\"fonts/NotoSansCJKjp-Bold.otf\">nested"
          "</font> size</font> Default font.\n"
          "</body>\n"
          "</html>\n",
          40, mathfu::vec2(500, 0), flatui::kTextAlignmentLeft, "id5");

      EndGroup();
    });
  }

  return 0;
}
