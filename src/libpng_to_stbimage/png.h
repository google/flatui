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
#ifndef LIBPNG_TO_STBIMAGE_PNG_H
#define LIBPNG_TO_STBIMAGE_PNG_H

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>

// libpng compatible bridge implementation over stb_image for FreeType.
// Note that the file name needs to be 'png.h' to fake FreeType.
#ifdef __cplusplus
extern "C" {
#endif

#define PNG_LIBPNG_VER_STRING "9.9.9"
#define PNG_LIBPNG_VER (99999)

struct png_row_info {
  uint32_t width;
  size_t rowbytes;
  uint8_t color_type;
  uint8_t bit_depth;
  uint8_t channels;
  uint8_t pixel_depth;
};

// Definitions to compile FreeType with FT_CONFIG_OPTION_USE_PNG defined.
typedef struct png_struct *png_structp;
typedef struct png_info *png_infop;
typedef void *png_voidp;
typedef uint8_t *png_bytep;
typedef const char *png_const_charp;
typedef size_t png_size_t;
typedef uint32_t png_uint_32;
typedef uint8_t png_byte;
typedef struct png_row_info *png_row_infop;
typedef void (*png_rw_ptr)(png_structp, png_bytep, png_size_t);
typedef void (*png_user_transform_ptr)(png_structp, png_row_infop, png_bytep);

// Definitions used in pngshim.cpp in FreeType.

// Number of channels in PNG.
#define PNG_COLOR_TYPE_PALETTE (0)
#define PNG_COLOR_TYPE_GRAY (1)
#define PNG_COLOR_TYPE_GRAY_ALPHA (2)
#define PNG_COLOR_TYPE_RGB (3)
#define PNG_COLOR_TYPE_RGB_ALPHA (4)

// Some other flags that stb_image handles implicitly.
#define PNG_INTERLACE_NONE (0)
#define PNG_FILLER_AFTER (0)

#define PNG_INFO_tRNS 0x0010

png_structp png_create_read_struct(png_const_charp user_png_ver,
                                   png_voidp error_ptr, void *error_fn,
                                   void *warn_fn);
png_infop png_create_info_struct(png_structp png_ptr);

void png_set_read_fn(png_structp png_ptr, png_voidp io_ptr,
                     png_rw_ptr read_data_fn);
void png_set_read_user_transform_fn(
    png_structp png_ptr, png_user_transform_ptr read_user_transform_fn);
void png_read_info(png_structp png_ptr, png_infop info_ptr);
void png_read_update_info(png_structp png_ptr, png_infop info_ptr);
png_uint_32 png_get_IHDR(png_structp png_ptr, png_infop info_ptr,
                         png_uint_32 *width, png_uint_32 *height,
                         int *bit_depth, int *color_type, int *interlace_method,
                         int *compression_method, int *filter_method);

void png_read_image(png_structp png_ptr, png_bytep *image);
void png_read_end(png_structp png_ptr, png_infop info_ptr);
void png_destroy_read_struct(png_structp *png_ptr_ptr, png_infop *info_ptr_ptr,
                             png_infop *end_info_ptr_ptr);

void png_error(png_structp png_ptr, png_const_charp error_message);
png_voidp png_get_io_ptr(png_structp png_ptr);
png_voidp png_get_error_ptr(const png_structp png_ptr);
void png_set_strip_16(png_structp png_ptr);
void png_set_gray_to_rgb(png_structp png_ptr);
void png_set_palette_to_rgb(png_structp png_ptr);
void png_set_packing(png_structp png_ptr);
void png_set_tRNS_to_alpha(png_structp png_ptr);
void png_set_expand_gray_1_2_4_to_8(png_structp png_ptr);
int png_set_interlace_handling(png_structp png_ptr);

void png_set_filler(png_structp png_ptr, int method, int filters);
png_uint_32 png_get_valid(const png_structp png_ptr, const png_infop info_ptr,
                          png_uint_32 flag);

jmp_buf *png_set_longjmp_fn(png_structp png_ptr);

#define png_jmpbuf(png_ptr) (*png_set_longjmp_fn(png_ptr))

#ifdef __cplusplus
}
#endif

#endif  // LIBPNG_TO_STBIMAGE_PNG_H
