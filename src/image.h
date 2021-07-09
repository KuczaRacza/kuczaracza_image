#pragma once
#include "types.h"
struct dict8_rgb {
  u8 size;
  u32 colors[256];
};
typedef struct dict8_rgb dict8_rgb;
// part of each image rects and pixels
struct part24_rgb {
  u16 x;
  u16 y;
  u16 w;
  u16 h;
  // pallete for each part
  dict8_rgb dict;
  u8 depth;
  stream pixels;
};
typedef struct part24_rgb part24_rgb;
struct image {
  u32 size_x;
  u32 size_y;
  u8 format;
  u8 max_depth;
  // number of part
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
// encodes whole image from bitmap
image *encode(bitmap *raw);
// decode whole image to bitmap
bitmap *decode(image *img);
// writes image into array of pixels
stream seralize(image *img);
// reads image form array of pixels
image *deserialize(stream str);

static stream pallete(stream str, dict8_rgb *d, u32 esize);
static stream depallete(stream str, dict8_rgb *d, u32 esize);
static u16 add_color(dict8_rgb *d, u32 color);
static u32 get_dict(u8 index, dict8_rgb *d);

// redeuces number of colors
static void linear_quantization(bitmap *b, u32 quant, u8 alpha);
static void cubic_quantization(bitmap *b, u32 quant, u8 alpha);
// encodes image into rectangular treee
static void rectangle_tree(image *img, bitmap *raw);
static u32 count_colors(bitmap *b);
// count colors in some area
static u32 count_colors_rect(bitmap *b, u32 x, u32 y, u32 w, u32 h);
// cerates reatangles in react tree
static void create_rect(vector *rects, bitmap *raw, rect area, u8 depth);
//count diffrence in colors
static rgba_color color_diffrence(u32 color_b, u32 color_a);
//omits unnessary blocks that might be interpolated based on corners
static stream cut_quads(bitmap *b, u8 quad_s, u8 threshold);
//interpoltes ommited blocks to fill gaps
static bitmap *recreate_quads(stream str, u8 quad_s, u8 threshold, rect size,
                              u8 format);
static void free_image(image *img);
