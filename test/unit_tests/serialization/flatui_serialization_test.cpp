// Copyright 2016 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/flatui_serialization.cpp"
#include "flatui/flatui_generated.h"
#include "fplutil/main.h"
#include "gtest/gtest.h"
#include "mocks/flatui_common_mocks.h"
#include "mocks/flatui_mocks.h"
#include "mocks/fplbase_mocks.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;
using flatui::FlatUIMocks;
using flatui::FlatUICommonMocks;

// Note: You cannot easily extact `va_list` arguments from the
// `fplbase::LogError` mock. Therefore, I simply check that the correct
// error message was called, without checking the format string conversion
// specifiers.

class FlatUISerializationTest : public ::testing::Test {
 protected:
  static void SetUpTestCase() {       // Called once, before any tests.
    flatui::SetErrorOutputCount(-1);  // Disable error count limit.
  }
};

TEST_F(FlatUISerializationTest, TestCreateMargin) {
  // Create a fake Margin flatbuffer.
  flatbuffers::FlatBufferBuilder builder;
  auto id = builder.CreateString("dummy ID");

  flatui_data::MarginBuilder margin_builder(builder);
  margin_builder.add_id(id);
  margin_builder.add_size_left(1.0f);
  margin_builder.add_size_top(2.0f);
  margin_builder.add_size_right(3.0f);
  margin_builder.add_size_bottom(4.0f);
  auto margin_offset = margin_builder.Finish();

  flatui_data::FlatUIElementBuilder element_builder(builder);
  element_builder.add_id(id);
  element_builder.add_margin(margin_offset);
  auto element_offset = element_builder.Finish();

  std::vector<flatbuffers::Offset<flatui_data::FlatUIElement>> elements_vector;
  elements_vector.push_back(element_offset);
  auto elements = builder.CreateVector(elements_vector);

  flatui_data::FlatUIBuilder flatui_builder(builder);
  flatui_builder.add_elements(elements);
  auto flatui_flatbuffer = flatui_builder.Finish();
  builder.Finish(flatui_flatbuffer);

  auto margin_buf = const_cast<flatui_data::Margin*>(
      flatui_data::GetFlatUI(builder.GetBufferPointer())
          ->elements()
          ->Get(0)
          ->margin());

  flatui::Margin m = flatui::CreateMargin(margin_buf);
  ASSERT_EQ(1.0f, m.borders.x());
  ASSERT_EQ(2.0f, m.borders.y());
  ASSERT_EQ(3.0f, m.borders.z());
  ASSERT_EQ(4.0f, m.borders.w());

  margin_buf->mutate_size_right(-1.0f);
  m = flatui::CreateMargin(margin_buf);
  ASSERT_EQ(1.0f, m.borders.x());
  ASSERT_EQ(2.0f, m.borders.y());
  ASSERT_EQ(1.0f, m.borders.z());
  ASSERT_EQ(2.0f, m.borders.w());

  margin_buf->mutate_size_top(-1.0f);
  m = flatui::CreateMargin(margin_buf);
  ASSERT_EQ(1.0f, m.borders.x());
  ASSERT_EQ(1.0f, m.borders.y());
  ASSERT_EQ(1.0f, m.borders.z());
  ASSERT_EQ(1.0f, m.borders.w());

  margin_buf->mutate_size_left(-1.0f);
  m = flatui::CreateMargin(margin_buf);
  ASSERT_EQ(0.0f, m.borders.x());
  ASSERT_EQ(0.0f, m.borders.y());
  ASSERT_EQ(0.0f, m.borders.z());
  ASSERT_EQ(0.0f, m.borders.w());

  // Invalid input test
  m = flatui::CreateMargin(nullptr);
  ASSERT_EQ(0.0f, m.borders.x());
  ASSERT_EQ(0.0f, m.borders.y());
  ASSERT_EQ(0.0f, m.borders.z());
  ASSERT_EQ(0.0f, m.borders.w());
}

TEST_F(FlatUISerializationTest, TestCreateSetVirtualResolution) {
  // Create a fake FlatUIElement flatbuffer.
  flatbuffers::FlatBufferBuilder builder;
  std::string test_id("set virtual resolution test ID");
  auto element_id = builder.CreateString(test_id.c_str());

  flatui_data::FlatUIElementBuilder element_builder(builder);
  element_builder.add_type(flatui_data::Type_SetVirtualResolution);
  element_builder.add_id(element_id);
  element_builder.add_virtual_resolution(100.0f);
  auto element_offset = element_builder.Finish();

  std::vector<flatbuffers::Offset<flatui_data::FlatUIElement>> elements_vector;
  elements_vector.push_back(element_offset);
  auto elements = builder.CreateVector(elements_vector);

  flatui_data::FlatUIBuilder flatui_builder(builder);
  flatui_builder.add_elements(elements);
  auto flatui_flatbuffer = flatui_builder.Finish();

  builder.Finish(flatui_flatbuffer);
  auto flatui_pointer = flatui_data::GetFlatUI(builder.GetBufferPointer());

  FlatUIMocks& flatui_mocks = FlatUIMocks::get_mocks();
  EXPECT_CALL(flatui_mocks, SetVirtualResolution(100.0f)).Times(1);

  flatui::CreateSetVirtualResolution(flatui_pointer->elements()->Get(0));

  // Test invalid input.
  //
  // Mock the `fplbase::LogError` function to assert it is called.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillOnce(SaveArg<0>(&error_output));

  auto element = const_cast<flatui_data::FlatUIElement*>(
      flatui_pointer->elements()->Get(0));
  element->mutate_virtual_resolution(-1.0f);
  flatui::CreateSetVirtualResolution(element);

  ASSERT_EQ(
      "Required field \"virtual_resolution\" is missing for FlatUI"
      " element with ID \"%s\".",
      error_output);

  // Test that dynamic data works when the input is missing.
  EXPECT_CALL(flatui_mocks, SetVirtualResolution(50.0f)).Times(1);
  float test_float = 50.0f;
  flatui::RegisterFloatData(test_id, &test_float);

  flatui::CreateSetVirtualResolution(element);
}

