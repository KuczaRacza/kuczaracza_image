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
struct dict8_rgb {
  u8 size;
  u32 colors[256];
};

typedef struct dict8_rgba dict8_rgba;
typedef struct dict8_rgb dict8_rgb;
struct part24_rgb {
  u16 x;
  u16 y;
  u16 w;
  u16 h;
  dict8_rgb dict;
  u8 depth;
  bitmap *map;
};
typedef struct part24_rgb part24_rgb;
struct part32_rgba {
  u16 x;
  u16 y;
  u16 w;
  u16 h;
  dict8_rgba dict;
  u8 depth;
  bitmap *map;
};
typedef struct part32_rgba part32_rgba;
struct image {
  u32 size_x;
  u32 size_y;
  u8 format;
  u32 length;
  part24_rgb *parts;
};
typedef struct image image;

image *encode(bitmap *raw);
bitmap *decode(image *img);
stream seralize(image *img);
image *deserialize(stream str);

bitmap *pallete_8rgb(bitmap *bit, dict8_rgb *d);
bitmap *depallete_8rgb(bitmap *bit, dict8_rgb *d);
u16 add_color_8rgb(dict8_rgb *d, u32 color);
u32 get_dict8rgb(u8 index, dict8_rgb *d);

bitmap *pallete_8rgba(bitmap *bit, dict8_rgba *d);
bitmap *depallete_8rgba(bitmap *bit, dict8_rgba *d);
u16 add_color_8rgba(dict8_rgba *d, u32 color);
u32 get_dict8rgba(u8 index, dict8_rgba *d);

void linear_quantization(bitmap *b, u32 quant, u8 alpha);
void cubic_quantization(bitmap *b, u32 quant, u8 alpha);
void rectangle_tree(image *img, bitmap *raw);
u32 count_colors(bitmap *b);
u32 count_colors_rect(bitmap *b, u32 x, u32 y, u32 w, u32 h);
