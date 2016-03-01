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

// Demo showing off how to use FlatUI in 3D, for use in e.g. VR.

#include "fplbase/renderer.h"
#include "fplbase/render_target.h"
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
  renderer.Initialize(vec2i(800, 600), "FlatUI 3D sample");

  fplbase::InputSystem input;
  input.Initialize();

  // Set the local directory to the assets folder for this sample.
  bool result = fplbase::ChangeToUpstreamDir(argv[0], "sample_3d/assets");
  assert(result);

  fplbase::AssetManager assetman(renderer);

  FontManager fontman;
  fontman.SetRenderer(renderer);
  // Open OpenType font.
  fontman.Open("fonts/NotoSansCJKjp-Bold.otf");

  // Off-screen framebuffer object that we'll render the UI into and use as a
  // texture afterwards.
  fplbase::RenderTarget render_target;
  vec2i render_target_size(1024, 1024);
  render_target.Initialize(render_target_size);

  auto tex_shader = assetman.LoadShader("shaders/textured");
  assert(tex_shader);

  // Allow the UI to be moved around. Start at 45 degrees.
  float obj_angle = M_PI / 4;

  while (!(input.exit_requested() ||
           input.GetButton(fplbase::FPLK_AC_BACK).went_down())) {
    input.AdvanceFrame(&renderer.window_size());
    renderer.AdvanceFrame(input.minimized(), input.Time());

    // Rotate the UI in world space using the A & D buttons.
    obj_angle += 0.01f * (input.GetButton(fplbase::FPLK_a).is_down() +
                         -input.GetButton(fplbase::FPLK_d).is_down());

    // Set up a perspective view to position the UI in the world in an
    // interesting way. This generates an mvp that we'll use to translate
    // UI events with. We compute that here ahead of time since we need the
    // mvp both for event processing in the UI and to display the UI.
    auto aspect = static_cast<float>(renderer.window_size().x()) /
                                     renderer.window_size().y();
    auto projection = mathfu::mat4::Perspective(70, aspect, 1, 10000);
    auto scale = mathfu::mat4::FromScaleVector(mathfu::kOnes3f * 0.1f);
    auto rot = mathfu::mat4::FromRotationMatrix(
                 mathfu::quat::FromAngleAxis(obj_angle,
                                             mathfu::kAxisY3f).ToMatrix());
    auto trans = mathfu::mat4::FromTranslationVector(
                   mathfu::vec3(-50.0f, -40.0, -150.0f));
    auto model = trans * rot * scale;
    auto mvp = projection * model;
    auto inverse_mvp = mvp.Inverse();

    // Render the UI into a render target.
    // This is most flexible, as it allows the UI to overlap other objects in
    // 3D, and for additional shader effects to be applied.
    // Alternatively, this technique would also work without a render target,
    // by drawing the UI directly in 3D, but doesn't work as well in a 3D
    // scene because depth-test has to be off.
    render_target.SetAsRenderTarget();

    renderer.ClearFrameBuffer(vec4(0.0f, 0.2f, 0.0f, 1.0f));

    // Set a custom ortho matrix for our square UI panel.
    renderer.set_model_view_projection(
        mathfu::mat4::Ortho(0.0, render_target_size.x(), render_target_size.y(),
                            0.0, -1.0, 1.0));

    // Create FlatUI with 3 buttons.
    Run(assetman, fontman, input, [&]() {
      // We're not rendering to the screen, so inform FlatUI of our render
      // target size, and use the existing projection.
      UseExistingProjection(render_target_size);
      SetVirtualResolution(300);

      // Here we specify our custom mvp, so that any pointer events will be
      // translated into the space we later place this UI.
      ApplyCustomTransform(inverse_mvp);

      StartGroup(kLayoutVerticalLeft);
      PositionGroup(kAlignCenter, kAlignCenter, mathfu::kZeros2f);

      // Show some buttons to click on.
      TextButton("Button 1", 40, Margin(5));
      TextButton("Button 2", 40, Margin(5));
      TextButton("Button 3", 40, Margin(5));

      EndGroup();
    });

    // Switch back to the original render target.
    fplbase::RenderTarget::ScreenRenderTarget(renderer).SetAsRenderTarget();
    renderer.ClearFrameBuffer(vec4(0.0f, 0.0f, 0.0f, 1.0f));

    // Now place the UI in the space we computed before.
    renderer.set_model_view_projection(mvp);

    tex_shader->Set(renderer);

    render_target.BindAsTexture(0);

    // Render UI as a quad, with object-space coordinates the same as pixels,
    // that way our projected coordinates match.
    fplbase::Mesh::RenderAAQuadAlongX(
          mathfu::kZeros3f,
          mathfu::vec3(render_target_size.x(), render_target_size.y(), 0),
          mathfu::vec2(0, 0),
          mathfu::vec2(1, 1));
  }

  return 0;
}