// Note: This function does not test the nested `elements` field, since it
// simply calls to MapElement, which has its own test.
TEST_F(FlatUISerializationTest, TestCreateGroup) {
  // Create a fake FlatUIElement flatbuffer.
  flatbuffers::FlatBufferBuilder builder;
  std::string id("create group ID");
  std::string missing_offset_id("create group w/ missing offset ID");
  auto element_id = builder.CreateString(id.c_str());
  auto missing_offset_element_id =
      builder.CreateString(missing_offset_id.c_str());

  flatui_data::FlatUIElementBuilder element_builder(builder);
  element_builder.add_id(element_id);
  element_builder.add_layout(flatui_data::Layout_HorizontalTop);
  element_builder.add_spacing(1.0f);
  element_builder.add_horizontal(flatui_data::Alignment_Center);
  element_builder.add_vertical(flatui_data::Alignment_Center);
  fplbase::Vec2 test_vec2(0.0f, 0.0f);
  element_builder.add_offset(&test_vec2);
  element_builder.add_is_modal_group(true);
  auto element_offset = element_builder.Finish();

  // Create a second fake FlatUIElement with a missing `offset` field.
  flatui_data::FlatUIElementBuilder missing_offset_element_builder(builder);
  missing_offset_element_builder.add_id(missing_offset_element_id);
  missing_offset_element_builder.add_layout(flatui_data::Layout_HorizontalTop);
  missing_offset_element_builder.add_spacing(1.0f);
  missing_offset_element_builder.add_horizontal(flatui_data::Alignment_Center);
  missing_offset_element_builder.add_vertical(flatui_data::Alignment_Center);
  auto missing_offset_element_offset = missing_offset_element_builder.Finish();

  std::vector<flatbuffers::Offset<flatui_data::FlatUIElement>> elements_vector;
  elements_vector.push_back(element_offset);
  elements_vector.push_back(missing_offset_element_offset);
  auto elements = builder.CreateVector(elements_vector);

  flatui_data::FlatUIBuilder flatui_builder(builder);
  flatui_builder.add_elements(elements);
  auto flatui_flatbuffer = flatui_builder.Finish();

  builder.Finish(flatui_flatbuffer);
  auto flatui_pointer = flatui_data::GetFlatUI(builder.GetBufferPointer());

  auto element = const_cast<flatui_data::FlatUIElement*>(
      flatui_pointer->elements()->Get(0));

  // Mock calls to `flatui` library functions.
  flatui::FlatUIMocks& flatui_mocks = flatui::FlatUIMocks::get_mocks();

  // Mock `fplbase::LogError` and `fplbase::FindTexture`.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillRepeatedly(SaveArg<0>(&error_output));

  // Test that the function gets called correctly with all params
  EXPECT_CALL(flatui_mocks, StartGroup(_, _, _)).Times(1);
  EXPECT_CALL(flatui_mocks, PositionGroup(_, _, _)).Times(1);
  EXPECT_CALL(flatui_mocks, ModalGroup()).Times(1);
  EXPECT_CALL(flatui_mocks, EndGroup()).Times(1);
  flatui::CreateGroup(element, nullptr, nullptr);

  // Test missing `layout` field.
  element->mutate_layout(flatui_data::Layout_None);
  flatui::CreateGroup(element, nullptr, nullptr);
  ASSERT_EQ(std::string("Required field \"layout\" is missing for FlatUI"
                        " element with ID \"%s\"."),
            error_output);

  // Test dynamic data for `layout` field.
  int dummy_int = 1;
  flatui::RegisterIntData(id, &dummy_int);
  EXPECT_CALL(flatui_mocks, StartGroup(_, _, _)).Times(1);
  EXPECT_CALL(flatui_mocks, PositionGroup(_, _, _)).Times(1);
  EXPECT_CALL(flatui_mocks, ModalGroup()).Times(1);
  EXPECT_CALL(flatui_mocks, EndGroup()).Times(1);
  flatui::CreateGroup(element, nullptr, nullptr);

  // Test missing `spacing` field.
  element->mutate_layout(flatui_data::Layout_HorizontalTop);
  element->mutate_spacing(0.0f);
  EXPECT_CALL(flatui_mocks, StartGroup(_, 0.0f, _)).Times(1);
  EXPECT_CALL(flatui_mocks, PositionGroup(_, _, _)).Times(1);
  EXPECT_CALL(flatui_mocks, ModalGroup()).Times(1);
  EXPECT_CALL(flatui_mocks, EndGroup()).Times(1);
  flatui::CreateGroup(element, nullptr, nullptr);

  // Test dyanmic data for `spacing` field.
  flatui::dynamic_data.erase(id);  // Reuse the same ID.
  float dummy_float = 1.0f;
  EXPECT_CALL(flatui_mocks, StartGroup(_, 1.0f, _)).Times(1);
  EXPECT_CALL(flatui_mocks, PositionGroup(_, _, _)).Times(1);
  EXPECT_CALL(flatui_mocks, ModalGroup()).Times(1);
  EXPECT_CALL(flatui_mocks, EndGroup()).Times(1);
  flatui::RegisterFloatData(id, &dummy_float);
  flatui::CreateGroup(element, nullptr, nullptr);

  // Test missing `horizontal` field.
  flatui::dynamic_data.erase(id);  // Clear dynamic data at the ID.
  element->mutate_spacing(1.0f);
  element->mutate_horizontal(flatui_data::Alignment_None);
  EXPECT_CALL(flatui_mocks, StartGroup(_, _, _)).Times(1);
  EXPECT_CALL(flatui_mocks, ModalGroup()).Times(1);
  EXPECT_CALL(flatui_mocks, EndGroup()).Times(1);
  flatui::CreateGroup(element, nullptr, nullptr);

  // Test dynamic data for `horizontal` field.
  EXPECT_CALL(flatui_mocks, StartGroup(_, _, _)).Times(1);
  EXPECT_CALL(flatui_mocks, PositionGroup(flatui::kAlignTop, _, _)).Times(1);
  EXPECT_CALL(flatui_mocks, ModalGroup()).Times(1);
  EXPECT_CALL(flatui_mocks, EndGroup()).Times(1);
  flatui::RegisterIntData(id, &dummy_int);
  flatui::CreateGroup(element, nullptr, nullptr);

  // Test missing `vertical` field.
  flatui::dynamic_data.erase(id);  // Clear dynamic data at the ID.
  element->mutate_horizontal(flatui_data::Alignment_TopOrLeft);
  element->mutate_vertical(flatui_data::Alignment_None);
  EXPECT_CALL(flatui_mocks, StartGroup(_, _, _)).Times(1);
  EXPECT_CALL(flatui_mocks, ModalGroup()).Times(1);
  EXPECT_CALL(flatui_mocks, EndGroup()).Times(1);
  flatui::CreateGroup(element, nullptr, nullptr);

  // Test dynamic data for `horizontal` field.
  EXPECT_CALL(flatui_mocks, StartGroup(_, _, _)).Times(1);
  EXPECT_CALL(flatui_mocks, PositionGroup(_, flatui::kAlignTop, _)).Times(1);
  EXPECT_CALL(flatui_mocks, ModalGroup()).Times(1);
  EXPECT_CALL(flatui_mocks, EndGroup()).Times(1);
  flatui::RegisterIntData(id, &dummy_int);
  flatui::CreateGroup(element, nullptr, nullptr);

  // Test missing `offset` field.
  EXPECT_CALL(flatui_mocks, StartGroup(_, _, _)).Times(1);
  EXPECT_CALL(flatui_mocks, EndGroup()).Times(1);
  flatui::CreateGroup(flatui_pointer->elements()->Get(1), nullptr, nullptr);

  // Test dynamic data for `offset` field.
  EXPECT_CALL(flatui_mocks, StartGroup(_, _, _)).Times(1);
  EXPECT_CALL(flatui_mocks, PositionGroup(_, _, _)).Times(1);
  EXPECT_CALL(flatui_mocks, EndGroup()).Times(1);
  mathfu::vec2 dummy_vec2;
  flatui::RegisterVec2Data(missing_offset_id, &dummy_vec2);
  flatui::CreateGroup(flatui_pointer->elements()->Get(1), nullptr, nullptr);
}

TEST_F(FlatUISerializationTest, TestCreateImageButton) {
  // Create a fake FlatUIElement flatbuffer.
  flatbuffers::FlatBufferBuilder builder;
  std::string id("create image button ID");
  std::string missing_texture_id("create image button w/ missing texture ID");
  auto element_id = builder.CreateString(id.c_str());
  auto missing_texture_element_id =
      builder.CreateString(missing_texture_id.c_str());

  flatui_data::FlatUIElementBuilder element_builder(builder);
  element_builder.add_id(element_id);
  element_builder.add_size(1.0f);
  element_builder.add_texture(element_id);
  auto element_offset = element_builder.Finish();

  // Create a second fake FlatBuffer with a missing `texture` field.
  flatui_data::FlatUIElementBuilder missing_texture_element_builder(builder);
  missing_texture_element_builder.add_id(missing_texture_element_id);
  missing_texture_element_builder.add_size(1.0f);
  auto missing_texture_element_offset =
      missing_texture_element_builder.Finish();

  std::vector<flatbuffers::Offset<flatui_data::FlatUIElement>> elements_vector;
  elements_vector.push_back(element_offset);
  elements_vector.push_back(missing_texture_element_offset);
  auto elements = builder.CreateVector(elements_vector);

  flatui_data::FlatUIBuilder flatui_builder(builder);
  flatui_builder.add_elements(elements);
  auto flatui_flatbuffer = flatui_builder.Finish();

  builder.Finish(flatui_flatbuffer);
  auto buf = builder.GetBufferPointer();

  auto flatui_pointer = flatui_data::GetFlatUI(buf);

  // Mock calls to `flatui::ImageButton`.
  flatui::FlatUICommonMocks& flatui_common_mocks =
      flatui::FlatUICommonMocks::get_mocks();

  // Mock `fplbase::LogError` and `fplbase::FindTexture`.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillRepeatedly(SaveArg<0>(&error_output));

  // Create a dummy `fplbase::AssetManager` to satisfy any nullptr checks.
  fplbase::Renderer dummy_renderer;
  fplbase::AssetManager dummy_assetman(dummy_renderer);

  // Create a dummy `fplbase::Texture`.
  fplbase::Texture dummy_texture;

  // Test event handler is called.
  EXPECT_CALL(fplbase_mocks, FindTexture(_)).WillOnce(Return(&dummy_texture));
  EXPECT_CALL(flatui_common_mocks, ImageButton(_, _, _, _))
      .WillOnce(Return(flatui::kEventWentUp));

  auto event_handler_func = [&](const flatui::Event& event,
                                const std::string& result_id,
                                flatui::DynamicData* dynamic_data) {
    ASSERT_EQ(flatui::kEventWentUp, event);
    ASSERT_EQ(id, result_id);
    ASSERT_EQ(nullptr, dynamic_data);
  };

  flatui::CreateImageButton(flatui_pointer->elements()->Get(0), &dummy_assetman,
                            event_handler_func);

  // Test with `size` missing.
  auto element = const_cast<flatui_data::FlatUIElement*>(
      flatui_pointer->elements()->Get(0));
  element->mutate_size(-1.0f);
  flatui::CreateImageButton(element, &dummy_assetman, nullptr);
  ASSERT_EQ(std::string("Required field \"size\" is missing for FlatUI element"
                        " with ID \"%s\"."),
            error_output);

  // Test with dynamic data for `size`.
  float dummy_float;
  flatui::RegisterFloatData(id, &dummy_float);
  EXPECT_CALL(fplbase_mocks, FindTexture(_)).WillOnce(Return(&dummy_texture));
  EXPECT_CALL(flatui_common_mocks, ImageButton(_, _, _, _)).Times(1);
  flatui::CreateImageButton(flatui_pointer->elements()->Get(0), &dummy_assetman,
                            nullptr);

  // Test with dynamic data for `texture`.
  std::string dummy_string;
  flatui::RegisterStringData(missing_texture_id, &dummy_string);
  EXPECT_CALL(fplbase_mocks, FindTexture(_)).WillOnce(Return(&dummy_texture));
  EXPECT_CALL(flatui_common_mocks, ImageButton(_, _, _, _)).Times(1);
  flatui::CreateImageButton(flatui_pointer->elements()->Get(1), &dummy_assetman,
                            nullptr);
}

