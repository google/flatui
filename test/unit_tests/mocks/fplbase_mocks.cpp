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

#include "mocks/fplbase_mocks.h"

#include "fplbase/asset_manager.h"
#include "fplbase/async_loader.h"
#include "fplbase/input.h"
#include "fplbase/mesh.h"
#include "fplbase/renderer.h"
#include "fplbase/shader.h"
#include "fplbase/utilities.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

// STB_image to resize PNG glyph.
#define STB_IMAGE_RESIZE_IMPLEMENTATION
// Disable warnings in STB_image_resize.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100)  // Disable 'unused reference' warning.
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif /* _MSC_VER */
#include "stb_image_resize.h"
// Pop warning status.
#ifdef _MSC_VER
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif

namespace fplbase {

void LogError(const char* fmt, va_list args) {
  FplbaseMocks::get_mocks().LogError(fmt, args);
}

Texture* AssetManager::FindTexture(const char* filename) {
  return FplbaseMocks::get_mocks().FindTexture(filename);
}

// Unused functions to satisfy the linker.

// asset_manager.h
AssetManager::AssetManager(Renderer& renderer) : renderer_(renderer) {}
void AssetManager::ClearAllAssets() {}
Shader* AssetManager::LoadShader(const char*) { return nullptr; }

// async_loader.h
AsyncLoader::AsyncLoader() {}
AsyncLoader::~AsyncLoader() {}
void AsyncLoader::Stop() {}

// input.h
Button fake_button_to_return;
Button& InputSystem::GetButton(int) { return fake_button_to_return; }
void InputSystem::StartTextInput() {}
void InputSystem::StopTextInput() {}
double InputSystem::Time() const { return 0.0; }
void InputSystem::SetTextInputRect(const mathfu::vec4&) {}
const std::vector<TextInputEvent>* InputSystem::GetTextInputEvents() {
  return nullptr;
}

// mesh.h
void Mesh::RenderArray(Primitive, int, const Attribute*, int, const void*,
                       const unsigned short*) {}
void Mesh::RenderAAQuadAlongX(const mathfu::vec3&, const mathfu::vec3&,
                              const mathfu::vec2&, const mathfu::vec2&) {}
void Mesh::RenderAAQuadAlongXNinePatch(const mathfu::vec3&, const mathfu::vec3&,
                                       const mathfu::vec2i&,
                                       const mathfu::vec4&) {}

// renderer.h
Renderer::Renderer() {}
Renderer::~Renderer() {}

// shader.h
void Shader::Set(const Renderer&) const {}
void Shader::SetUniform(UniformHandle, const float*, size_t) {}
UniformHandle Shader::FindUniform(const char*) { return 0; }

// texture.h
Texture::Texture(const char *, TextureFormat, TextureFlags) {}
void Texture::Delete() {}
void Texture::Load() {}
void Texture::Finalize() {}
void Texture::LoadFromMemory(const uint8_t*, const mathfu::vec2i&,
                             TextureFormat) {}
void Texture::Set(size_t) {}
void Texture::Set(size_t) const {}
void Texture::UpdateTexture(TextureFormat, int, int, int, int, const void*) {}

// utilities.h
void LogInfo(const char*, va_list) {}
void LogInfo(LogCategory, const char*, va_list) {}
void LogError(LogCategory, const char*, va_list) {}
void LogError(const char*, ...) {}
void LogInfo(const char*, ...) {}
void LogInfo(LogCategory, const char*, ...) {}
void LogError(LogCategory, const char*, ...) {}
bool LoadFile(const char*, std::string*) { return false; }
bool LoeadWithIncludes(const char*, std::string*, std::string*) {
  return false;
}
#ifdef __ANDROID__
JNIEnv *AndroidGetJNIEnv() { return nullptr; }
int32_t AndroidGetApiLevel() { return 0; }
#endif // __ANDROID__

}  // fplbase
