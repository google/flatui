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

#include "motive/engine.h"
#include "motive/init.h"

using flatui::AnimCurveDescription;
using flatui::EndGroup;
using flatui::Margin;
using flatui::Run;
using flatui::SetVirtualResolution;
using flatui::StartGroup;
using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec3;
using mathfu::vec4;
using mathfu::kZeros2f;
using mathfu::kZeros4f;
using motive::MotiveEngine;
using motive::MotiveTime;
using motive::Range;

static const vec4 kBackgroundColors[] = {
    vec4(0.5f, 0.5f, 0.5f, 1.0f), vec4(1.0f, 0.0f, 0.0f, 1.0f),
    vec4(0.0f, 0.0f, 1.0f, 1.0f), vec4(0.0f, 1.0f, 1.0f, 1.0f),
};
// Create curve's typical shape with a typical delta distance of 1.0f,
// a typical total time of 7000.0f, and a bias of 0.5f.
// For this sample, we want a gradual transition from one background color to
// another. By choosing a small distance to traverse and a long total time for
// that traversal, we are able to generate a gentler, more gradual animation
// curve.
// Additionally, the bias of 0.5f gives us an even ease-in and ease-out
// change from one color to another.
// For this specific sample, we are working with animating colors. Since colors
// range from 0~1, the maximum distance a color can travel is 1. We have chosen
// that maximum as our typical distance. We choose 7000 as our typical total
// time so that the transition will take 7 seconds when the color has to travel
// from 0 to 1, its typical delta distance.
static const AnimCurveDescription kColorCurve(flatui::kAnimEaseInEaseOut, 1.0f,
                                              7000.0f, 0.5f);

// The spring and ease curves are parameterized so that they settle down at
// the target at approximately the same time. This means that the spring must
// have a faster typical curve because it needs to oscillate about the target
// after it's reached it the first time.
static const AnimCurveDescription kSpringCurve(flatui::kAnimSpring, 300.0f,
                                               2000.0f, 0.4f);
static const AnimCurveDescription kEaseCurve(flatui::kAnimEaseInEaseOut, 300.0f,
                                             6000.0f, 0.2f);
static const float kMovementX = 300.0f;
static const float kEaseY = 100.0f;
static const float kSpringY = 200.0f;

extern "C" int FPL_main(int /*argc*/, char** argv) {
  MotiveEngine motive_engine;

  size_t color_idx = 0;

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

  vec4 bg = kZeros4f;
  float movement_sign = 1.0f;

  while (!(input.exit_requested() ||
           input.GetButton(fplbase::FPLK_AC_BACK).went_down())) {
    input.AdvanceFrame(&renderer.window_size());
    renderer.AdvanceFrame(input.minimized(), input.Time());
    motive_engine.AdvanceFrame(static_cast<MotiveTime>(
        input.DeltaTime() * flatui::kSecondsToMotiveTime));
    renderer.ClearFrameBuffer(bg);

    // Show test GUI using flatui API.
    // In this sample, it shows basic UI elements of a label and an image.
    // Define flatui block. Note that the block is executed multiple times,
    // One for a layout pass and another for a rendering pass.
    Run(assetman, fontman, input, &motive_engine, [&]() {
      // Set a virtual resolution all coordinates will use. 1000 is now the
      // size of the smallest dimension (Y in landscape mode).
      SetVirtualResolution(1000);

      // Start our root group and position in the center of the screen.
      StartGroup(flatui::kLayoutVerticalCenter, 100.0f);
      PositionGroup(flatui::kAlignCenter, flatui::kAlignCenter, kZeros2f);

      // Allow the background color to be animated.
      bg = flatui::Animatable<vec4>("color_fading",
                                    kBackgroundColors[color_idx]);

      // Create the button and check for input event.
      StartGroup(flatui::kLayoutHorizontalCenter);
      flatui::ColorBackground(vec4(225.0f, 128.0f, 0.0f, 0.1f));
      flatui::SetHoverClickColor(vec4(255.0f, 0.0f, 255.0f, 0.8f),
                                 vec4(255.0f, 228.0f, 196.0f, 0.5f));
      const auto button_event = TextButton("Animate!", 100, Margin(10));
      EndGroup();

      // Draw "Ease" test, that will be animated with ease-in ease-out motion.
      StartGroup(flatui::kLayoutHorizontalCenter);
      PositionGroup(flatui::kAlignCenter, flatui::kAlignCenter,
                    vec2(flatui::Animatable("ease_x", kMovementX), kEaseY));
      flatui::Label("Ease", 50.0f);
      EndGroup();

      // Draw "Spring" test, that will be animated with spring motion.
      StartGroup(flatui::kLayoutHorizontalCenter);
      PositionGroup(flatui::kAlignCenter, flatui::kAlignCenter,
                    vec2(flatui::Animatable("spring_x", kMovementX), kSpringY));
      flatui::Label("Spring", 50.0f);
      EndGroup();

      // On button click start animating all variables.
      if (button_event & flatui::kEventWentUp) {
        color_idx = (color_idx + 1) % FPL_ARRAYSIZE(kBackgroundColors);
        flatui::StartAnimation<vec4>("color_fading",
                                     kBackgroundColors[color_idx], kZeros4f,
                                     kColorCurve);

        movement_sign *= -1.0f;
        flatui::StartAnimation<float>("ease_x", movement_sign * kMovementX,
                                      0.0f, kEaseCurve);
        flatui::StartAnimation<float>("spring_x", movement_sign * kMovementX,
                                      0.0f, kSpringCurve);
      }

      // End root group.
      EndGroup();
    });
  }

  return 0;
}
