#include "precompiled.h"
#include "fplbase/renderer.h"
#include "fplbase/input.h"
#include "imgui/imgui.h"
#include <cassert>

class ImGuiSample {
public:
  ImGuiSample() : matman_(renderer_) {}

  void Initialize() {
    renderer_.Initialize();
    input_.Initialize();

    // Open OpenType font.
    fontman_.Open("fonts/NotoSansCJKjp-Bold.otf");
    fontman_.SetRenderer(renderer_);

    // Load textures.
    matman_.LoadTexture("textures/text_about.webp");
    matman_.StartLoadingTextures();

    // While an initialization of Imgui, it implicitly loads shaders used in the
    // API below using MaterialManager.
    // shaders/color.glslv & .glslf, shaders/font.glslv & .glslf
    // shaders/textured.glslv & .glslf

    // Wait for everything to finish loading...
    while (matman_.TryFinalize() == false) {
      renderer_.AdvanceFrame(input_.minimized_);
    }
  }

  void ShutDown() { renderer_.ShutDown(); }

  void Run() {
    while (!input_.exit_requested_) {
      input_.AdvanceFrame(&renderer_.window_size());
      renderer_.AdvanceFrame(input_.minimized_);
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
  // Show test GUI using imgui API.
  // In this sample, it shows basic UI elements of a label and an image.
  void TestGUI(fpl::MaterialManager &matman, fpl::FontManager &fontman,
               fpl::InputSystem &input) {
    // Define Imgui block. Note that the block is executed multiple times,
    // One for a layout pass and another for a rendering pass.
    fpl::gui::Run(matman, fontman, input, [&]() {

      // Position UI in the center of the screen. '1000' indicates that the
      // virtual resolution of the entire screen is now 1000x1000.
      fpl::gui::PositionUI(1000, fpl::gui::LAYOUT_HORIZONTAL_CENTER,
                           fpl::gui::LAYOUT_VERTICAL_CENTER);

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
  fpl::MaterialManager matman_;
};

int main() {
  ImGuiSample imguisample;
  imguisample.Initialize();
  imguisample.Run();
  imguisample.ShutDown();
  return 0;
}