TEST_F(FlatUISerializationTest, TestCreateTextButton) {
  // Create a fake FlatUIElement flatbuffer.
  flatbuffers::FlatBufferBuilder builder;
  std::string id("create text button ID");
  std::string missing_text_id("create text button w/ missing text ID");
  std::string missing_texture_id("create text button w/ missing texture ID");
  auto element_id = builder.CreateString(id.c_str());
  auto missing_text_element_id = builder.CreateString(missing_text_id.c_str());
  auto missing_texture_element_id =
      builder.CreateString(missing_texture_id.c_str());

  flatui_data::FlatUIElementBuilder element_builder(builder);
  element_builder.add_id(element_id);
  element_builder.add_size(10);
  element_builder.add_text(element_id);
  element_builder.add_texture(element_id);
  auto element_offset = element_builder.Finish();

  // Create a second fake FlatBuffer with a missing `text` field.
  flatui_data::FlatUIElementBuilder missing_text_element_builder(builder);
  missing_text_element_builder.add_id(missing_text_element_id);
  missing_text_element_builder.add_size(10);
  auto missing_text_element_offset = missing_text_element_builder.Finish();

  // Create a third fake FlatBuffer with a missing `texture` field.
  flatui_data::FlatUIElementBuilder missing_texture_element_builder(builder);
  missing_texture_element_builder.add_id(missing_texture_element_id);
  missing_texture_element_builder.add_size(10);
  missing_texture_element_builder.add_text(missing_texture_element_id);
  auto missing_texture_element_offset =
      missing_texture_element_builder.Finish();

  std::vector<flatbuffers::Offset<flatui_data::FlatUIElement>> elements_vector;
  elements_vector.push_back(element_offset);
  elements_vector.push_back(missing_text_element_offset);
  elements_vector.push_back(missing_texture_element_offset);
  auto elements = builder.CreateVector(elements_vector);

  flatui_data::FlatUIBuilder flatui_builder(builder);
  flatui_builder.add_elements(elements);
  auto flatui_flatbuffer = flatui_builder.Finish();

  builder.Finish(flatui_flatbuffer);
  auto buf = builder.GetBufferPointer();

  auto flatui_pointer = flatui_data::GetFlatUI(buf);

  // Mock calls to `flatui::TextButton`.
  flatui::FlatUICommonMocks& flatui_common_mocks =
      flatui::FlatUICommonMocks::get_mocks();

  // Mock `fplbase::LogError`.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillRepeatedly(SaveArg<0>(&error_output));

  // Create a dummy `fplbase::AssetManager` to satisfy any nullptr checks.
  fplbase::Renderer dummy_renderer;
  fplbase::AssetManager dummy_assetman(dummy_renderer);

  // Test with `ButtonProperty` missing.
  fplbase::Texture dummy_texture;
  EXPECT_CALL(fplbase_mocks, FindTexture(_)).WillOnce(Return(&dummy_texture));
  EXPECT_CALL(flatui_common_mocks,
              TextButton(_, _, _, _, _, flatui::kButtonPropertyDisabled))
      .Times(1);
  flatui::CreateTextButton(flatui_pointer->elements()->Get(0), &dummy_assetman,
                           nullptr);

  // Test with dynamic data for `Button Property`.
  int dummy_int = 2;
  flatui::RegisterIntData(id, &dummy_int);
  EXPECT_CALL(fplbase_mocks, FindTexture(_)).WillOnce(Return(&dummy_texture));
  EXPECT_CALL(flatui_common_mocks,
              TextButton(_, _, _, _, _, flatui::kButtonPropertyImageLeft))
      .Times(1);
  flatui::CreateTextButton(flatui_pointer->elements()->Get(0), &dummy_assetman,
                           nullptr);

  // Test with `text` missing.
  flatui::CreateTextButton(flatui_pointer->elements()->Get(1), nullptr,
                           nullptr);
  ASSERT_EQ(std::string("Required field \"text\" is missing for FlatUI element"
                        " with ID \"%s\"."),
            error_output);

  // Test with dynamic data for `text`.
  std::string dummy_string;
  flatui::RegisterStringData(missing_text_id, &dummy_string);
  EXPECT_CALL(flatui_common_mocks, TextButton(_, _, _)).Times(1);
  flatui::CreateTextButton(flatui_pointer->elements()->Get(1), nullptr,
                           nullptr);

  // Test event handler is called.
  EXPECT_CALL(flatui_common_mocks, TextButton(_, _, _))
      .WillOnce(Return(flatui::kEventWentUp));

  auto event_handler_func = [&](const flatui::Event& event,
                                const std::string& id,
                                flatui::DynamicData* dynamic_data) {
    ASSERT_EQ(flatui::kEventWentUp, event);
    ASSERT_EQ(missing_text_id, id);
    ASSERT_EQ(&dummy_string, dynamic_data->data.string_data);
  };

  flatui::CreateTextButton(flatui_pointer->elements()->Get(1), nullptr,
                           event_handler_func);

  // Test with `size` missing.
  auto element = const_cast<flatui_data::FlatUIElement*>(
      flatui_pointer->elements()->Get(1));
  element->mutate_size(-1.0f);
  flatui::CreateTextButton(element, nullptr, nullptr);
  ASSERT_EQ(std::string("Required field \"size\" is missing for FlatUI element"
                        " with ID \"%s\"."),
            error_output);

  // Test with `texture` missing.
  EXPECT_CALL(flatui_common_mocks, TextButton(_, _, _)).Times(1);
  flatui::CreateTextButton(flatui_pointer->elements()->Get(2), nullptr,
                           nullptr);

  // Test with dynamic data for `texture`.
  flatui::RegisterStringData(missing_texture_id, &dummy_string);
  EXPECT_CALL(fplbase_mocks, FindTexture(_)).WillOnce(Return(&dummy_texture));
  EXPECT_CALL(flatui_common_mocks, TextButton(_, _, _, _, _, _)).Times(1);
  flatui::CreateTextButton(flatui_pointer->elements()->Get(2), &dummy_assetman,
                           nullptr);

  // Test with dynamic data for `size`.
  flatui::dynamic_data.erase(missing_texture_id);  // Reuse this ID.
  float dummy_float;
  flatui::RegisterFloatData(missing_texture_id, &dummy_float);
  EXPECT_CALL(flatui_common_mocks, TextButton(_, _, _)).Times(1);
  flatui::CreateTextButton(flatui_pointer->elements()->Get(2), nullptr,
                           nullptr);
}

TEST_F(FlatUISerializationTest, TestCreateLabel) {
  // Create a dummy FlatBuffer data for the test.
  flatbuffers::FlatBufferBuilder builder;
  std::string test_id("test create label ID");
  std::string missing_text_test_id("test create label w/ missing text ID");
  std::string missing_size_2f_test_id(
      "test create label w/ missing size_2f ID");
  auto id = builder.CreateString(test_id.c_str());
  auto missing_text_id = builder.CreateString(missing_text_test_id);
  auto missing_size_2f_id = builder.CreateString(missing_size_2f_test_id);
  fplbase::Vec2 test_vec2(0.0f, 0.0f);
  flatui_data::FlatUIElementBuilder element_builder(builder);
  element_builder.add_id(id);
  element_builder.add_text(id);
  element_builder.add_size_2f(&test_vec2);
  auto element_offset = element_builder.Finish();

  // Create a second dummy FlatBuffer that is missing the `text` field,
  // since it cannot be mutated.
  flatui_data::FlatUIElementBuilder missing_text_element_builder(builder);
  missing_text_element_builder.add_id(missing_text_id);
  missing_text_element_builder.add_ysize(1.0f);
  auto missing_text_element_offset = missing_text_element_builder.Finish();

  // Create a third dummy FlatBuffer that is missing the `size_2f` field.
  flatui_data::FlatUIElementBuilder missing_size_2f_element_builder(builder);
  missing_size_2f_element_builder.add_id(missing_size_2f_id);
  missing_size_2f_element_builder.add_text(missing_size_2f_id);
  missing_size_2f_element_builder.add_ysize(1.0f);
  auto missing_size_2f_element_offset =
      missing_size_2f_element_builder.Finish();

  std::vector<flatbuffers::Offset<flatui_data::FlatUIElement>> elements_vector;
  elements_vector.push_back(element_offset);
  elements_vector.push_back(missing_text_element_offset);
  elements_vector.push_back(missing_size_2f_element_offset);
  auto elements = builder.CreateVector(elements_vector);

  flatui_data::FlatUIBuilder flatui_builder(builder);
  flatui_builder.add_elements(elements);
  auto flatui_flatbuffer = flatui_builder.Finish();
  builder.Finish(flatui_flatbuffer);

  auto flatui_pointer = flatui_data::GetFlatUI(builder.GetBufferPointer());

  // Mock flatui::Label calls.
  FlatUIMocks& flatui_mocks = FlatUIMocks::get_mocks();

  // Mock `fplbase::LogError`.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillRepeatedly(SaveArg<0>(&error_output));

  // Test missing `ysize`.
  flatui::CreateLabel(flatui_pointer->elements()->Get(0));
  ASSERT_EQ(std::string("Required field \"ysize\" is missing for FlatUI element"
                        " with ID \"%s\"."),
            error_output);

  // Test with dynamic data for `ysize`.
  float dummy_float;
  flatui::RegisterFloatData(test_id, &dummy_float);
  EXPECT_CALL(flatui_mocks, Label(_, _, _)).Times(1);
  flatui::CreateLabel(flatui_pointer->elements()->Get(0));

  // Test missing text.
  flatui::CreateLabel(flatui_pointer->elements()->Get(1));
  ASSERT_EQ(std::string("Required field \"text\" is missing for FlatUI element"
                        " with ID \"%s\"."),
            error_output);

  // Test with dynamic data for `text`.
  std::string dummy_string;
  flatui::RegisterStringData(missing_text_test_id, &dummy_string);
  EXPECT_CALL(flatui_mocks, Label(_, _)).Times(1);
  flatui::CreateLabel(flatui_pointer->elements()->Get(1));

  // Test with missing `size_2f`.
  EXPECT_CALL(flatui_mocks, Label(_, _)).Times(1);
  flatui::CreateLabel(flatui_pointer->elements()->Get(2));

  // Test with dynamic data for `size_2f`.
  mathfu::vec2 dummy_vec2;
  flatui::RegisterVec2Data(missing_size_2f_test_id, &dummy_vec2);
  EXPECT_CALL(flatui_mocks, Label(_, _, _)).Times(1);
  flatui::CreateLabel(flatui_pointer->elements()->Get(2));
}

