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
#include <cassert>

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
  bool result = fplbase::ChangeToUpstreamDir(argv[0], "sample/assets");
  assert(result);

  fplbase::AssetManager assetman(renderer);

  flatui::FontManager fontman;
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

      // Start our root group.
      StartGroup(flatui::kLayoutVerticalLeft);

      // Position group in the center of the screen.
      PositionGroup(flatui::kAlignCenter, flatui::kAlignCenter,
                    mathfu::kZeros2f);

      auto callback = [](const char *text, flatui::FontBuffer *buffer,
                         flatui::FontBufferParameters *params,
                         mathfu::vec2 *pos)->size_t {
        (void)buffer;
        (void)pos;
        // Easy parse of the tag string.
        int32_t size = 0;
        int32_t skip = 1;
        char s = *(text + skip);
        while (s >= '0' && s <= '9') {
          size = size * 10 + s - '0';
          skip++;
          s = *(text + skip);
        }
        params->set_font_size(size);

        return skip;  // Return how many characters were in the tag.
      };
      // Show a label with a font size of 40px in a virtual resotuion.
      AttributedLabel("The quick <20>brown fox jump<25>s ove<20>r the laz<30>y dog.", 40,
                      mathfu::vec2(0, 40), "id1", flatui::kTextAlignmentLeft,
                      "<", callback);

      flatui::SetTextEllipsis("...");
      // Show a label with a font size of 40px in a virtual resotuion.
      AttributedLabel("The quick <20>brown fox jump<25>s over <20>the lazy dog.", 40,
                      mathfu::vec2(200, 40), "id2", flatui::kTextAlignmentLeft,
                      "<", callback);

      // Show a label with a font size of 40px in a virtual resotuion.
      AttributedLabel("The quick <20>brown fox jump<25>s over the laz<30>y dog.", 40,
                      mathfu::vec2(250, 160), "id3", flatui::kTextAlignmentCenter,
                      "<", callback);

      AttributedLabel("The quick <20>brown fox jump<25>s over the lazy dog.", 40,
                      mathfu::vec2(300, 40), "id4", flatui::kTextAlignmentLeft,
                      "<", callback);

      // Show an image with a virtical size of 60px in a virtual resotuion.
      // A width is automatically derived from the image size.
      Image(*tex_about, 60);

      EndGroup();
    });
  }

  return 0;
}
