#pragma once
#include "types.h"
#define VERSION 3
#define MAGIC "KCZI"
struct dict8 {
  u8 size;
  u32 colors[256];
};
typedef struct dict8 dict8;
// part of each image rects and pixels
struct region {
  u16 x;
  u16 y;
  u16 w;
  u16 h;
  // pallete for each part
  u16 dict_index;
  u8 depth;
  stream pixels;
  stream blokcs;
};
typedef struct region region;
struct image {
  u16 size_x;
  u16 size_y;
  u8 format;
  u8 max_depth;
  u16 blok_size;
  u16 color_quant;
  u16 color_sensivity;
  // number of regions
  u32 length;
  u32 dicts_length;
  region *parts;
  dict8 *dicts;
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
image *encode(bitmap *raw, u32 max_block_size, u32 color_reduction,
              u32 block_color_sensivity , u32 complexity);
// decode whole image to bitmap
bitmap *decode(image *img);
// writes image into array of pixels
stream seralize(image *img);
// reads image form array of pixels
image *deserialize(stream compressed_str);

static stream pallete(stream str, dict8 *d, u32 esize);
static stream depallete(stream str, dict8 *d, u32 esize);
static u16 add_color(dict8 *d, u32 color);
static u32 get_dict(u8 index, dict8 *d);

// redeuces number of colors
static void linear_quantization(bitmap *b, u32 quant, u8 alpha);
static void cubic_quantization(bitmap *b, u32 quant, u8 alpha);
// encodes image into rectangular treee
static void rectangle_tree(image *img, bitmap *raw, u32 max_block_size,
                           u32 block_color_sensivity,u32  complexity);
static u32 count_colors(bitmap *b);
// count colors in some area
static u32 count_colors_rect(bitmap *b, u32 x, u32 y, u32 w, u32 h);
// cerates reatangles in react tree
static void create_rect(vector *rects, bitmap *raw, rect area, u8 depth);
// count diffrence in colors
static rgba_color color_diffrence(u32 color_b, u32 color_a);
// omits unnessary blocks that might be interpolated based on corners
static stream cut_quads(bitmap *b, u8 quad_s, u8 threshold, stream blocks);
// interpoltes ommited blocks to fill gaps
static bitmap *recreate_quads(stream str, u8 quad_s, rect size,
                              u8 format,stream blocks);
void free_image(image *img);
//needs  to be implemented
u32  average_color(rect area, bitmap * b);
static u8 merge_dicts(dict8 *dst, dict8 *src, stream *pixels);
