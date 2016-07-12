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

#include "flatui/flatui_serialization.h"

#include <stdarg.h>
#include <unordered_map>

#include "fplbase/flatbuffer_utils.h"
#include "fplbase/utilities.h"

using flatui_data::FlatUIElement;
using flatui_data::GetFlatUI;
using fplbase::AssetManager;

namespace flatui {

// A map to store any dynamic data registered with this API. This exists
// to allow users to register pointers with the API to be used in place
// of a missing field for a FlatUI element.
std::unordered_map<std::string, DynamicData> dynamic_data;

// Keep a mapping of custom widget functions to search if a core widget is not
// matched with the `FlatUIElement->type()`.
std::unordered_map<int, CustomWidget> custom_elements;

// Forward declaration to support nesting elements.
static void MapElement(const FlatUIElement* element, AssetManager* assetman,
                       FlatUIHandler event_handler);

int error_count = 10;  // Tracks the max number of errors to print.

// Helper function to only print a fixed number of error messages (maximum) to
// prevent flooding of the log on each frame. This value can be changed using
// `SetErrorCount()`.
//
// Note: If error_count is set to `kNoErrorOutputLimit`, then all errors will
// be printed.
static void Error(const char* fmt, ...) {
  static int num_errors = 0;

  if (error_count != kNoErrorOutputLimit) {
    if (num_errors >= error_count) {
      fplbase::LogError("More errors occurred while processing the FlatUI...");
      num_errors++;
      return;
    } else if (num_errors > error_count) {
      return;
    }
  }

  va_list args;
  va_start(args, fmt);
  fplbase::LogError(fmt, args);
  va_end(args);
  num_errors++;
}

// Helper function to adjust the error count.
void SetErrorOutputCount(int count) {
  if (count < -1) {
    Error("Invalid value \"%d\" passed to \"SetErrorOutputCount\".", count);
    return;
  }

  error_count = count;
}

// Helper function to check if the AssetManager is a valid pointer.
static bool HasValidAssetManager(const fplbase::AssetManager* assetman,
                                 const char* widget_id) {
  if (assetman == nullptr) {
    Error("\"AssetManager\", required by widget with ID \"%s\", is nullptr.",
          widget_id);
    return false;
  }

  return true;
}

// Helper function to check if the AssetManager has a Texture, and return
// it through the `texture` parameter.
static bool GetTextureFromAssetManager(const char* texture_name,
                                       const char* widget_id,
                                       fplbase::AssetManager* assetman,
                                       fplbase::Texture* texture) {
  if (!HasValidAssetManager(assetman, widget_id)) return false;

  auto tex = assetman->FindTexture(texture_name);
  if (tex == nullptr) {
    Error("Texture \"%s\" cannot be found for FlatUI widget with ID \"%s\".",
          texture_name, widget_id);
    return false;
  }

  *texture = *tex;
  return true;
}

// Use a bitmap to check which required fields map to which core widgets.
enum CheckFieldsBitmap {
  kRequiredDynamicData = 1,  // Some elements require dynamic data for a field
                             // external to those specified in the schema.
  kBarSizeBm = 2,
  kLayoutBm = 4,
  kSizeBm = 8,
  kSize_2fBm = 16,
  kTextBm = 32,
  kTextureBm = 64,
  kTextureSecondaryBm = 128,
  kTypeBm = 256,
  kVirtualResolutionBm = 512,
  kYSizeBm = 1024
};

// Returns a bitmap containing all of the required fields used by the given
// type.
static int RequiredFields(const FlatUIElement* element) {
  switch (element->type()) {
    case flatui_data::Type_CheckBox:
      return kTextureBm | kTextureSecondaryBm | kTextBm | kSizeBm;
    case flatui_data::Type_Edit:
      return kYSizeBm | kSize_2fBm | kRequiredDynamicData;
    case flatui_data::Type_Group:
      return kLayoutBm;
    case flatui_data::Type_Image:
      return kTextureBm | kYSizeBm;
    case flatui_data::Type_ImageButton:
      return kTextureBm | kSizeBm;
    case flatui_data::Type_Label:
      return kTextBm | kYSizeBm;
    case flatui_data::Type_ScrollBar:
      return kTextureBm | kTextureSecondaryBm | kBarSizeBm | kSize_2fBm |
             kRequiredDynamicData;
    case flatui_data::Type_SetVirtualResolution:
      return kVirtualResolutionBm;
    case flatui_data::Type_Slider:
      return kTextureBm | kTextureSecondaryBm | kBarSizeBm | kSize_2fBm |
             kRequiredDynamicData;
    case flatui_data::Type_TextButton:
      return kTextBm | kYSizeBm;
    default:
      return kTypeBm;
  }
}

static DynamicData* GetDynamicData(const std::string& id) {
  auto data = dynamic_data.find(id);
  return data == dynamic_data.end() ? nullptr : &data->second;
}

// A helper function to register the dynamic data pointer to the union.
// It also checks if a duplicate key is detected, and provides the user with
// as much error info as possible.
//
// `caller_name` is a string corresponding to the name of the function that
// called this, for error outputting purposes.
template <typename T>
static void RegisterDynamicData(const std::string& id, T* data,
                                const char* caller_name) {
  if (GetDynamicData(id) != nullptr) {
    Error("Duplicate key \"%s\" used for \"%s\".", id.c_str(), caller_name);
    return;
  }

  int** union_field_pointer = &dynamic_data[id].data.int_data;
  T** dynamic_data_ptr = reinterpret_cast<T**>(union_field_pointer);
  *dynamic_data_ptr = data;
}

void RegisterIntData(const std::string& id, int* data) {
  RegisterDynamicData(id, data, "RegisterIntData");
  dynamic_data[id].type = DynamicData::kIntData;
}

void RegisterFloatData(const std::string& id, float* data) {
  RegisterDynamicData(id, data, "RegisterFloatData");
  dynamic_data[id].type = DynamicData::kFloatData;
}

void RegisterCharData(const std::string& id, char* data) {
  RegisterDynamicData(id, data, "RegisterCharData");
  dynamic_data[id].type = DynamicData::kCharData;
}

void RegisterBoolData(const std::string& id, bool* data) {
  RegisterDynamicData(id, data, "RegisterBoolData");
  dynamic_data[id].type = DynamicData::kBoolData;
}

void RegisterVec2Data(const std::string& id, mathfu::vec2* data) {
  RegisterDynamicData(id, data, "RegisterVec2Data");
  dynamic_data[id].type = DynamicData::kVec2Data;
}

void RegisterVec2iData(const std::string& id, mathfu::vec2i* data) {
  RegisterDynamicData(id, data, "RegisterVec2iData");
  dynamic_data[id].type = DynamicData::kVec2iData;
}

void RegisterVec4Data(const std::string& id, mathfu::vec4* data) {
  RegisterDynamicData(id, data, "RegisterVec4Data");
  dynamic_data[id].type = DynamicData::kVec4Data;
}

void RegisterStringData(const std::string& id, std::string* data) {
  RegisterDynamicData(id, data, "RegisterVec4Data");
  dynamic_data[id].type = DynamicData::kStringData;
}

void RegisterVoidData(const std::string& id, void* data) {
  RegisterDynamicData(id, data, "RegisterVoidData");
  dynamic_data[id].type = DynamicData::kVoidData;
}

// Helper function to check if dynamic data exists for a given `id` with a
// certain type.
static bool HasDynamicData(const std::string& id,
                           flatui::DynamicData::DynamicDataType type) {
  auto dynamic = GetDynamicData(id);
  if (dynamic == nullptr) {
    return false;
  }

  if (dynamic->type != type) {
    return false;
  }

  return true;
}

// Helper function to print an error message for a missing field.
//
// Note: The `bool dynamic_data_reserved` flag is used to print an additional
// line of error message informing that user that the FlatUI element with
// the missing field does not support dynamic data for missing fields. (Because
// the element uses dynamic data to capture output through a parameter.)
static void MissingFieldError(const std::string& field, const std::string& id,
                              bool dynamic_data_reserved) {
  std::string missing_field_error("Required field \"");
  missing_field_error += field;
  missing_field_error += "\" is missing for FlatUI element with ID \"%s\".";

  if (dynamic_data_reserved) {
    missing_field_error += " (Dynamic data cannot be used for this field.)";
  }

  Error(missing_field_error.c_str(), id.c_str());
}

static void MissingFieldError(const std::string& field, const std::string& id) {
  MissingFieldError(field, id, false);
}

// Helper function to check if a field is actually missing, or if dynamic data
// can be used in place of it during `CheckElements`.
//
// `dynamic_data_reserved` is a bool determining if the field may have dynamic
// data or not. (Some widgets have reserve the dynamic data for their own
// parameters).
//
// `num_dynamic_data_uses` is a pointer to an int that tracks how many times
// dynamic data was used to avoid erroring on a missing field. There may only
// be a single missing field that can be replaced by dynamic data.
static bool CheckMissingField(const std::string& field, const std::string& id,
                              const DynamicData::DynamicDataType data_type,
                              const bool dynamic_data_reserved,
                              int* num_dynamic_data_uses) {
  if (!HasDynamicData(id, data_type)) {
    MissingFieldError(field, id);
    return false;
  } else {
    if (dynamic_data_reserved) {
      MissingFieldError(field, id, true);
      return false;
    }
  }

  (*num_dynamic_data_uses)++;
  return true;
}

// Print an error if any required fields are missing, and return `false` if an
// error occurred.
//
// Note: If any dynamic data is available for a missing field for this element,
// then no error is output.
static bool CheckElements(const FlatUIElement* element, int required_elements) {
  bool success = true;            // Tracks if an error occurred.
  int num_dynamic_data_uses = 0;  // Only one missing field may be replaced by
                                  // dynamic data.

  const bool dynamic_data_reserved =
      (required_elements & kRequiredDynamicData) != 0;

  // bar_size
  if (required_elements & kBarSizeBm && element->bar_size() < 0.0f) {
    success = CheckMissingField("bar_size", element->id()->str(),
                                flatui::DynamicData::kFloatData,
                                dynamic_data_reserved, &num_dynamic_data_uses);
  }

  // layout
  if (required_elements & kLayoutBm &&
      element->layout() == flatui_data::Layout_None) {
    success = CheckMissingField("layout", element->id()->str(),
                                flatui::DynamicData::kIntData,
                                dynamic_data_reserved, &num_dynamic_data_uses);
  }

  // size
  if (required_elements & kSizeBm && element->size() < 0.0f) {
    success = CheckMissingField("size", element->id()->str(),
                                flatui::DynamicData::kFloatData,
                                dynamic_data_reserved, &num_dynamic_data_uses);
  }

  // size_2f
  if (required_elements & kSize_2fBm && element->size_2f() == nullptr) {
    success = CheckMissingField("size_2f", element->id()->str(),
                                flatui::DynamicData::kVec2Data,
                                dynamic_data_reserved, &num_dynamic_data_uses);
  }

  // text
  if (required_elements & kTextBm && element->text() == nullptr) {
    success = CheckMissingField("text", element->id()->str(),
                                flatui::DynamicData::kStringData,
                                dynamic_data_reserved, &num_dynamic_data_uses);
  }

  // texture
  if (required_elements & kTextureBm && element->texture() == nullptr) {
    success = CheckMissingField("texture", element->id()->str(),
                                flatui::DynamicData::kStringData,
                                dynamic_data_reserved, &num_dynamic_data_uses);
  }

  // texture_secondary
  if (required_elements & kTextureSecondaryBm &&
      element->texture_secondary() == nullptr) {
    success = CheckMissingField("texture_secondary", element->id()->str(),
                                flatui::DynamicData::kStringData,
                                dynamic_data_reserved, &num_dynamic_data_uses);
  }

  // type
  if (required_elements & kTypeBm &&
      element->type() == flatui_data::Type_InvalidType) {
    MissingFieldError("type", element->id()->str());
    success = false;
  }

  // virtual_resolution
  if (required_elements & kVirtualResolutionBm &&
      element->virtual_resolution() < 0.0f) {
    success = CheckMissingField("virtual_resolution", element->id()->str(),
                                flatui::DynamicData::kFloatData,
                                dynamic_data_reserved, &num_dynamic_data_uses);
  }

  // ysize
  if (required_elements & kYSizeBm && element->ysize() < 0.0f) {
    success = CheckMissingField("ysize", element->id()->str(),
                                flatui::DynamicData::kFloatData,
                                dynamic_data_reserved, &num_dynamic_data_uses);
  }

  // Ensure that a max of 1 field is replaced by dynamic data.
  if (num_dynamic_data_uses > 1) {
    Error(
        "\"%d\" required fields are missing for element ID \"%s\". You may"
        " only have one piece of dynamic data per element.",
        num_dynamic_data_uses, element->id()->c_str());
    return false;
  }

  return success;
}

void RegisterCustomWidget(const uint32_t& type, CustomWidget widget) {
  if (type >= flatui_data::Type_ReservedDefaultTypes) {
    Error(
        "Custom widget with type \"%u\" must be less than the reserved"
        " \"%u\" widgets.",
        type, flatui_data::Type_ReservedDefaultTypes);
    return;
  }

  if (!widget) {
    Error(
        "Custom widget with type \"%u\" must have a callable \"CustomWidget\""
        " function.",
        type);
    return;
  }

  custom_elements[type] = widget;
}

// Call the appropriate function, if it exists.
static void CreateCustomWidget(const FlatUIElement* element,
                               AssetManager* assetman,
                               FlatUIHandler event_handler) {
  auto widget = custom_elements.find(element->type());
  if (widget == custom_elements.end()) {
    Error("Custom widget with type \"%u\" was not registered.",
          element->type());
    return;
  }

  auto output_data = GetDynamicData(element->id()->str());
  widget->second(element, assetman, event_handler, output_data);
}

static void CreateGroup(const FlatUIElement* element, AssetManager* assetman,
                        FlatUIHandler event_handler) {
  Layout layout = kLayoutHorizontalTop;  // Dummy value for initialization.
  if (element->layout() == flatui_data::Layout_None) {
    if (HasDynamicData(element->id()->str(), DynamicData::kIntData)) {
      layout = static_cast<Layout>(
          *GetDynamicData(element->id()->str())->data.int_data);
    } else {
      MissingFieldError("layout", element->id()->str());
      return;
    }
  } else {
    layout = static_cast<Layout>(element->layout());
  }

  float spacing = element->spacing();
  if (spacing == 0.0f) {
    if (HasDynamicData(element->id()->str(), DynamicData::kFloatData)) {
      spacing = *GetDynamicData(element->id()->str())->data.float_data;
    }
  }

  int horizontal = element->horizontal();
  if (horizontal == 0) {
    if (HasDynamicData(element->id()->str(), DynamicData::kIntData)) {
      horizontal = *GetDynamicData(element->id()->str())->data.int_data;
    }
  }

  int vertical = element->vertical();
  if (vertical == 0) {
    if (horizontal != 0 &&
        HasDynamicData(element->id()->str(), DynamicData::kIntData)) {
      vertical = *GetDynamicData(element->id()->str())->data.int_data;
    }
  }

  flatui::StartGroup(layout, spacing, element->id()->c_str());

  if (horizontal != 0 && vertical != 0) {
    if (element->offset() == nullptr) {
      if (HasDynamicData(element->id()->str(), DynamicData::kVec2Data)) {
        flatui::PositionGroup(
            static_cast<Alignment>(horizontal),
            static_cast<Alignment>(vertical),
            *GetDynamicData(element->id()->str())->data.vec2_data);
      }
    } else {
      flatui::PositionGroup(static_cast<Alignment>(horizontal),
                            static_cast<Alignment>(vertical),
                            fplbase::LoadVec2(element->offset()));
    }
  }

  if (element->is_modal_group()) {
    flatui::ModalGroup();
  }

  auto child_elements = element->elements();
  if (child_elements != nullptr) {
    for (unsigned int i = 0; i < child_elements->Length(); i++) {
      MapElement(child_elements->Get(i), assetman, event_handler);
    }
  }

  flatui::EndGroup();
}

static void CreateImage(const FlatUIElement* element, AssetManager* assetman) {
  const char* texture_name = nullptr;
  if (element->texture() == nullptr) {
    if (HasDynamicData(element->id()->str(), DynamicData::kStringData)) {
      texture_name =
          GetDynamicData(element->id()->str())->data.string_data->c_str();
    } else {
      MissingFieldError("texture", element->id()->str());
      return;
    }
  } else {
    texture_name = element->texture()->c_str();
  }

  fplbase::Texture texture;
  if (!GetTextureFromAssetManager(texture_name, element->id()->c_str(),
                                  assetman, &texture)) {
    return;
  }

  float ysize = element->ysize();
  if (ysize < 0.0f) {
    if (HasDynamicData(element->id()->str(), DynamicData::kFloatData)) {
      ysize = *GetDynamicData(element->id()->str())->data.float_data;
    } else {
      MissingFieldError("ysize", element->id()->str());
      return;
    }
  }

  flatui::Image(texture, ysize);
}

static void CreateLabel(const FlatUIElement* element) {
  const char* text = nullptr;
  if (element->text() == nullptr) {
    if (HasDynamicData(element->id()->str(), DynamicData::kStringData)) {
      text = GetDynamicData(element->id()->str())->data.string_data->c_str();
    } else {
      MissingFieldError("text", element->id()->str());
      return;
    }
  } else {
    text = element->text()->c_str();
  }

  float ysize = element->ysize();
  if (ysize < 0.0f) {
    if (HasDynamicData(element->id()->str(), DynamicData::kFloatData)) {
      ysize = *GetDynamicData(element->id()->str())->data.float_data;
    } else {
      MissingFieldError("ysize", element->id()->str());
      return;
    }
  }

  if (element->size_2f() == nullptr &&
      HasDynamicData(element->id()->str(), DynamicData::kVec2Data)) {
    flatui::Label(text, ysize,
                  *GetDynamicData(element->id()->str())->data.vec2_data);
  } else if (element->size_2f() == nullptr) {
    flatui::Label(text, ysize);
  } else {
    flatui::Label(text, ysize, fplbase::LoadVec2(element->size_2f()));
  }
}

static Margin CreateMargin(const flatui_data::Margin* margin) {
  if (margin == nullptr) {
    return Margin(0.0f);
  }

  if (margin->size_bottom() >= 0.0f && margin->size_right() >= 0.0f) {
    return Margin(margin->size_left(), margin->size_top(), margin->size_right(),
                  margin->size_bottom());
  }

  if (margin->size_top() >= 0.0f) {
    return Margin(margin->size_left(), margin->size_top());
  }

  if (margin->size_left() >= 0.0f) {
    return Margin(margin->size_left());
  }

  return Margin(0.0f);
}

static void CreateSetVirtualResolution(const FlatUIElement* element) {
  float vr = element->virtual_resolution();

  if (vr < 0.0f) {
    if (HasDynamicData(element->id()->str(), DynamicData::kFloatData)) {
      vr = *GetDynamicData(element->id()->str())->data.float_data;
    } else {
      MissingFieldError("virtual_resolution", element->id()->str());
      return;
    }
  }

  flatui::SetVirtualResolution(vr);
}

static void CreateImageButton(const FlatUIElement* element,
                              AssetManager* assetman,
                              FlatUIHandler event_handler) {
  float size = element->size();
  if (size < 0.0f) {
    if (HasDynamicData(element->id()->str(), DynamicData::kFloatData)) {
      size = *GetDynamicData(element->id()->str())->data.float_data;
    } else {
      MissingFieldError("size", element->id()->str());
      return;
    }
  }

  const char* texture_name = nullptr;
  if (element->texture() == nullptr) {
    if (HasDynamicData(element->id()->str(), DynamicData::kStringData)) {
      texture_name =
          GetDynamicData(element->id()->str())->data.string_data->c_str();
    } else {
      MissingFieldError("texture", element->id()->str());
      return;
    }
  } else {
    texture_name = element->texture()->c_str();
  }

  fplbase::Texture texture;
  if (!GetTextureFromAssetManager(texture_name, element->id()->c_str(),
                                  assetman, &texture)) {
    return;
  }

  Event e = flatui::ImageButton(texture, size, CreateMargin(element->margin()),
                                element->id()->c_str());

  if (event_handler != nullptr) {
    event_handler(e, element->id()->str(),
                  GetDynamicData(element->id()->str()));
  }
}

static void CreateTextButton(const FlatUIElement* element,
                             AssetManager* assetman,
                             FlatUIHandler event_handler) {
  const char* text = nullptr;
  if (element->text() == nullptr) {
    if (HasDynamicData(element->id()->str(), DynamicData::kStringData)) {
      text = GetDynamicData(element->id()->str())->data.string_data->c_str();
    } else {
      MissingFieldError("text", element->id()->str());
      return;
    }
  } else {
    text = element->text()->c_str();
  }

  float size = element->size();
  if (size < 0.0f) {
    if (HasDynamicData(element->id()->str(), DynamicData::kFloatData)) {
      size = *GetDynamicData(element->id()->str())->data.float_data;
    } else {
      MissingFieldError("size", element->id()->str());
      return;
    }
  }

  const char* texture_name = nullptr;
  if (element->texture() == nullptr) {
    if (HasDynamicData(element->id()->str(), DynamicData::kStringData) &&
        element->text() != nullptr) {
      texture_name =
          GetDynamicData(element->id()->str())->data.string_data->c_str();
    } else {
      texture_name = nullptr;
    }
  } else {
    texture_name = element->texture()->c_str();
  }

  Event e = kEventNone;
  if (texture_name != nullptr) {
    fplbase::Texture texture;
    if (!GetTextureFromAssetManager(texture_name, element->id()->c_str(),
                                    assetman, &texture)) {
      return;
    }

    int button_property = element->property();
    if (button_property == flatui_data::ButtonProperty_Disabled) {
      if (HasDynamicData(element->id()->str(), DynamicData::kIntData)) {
        button_property = *GetDynamicData(element->id()->str())->data.int_data;
      }
    }

    e = flatui::TextButton(texture, CreateMargin(element->texture_margin()),
                           text, size, CreateMargin(element->margin()),
                           static_cast<ButtonProperty>(button_property));
  } else {
    e = flatui::TextButton(text, size, CreateMargin(element->margin()));
  }

  if (event_handler != nullptr) {
    event_handler(e, element->id()->str(),
                  GetDynamicData(element->id()->str()));
  }
}

static void CreateEdit(const FlatUIElement* element,
                       FlatUIHandler event_handler) {
  if (!HasDynamicData(element->id()->str(), DynamicData::kStringData)) {
    Error(
        "\"Edit\" with ID \"%s\" requires a string dynamic data to be"
        " registered with it.",
        element->id()->c_str());
    return;
  }

  auto dynamic = GetDynamicData(element->id()->str());
  std::string* edit_output = dynamic->data.string_data;
  EditStatus status;

  Event e =
      flatui::Edit(element->ysize(), fplbase::LoadVec2(element->size_2f()),
                   element->id()->c_str(), &status, edit_output);
  if (event_handler != nullptr) {
    event_handler(e, element->id()->str(), dynamic);
  }
}

static void CreateCheckBox(const FlatUIElement* element, AssetManager* assetman,
                           FlatUIHandler event_handler) {
  if (!HasDynamicData(element->id()->str(), DynamicData::kBoolData)) {
    Error(
        "\"CheckBox\" with ID \"%s\" requires a bool dynamic data to be"
        " registered with it.",
        element->id()->c_str());
    return;
  }

  fplbase::Texture texture_checked, texture_unchecked;
  if (!GetTextureFromAssetManager(element->texture()->c_str(),
                                  element->id()->c_str(), assetman,
                                  &texture_checked) ||
      !GetTextureFromAssetManager(element->texture_secondary()->c_str(),
                                  element->id()->c_str(), assetman,
                                  &texture_unchecked)) {
    return;
  }

  auto dynamic = GetDynamicData(element->id()->str());
  bool* check_box_output = dynamic->data.bool_data;

  Event e = flatui::CheckBox(texture_checked, texture_unchecked,
                             element->text()->c_str(), element->size(),
                             CreateMargin(element->margin()), check_box_output);

  if (event_handler != nullptr) {
    event_handler(e, element->id()->str(), dynamic);
  }
}

static void CreateScrollBar(const FlatUIElement* element,
                            AssetManager* assetman,
                            FlatUIHandler event_handler) {
  if (!HasDynamicData(element->id()->str(), DynamicData::kFloatData)) {
    Error(
        "\"ScrollBar\" with ID \"%s\" requires a float dynamic data to be"
        " registered with it.",
        element->id()->c_str());
    return;
  }

  fplbase::Texture texture_bg, texture_fg;
  if (!GetTextureFromAssetManager(element->texture()->c_str(),
                                  element->id()->c_str(), assetman,
                                  &texture_bg) ||
      !GetTextureFromAssetManager(element->texture_secondary()->c_str(),
                                  element->id()->c_str(), assetman,
                                  &texture_fg)) {
    return;
  }

  auto dynamic = GetDynamicData(element->id()->str());
  float* scroll_bar_output = dynamic->data.float_data;

  Event e = flatui::ScrollBar(
      texture_bg, texture_fg, fplbase::LoadVec2(element->size_2f()),
      element->bar_size(), element->id()->c_str(), scroll_bar_output);

  if (event_handler != nullptr) {
    event_handler(e, element->id()->str(), dynamic);
  }
}

static void CreateSlider(const FlatUIElement* element, AssetManager* assetman,
                         FlatUIHandler event_handler) {
  if (!HasDynamicData(element->id()->str(), DynamicData::kFloatData)) {
    Error(
        "\"Slider\" with ID \"%s\" requires a float dynamic data to be"
        " registered with it.",
        element->id()->c_str());
    return;
  }

  fplbase::Texture texture_bar, texture_knob;
  if (!GetTextureFromAssetManager(element->texture()->c_str(),
                                  element->id()->c_str(), assetman,
                                  &texture_bar) ||
      !GetTextureFromAssetManager(element->texture_secondary()->c_str(),
                                  element->id()->c_str(), assetman,
                                  &texture_knob)) {
    return;
  }

  auto dynamic = GetDynamicData(element->id()->str());
  float* slider_output = dynamic->data.float_data;

  Event e = flatui::Slider(
      texture_bar, texture_knob, fplbase::LoadVec2(element->size_2f()),
      element->bar_size(), element->id()->c_str(), slider_output);

  if (event_handler != nullptr) {
    event_handler(e, element->id()->str(), dynamic);
  }
}

// Helper function to map from FlatUiElement's enum to a FlatUi function.
static void MapElement(const FlatUIElement* element, AssetManager* assetman,
                       FlatUIHandler event_handler) {
  if (!CheckElements(element, RequiredFields(element))) {
    return;
  }

  switch (element->type()) {
    case flatui_data::Type_CheckBox:
      CreateCheckBox(element, assetman, event_handler);
      break;
    case flatui_data::Type_Edit:
      CreateEdit(element, event_handler);
      break;
    case flatui_data::Type_Group:
      CreateGroup(element, assetman, event_handler);
      break;
    case flatui_data::Type_Image:
      CreateImage(element, assetman);
      break;
    case flatui_data::Type_ImageButton:
      CreateImageButton(element, assetman, event_handler);
      break;
    case flatui_data::Type_Label:
      CreateLabel(element);
      break;
    case flatui_data::Type_ScrollBar:
      CreateScrollBar(element, assetman, event_handler);
      break;
    case flatui_data::Type_SetVirtualResolution:
      CreateSetVirtualResolution(element);
      break;
    case flatui_data::Type_Slider:
      CreateSlider(element, assetman, event_handler);
      break;
    case flatui_data::Type_TextButton:
      CreateTextButton(element, assetman, event_handler);
      break;
    default:  // Handle custom registered widgets.
      CreateCustomWidget(element, assetman, event_handler);
  }
}

void CreateFlatUIFromData(const void* flatui_data, AssetManager* assetman,
                          FlatUIHandler event_handler) {
  if (flatui_data == nullptr) {
    Error(
        "\"CreateFlatUIFromData\" requires that \"flatui_data\" is not a"
        " \"nullptr\".");
    return;
  }

  auto flatui_elements = GetFlatUI(flatui_data)->elements();

  if (flatui_elements == nullptr || flatui_elements->Length() == 0) {
    Error(
        "Required field \"elements\" is missing, or empty, for \"FlatUI\" "
        "root table.");
    return;
  }

  for (unsigned int i = 0; i < flatui_elements->Length(); i++) {
    MapElement(flatui_elements->Get(i), assetman, event_handler);
  }
}

}  // flatui
