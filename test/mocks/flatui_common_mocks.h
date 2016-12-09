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

#ifndef FLATUI_MOCKS_FLATUI_COMMON_MOCKS_H_
#define FLATUI_MOCKS_FLATUI_COMMON_MOCKS_H_

#include "flatui/flatui.h"
#include "flatui/flatui_common.h"
#include "gmock/gmock.h"

namespace flatui {

// googlemock cannot mock free-functions directly, so this is a singleton,
// wrapper class to hold all of the mock implementations. The instance of this
// class is used in the fake functions below to bypass this restriction.
class FlatUICommonMocks {
 public:
  static FlatUICommonMocks& get_mocks() {
    static FlatUICommonMocks mocks;
    return mocks;
  };

  MOCK_METHOD2(SetHoverClickColor,
               void(const mathfu::vec4&, const mathfu::vec4&));
  MOCK_METHOD3(TextButton, Event(const char*, float, const Margin&));
  MOCK_METHOD4(ImageButton, Event(const fplbase::Texture&, float, const Margin&,
                                  const char*));
  MOCK_METHOD6(TextButton,
               Event(const fplbase::Texture&, const Margin&, const char*, float,
                     const Margin&, const ButtonProperty));
  MOCK_METHOD6(CheckBox, Event(const fplbase::Texture&, const fplbase::Texture&,
                               const char*, float, const Margin&, bool*));
  MOCK_METHOD6(Slider, Event(const fplbase::Texture&, const fplbase::Texture&,
                             const mathfu::vec2&, float, const char*, float*));
  MOCK_METHOD6(ScrollBar,
               Event(const fplbase::Texture&, const fplbase::Texture&,
                     const mathfu::vec2&, float, const char*, float*));
  MOCK_METHOD1(EventBackground, void(Event));

 private:
  FlatUICommonMocks(){};
};

}  // flatui

#endif  // FLATUI_MOCKS_FLATUI_COMMON_MOCKS_H_