TEST_F(FlatUISerializationTest, TestCreateCheckBox) {
  // Create a dummy FlatBuffer data for the test.
  flatbuffers::FlatBufferBuilder builder;
  std::string test_id("test create check box ID");
  auto id = builder.CreateString(test_id.c_str());
  fplbase::Vec2 test_vec2(0.0f, 0.0f);
  flatui_data::FlatUIElementBuilder element_builder(builder);
  element_builder.add_id(id);
  element_builder.add_text(id);
  element_builder.add_texture(id);
  element_builder.add_texture_secondary(id);
  element_builder.add_size(1.0f);
  auto element_offset = element_builder.Finish();

  std::vector<flatbuffers::Offset<flatui_data::FlatUIElement>> elements_vector;
  elements_vector.push_back(element_offset);
  auto elements = builder.CreateVector(elements_vector);

  flatui_data::FlatUIBuilder flatui_builder(builder);
  flatui_builder.add_elements(elements);
  auto flatui_flatbuffer = flatui_builder.Finish();
  builder.Finish(flatui_flatbuffer);

  auto flatui_pointer = flatui_data::GetFlatUI(builder.GetBufferPointer());

  // Mock `fplbase::LogError` and `fplbase::FindTexture`.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Create a dummy `fplbase::AssetManager` to satisfy any nullptr checks.
  fplbase::Renderer dummy_renderer;
  fplbase::AssetManager dummy_assetman(dummy_renderer);

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillRepeatedly(SaveArg<0>(&error_output));

  // Test missing dynamic data.
  flatui::CreateCheckBox(flatui_pointer->elements()->Get(0), &dummy_assetman,
                         nullptr);
  ASSERT_EQ(std::string("\"CheckBox\" with ID \"%s\" requires a bool dynamic"
                        " data to be registered with it."),
            error_output);

  // Test valid data.
  bool dummy_bool;
  flatui::RegisterBoolData(test_id, &dummy_bool);

  fplbase::Texture dummy_texture;
  EXPECT_CALL(fplbase_mocks, FindTexture(_))
      .WillOnce(Return(&dummy_texture))
      .WillOnce(Return(&dummy_texture));

  flatui::FlatUICommonMocks& flatui_common_mocks =
      flatui::FlatUICommonMocks::get_mocks();
  EXPECT_CALL(flatui_common_mocks, CheckBox(_, _, _, _, _, _))
      .WillOnce(Return(flatui::kEventWentUp));

  flatui::CreateCheckBox(flatui_pointer->elements()->Get(0), &dummy_assetman,
                         [&](const flatui::Event& e, const std::string& id,
                             flatui::DynamicData* dynamic_data) {
                           ASSERT_EQ(flatui::kEventWentUp, e);
                           ASSERT_EQ(&dummy_bool, dynamic_data->data.bool_data);
                           ASSERT_EQ(test_id, id);
                         });
}

TEST_F(FlatUISerializationTest, TestCreateScrollBar) {
  // Create a dummy FlatBuffer data for the test.
  flatbuffers::FlatBufferBuilder builder;
  std::string test_id("test create scroll bar ID");
  auto id = builder.CreateString(test_id.c_str());
  fplbase::Vec2 test_vec2(0.0f, 0.0f);
  flatui_data::FlatUIElementBuilder element_builder(builder);
  element_builder.add_id(id);
  element_builder.add_texture(id);
  element_builder.add_texture_secondary(id);
  element_builder.add_ysize(1.0f);
  element_builder.add_size_2f(&test_vec2);
  auto element_offset = element_builder.Finish();

  std::vector<flatbuffers::Offset<flatui_data::FlatUIElement>> elements_vector;
  elements_vector.push_back(element_offset);
  auto elements = builder.CreateVector(elements_vector);

  flatui_data::FlatUIBuilder flatui_builder(builder);
  flatui_builder.add_elements(elements);
  auto flatui_flatbuffer = flatui_builder.Finish();
  builder.Finish(flatui_flatbuffer);

  auto flatui_pointer = flatui_data::GetFlatUI(builder.GetBufferPointer());

  // Mock `fplbase::LogError` and `fplbase::FindTexture`.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Create a dummy `fplbase::AssetManager` to satisfy any nullptr checks.
  fplbase::Renderer dummy_renderer;
  fplbase::AssetManager dummy_assetman(dummy_renderer);

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillRepeatedly(SaveArg<0>(&error_output));

  // Test missing dynamic data.
  flatui::CreateScrollBar(flatui_pointer->elements()->Get(0), &dummy_assetman,
                          nullptr);
  ASSERT_EQ(std::string("\"ScrollBar\" with ID \"%s\" requires a float dynamic"
                        " data to be registered with it."),
            error_output);

  // Test valid data.
  float dummy_float;
  flatui::RegisterFloatData(test_id, &dummy_float);

  fplbase::Texture dummy_texture;
  EXPECT_CALL(fplbase_mocks, FindTexture(_))
      .WillOnce(Return(&dummy_texture))
      .WillOnce(Return(&dummy_texture));

  flatui::FlatUICommonMocks& flatui_common_mocks =
      flatui::FlatUICommonMocks::get_mocks();
  EXPECT_CALL(flatui_common_mocks, ScrollBar(_, _, _, _, _, _))
      .WillOnce(Return(flatui::kEventWentUp));

  flatui::CreateScrollBar(flatui_pointer->elements()->Get(0), &dummy_assetman,
                          [&](const flatui::Event& e, const std::string& id,
                              flatui::DynamicData* dynamic_data) {
                            ASSERT_EQ(flatui::kEventWentUp, e);
                            ASSERT_EQ(&dummy_float,
                                      dynamic_data->data.float_data);
                            ASSERT_EQ(test_id, id);
                          });
}

TEST_F(FlatUISerializationTest, TestCreateSlider) {
  // Create a dummy FlatBuffer data for the test.
  flatbuffers::FlatBufferBuilder builder;
  std::string test_id("test create slider ID");
  auto id = builder.CreateString(test_id.c_str());
  fplbase::Vec2 test_vec2(0.0f, 0.0f);
  flatui_data::FlatUIElementBuilder element_builder(builder);
  element_builder.add_id(id);
  element_builder.add_texture(id);
  element_builder.add_texture_secondary(id);
  element_builder.add_ysize(1.0f);
  element_builder.add_size_2f(&test_vec2);
  auto element_offset = element_builder.Finish();

  std::vector<flatbuffers::Offset<flatui_data::FlatUIElement>> elements_vector;
  elements_vector.push_back(element_offset);
  auto elements = builder.CreateVector(elements_vector);

  flatui_data::FlatUIBuilder flatui_builder(builder);
  flatui_builder.add_elements(elements);
  auto flatui_flatbuffer = flatui_builder.Finish();
  builder.Finish(flatui_flatbuffer);

  auto flatui_pointer = flatui_data::GetFlatUI(builder.GetBufferPointer());

  // Mock `fplbase::LogError` and `fplbase::FindTexture`.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Create a dummy `fplbase::AssetManager` to satisfy any nullptr checks.
  fplbase::Renderer dummy_renderer;
  fplbase::AssetManager dummy_assetman(dummy_renderer);

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillRepeatedly(SaveArg<0>(&error_output));

  // Test missing dynamic data.
  flatui::CreateSlider(flatui_pointer->elements()->Get(0), &dummy_assetman,
                       nullptr);
  ASSERT_EQ(std::string("\"Slider\" with ID \"%s\" requires a float dynamic"
                        " data to be registered with it."),
            error_output);

  // Test valid data.
  float dummy_float;
  flatui::RegisterFloatData(test_id, &dummy_float);

  fplbase::Texture dummy_texture;
  EXPECT_CALL(fplbase_mocks, FindTexture(_))
      .WillOnce(Return(&dummy_texture))
      .WillOnce(Return(&dummy_texture));

  flatui::FlatUICommonMocks& flatui_common_mocks =
      flatui::FlatUICommonMocks::get_mocks();
  EXPECT_CALL(flatui_common_mocks, Slider(_, _, _, _, _, _))
      .WillOnce(Return(flatui::kEventWentUp));

  flatui::CreateSlider(flatui_pointer->elements()->Get(0), &dummy_assetman,
                       [&](const flatui::Event& e, const std::string& id,
                           flatui::DynamicData* dynamic_data) {
                         ASSERT_EQ(flatui::kEventWentUp, e);
                         ASSERT_EQ(&dummy_float, dynamic_data->data.float_data);
                         ASSERT_EQ(test_id, id);
                       });
}

