#include "precompiled.h"
#include "fplbase/renderer.h"
#include "fplbase/input.h"
#include "fplbase/utilities.h"
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

// Example how to create a checkbox.
Event CheckBox(const char *texture_name_checked,
               const char *texture_name_unchecked,
               const char *label,
               float size, const char *id, bool *is_checked) {
  StartGroup(LAYOUT_HORIZONTAL_BOTTOM, 0, id);
  auto event = CheckEvent();
  Image(*is_checked ? texture_name_checked : texture_name_unchecked, size);
  SetMargin(Margin(6, 0));
  Label(label, size);

  // Note that this event check needs to come after the Image() API that is
  // using the variable, because the event check happens in the render pass,
  // and we want to specify same texture name for both the layout and render
  // pass (otherwise the IMGUI system can not find a correct layout info).
  if (event & EVENT_WENT_UP) {
    *is_checked = !*is_checked;
  }
  EndGroup();
  return event;
}

void TestGUI(MaterialManager &matman, FontManager &fontman,
             InputSystem &input) {
  static float f = 0.0f;
  f += 0.04f;
  static bool show_about = false;
  static vec2i scroll_offset(mathfu::kZeros2i);
  static bool checkbox1_checked;

  auto click_about_example = [&](const char *id, bool about_on) {
    if (ImageButton("textures/text_about.webp", 50, id) == EVENT_WENT_UP) {
      SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "You clicked: %s", id);
      show_about = about_on;
    }
  };

  Run(matman, fontman, input, [&]() {
    PositionUI(1000, LAYOUT_HORIZONTAL_CENTER, LAYOUT_VERTICAL_CENTER);
    StartGroup(LAYOUT_OVERLAY_CENTER, 0);
    StartGroup(LAYOUT_HORIZONTAL_TOP, 10);
    StartGroup(LAYOUT_VERTICAL_LEFT, 20);
    click_about_example("my_id1", true);
    CheckBox("textures/btn_check_on.webp",
             "textures/btn_check_off.webp",
             "CheckBox", 30, "checkbox_1", &checkbox1_checked);
    StartGroup(LAYOUT_HORIZONTAL_TOP, 0);
    Label("Property T", 30);
    SetTextColor(mathfu::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    Label("Test ", 30);
    SetTextColor(mathfu::kOnes4f);
    Label("ffWAWÄテスト", 30);
    EndGroup();

    StartGroup(LAYOUT_VERTICAL_LEFT, 20, "scroll");
    StartScroll(vec2(200, 100), &scroll_offset);
    auto splash_tex = matman.FindTexture("textures/text_about.webp");
    ImageBackgroundNinePatch(*splash_tex, vec4(0.2f, 0.2f, 0.8f, 0.8f));
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
  matman.LoadTexture("textures/btn_check_on.webp");
  matman.LoadTexture("textures/btn_check_off.webp");
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
