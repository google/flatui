#include "precompiled.h"
#include "fplbase/renderer.h"
#include "fplbase/input.h"
#include "flatui/flatui.h"
#include <cassert>

class FlatUISample {
public:
  FlatUISample() : matman_(renderer_) {}

  void Initialize() {
    renderer_.Initialize();
    input_.Initialize();

    // Open OpenType font.
    fontman_.Open("fonts/NotoSansCJKjp-Bold.otf");
    fontman_.SetRenderer(renderer_);

    // Load textures.
    matman_.LoadTexture("textures/text_about.webp");
    matman_.StartLoadingTextures();

    // While an initialization of flatui, it implicitly loads shaders used in the
    // API below using AssetManager.
    // shaders/color.glslv & .glslf, shaders/font.glslv & .glslf
    // shaders/textured.glslv & .glslf

    // Wait for everything to finish loading...
    while (matman_.TryFinalize() == false) {
      renderer_.AdvanceFrame(input_.minimized(), input_.Time());
    }
  }

  void ShutDown() { renderer_.ShutDown(); }

  void Run() {
    while (!input_.exit_requested()) {
      input_.AdvanceFrame(&renderer_.window_size());
      renderer_.AdvanceFrame(input_.minimized(), input_.Time());
      Render();
    }
  }

  void Render() {
    const float kColorGray = 0.5f;
    renderer_.ClearFrameBuffer(
        fpl::vec4(kColorGray, kColorGray, kColorGray, 1.0f));

    // Draw GUI test
    TestGUI(matman_, fontman_, input_);
  }

private:
  // Show test GUI using flatui API.
  // In this sample, it shows basic UI elements of a label and an image.
  void TestGUI(fpl::AssetManager &assetman, fpl::FontManager &fontman,
               fpl::InputSystem &input) {
    // Define flatui block. Note that the block is executed multiple times,
    // One for a layout pass and another for a rendering pass.
    fpl::gui::Run(assetman, fontman, input, [&]() {

      // Position UI in the center of the screen. '1000' indicates that the
      // virtual resolution of the entire screen is now 1000x1000.
      fpl::gui::PositionUI(1000, fpl::gui::kAlignCenter,
                                 fpl::gui::kAlignCenter);

      // Show a label with a font size of 40px in a virtual resotuion.
      fpl::gui::Label("The quick brown fox jumps over the lazy dog.", 40);

      // Show an image with a virtical size of 40px in a virtual resotuion.
      // A width is automatically derived from the image size.
      fpl::gui::Image("textures/text_about.webp", 60);
    });
  }

  fpl::Renderer renderer_;
  fpl::InputSystem input_;

  fpl::FontManager fontman_;
  fpl::AssetManager matman_;
};

int main() {
  FlatUISample flatuisample;
  flatuisample.Initialize();
  flatuisample.Run();
  flatuisample.ShutDown();
  return 0;
}