TEST_F(FlatUISerializationTest, TestCreateImage) {
  // Create a dummy FlatBuffer data for the test.
  flatbuffers::FlatBufferBuilder builder;
  auto id = builder.CreateString("test create image ID");
  auto missing_texture_id =
      builder.CreateString("test create image w/ missing texture ID");
  flatui_data::FlatUIElementBuilder element_builder(builder);
  element_builder.add_id(id);
  element_builder.add_texture(id);
  element_builder.add_ysize(1.0f);
  auto element_offset = element_builder.Finish();

  flatui_data::FlatUIElementBuilder missing_texture_builder(builder);
  missing_texture_builder.add_id(missing_texture_id);
  missing_texture_builder.add_ysize(1.0f);
  auto missing_texture_element_offset = missing_texture_builder.Finish();

  std::vector<flatbuffers::Offset<flatui_data::FlatUIElement>> elements_vector;
  elements_vector.push_back(element_offset);
  elements_vector.push_back(missing_texture_element_offset);
  auto elements = builder.CreateVector(elements_vector);

  flatui_data::FlatUIBuilder flatui_builder(builder);
  flatui_builder.add_elements(elements);
  auto flatui_flatbuffer = flatui_builder.Finish();
  builder.Finish(flatui_flatbuffer);

  auto flatui_pointer = flatui_data::GetFlatUI(builder.GetBufferPointer());

  // Mock `fplbase::LogError` and `fplbase::FindTexture`.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Create a dummy `fplbase::AssetManager` to satisfy any nullptr checks.
  fplbase::Renderer dummy_renderer;
  fplbase::AssetManager dummy_assetman(dummy_renderer);

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;
  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillRepeatedly(SaveArg<0>(&error_output));

  // Test valid data.
  fplbase::Texture dummy_texture;
  EXPECT_CALL(fplbase_mocks, FindTexture(_)).WillOnce(Return(&dummy_texture));

  flatui::FlatUIMocks& flatui_mocks = flatui::FlatUIMocks::get_mocks();
  EXPECT_CALL(flatui_mocks, Image(_, _)).Times(1);

  flatui::CreateImage(flatui_pointer->elements()->Get(0), &dummy_assetman);

  // Test registering with dynamic data for `ysize`.
  auto element = const_cast<flatui_data::FlatUIElement*>(
      flatui_pointer->elements()->Get(0));

  element->mutate_ysize(-1.0f);
  EXPECT_CALL(fplbase_mocks, FindTexture(_)).WillOnce(Return(&dummy_texture));

  flatui::CreateImage(element, &dummy_assetman);
  ASSERT_EQ(
      "Required field \"ysize\" is missing for FlatUI element with"
      " ID \"%s\".",
      error_output);

  float dummy_float;
  flatui::RegisterFloatData(std::string("test create image ID"), &dummy_float);
  EXPECT_CALL(fplbase_mocks, FindTexture(_)).WillOnce(Return(&dummy_texture));
  EXPECT_CALL(flatui_mocks, Image(_, _)).Times(1);

  flatui::CreateImage(flatui_pointer->elements()->Get(0), &dummy_assetman);

  // Test registering with dynamic data for `texture`.
  flatui::CreateImage(flatui_pointer->elements()->Get(1), &dummy_assetman);
  ASSERT_EQ(
      "Required field \"texture\" is missing for FlatUI element with"
      " ID \"%s\".",
      error_output);

  std::string dummy_string;
  flatui::RegisterStringData(
      std::string("test create image w/ missing texture ID"), &dummy_string);
  EXPECT_CALL(flatui_mocks, Image(_, _)).Times(1);
  EXPECT_CALL(fplbase_mocks, FindTexture(_)).WillOnce(Return(&dummy_texture));

  flatui::CreateImage(flatui_pointer->elements()->Get(1), &dummy_assetman);
}

TEST_F(FlatUISerializationTest, TestCreateEdit) {
  // Create a dummy FlatBuffer data for the test.
  flatbuffers::FlatBufferBuilder builder;
  std::string test_id("test create edit ID");
  auto id = builder.CreateString(test_id.c_str());
  fplbase::Vec2 test_vec2(0.0f, 0.0f);
  flatui_data::FlatUIElementBuilder element_builder(builder);
  element_builder.add_id(id);
  element_builder.add_ysize(1);
  element_builder.add_size_2f(&test_vec2);
  element_builder.add_type(flatui_data::Type_Edit);
  auto element_offset = element_builder.Finish();

  std::vector<flatbuffers::Offset<flatui_data::FlatUIElement>> elements_vector;
  elements_vector.push_back(element_offset);
  auto elements = builder.CreateVector(elements_vector);

  flatui_data::FlatUIBuilder flatui_builder(builder);
  flatui_builder.add_elements(elements);
  auto flatui_flatbuffer = flatui_builder.Finish();
  builder.Finish(flatui_flatbuffer);

  auto flatui_pointer = flatui_data::GetFlatUI(builder.GetBufferPointer());

  // Mock the `fplbase::LogError` function to assert it is called.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillOnce(SaveArg<0>(&error_output));

  // Test missing dynamic data.
  flatui::CreateEdit(flatui_pointer->elements()->Get(0), nullptr);
  ASSERT_EQ(std::string("\"Edit\" with ID \"%s\" requires a string dynamic data"
                        " to be registered with it."),
            error_output);

  // Test valid data.
  std::string str("my data");
  flatui::RegisterStringData(test_id, &str);

  // Mock calls to `flatui::Edit`.
  //
  flatui::FlatUIMocks& flatui_mocks = flatui::FlatUIMocks::get_mocks();
  EXPECT_CALL(flatui_mocks, Edit(_, _, _, _, _))
      .WillOnce(Return(flatui::kEventNone))
      .WillOnce(Return(flatui::kEventNone));

  flatui::CreateEdit(
      flatui_pointer->elements()->Get(0),
      [](const flatui::Event&, const std::string&, flatui::DynamicData*) {});

  flatui::CreateEdit(flatui_pointer->elements()->Get(0),
                     [&](const flatui::Event& e, const std::string& id,
                         flatui::DynamicData* dynamic_data) {
                       ASSERT_EQ(flatui::kEventNone, e);
                       ASSERT_EQ(&str, dynamic_data->data.string_data);
                       ASSERT_EQ(test_id, id);
                     });
}

TEST_F(FlatUISerializationTest, TestCreateCustomWidget) {
  // Create a fake FlatUIElement flatbuffer.
  flatbuffers::FlatBufferBuilder builder;
  std::string test_id("test create custom widget ID");
  auto element_id = builder.CreateString(test_id.c_str());

  flatui_data::FlatUIElementBuilder element_builder(builder);
  element_builder.add_type(10);
  element_builder.add_id(element_id);
  auto element_offset = element_builder.Finish();

  std::vector<flatbuffers::Offset<flatui_data::FlatUIElement>> elements_vector;
  elements_vector.push_back(element_offset);
  auto elements = builder.CreateVector(elements_vector);

  flatui_data::FlatUIBuilder flatui_builder(builder);
  flatui_builder.add_elements(elements);
  auto flatui_flatbuffer = flatui_builder.Finish();

  builder.Finish(flatui_flatbuffer);
  auto flatui_pointer = flatui_data::GetFlatUI(builder.GetBufferPointer());

  // Mock the `fplbase::LogError` function to assert it is called.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillRepeatedly(SaveArg<0>(&error_output));

  flatui::CreateCustomWidget(flatui_pointer->elements()->Get(0), nullptr,
                             nullptr);

  ASSERT_EQ(std::string("Custom widget with type \"%u\" was not registered."),
            error_output);

  std::string test_string("this is a test string");

  flatui::CustomWidget dummy_widget = [&](
      const flatui_data::FlatUIElement* ele, fplbase::AssetManager* assetman,
      flatui::FlatUIHandler error_handler, flatui::DynamicData* dynamic_data) {
    ASSERT_EQ(test_id, ele->id()->str());
    ASSERT_EQ(nullptr, assetman);
    ASSERT_EQ(nullptr, error_handler);
    ASSERT_EQ(&test_string, dynamic_data->data.string_data);
  };

  flatui::RegisterCustomWidget(10, dummy_widget);
  flatui::RegisterStringData(test_id, &test_string);

  flatui::CreateCustomWidget(flatui_pointer->elements()->Get(0), nullptr,
                             nullptr);
}

TEST_F(FlatUISerializationTest, TestRegisterCustomWidget) {
  // Mock the `fplbase::LogError` function to assert it is called.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillRepeatedly(SaveArg<0>(&error_output));

  // Test invalid types.
  uint32_t reserved_value =
      static_cast<uint32_t>(flatui_data::Type_ReservedDefaultTypes);
  for (uint32_t i = reserved_value; i < reserved_value + 10; i++) {
    flatui::RegisterCustomWidget(i, nullptr);
    ASSERT_EQ(
        "Custom widget with type \"%u\" must be less than the reserved"
        " \"%u\" widgets.",
        error_output);
  }

  // Test invalid widget creation functions.
  flatui::RegisterCustomWidget(1, nullptr);
  ASSERT_EQ(
      "Custom widget with type \"%u\" must have a callable"
      " \"CustomWidget\" function.",
      error_output);

  // Test register custom widget with a dummy function.
  flatui::CustomWidget dummy_widget = [](
      const flatui_data::FlatUIElement*, fplbase::AssetManager*,
      flatui::FlatUIHandler, flatui::DynamicData*) {};

  uint32_t dummy_id = 2;
  flatui::RegisterCustomWidget(dummy_id, dummy_widget);
  ASSERT_NE(nullptr, flatui::custom_elements[dummy_id]);
}

