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
  stream pixels;
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
struct rgba_color {
  u8 r;
  u8 g;
  u8 b;
  u8 a;
};
typedef struct rgba_color rgba_color;

image *encode(bitmap *raw);
bitmap *decode(image *img);
stream seralize(image *img);
image *deserialize(stream str);

stream pallete(stream str, dict8_rgb *d, u32 esize);
stream depallete(stream str, dict8_rgb *d, u32 esize);
u16 add_color(dict8_rgb *d, u32 color);
u32 get_dict(u8 index, dict8_rgb *d);

void linear_quantization(bitmap *b, u32 quant, u8 alpha);
void cubic_quantization(bitmap *b, u32 quant, u8 alpha);
void rectangle_tree(image *img, bitmap *raw);
u32 count_colors(bitmap *b);
u32 count_colors_rect(bitmap *b, u32 x, u32 y, u32 w, u32 h);
void create_rect(vector *rects, bitmap *raw, rect area, u8 depth);
rgba_color color_diffrence(u32 color_b, u32 color_a);
stream cut_quads(bitmap *b, u8 quad_s, u8 threshold);
bitmap *recreate_quads(stream str, u8 quad_s, u8 threshold,rect size,u8 format);