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

#ifndef FPL_FLATUI_SERIALIZATION_H_
#define FPL_FLATUI_SERIALIZATION_H_

#include "flatui/flatui.h"
#include "flatui/flatui_common.h"
#include "flatui/flatui_generated.h"
#include "fplbase/asset_manager.h"

#include <functional>

namespace flatui {

// Use this constant as the value of `flatui::SetErrorOutputCount` to print
// every error on every frame.
const int kNoErrorOutputLimit = -1;

/// @brief To support some type safety, pointers registered with this API
/// (which contain data that persists across frames) use a union consisting of
/// many common data types, while allowing `void*` to be used for any custom
/// data types you wish to use.
///
/// @note When using the `void*`, the user is responsible for casting the
/// data appropriately.
struct DynamicData {
  enum DynamicDataType {  // Used for type safety.
    kIntData,
    kFloatData,
    kCharData,
    kBoolData,
    kVec2Data,
    kVec2iData,
    kVec4Data,
    kStringData,
    kVoidData
  };

  DynamicDataType type;

  union {
    int* int_data;
    float* float_data;
    char* char_data;
    bool* bool_data;
    mathfu::vec2* vec2_data;
    mathfu::vec2i* vec2i_data;
    mathfu::vec4* vec4_data;
    std::string* string_data;
    void* void_data;
  } data;
};

/// @brief A `std::function` corresponding to the Event Handler function, which
/// is used whenever an Event occurs in the deserialized GUI.
///
/// The first parameter is a `const Event&` containing the `flatui::Event` that
/// occurred this frame.
///
/// The second parameter is a `const std::string&` containing the ID of the
/// widget that receieved the Event.
///
/// The third parameter is a `DynamicData*` to the dynamic data that is
/// registered with the widget that receieved the Event. If no data is
/// registered with the widget, then this parameter is a `nullptr`.
typedef std::function<void(const Event&, const std::string&, DynamicData*)>
    FlatUIHandler;

/// @brief A `std::function` referring to a custom widget deserialization
/// function, which is called whenever a corresponding custom widget type
/// is encountered.
///
/// The first parmeter is a `const FlatUIElement*`, which receives the
/// FlatBuffer data for the custom widget.
///
/// The second parameter is a `fplbase::AssetManager*`, which points to the
/// `fplbase::AssetManager` that should be used for rendering any Textures
/// in the custom widget. If there is no `fplbase::AssetManager` provided
/// for this GUI, then this will be a `nullptr`.
///
/// The third parameter is a `FlatUIHandler`, which is a `std::function`
/// that acts as a function pointer to the Event Handler function. Any
/// events generated in the custom widget should be passed as a parameter to
/// this function. If this UI does not have an Event Handler, then this
/// parameter will be a `nullptr`.
///
/// The fourth parameter is a `DynamicData*`, which points to the dynamic
/// data that is registered with the custom widget. If no data is registered
/// with this custom widget, then this parameter is a `nullptr`.
typedef std::function<void(const flatui_data::FlatUIElement*,
                           fplbase::AssetManager*, FlatUIHandler, DynamicData*)>
    CustomWidget;

/// @brief Used to register a pointer of dynamically changing int data with the
/// API.
///
/// This is useful anytime a FlatUI element requires dynamic data, which
/// changes between frames. This data can also be accessed within the
/// FlatUIHandler function.
///
/// @note `Register` functions should be called BEFORE creating the GUI using
/// `CreateFlatUIFromData` and `flatui::Run()`.
///
/// @param[in] id A `std::string&` to the ID of the FlatUI element that this
/// data is associated with.
/// @param[in,out] data A pointer to `int` data that will persist for the
/// lifetime of the GUI.
///
/// @warning Do not register any pointers that may become invalid before the
/// GUI is finished running. All pointers must persist for the lifetime of
/// the GUI.
void RegisterIntData(const std::string& id, int* data);

/// @brief Used to register a pointer of dynamically changing float data with
/// the API.
/// @see RegisterIntData
void RegisterFloatData(const std::string& id, float* data);

/// @brief Used to register a pointer of dynamically changing char data with the
/// API.
/// @see RegisterIntData
void RegisterCharData(const std::string& id, char* data);

/// @brief Used to register a pointer of dynamically changing bool data with the
/// API.
/// @see RegisterIntData
void RegisterBoolData(const std::string& id, bool* data);

/// @brief Used to register a pointer of dynamically changing vec2 data with the
/// API.
/// @see RegisterIntData
void RegisterVec2Data(const std::string& id, mathfu::vec2* data);

/// @brief Used to register a pointer of dynamically changing vec2i data with
/// the API.
/// @see RegisterIntData
void RegisterVec2iData(const std::string& id, mathfu::vec2i* data);

/// @brief Used to register a pointer of dynamically changing vec4 data with the
/// API.
/// @see RegisterIntData
void RegisterVec4Data(const std::string& id, mathfu::vec4* data);

/// @brief Used to register a pointer of dynamically changing string data with
/// the API.
/// @see RegisterIntData
void RegisterStringData(const std::string& id, std::string* data);

/// @brief Used to register a pointer of dynamically changing data with the API.
/// @see RegisterIntData
void RegisterVoidData(const std::string& id, void* data);

/// @brief Register a custom FlatUI widget's deserialization function.
///
/// If a user wishes to support custom widget deserialization, then this
/// function should be used to register that function with this API. Whenever
/// the `type` of a widget cannot be matched with a core widget, then the
/// registered custom widgets will be checked next. If a matching custom widget
/// function exists for a given `type`, then it will be used to deserialize
/// the FlatBuffer data and handle any FlatUI calls.
///
/// @note `Register` functions should be called BEFORE creating the GUI using
/// `CreateFlatUIFromData` and `flatui::Run()`.
///
/// @param[in] type An uint32_t, corresponding to the Enum value of the widget
/// type.
/// @param[in] widget A `std::function` that acts like a function pointer
/// to the function that will be called handle the custom widget deserialization
/// and UI element construction.
void RegisterCustomWidget(const uint32_t& type, CustomWidget widget);

/// @brief Creates a FlatUI GUI from FlatBuffer data.
///
/// @note This is the core function of this API, which is typically passed to
/// `flatui::Run()` to render the GUI.
///
/// @param[in] data A pointer to the FlatBuffer data to use to create the UI.
/// This data should conform to the `flatui/schema/flatui.fbs` schema.
/// @param[in] assetman An optional parameter, which points to the
/// `fplbase::AssetManager` to be used for rendering textures. If you do not
/// need to render any textures, set this parameter to `nullptr`.
/// @param[in] event_handler An optional parameter, which acts as a function
/// pointer to the Event Handler function that will be called whenever an
/// Event occurs in the generated GUI.
void CreateFlatUIFromData(const void* data,
                          fplbase::AssetManager* assetman = nullptr,
                          FlatUIHandler event_handler = nullptr);

/// @brief Called to set the maximum number of errors to output.
///
/// The default is 10. Set to `kNoErrorOutputLimit` to print out every error on
/// every frame.
void SetErrorOutputCount(int count);

}  // flatui

#endif  // FPL_FLATUI_SERIALIZATION_H_
