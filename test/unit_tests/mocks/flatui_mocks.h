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

#ifndef FLATUI_MOCKS_FLATUI_MOCKS_H_
#define FLATUI_MOCKS_FLATUI_MOCKS_H_

#include "flatui/flatui.h"
#include "gmock/gmock.h"

namespace flatui {

// googlemock cannot mock free-functions directly, so this is a singleton,
// wrapper class to hold all of the mock implementations. The instance of this
// class is used in the fake functions below to bypass this restriction.
class FlatUIMocks {
 public:
  static FlatUIMocks &get_mocks() {
    static FlatUIMocks mocks;
    return mocks;
  };

  MOCK_METHOD2(Label, void(const char *, float));
  MOCK_METHOD3(Label, void(const char *, float, const mathfu::vec2 &));
  MOCK_METHOD4(Label,
               void(const char *, float, const mathfu::vec2 &, TextAlignment));
  MOCK_METHOD5(Edit, Event(float, const mathfu::vec2 &, const char *,
                           EditStatus *, std::string *));
  MOCK_METHOD6(Edit, Event(float, const mathfu::vec2 &, TextAlignment,
                           const char *, EditStatus *, std::string *));
  MOCK_METHOD2(Image, void(const fplbase::Texture &, float));
  MOCK_METHOD3(StartGroup, void(Layout, float, const char *));
  MOCK_METHOD0(EndGroup, void());
  MOCK_METHOD0(ModalGroup, void());
  MOCK_METHOD2(StartScroll, void(const mathfu::vec2 &, mathfu::vec2 *));
  MOCK_METHOD0(EndScroll, void());
  MOCK_METHOD3(StartSlider, void(Direction, float, float *));
  MOCK_METHOD0(EndSlider, void());
  MOCK_METHOD1(SetVirtualResolution, void(float));
  MOCK_METHOD3(PositionGroup, void(Alignment, Alignment, const mathfu::vec2 &));

 private:
  FlatUIMocks() {};
};

}  // flatui

#endif  // FLATUI_MOCKS_FLATUI_MOCKS_H_
