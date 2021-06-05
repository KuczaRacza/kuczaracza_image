#pragma once
#include "types.h"
enum compression {
  PALLETE = 0x1,
  QUANTIZATION = 0x2,
  QUAD_TREE = 0x4,
  BLOCKS = 0x8
};
typedef enum compression compression;
struct dict8_rgba {
  u8 size;
  u32 colors[256];
};
struct dict16_rgba {
  u16 size;
  u16 capacity;
  u32 *colors;
};
struct dict8_rgb {
  u8 size;
  u32 colors[192];
};
struct dict16_rgb {
  u16 size;
  u16 capacity;
  u32 *colors;
};
typedef struct dict8_rgba dict8_rgba;
typedef struct dict16_rgba dict16_rgba;
typedef struct dict8_rgb dict8_rgb;
typedef struct dict16_rgb dict16_rgb;
struct image {
  u32 size_x;
  u32 size_y;
  u8 format;
  dict8_rgb dict;
  bitmap *pixels;
};
typedef struct image image;

image *encode(bitmap *raw);
bitmap *decode(image *img);
stream seralize(image *img);
image *deserialize(stream str);
bitmap *pallete_8rgb(bitmap *bit, dict8_rgb *d);
bitmap *depallete_8rgb(bitmap *bit, dict8_rgb d);
i16 add_color_8rgb(dict8_rgb *d, u32 color);
u32 get_dict8rgb(u8 index, dict8_rgb *d);