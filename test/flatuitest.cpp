#include "precompiled.h"
#include "fplbase/renderer.h"
#include "fplbase/input.h"
#include "fplbase/utilities.h"
#include "flatui/flatui.h"
#include <cassert>

namespace fpl {
namespace gui {

// Example how to create a button. We will provide convenient pre-made
// buttons like this, but it is expected many games will make custom buttons.
Event ImageButton(const char *texture_name, float size, const char *id) {
  StartGroup(kLayoutVerticalLeft, size, id);
  SetMargin(Margin(10));
  auto event = CheckEvent();
  if (event & kEventIsDown)
    ColorBackground(vec4(1.0f, 1.0f, 1.0f, 0.5f));
  else if (event & kEventHover)
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
  StartGroup(kLayoutHorizontalBottom, 0, id);
  auto event = CheckEvent();
  Image(*is_checked ? texture_name_checked : texture_name_unchecked, size);
  SetMargin(Margin(6, 0));
  Label(label, size);

  // Note that this event check needs to come after the Image() API that is
  // using the variable, because the event check happens in the render pass,
  // and we want to specify same texture name for both the layout and render
  // pass (otherwise the FlatUI system can not find a correct layout info).
  if (event & kEventWentUp) {
    *is_checked = !*is_checked;
  }
  EndGroup();
  return event;
}

// Example how to create a slider.
Event Slider(const Texture &foreground_tex, const Texture &background_tex,
             const vec2 &size, float bar_height, const char *id,
             float *slider_value) {
  StartGroup(kLayoutHorizontalBottom, 0, id);
  StartSlider(kDirHorizontal, slider_value);
  auto event = CheckEvent();
  CustomElement(size, id, [&foreground_tex, &background_tex, bar_height,
                           slider_value](const vec2i &pos,
                                         const vec2i &size){
    // Render the slider.
    auto bar_pos = pos;
    auto bar_size = size;
    bar_pos += vec2i(size.y() * 0.5, size.y() * (1.0 - bar_height) * 0.5);
    bar_size = vec2i(std::max(bar_size.x() - size.y(), 0),
                     bar_size.y() * bar_height);

    auto knob_pos = pos;
    auto knob_sizes = vec2i(size.y(), size.y());
    knob_pos.x() += *slider_value * static_cast<float>(size.x() - size.y());
    RenderTextureNinePatch(background_tex, vec4(0.5f, 0.5f, 0.5f, 0.5f),
                  bar_pos, bar_size);
    RenderTexture(foreground_tex, knob_pos, knob_sizes);
  });

  EndSlider();
  EndGroup();
  return event;
}

void TestGUI(AssetManager &assetman, FontManager &fontman,
             InputSystem &input) {
  static float f = 0.0f;
  f += 0.04f;
  static bool show_about = false;
  static vec2i scroll_offset(mathfu::kZeros2i);
  static bool checkbox1_checked;
  static float slider_value;
  static std::string str("Edit box.");
  static std::string str2("More Edit box.");
  static std::string str3("The\nquick brown fox jumps over the lazy dog.\n"
                          "The quick brown fox jumps over the lazy dog. "
                          "The quick brown fox jumps over the lazy dog. "
                          "The quick brown fox jumps over the lazy dog. ");

  auto click_about_example = [&](const char *id, bool about_on) {
    if (ImageButton("textures/text_about.webp", 50, id) == kEventWentUp) {
      SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "You clicked: %s", id);
      show_about = about_on;
    }
  };

  Run(assetman, fontman, input, [&]() {
    SetVirtualResolution(1000);
    StartGroup(kLayoutOverlay, 0);
      StartGroup(kLayoutHorizontalTop, 10);
        PositionGroup(kAlignCenter, kAlignCenter, mathfu::kZeros2f);
        StartGroup(kLayoutVerticalLeft, 20);
          click_about_example("my_id1", true);
          // Textures used in widgets.
          auto slider_background_tex =
              assetman.FindTexture("textures/gray_bar.webp");
          auto slider_knob_tex =
              assetman.FindTexture("textures/white_circle.webp");
          Edit(30, vec2(400, 30), "edit2", &str2);
          StartGroup(kLayoutHorizontalTop, 0);
            Edit(30, vec2(0, 30), "edit", &str);
            Label(">Tail", 30);
          EndGroup();
          Slider(*slider_knob_tex, *slider_background_tex,
                 vec2(300, 25), 0.5f, "slider", &slider_value);
          CheckBox("textures/btn_check_on.webp",
                   "textures/btn_check_off.webp",
                   "CheckBox", 30, "checkbox_1", &checkbox1_checked);
          StartGroup(kLayoutHorizontalTop, 0);
            Label("Property T", 30);
            SetTextColor(mathfu::vec4(1.0f, 0.0f, 0.0f, 1.0f));
            Label("Test ", 30);
            SetTextColor(mathfu::kOnes4f);
            Label("ffWAWÄテスト", 30);
          EndGroup();
          StartGroup(kLayoutVerticalLeft, 20, "scroll");
            StartScroll(vec2(200, 100), &scroll_offset);
              auto splash_tex =
                  assetman.FindTexture("textures/text_about.webp");
              ImageBackgroundNinePatch(*splash_tex,
                                       vec4(0.2f, 0.2f, 0.8f, 0.8f));
              click_about_example("my_id4", true);
              Label("The quick brown fox jumps over the lazy dog", 24);
              Label("The quick brown fox jumps over the lazy dog", 20);
            EndScroll();
          EndGroup();
        EndGroup();
        StartGroup(kLayoutVerticalCenter, 40);
          click_about_example("my_id2", true);
          Image("textures/text_about.webp", 40);
          Image("textures/text_about.webp", 30);
        EndGroup();
        StartGroup(kLayoutVerticalRight, 0);
          Edit(24, vec2(400, 400), "edit3", &str3);
        EndGroup();
      EndGroup();
      if (show_about) {
        StartGroup(kLayoutVerticalLeft, 20, "about_overlay");
          ModalGroup();
          PositionGroup(kAlignRight, kAlignTop, vec2(-30, 30));
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
  fpl::AssetManager assetman(renderer);

  // Initialize stuff.
  renderer.Initialize();
  renderer.SetCulling(fpl::Renderer::kCullBack);
  input.Initialize();

  // Open OpenType font.
  fontman.Open("fonts/NotoSansCJKjp-Bold.otf");
  fontman.SetRenderer(renderer);

  // Load textures.
  assetman.LoadTexture("textures/text_about.webp");
  assetman.LoadTexture("textures/btn_check_on.webp");
  assetman.LoadTexture("textures/btn_check_off.webp");
  assetman.LoadTexture("textures/white_circle.webp");
  assetman.LoadTexture("textures/gray_bar.webp");
  assetman.StartLoadingTextures();

  // Wait for everything to finish loading...
  while (assetman.TryFinalize() == false) {
    renderer.AdvanceFrame(input.minimized(), input.Time());
  }

  // Main loop.
  while (!input.exit_requested()) {
    input.AdvanceFrame(&renderer.window_size());
    renderer.AdvanceFrame(input.minimized(), input.Time());

    const float kColorGray = 0.5f;
    renderer.ClearFrameBuffer(
        fpl::vec4(kColorGray, kColorGray, kColorGray, 1.0f));

    // Draw GUI test
    fpl::gui::TestGUI(assetman, fontman, input);
  }

  // Shut down the renderer.
  renderer.ShutDown();

  return 0;
}
