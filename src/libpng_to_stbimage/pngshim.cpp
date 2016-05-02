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

// A stb_image.h to libpng bridge implementation for FreeType.
#include <assert.h>
#include "png.h"  // stb_image.h to libpng bridge header.
#define STBI_ONLY_PNG
#include "stb_image.h"

#include <ft2build.h>
#include <freetype.h>

// Structures used in FreeType's libpng code.
// We define those structures for our own use to bridge stb_image to libpng.

// Structure containing PNG image information.
struct png_info {
  png_row_info row_info;
  int32_t height;
};

// Structure containing callbacks and intermediate data used while decoding
// a PNG image.
// Callbacks are invoked from FreeType.
struct png_struct {
  png_voidp io_ptr;
  png_voidp error_ptr;
  png_rw_ptr read_data_fn;
  png_user_transform_ptr read_user_transform_fn;
  png_info info;
};

// Global instance of PNG reader. FreeType decodes PNG files sequentially, so
// we just need one instance of the reader.
static png_struct png_reader;

// Read functions definition for stb_image.
// They invoke callbacks supplied from FreeType to read data from FreeType
// stream.
static int32_t read_data(void *user, char *data, int size) {
  png_structp png_ptr = reinterpret_cast<png_structp>(user);
  FT_Stream stream = reinterpret_cast<FT_Stream>(png_ptr->io_ptr);
  if (stream->pos + size > stream->size) {
    size = stream->size - stream->pos;
  }

  if (size) {
    png_ptr->read_data_fn(png_ptr, reinterpret_cast<png_bytep>(data), size);
  }
  return size;
}

static void read_skip(void * /*user*/, int /*n*/) {
  assert(0);  // Not implemented.
}

static int read_eof(void *user) {
  png_structp png_ptr = reinterpret_cast<png_structp>(user);
  FT_Stream stream = reinterpret_cast<FT_Stream>(png_ptr->io_ptr);
  return stream->size == stream->pos;
}

static stbi_io_callbacks callbacks{read_data, read_skip, read_eof};

// libPng comatible functions over stb_image.
png_structp png_create_read_struct(png_const_charp /*user_png_ver*/,
                                   png_voidp error_ptr, void * /*error_fn*/,
                                   void * /*warn_fn*/) {
  png_reader.error_ptr = error_ptr;
  return &png_reader;
}

png_infop png_create_info_struct(png_structp /*png_ptr*/) {
  return &png_reader.info;
}

void png_set_read_fn(png_structp png_ptr, png_voidp io_ptr,
                     png_rw_ptr read_data_fn) {
  // Keep given read function and pointer for the later usse.
  png_ptr->io_ptr = io_ptr;
  png_ptr->read_data_fn = read_data_fn;
}

void png_set_read_user_transform_fn(
    png_structp png_ptr, png_user_transform_ptr read_user_transform_fn) {
  png_ptr->read_user_transform_fn = read_user_transform_fn;
}

void png_read_info(png_structp png_ptr, png_infop info_ptr) {
  // Read png image and info here.
  int32_t width;
  int32_t height;
  int32_t channels;
  stbi_info_from_callbacks(&callbacks, png_ptr, &width, &height, &channels);
  info_ptr->row_info.pixel_depth = 8;
  info_ptr->row_info.bit_depth = 8;
  info_ptr->row_info.channels = channels;
  info_ptr->row_info.width = width;
  info_ptr->row_info.rowbytes = width * channels;
  info_ptr->row_info.color_type = channels;
  info_ptr->height = height;
}

void png_read_update_info(png_structp /*png_ptr*/, png_infop /*info_ptr*/) {}

png_uint_32 png_get_IHDR(png_structp png_ptr, png_infop info_ptr,
                         png_uint_32 *width, png_uint_32 *height,
                         int *bit_depth, int *color_type, int *interlace_method,
                         int *compression_method, int *filter_method) {
  (void)png_ptr;
  *width = info_ptr->row_info.width;
  *height = info_ptr->height;
  *bit_depth = info_ptr->row_info.bit_depth;
  *color_type = info_ptr->row_info.color_type;
  if (interlace_method) {
    *interlace_method = 0;  // Unused value.
  }
  if (compression_method) {
    *compression_method = 0;  // Unused value.
  }
  if (filter_method) {
    *filter_method = 0;  // Unused value.
  }
  return 0;
}

void png_read_image(png_structp png_ptr, png_bytep *image) {
  assert(png_ptr->read_user_transform_fn);
  // Reset stream.
  FT_Stream stream = reinterpret_cast<FT_Stream>(png_ptr->io_ptr);
  stream->pos = 0;

  // Decode the PNG buffer.
  int32_t width;
  int32_t height;
  int32_t channels;
  stbi_uc *buffer = stbi_load_from_callbacks(&callbacks, png_ptr, &width,
                                             &height, &channels, 0);

  // Copy data to the supplied buffer.
  memcpy(*image, buffer,
         png_ptr->info.row_info.rowbytes * png_ptr->info.height);

  // Free up PNG data.
  stbi_image_free(buffer);

  // TODO: Add pre-multiplied blend mode in FPLBase.
  // Invoke user-supplied transform function as below.
  // for (auto i = 0; i < png_ptr->info.height; ++i) {
  //  png_ptr->read_user_transform_fn(png_ptr, &png_ptr->info.row_info,
  //  image[i]);
  //}
}

void png_read_end(png_structp /*png_ptr*/, png_infop /*info_ptr*/) {}

void png_destroy_read_struct(png_structp * /*png_ptr_ptr*/,
                             png_infop * /*info_ptr_ptr*/,
                             png_infop * /*end_info_ptr_ptr*/) {}

png_voidp png_get_io_ptr(png_structp png_ptr) { return png_ptr->io_ptr; }

png_voidp png_get_error_ptr(const png_structp png_ptr) {
  return png_ptr->error_ptr;
}

// Dummy functions.
// Those parameters are automatically handled in stb_image and
// don't have to be manually configured.
void png_error(png_structp /*png_ptr*/, png_const_charp /*error_message*/) {}

void png_set_strip_16(png_structp /*png_ptr*/) {}

void png_set_gray_to_rgb(png_structp /*png_ptr*/) {}

void png_set_palette_to_rgb(png_structp /*png_ptr*/) {}

void png_set_packing(png_structp /*png_ptr*/) {}

void png_set_filler(png_structp /*png_ptr*/, int /*method*/, int /*filters*/) {}

int png_set_interlace_handling(png_structp /*png_ptr*/) { return 0; }

void png_set_tRNS_to_alpha(png_structp /*png_ptr*/) {}

void png_set_expand_gray_1_2_4_to_8(png_structp /*png_ptr*/) {}

jmp_buf *png_set_longjmp_fn(png_structp /*png_ptr*/) {
  static jmp_buf longjump;
  return &longjump;
}

png_uint_32 png_get_valid(const png_structp /*png_ptr*/,
                          const png_infop /*info_ptr*/, png_uint_32 /*flag*/) {
  return 0;
}