// Test that the correct required elements are returned during error checking.
TEST_F(FlatUISerializationTest, TestRequiredFields) {
  // Create a fake FlatBuffer data for the tests.
  flatbuffers::FlatBufferBuilder builder;
  auto id = builder.CreateString("dummy ID");
  flatui_data::FlatUIElementBuilder element_builder(builder);
  element_builder.add_id(id);
  element_builder.add_type(flatui_data::Type_Edit);
  auto element_offset = element_builder.Finish();

  std::vector<flatbuffers::Offset<flatui_data::FlatUIElement>> elements_vector;
  elements_vector.push_back(element_offset);
  auto elements = builder.CreateVector(elements_vector);

  flatui_data::FlatUIBuilder flatui_builder(builder);
  flatui_builder.add_elements(elements);
  auto flatui_flatbuffer = flatui_builder.Finish();
  builder.Finish(flatui_flatbuffer);

  auto flatui_pointer = flatui_data::GetFlatUI(builder.GetBufferPointer());
  FlatUIElement* element =
      const_cast<FlatUIElement*>(flatui_pointer->elements()->Get(0));

  // Edit
  ASSERT_EQ(
      flatui::kYSizeBm | flatui::kSize_2fBm | flatui::kRequiredDynamicData,
      flatui::RequiredFields(element));

  // Group
  element->mutate_type(flatui_data::Type_Group);
  ASSERT_EQ(flatui::kLayoutBm, flatui::RequiredFields(element));

  // Image
  element->mutate_type(flatui_data::Type_Image);
  ASSERT_EQ(flatui::kTextureBm | flatui::kYSizeBm,
            flatui::RequiredFields(element));

  // ImageButton
  element->mutate_type(flatui_data::Type_ImageButton);
  ASSERT_EQ(flatui::kTextureBm | flatui::kSizeBm,
            flatui::RequiredFields(element));

  // Label
  element->mutate_type(flatui_data::Type_Label);
  ASSERT_EQ(flatui::kTextBm | flatui::kYSizeBm,
            flatui::RequiredFields(element));

  // ScrollBar
  element->mutate_type(flatui_data::Type_ScrollBar);
  ASSERT_EQ(flatui::kTextureBm | flatui::kTextureSecondaryBm |
                flatui::kSize_2fBm | flatui::kBarSizeBm |
                flatui::kRequiredDynamicData,
            flatui::RequiredFields(element));

  // SetVirtualResolution
  element->mutate_type(flatui_data::Type_SetVirtualResolution);
  ASSERT_EQ(flatui::kVirtualResolutionBm, flatui::RequiredFields(element));

  // Slider
  element->mutate_type(flatui_data::Type_Slider);
  ASSERT_EQ(flatui::kTextureBm | flatui::kTextureSecondaryBm |
                flatui::kSize_2fBm | flatui::kBarSizeBm |
                flatui::kRequiredDynamicData,
            flatui::RequiredFields(element));

  // TextButton
  element->mutate_type(flatui_data::Type_TextButton);
  ASSERT_EQ(flatui::kTextBm | flatui::kYSizeBm,
            flatui::RequiredFields(element));

  // Default (only check if they have a valid type)
  element->mutate_type(flatui_data::Type_InvalidType);
  ASSERT_EQ(flatui::kTypeBm, flatui::RequiredFields(element));
}

TEST_F(FlatUISerializationTest, TestGetTextureFromAssetManager) {
  // Mock the `fplbase::LogError` function and `fplbase::FindTexture`.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  fplbase::Renderer dummy_renderer;
  fplbase::AssetManager dummy_assetman(dummy_renderer);
  fplbase::Texture dummy_texture;

  EXPECT_CALL(fplbase_mocks, FindTexture(_))
      .WillOnce(Return(nullptr))
      .WillOnce(Return(&dummy_texture));

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillOnce(SaveArg<0>(&error_output));

  // Test error message.
  bool ret_val = flatui::GetTextureFromAssetManager(
      "dummy name", "dummy ID", &dummy_assetman, &dummy_texture);
  ASSERT_FALSE(ret_val);
  ASSERT_EQ("Texture \"%s\" cannot be found for FlatUI widget with ID \"%s\".",
            error_output);

  // Test valid Texture.
  ret_val = flatui::GetTextureFromAssetManager("dummy name", "dummy ID",
                                               &dummy_assetman, &dummy_texture);
  ASSERT_TRUE(ret_val);
}

TEST_F(FlatUISerializationTest, TestHasValidAssetManager) {
  // Mock the `fplbase::LogError` function to assert it is called.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillOnce(SaveArg<0>(&error_output));

  // Test error message.
  bool ret_val = flatui::HasValidAssetManager(nullptr, "dummy ID");
  ASSERT_EQ(
      "\"AssetManager\", required by widget with ID \"%s\","
      " is nullptr.",
      error_output);
  ASSERT_FALSE(ret_val);

  // Test valid AssetManager.
  fplbase::Renderer dummy_renderer;
  fplbase::AssetManager dummy_assetman(dummy_renderer);
  ret_val = flatui::HasValidAssetManager(&dummy_assetman, "dummy ID");
  ASSERT_TRUE(ret_val);
}

TEST_F(FlatUISerializationTest, TestMissingFieldError) {
  // Mock the `fplbase::LogError` function to assert it is called.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillOnce(SaveArg<0>(&error_output))
      .WillOnce(SaveArg<0>(&error_output));

  flatui::MissingFieldError("fake_field", "fake_id", false);
  ASSERT_EQ(
      "Required field \"fake_field\" is missing for FlatUI element with"
      " ID \"%s\".",
      error_output);

  flatui::MissingFieldError("fake_field", "fake_id", true);
  ASSERT_EQ(
      "Required field \"fake_field\" is missing for FlatUI element with"
      " ID \"%s\". (Dynamic data cannot be used for this field.)",
      error_output);
}

// Test that the switch statement calls the appropriate function type.
TEST_F(FlatUISerializationTest, TestMapElement) {
  // Create fake FlatBuffer data for the tests.
  // We will need a buffer with a unique ID for each dynamic data type,
  // so that we can register dummy dat a without colissions. Also add
  // dummy fields to fulfill all the "required elements" checks.
  flatbuffers::FlatBufferBuilder string_data_builder;
  std::string string_data_id("test map element w/ string data ID");
  auto id = string_data_builder.CreateString(string_data_id);
  fplbase::Vec2 test_vec2(0.0f, 0.0f);
  flatui_data::FlatUIElementBuilder string_data_element_builder(
      string_data_builder);
  string_data_element_builder.add_id(id);
  string_data_element_builder.add_layout(flatui_data::Layout_Overlay);
  string_data_element_builder.add_text(id);
  string_data_element_builder.add_texture(id);
  string_data_element_builder.add_type(flatui_data::Type_Edit);
  string_data_element_builder.add_size(1.0f);
  string_data_element_builder.add_size_2f(&test_vec2);
  string_data_element_builder.add_virtual_resolution(1.0f);
  string_data_element_builder.add_ysize(1.0f);
  auto element_offset = string_data_element_builder.Finish();

  std::vector<flatbuffers::Offset<flatui_data::FlatUIElement>> elements_vector;
  elements_vector.push_back(element_offset);
  auto elements = string_data_builder.CreateVector(elements_vector);

  flatui_data::FlatUIBuilder flatui_string_data_builder(string_data_builder);
  flatui_string_data_builder.add_elements(elements);
  auto flatui_flatbuffer = flatui_string_data_builder.Finish();
  string_data_builder.Finish(flatui_flatbuffer);

  auto flatui_pointer =
      flatui_data::GetFlatUI(string_data_builder.GetBufferPointer());
  FlatUIElement* string_data_element =
      const_cast<FlatUIElement*>(flatui_pointer->elements()->Get(0));

  // Dummy FlatBuffer with `float` data.
  flatbuffers::FlatBufferBuilder float_data_builder;
  std::string float_data_id("test map element w/ float data ID");
  id = float_data_builder.CreateString(float_data_id);
  flatui_data::FlatUIElementBuilder float_data_element_builder(
      float_data_builder);
  float_data_element_builder.add_bar_size(1.0f);
  float_data_element_builder.add_id(id);
  float_data_element_builder.add_texture(id);
  float_data_element_builder.add_texture_secondary(id);
  float_data_element_builder.add_type(flatui_data::Type_Slider);
  float_data_element_builder.add_size_2f(&test_vec2);
  element_offset = float_data_element_builder.Finish();

  elements_vector.clear();
  elements_vector.push_back(element_offset);
  elements = float_data_builder.CreateVector(elements_vector);

  flatui_data::FlatUIBuilder flatui_float_data_builder(float_data_builder);
  flatui_float_data_builder.add_elements(elements);
  flatui_flatbuffer = flatui_float_data_builder.Finish();
  float_data_builder.Finish(flatui_flatbuffer);

  flatui_pointer =
      flatui_data::GetFlatUI(float_data_builder.GetBufferPointer());
  FlatUIElement* float_data_element =
      const_cast<FlatUIElement*>(flatui_pointer->elements()->Get(0));

  // Dummy FlatBuffer with `float` data.
  flatbuffers::FlatBufferBuilder bool_data_builder;
  std::string bool_data_id("test map element w/ bool data ID");
  id = bool_data_builder.CreateString(bool_data_id);
  flatui_data::FlatUIElementBuilder bool_data_element_builder(
      bool_data_builder);
  bool_data_element_builder.add_id(id);
  bool_data_element_builder.add_text(id);
  bool_data_element_builder.add_texture(id);
  bool_data_element_builder.add_texture_secondary(id);
  bool_data_element_builder.add_type(flatui_data::Type_ScrollBar);
  bool_data_element_builder.add_size(1.0f);
  element_offset = bool_data_element_builder.Finish();

  elements_vector.clear();
  elements_vector.push_back(element_offset);
  elements = bool_data_builder.CreateVector(elements_vector);

  flatui_data::FlatUIBuilder flatui_bool_data_builder(bool_data_builder);
  flatui_bool_data_builder.add_elements(elements);
  flatui_flatbuffer = flatui_bool_data_builder.Finish();
  bool_data_builder.Finish(flatui_flatbuffer);

  flatui_pointer = flatui_data::GetFlatUI(bool_data_builder.GetBufferPointer());
  FlatUIElement* bool_data_element =
      const_cast<FlatUIElement*>(flatui_pointer->elements()->Get(0));

  // Register dummy data to satisfy the checks.
  std::string dummy_string;
  flatui::RegisterStringData(string_data_id, &dummy_string);
  float dummy_float;
  flatui::RegisterFloatData(float_data_id, &dummy_float);
  bool dummy_bool;
  flatui::RegisterBoolData(bool_data_id, &dummy_bool);

  // Get access to the fplbase mocks.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Mock the fplbase::FindTexture function to return a valid pointer.
  fplbase::Texture dummy_texture;
  EXPECT_CALL(fplbase_mocks, FindTexture(_))
      .WillRepeatedly(Return(&dummy_texture));

  // Get access to the FlatUI mocks.
  FlatUIMocks& flatui_mocks = FlatUIMocks::get_mocks();
  FlatUICommonMocks& flatui_common_mocks = FlatUICommonMocks::get_mocks();

  // Create a dummy `fplbase::AssetManager` to satisfy any nullptr checks.
  fplbase::Renderer dummy_renderer;
  fplbase::AssetManager dummy_assetman(dummy_renderer);

  // CheckBox
  bool_data_element->mutate_type(flatui_data::Type_CheckBox);
  EXPECT_CALL(flatui_common_mocks, CheckBox(_, _, _, _, _, _)).Times(1);
  flatui::MapElement(bool_data_element, &dummy_assetman, nullptr);

  // Edit
  string_data_element->mutate_type(flatui_data::Type_Edit);
  EXPECT_CALL(flatui_mocks, Edit(_, _, _, _, _)).Times(1);
  flatui::MapElement(string_data_element, nullptr, nullptr);

  // Group
  string_data_element->mutate_type(flatui_data::Type_Group);
  EXPECT_CALL(flatui_mocks, StartGroup(_, _, _)).Times(1);
  EXPECT_CALL(flatui_mocks, EndGroup()).Times(1);
  flatui::MapElement(string_data_element, nullptr, nullptr);

  // Image
  string_data_element->mutate_type(flatui_data::Type_Image);
  EXPECT_CALL(flatui_mocks, Image(_, _)).Times(1);
  flatui::MapElement(string_data_element, &dummy_assetman, nullptr);

  // ImageButton
  string_data_element->mutate_type(flatui_data::Type_ImageButton);
  EXPECT_CALL(flatui_common_mocks, ImageButton(_, _, _, _)).Times(1);
  flatui::MapElement(string_data_element, &dummy_assetman, nullptr);

  // Label
  string_data_element->mutate_type(flatui_data::Type_Label);
  EXPECT_CALL(flatui_mocks, Label(_, _, _)).Times(1);
  flatui::MapElement(string_data_element, nullptr, nullptr);

  // ScrollBar
  float_data_element->mutate_type(flatui_data::Type_ScrollBar);
  EXPECT_CALL(flatui_common_mocks, ScrollBar(_, _, _, _, _, _)).Times(1);
  flatui::MapElement(float_data_element, &dummy_assetman, nullptr);

  // SetVirtualResolution
  string_data_element->mutate_type(flatui_data::Type_SetVirtualResolution);
  EXPECT_CALL(flatui_mocks, SetVirtualResolution(_)).Times(1);
  flatui::MapElement(string_data_element, nullptr, nullptr);

  // Slider
  float_data_element->mutate_type(flatui_data::Type_Slider);
  EXPECT_CALL(flatui_common_mocks, Slider(_, _, _, _, _, _)).Times(1);
  flatui::MapElement(float_data_element, &dummy_assetman, nullptr);

  // TextButton
  string_data_element->mutate_type(flatui_data::Type_TextButton);
  EXPECT_CALL(flatui_common_mocks, TextButton(_, _, _, _, _, _)).Times(1);
  flatui::MapElement(string_data_element, &dummy_assetman, nullptr);

  // Default (custom widget)
  int test_default_type = 10;
  flatui::CustomWidget dummy_widget = [&](
      const flatui_data::FlatUIElement* ele, fplbase::AssetManager* assetman,
      flatui::FlatUIHandler error_handler, flatui::DynamicData* dynamic_data) {
    ASSERT_EQ(string_data_id, ele->id()->str());
    ASSERT_EQ(nullptr, assetman);
    ASSERT_EQ(nullptr, error_handler);
    ASSERT_EQ(&dummy_string, dynamic_data->data.string_data);
  };
  flatui::RegisterCustomWidget(test_default_type, dummy_widget);

  string_data_element->mutate_type(test_default_type);
  flatui::MapElement(string_data_element, nullptr, nullptr);
}

