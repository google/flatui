#include "precompiled.h"
#include "fplbase/renderer.h"
#include "fplbase/input.h"
#include "imgui/imgui.h"
#include <cassert>

namespace fpl {
namespace gui {

// Example how to create a button. We will provide convenient pre-made
// buttons like this, but it is expected many games will make custom buttons.
Event ImageButton(const char *texture_name, float size, const char *id) {
  StartGroup(LAYOUT_VERTICAL_LEFT, size, id);
  SetMargin(Margin(10));
  auto event = CheckEvent();
  if (event & EVENT_IS_DOWN)
    ColorBackground(vec4(1.0f, 1.0f, 1.0f, 0.5f));
  else if (event & EVENT_HOVER)
    ColorBackground(vec4(0.5f, 0.5f, 0.5f, 0.5f));
  Image(texture_name, size);
  EndGroup();
  return event;
}

void TestGUI(MaterialManager &matman, FontManager &fontman,
             InputSystem &input) {
  static float f = 0.0f;
  f += 0.04f;
  static bool show_about = false;
  static vec2i scroll_offset(mathfu::kZeros2i);

  auto click_about_example = [&](const char *id, bool about_on) {
    if (ImageButton("textures/text_about.webp", 50, id) == EVENT_WENT_UP) {
      SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "You clicked: %s", id);
      show_about = about_on;
    }
  };

  Run(matman, fontman, input, [&]() {
    PositionUI(1000, LAYOUT_HORIZONTAL_CENTER, LAYOUT_VERTICAL_RIGHT);
    StartGroup(LAYOUT_OVERLAY_CENTER, 0);
    StartGroup(LAYOUT_HORIZONTAL_TOP, 10);
    StartGroup(LAYOUT_VERTICAL_LEFT, 20);
    click_about_example("my_id1", true);
    StartGroup(LAYOUT_HORIZONTAL_TOP, 0);
    Label("Property T", 30);
    SetTextColor(mathfu::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    Label("Test ", 30);
    SetTextColor(mathfu::kOnes4f);
    Label("ffWAWÄテスト", 30);
    EndGroup();
    StartGroup(LAYOUT_VERTICAL_LEFT, 20);
    StartScroll(vec2(200, 100), &scroll_offset);
    auto splash_tex = matman.FindTexture("textures/text_about.webp");
    ImageBackgroundNinePatch(*splash_tex, vec4(0.2f, 0.2f, 0.8f, 0.8f));
    Label("The quick brown fox jumps over the lazy dog", 32);
    click_about_example("my_id4", true);
    Label("The quick brown fox jumps over the lazy dog", 24);
    Label("The quick brown fox jumps over the lazy dog", 20);
    EndScroll();
    EndGroup();
    EndGroup();
    StartGroup(LAYOUT_VERTICAL_CENTER, 40);
    click_about_example("my_id2", true);
    Image("textures/text_about.webp", 40);
    Image("textures/text_about.webp", 30);
    EndGroup();
    StartGroup(LAYOUT_VERTICAL_RIGHT, 0);
    Label("The\nquick brown fox jumps over the lazy dog.\n"
          "The quick brown fox jumps over the lazy dog. "
          "The quick brown fox jumps over the lazy dog. "
          "The quick brown fox jumps over the lazy dog. ",
          24,vec2(400, 400));
    EndGroup();
    EndGroup();
    if (show_about) {
      StartGroup(LAYOUT_VERTICAL_LEFT, 20, "about_overlay");
      SetMargin(Margin(10));
      ColorBackground(vec4(0.5f, 0.5f, 0.0f, 1.0f));
      click_about_example("my_id3", false);
      Label("This is the about window! すし!", 32);
      Label("You should only be able to click on the", 24);
      Label("about button above, not anywhere else", 20);
      EndGroup();
    }
    EndGroup();
  });
}

}  // gui
}  // fpl

int main() {
  fpl::Renderer renderer;
  fpl::InputSystem input;
  fpl::FontManager fontman;
  fpl::MaterialManager matman(renderer);

  // Initialize stuff.
  renderer.Initialize();
  input.Initialize();

  // Open OpenType font.
  fontman.Open("fonts/NotoSansCJKjp-Bold.otf");
  fontman.SetRenderer(renderer);

  // Load textures.
  matman.LoadTexture("textures/text_about.webp");
  matman.StartLoadingTextures();

  // Wait for everything to finish loading...
  while (matman.TryFinalize() == false) {
    renderer.AdvanceFrame(input.minimized_);
  }

  // Main loop.
  while (!input.exit_requested_) {
    input.AdvanceFrame(&renderer.window_size());
    renderer.AdvanceFrame(input.minimized_);

    const float kColorGray = 0.5f;
    renderer.ClearFrameBuffer(
        fpl::vec4(kColorGray, kColorGray, kColorGray, 1.0f));

    // Draw GUI test
    fpl::gui::TestGUI(matman, fontman, input);
  }

  // Shut down the renderer.
  renderer.ShutDown();

  return 0;
}
