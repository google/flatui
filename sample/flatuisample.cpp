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
#include "flatui/flatui.h"
#include <cassert>

using namespace fpl;

extern "C" int FPL_main() {
  Renderer renderer;
  renderer.Initialize();

  InputSystem input;
  input.Initialize();

  AssetManager assetman(renderer);

  FontManager fontman;
  fontman.SetRenderer(renderer);
  // Open OpenType font.
  fontman.Open("fonts/NotoSansCJKjp-Bold.otf");

  // Load textures.
  auto tex_about = assetman.LoadTexture("textures/text_about.webp");
  assetman.StartLoadingTextures();

  // While an initialization of flatui, it implicitly loads shaders used in the
  // API below using AssetManager.
  // shaders/color.glslv & .glslf, shaders/font.glslv & .glslf
  // shaders/textured.glslv & .glslf

  // Wait for everything to finish loading...
  while (assetman.TryFinalize() == false) {
    renderer.AdvanceFrame(input.minimized(), input.Time());
  }

  while (!(input.exit_requested() ||
           input.GetButton(fpl::FPLK_AC_BACK).went_down())) {
    input.AdvanceFrame(&renderer.window_size());
    renderer.AdvanceFrame(input.minimized(), input.Time());
    const float kColorGray = 0.5f;
    renderer.ClearFrameBuffer(
        vec4(kColorGray, kColorGray, kColorGray, 1.0f));

    // Show test GUI using flatui API.
    // In this sample, it shows basic UI elements of a label and an image.
    // Define flatui block. Note that the block is executed multiple times,
    // One for a layout pass and another for a rendering pass.
    gui::Run(assetman, fontman, input, [&]() {
      // Set a virtual resolution all coordinates will use. 1000 is now the
      // size of the smallest dimension (Y in landscape mode).
      gui::SetVirtualResolution(1000);

      // Start our root group.
      gui::StartGroup(gui::kLayoutVerticalLeft);

      // Position group in the center of the screen.
      gui::PositionGroup(gui::kAlignCenter, gui::kAlignCenter,
                              mathfu::kZeros2f);

      // Show a label with a font size of 40px in a virtual resotuion.
      gui::Label("The quick brown fox jumps over the lazy dog.", 40);

      // Show an image with a virtical size of 60px in a virtual resotuion.
      // A width is automatically derived from the image size.
      gui::Image(*tex_about, 60);

      gui::EndGroup();
    });
  }

  renderer.ShutDown();
  return 0;
}