// Since this is basically just a wrapper function, we only need to
// test invalid input, since the other functions have their own tests.
TEST_F(FlatUISerializationTest, TestCreateFlatUIFromData) {
  // Mock the `fplbase::LogError` function to assert it is called.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillRepeatedly(SaveArg<0>(&error_output));

  flatui::CreateFlatUIFromData(nullptr, nullptr, nullptr);
  ASSERT_EQ(
      "\"CreateFlatUIFromData\" requires that \"flatui_data\" is not a"
      " \"nullptr\".",
      error_output);

  // Create a fake FlatBuffer data that is missing "elements".
  flatbuffers::FlatBufferBuilder builder;
  flatui_data::FlatUIBuilder flatui_builder(builder);
  auto flatui_flatbuffer = flatui_builder.Finish();
  builder.Finish(flatui_flatbuffer);

  flatui::CreateFlatUIFromData(builder.GetBufferPointer(), nullptr, nullptr);
  ASSERT_EQ(
      "Required field \"elements\" is missing, or empty, for \"FlatUI\""
      " root table.",
      error_output);

  // Create a fake FlatBuffer data with an elements vector of size 0.
  flatbuffers::FlatBufferBuilder builder2;

  std::vector<flatbuffers::Offset<flatui_data::FlatUIElement>> elements_vector;
  auto elements = builder2.CreateVector(elements_vector);

  flatui_data::FlatUIBuilder flatui_builder2(builder2);
  flatui_builder2.add_elements(elements);
  flatui_flatbuffer = flatui_builder2.Finish();
  builder2.Finish(flatui_flatbuffer);

  ASSERT_EQ(0u, GetFlatUI(builder2.GetBufferPointer())->elements()->Length());

  flatui::CreateFlatUIFromData(builder2.GetBufferPointer(), nullptr, nullptr);
  ASSERT_EQ(
      "Required field \"elements\" is missing, or empty, for \"FlatUI\""
      " root table.",
      error_output);
}

TEST_F(FlatUISerializationTest, TestHasDynamicData) {
  bool result = flatui::HasDynamicData("This should be a fake ID",
                                       flatui::DynamicData::kIntData);
  ASSERT_FALSE(result);  // Not found.

  // Register a sample int.
  int test_int;
  flatui::RegisterIntData("HasDynamicData test ID", &test_int);
  result = flatui::HasDynamicData("HasDynamicData test ID",
                                  flatui::DynamicData::kFloatData);
  ASSERT_FALSE(result);  // Found but wrong type.

  result = flatui::HasDynamicData("HasDynamicData test ID",
                                  flatui::DynamicData::kIntData);
  ASSERT_TRUE(result);  // Valid.
}

// Check that error handling works for each missing element field.
TEST_F(FlatUISerializationTest, TestCheckElements) {
  // Mock the `fplbase::LogError` function to assert it is called.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillRepeatedly(SaveArg<0>(&error_output));

  // Create the fake FlatBuffer data.
  flatbuffers::FlatBufferBuilder builder;
  std::string test_id("check elements w/ dynamic data ID");
  auto id = builder.CreateString(test_id.c_str());

  flatui_data::FlatUIElementBuilder element_builder(builder);
  element_builder.add_id(id);

  std::vector<flatbuffers::Offset<flatui_data::FlatUIElement>> elements_vector;
  elements_vector.push_back(element_builder.Finish());
  auto elements = builder.CreateVector(elements_vector);

  flatui_data::FlatUIBuilder flatui_builder(builder);
  flatui_builder.add_elements(elements);
  builder.Finish(flatui_builder.Finish());

  auto flatui_pointer = flatui_data::GetFlatUI(builder.GetBufferPointer());
  auto element = flatui_pointer->elements()->Get(0);

  // Inputs
  uint32_t fields[] = {flatui::kBarSizeBm,
                       flatui::kLayoutBm,
                       flatui::kSizeBm,
                       flatui::kSize_2fBm,
                       flatui::kTextBm,
                       flatui::kTextureBm,
                       flatui::kTextureSecondaryBm,
                       flatui::kTypeBm,
                       flatui::kVirtualResolutionBm,
                       flatui::kYSizeBm};

  // Expected output strings
  const char* outputs[] = {"bar_size",
                           "layout",
                           "size",
                           "size_2f",
                           "text",
                           "texture",
                           "texture_secondary",
                           "type",
                           "virtual_resolution",
                           "ysize"};

  for (unsigned int i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
    bool ret_val = flatui::CheckElements(element, fields[i]);
    ASSERT_FALSE(ret_val);
    std::string expected_output("Required field \"");
    expected_output.append(outputs[i]);
    expected_output.append("\" is missing for FlatUI element with ID \"%s\".");
    ASSERT_EQ(std::string(expected_output), error_output);
  }

  // Check that having some dynamic data registered can negate an error.
  float test_float;
  flatui::RegisterFloatData(test_id, &test_float);
  bool ret_val = flatui::CheckElements(element, flatui::kSizeBm);
  ASSERT_TRUE(ret_val);

  ret_val =
      flatui::CheckElements(element, flatui::kBarSizeBm | flatui::kSizeBm);
  ASSERT_FALSE(ret_val);
  ASSERT_EQ(std::string("\"%d\" required fields are missing for element ID \"%s"
                        "\". You may only have one piece of dynamic data per"
                        " element."),
            error_output);

  // Check that an error still occurs with a required dynamic data field.
  ret_val = flatui::CheckElements(
      element, flatui::kSizeBm | flatui::kRequiredDynamicData);
  ASSERT_FALSE(ret_val);
  ASSERT_EQ(std::string(
                "Required field \"size\" is missing for FlatUI"
                " element with ID \"%s\". (Dynamic data cannot be used for this"
                " field.)"),
            error_output);
}

