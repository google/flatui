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

#include "flatbuffers/idl.h"
#include "flatbuffers/util.h"
#include "flatui/custom_widgets_generated.h"
#include "flatui/flatui_serialization.h"
#include "fplbase/flatbuffer_utils.h"
#include "fplbase/input.h"
#include "fplbase/renderer.h"
#include "fplbase/utilities.h"

#include <cassert>
#include <functional>

using flatui::Run;
using mathfu::vec2i;
using mathfu::vec3;
using mathfu::vec4;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

// A helper enum to determine which menu we are showing.
enum Menu { kFirstMenu, kSecondMenu };

// Custom FlatUI Widget (to be executed with deserialized data).
//
// Must take a `flatui_data::FlatUIElement` as the first parameter,
// `fplbase::AssetManager` as the second parameter (to handle
// textures), a `FlatUIHandler` std::function to handle events as
// the third parameter, and lastly a `DynamicData*` that contains any
// dynamic data that is registered for this widget.
void ChangeMenuButton(const flatui_data::FlatUIElement* element,
                      fplbase::AssetManager* /*for_texture_rendering*/,
                      flatui::FlatUIHandler event_handler,
                      flatui::DynamicData* dynamic_data) {
  if (element->layout() != flatui_data::Layout_None) {
    flatui::StartGroup(static_cast<flatui::Layout>(element->layout()));
  } else {
    flatui::StartGroup(flatui::kLayoutVerticalLeft);
  }

  // Custom elements can define `attributes` (generic parameters), if they
  // wish to support fields outside of the core widgets. The `attributes`
  // function like a C++ union-like class, with an enum to identify the type.
  //
  // For this example, all of the `attributes` are `Vec4`s, so there is no
  // need to do type checking.
  auto attributes = element->attributes();
  flatui::ColorBackground(
      fplbase::LoadVec4(attributes->Get(0)->vec4_attribute()));
  flatui::SetHoverClickColor(
      fplbase::LoadVec4(attributes->Get(1)->vec4_attribute()),
      fplbase::LoadVec4(attributes->Get(2)->vec4_attribute()));

  flatui::Event e = flatui::TextButton(
      element->text()->c_str(), element->size(),
      flatui::Margin(
          element->margin()->size_left(), element->margin()->size_top(),
          element->margin()->size_right(), element->margin()->size_bottom()));
  flatui::EndGroup();
  event_handler(e, element->id()->str(), dynamic_data);
}

// This function will handle all of the FlatUI events for us. It gets bound to
// the last parameter of `flatui::CreateFlatUIFromData()`.
//
// If you wish, the `event_handler` function could be replaced using a lambda,
// rather than a named function.
void EventHandler(const flatui::Event& event, const std::string& id,
                  flatui::DynamicData* input_data, Menu& menu_id) {
  if (id == "change menu button") {
    if (event & flatui::kEventWentUp) {
      menu_id = kSecondMenu;
    }
  }

  if (id == "back button") {
    if (event & flatui::kEventWentUp) {
      menu_id = kFirstMenu;
    }
  }

  if (id == "first menu edit text") {
    // TODO: Handle the `Edit` event more cleanly once the return value is
    // changed: b/26907562.
    if (event) {
      std::string* str = input_data->data.string_data;
      fplbase::LogInfo("%s\n", str->c_str());
    }
  }
}

extern "C" int FPL_main(int /*argc*/, char** argv) {
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
  fontman.Open("fonts/NotoSansCJKjp-Bold.otf");  // Open OpenType font.

  // Wait for everything to finish loading...
  while (assetman.TryFinalize() == false) {
    renderer.AdvanceFrame(input.minimized(), input.Time());
  }

  // Load then parse the JSON using the FlatBuffer schema.
  std::string first_menu_json, second_menu_json, schema, custom_widgets;
  bool ok = flatbuffers::LoadFile("../serialization/first_menu.json", false,
                                  &first_menu_json) &&
            flatbuffers::LoadFile("../serialization/second_menu.json", false,
                                  &second_menu_json) &&
            flatbuffers::LoadFile("../serialization/custom_widgets.fbs", false,
                                  &custom_widgets) &&
            flatbuffers::LoadFile("../../schemas/flatui.fbs", false, &schema);

  if (!ok) {
    fplbase::LogError("Couldn't load files!\n");
    return 1;
  }

  flatbuffers::Parser first_parser, second_parser;
  const char* include_directories[] = {"../../../fplbase/schemas", nullptr};
  ok = first_parser.Parse(schema.c_str(), include_directories) &&
       first_parser.Parse(custom_widgets.c_str()) &&
       first_parser.Parse(first_menu_json.c_str()) &&
       second_parser.Parse(schema.c_str(), include_directories) &&
       second_parser.Parse(custom_widgets.c_str()) &&
       second_parser.Parse(second_menu_json.c_str());

  if (!ok) {
    fplbase::LogError("Couldn't parse files! Your JSON is likely invalid!\n");
    return 1;
  }

  auto first_menu_data = first_parser.builder_.GetBufferPointer();
  auto second_menu_data = second_parser.builder_.GetBufferPointer();

  // Set the first menu to show by default.
  Menu menu_id = kFirstMenu;

  // The event handler must match the signature of type `FlatUIHandler`:
  // `std::function<void (const flatui::Event*, const std::string&,
  //                      flatui::DynamicData*)>`.
  auto event_handler = [&](const flatui::Event& e, const std::string& id,
                           flatui::DynamicData* dynamic_data) {
    EventHandler(e, id, dynamic_data, menu_id);
  };

  // Register our custom widget for flatui deserialization and creation.
  flatui::RegisterCustomWidget(custom_widgets::Type_ChangeMenuButton,
                               ChangeMenuButton);

  // You must register any "dynamic data" as pointers to variables that
  // will retain their data/state between frames.
  //
  // Note:: Any registered data MUST persist longer than the lifetime of
  // the GUI!
  std::string edit_text_box("Edit me!");
  flatui::RegisterStringData("first menu edit text", &edit_text_box);

  while (!(input.exit_requested() ||
           input.GetButton(fplbase::FPLK_AC_BACK).went_down())) {
    input.AdvanceFrame(&renderer.window_size());
    renderer.AdvanceFrame(input.minimized(), input.Time());
    renderer.ClearFrameBuffer(vec4(vec3(0.5f), 1.0f));

    // Show the GUI, which is generated by calls to our `std::function<void()>`,
    // depending on which menu is currently being rendered.
    switch (menu_id) {
      case kFirstMenu:
        Run(assetman, fontman, input, [&]() {
          flatui::CreateFlatUIFromData(first_menu_data, &assetman,
                                       event_handler);
        });
        break;
      case kSecondMenu:
        Run(assetman, fontman, input, [&]() {
          flatui::CreateFlatUIFromData(second_menu_data, &assetman,
                                       event_handler);
        });
        break;
    }
  }

  return 0;
}
