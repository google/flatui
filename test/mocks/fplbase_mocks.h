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

#ifndef FLATUI_MOCKS_FPLBASE_MOCKS_H_
#define FLATUI_MOCKS_FPLBASE_MOCKS_H_

#include <stdarg.h>

#include "fplbase/texture.h"
#include "gmock/gmock.h"

namespace fplbase {

// googlemock cannot mock non-virtual functions directly, so this is a singleton
// wrapper class to hold all of the mock implementations. The instance of this
// class is used in the fake functions, defined in the `.cpp`, to bypass this
// restriction.
class FplbaseMocks {
 public:
  static FplbaseMocks& get_mocks() {
    static FplbaseMocks mocks;
    return mocks;
  };

  // asset_manager.h
  MOCK_METHOD1(FindTexture, fplbase::Texture*(const char*));

  // utilities.h
  MOCK_METHOD2(LogError, void(const char* fmt, va_list args));

 private:
  FplbaseMocks(){};
};

}  // fplbase

#endif  // FLATUI_MOCKS_FPLBASE_MOCKS_H_