// Test Register Data functions.
TEST_F(FlatUISerializationTest, TestRegisterIntData) {
  int test_int;
  std::string test_id("my test int ID");
  flatui::RegisterIntData(test_id, &test_int);
  ASSERT_EQ(flatui::dynamic_data[test_id].type, flatui::DynamicData::kIntData);
  ASSERT_EQ(&test_int, flatui::dynamic_data[test_id].data.int_data);

  // Test duplicate key errors.
  //
  // Mock the `fplbase::LogError` function to assert it is called.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillOnce(SaveArg<0>(&error_output));

  flatui::RegisterIntData(test_id, &test_int);
  ASSERT_EQ(std::string("Duplicate key \"%s\" used for \"%s\"."), error_output);
}

TEST_F(FlatUISerializationTest, TestRegisterFloatData) {
  float test_float;
  std::string test_id("my test float ID");
  flatui::RegisterFloatData(test_id, &test_float);
  ASSERT_EQ(flatui::DynamicData::kFloatData,
            flatui::dynamic_data[test_id].type);
  ASSERT_EQ(&test_float, flatui::dynamic_data[test_id].data.float_data);

  // Test duplicate key errors.
  //
  // Mock the `fplbase::LogError` function to assert it is called.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillOnce(SaveArg<0>(&error_output));

  flatui::RegisterFloatData(test_id, &test_float);
  ASSERT_EQ(std::string("Duplicate key \"%s\" used for \"%s\"."), error_output);
}

TEST_F(FlatUISerializationTest, TestRegisterCharData) {
  char test_char;
  std::string test_id("my test char ID");
  flatui::RegisterCharData(test_id, &test_char);
  ASSERT_EQ(flatui::DynamicData::kCharData, flatui::dynamic_data[test_id].type);
  ASSERT_EQ(&test_char, flatui::dynamic_data[test_id].data.char_data);

  // Test duplicate key errors.
  //
  // Mock the `fplbase::LogError` function to assert it is called.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillOnce(SaveArg<0>(&error_output));

  flatui::RegisterCharData(test_id, &test_char);
  ASSERT_EQ(std::string("Duplicate key \"%s\" used for \"%s\"."), error_output);
}

TEST_F(FlatUISerializationTest, TestRegisterBoolData) {
  bool test_bool;
  std::string test_id("my test bool ID");
  flatui::RegisterBoolData(test_id, &test_bool);
  ASSERT_EQ(flatui::DynamicData::kBoolData, flatui::dynamic_data[test_id].type);
  ASSERT_EQ(&test_bool, flatui::dynamic_data[test_id].data.bool_data);

  // Test duplicate key errors.
  //
  // Mock the `fplbase::LogError` function to assert it is called.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillOnce(SaveArg<0>(&error_output));

  flatui::RegisterBoolData(test_id, &test_bool);
  ASSERT_EQ(std::string("Duplicate key \"%s\" used for \"%s\"."), error_output);
}

TEST_F(FlatUISerializationTest, TestRegisterVec2Data) {
  mathfu::vec2 test_vec2;
  std::string test_id("my test vec2 ID");
  flatui::RegisterVec2Data(test_id, &test_vec2);
  ASSERT_EQ(flatui::DynamicData::kVec2Data, flatui::dynamic_data[test_id].type);
  ASSERT_EQ(&test_vec2, flatui::dynamic_data[test_id].data.vec2_data);

  // Test duplicate key errors.
  //
  // Mock the `fplbase::LogError` function to assert it is called.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillOnce(SaveArg<0>(&error_output));

  flatui::RegisterVec2Data(test_id, &test_vec2);
  ASSERT_EQ(std::string("Duplicate key \"%s\" used for \"%s\"."), error_output);
}

TEST_F(FlatUISerializationTest, TestRegisterVec2iData) {
  mathfu::vec2i test_vec2i;
  std::string test_id("my test vec2i ID");
  flatui::RegisterVec2iData(test_id, &test_vec2i);
  ASSERT_EQ(flatui::DynamicData::kVec2iData,
            flatui::dynamic_data[test_id].type);
  ASSERT_EQ(&test_vec2i, flatui::dynamic_data[test_id].data.vec2i_data);

  // Test duplicate key errors.
  //
  // Mock the `fplbase::LogError` function to assert it is called.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillOnce(SaveArg<0>(&error_output));

  flatui::RegisterVec2iData(test_id, &test_vec2i);
  ASSERT_EQ(std::string("Duplicate key \"%s\" used for \"%s\"."), error_output);
}

TEST_F(FlatUISerializationTest, TestRegisterVec4Data) {
  mathfu::vec4 test_vec4;
  std::string test_id("my test vec4 ID");
  flatui::RegisterVec4Data(test_id, &test_vec4);
  ASSERT_EQ(flatui::DynamicData::kVec4Data, flatui::dynamic_data[test_id].type);
  ASSERT_EQ(&test_vec4, flatui::dynamic_data[test_id].data.vec4_data);

  // Test duplicate key errors.
  //
  // Mock the `fplbase::LogError` function to assert it is called.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillOnce(SaveArg<0>(&error_output));

  flatui::RegisterVec4Data(test_id, &test_vec4);
  ASSERT_EQ(std::string("Duplicate key \"%s\" used for \"%s\"."), error_output);
}

TEST_F(FlatUISerializationTest, TestRegisterStringData) {
  std::string test_string;
  std::string test_id("my test string ID");
  flatui::RegisterStringData(test_id, &test_string);
  ASSERT_EQ(flatui::DynamicData::kStringData,
            flatui::dynamic_data[test_id].type);
  ASSERT_EQ(&test_string, flatui::dynamic_data[test_id].data.string_data);

  // Test duplicate key errors.
  //
  // Mock the `fplbase::LogError` function to assert it is called.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillOnce(SaveArg<0>(&error_output));

  flatui::RegisterStringData(test_id, &test_string);
  ASSERT_EQ(std::string("Duplicate key \"%s\" used for \"%s\"."), error_output);
}

TEST_F(FlatUISerializationTest, TestRegisterVoidData) {
  int test_void;  // Random type for void*.
  std::string test_id("my test void ID");
  flatui::RegisterVoidData(test_id, &test_void);
  ASSERT_EQ(flatui::DynamicData::kVoidData, flatui::dynamic_data[test_id].type);
  ASSERT_EQ(&test_void, flatui::dynamic_data[test_id].data.void_data);

  // Test duplicate key errors.
  //
  // Mock the `fplbase::LogError` function to assert it is called.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillOnce(SaveArg<0>(&error_output));

  flatui::RegisterVoidData(test_id, &test_void);
  ASSERT_EQ(std::string("Duplicate key \"%s\" used for \"%s\"."), error_output);
}

TEST_F(FlatUISerializationTest, TestGetDynamicData) {
  // Create some dummy data and register it.
  int test_int;
  std::string int_id("my int ID");
  flatui::RegisterIntData(int_id, &test_int);

  float test_float;
  std::string float_id("my float ID");
  flatui::RegisterFloatData(float_id, &test_float);

  // Get it back and check to make sure it's valid.
  auto dynamic_int_data = flatui::GetDynamicData(int_id);
  ASSERT_EQ(flatui::DynamicData::kIntData, dynamic_int_data->type);
  ASSERT_EQ(&test_int, dynamic_int_data->data.int_data);

  auto dynamic_float_data = flatui::GetDynamicData(float_id);
  ASSERT_EQ(flatui::DynamicData::kFloatData, dynamic_float_data->type);
  ASSERT_EQ(&test_float, dynamic_float_data->data.float_data);

  // Check invalid ID input.
  auto invalid_dynamic_data = flatui::GetDynamicData("This ID does not exist.");
  ASSERT_EQ(nullptr, invalid_dynamic_data);
}

// Note: This test alters the error count used in the global test space. There
// is no real way around this, so set the test count to a very large number and
// reset it back to `-1` when done.
TEST_F(FlatUISerializationTest, TestSetErrorOutputCount) {
  int new_count = 400000000;
  flatui::SetErrorOutputCount(new_count);
  ASSERT_EQ(new_count, flatui::error_count);

  // Test for an invalid error count (something less than -1).
  //
  // Mock the `fplbase::LogError` function to assert it is called.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillOnce(SaveArg<0>(&error_output));

  flatui::SetErrorOutputCount(-10);

  ASSERT_EQ(std::string("Invalid value \"%d\" passed to"
                        " \"SetErrorOutputCount\"."),
            error_output);

  // Reset the error count to `-1`, indicating print every error.
  flatui::SetErrorOutputCount(-1);
}

TEST_F(FlatUISerializationTest, TestError) {
  // Note: We are limited in how much we can test of this function, since it
  // relies on a shared variable `flatui::error_count`.
  //
  // Mock the `fplbase::LogError` function to assert it is called.
  fplbase::FplbaseMocks& fplbase_mocks = fplbase::FplbaseMocks::get_mocks();

  // Used to capture the error output of the `LogError` mock.
  std::string error_output;

  EXPECT_CALL(fplbase_mocks, LogError(_, _))
      .WillOnce(SaveArg<0>(&error_output));

  flatui::Error("This is a test!");

  ASSERT_EQ(std::string("This is a test!"), error_output);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
