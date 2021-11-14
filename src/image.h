#pragma once
#include "types.h"
#define VERSION 6
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
	// palette for each part
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
	u16 color_sensitivity;
	// number of regions
	u32 length;
	u32 dicts_length;
	region *parts;
	dict8 *dicts;
	stream edges_map;
	u32 avg_edges;
	u16 dct_start;
	u16 dct_end;
};
typedef struct image image;
struct encode_args {
	// 0 standard
	// 1 force yuv
	// 2 force  index
	u8 encode_method;
	u32 max_block_size;
	u32 color_reduction;
	u32 block_color_sensitivity;
	u32 complexity;
	u32 dct_quant_min;
	u32 dct_quant_max;
};
typedef struct encode_args encode_args;

// encodes whole image from bitmap
image *encode(bitmap *raw, encode_args args);
// decode whole image to bitmap
bitmap *decode(image *img);
// writes image into array of pixels
stream seralize(image *img);
// reads image form array of pixels
image *deserialize(stream compressed_str);

stream palette(stream str, dict8 *d, u32 esize);
stream depalette(stream str, dict8 *d, u32 esize);
u16 add_color(dict8 *d, u32 color);
u32 get_dict(u8 index, dict8 *d);

// redeuces number of colors
void linear_quantization(bitmap *b, u32 quant, u8 alpha);
// encodes image into rectangular tree
void rectangle_tree(image *img, bitmap *raw, u32 max_block_size,
										u32 block_color_sensitivity, u32 complexity);
u32 count_colors(bitmap *b);
// count colors in some area
u32 count_colors_rect(bitmap *b, u32 x, u32 y, u32 w, u32 h);
// cerates reatangles in rect tree
void create_rect(vector *rects, bitmap *raw, rect area, u8 depth, u8 format,
								 stream edges_map, u32 avg_edge);

// omits unnecessary blocks that might be interpolated based on corners
stream cut_quads(bitmap *b, u8 quad_s, u8 threshold, stream blocks, image *img,
								 rect area);
// interpolates omitted blocks to fill gaps
bitmap *recreate_quads(stream str, u8 quad_s, rect size, u8 format,
											 stream blocks, image *img);
void free_image(image *img);
// needs to be implemented
u32 average_color(rect area, bitmap *b);
u8 merge_dicts(dict8 *dst, dict8 *src, stream *pixels);
bitmap *rgb_to_yuv(bitmap *b);
bitmap *yuv_to_rgb(bitmap *b);
u64 edge_detection_yuv(bitmap *b, u32 x, u32 y);
void edeges_map(image *img, bitmap *yuv);
void subsampling_yuv(bitmap *b, rect area, stream out, u32 *offset);
void desubsampling_yuv(bitmap *b, rect area, stream str, u32 *offset);
void defullsampling(bitmap *b, rect area, stream str, u32 *offset);
void fullsampling(bitmap *b, rect area, stream out, u32 *offset);
void zigzag_to_xy(void *matrix, u32 esize, u32 sizex, u32 sizey);
void xy_to_zigzag(void *matrix, u32 esize, u32 sizex, u32 sizey);
void dct_quatization(u32 sizex, u32 sizey, i16 *dct, u16 *quants, u8 channels, u32 matrix_w, u32 matrix_h);
void dct_de_quatization(u32 sizex, u32 sizey, i16 *dct, u16 *quants, u8 channels, u32 matrix_w, u32 matrix_h);
stream dct_quatization_matrix(u8 sizex, u8 sizey, u16 start, u16 end, u16 offset);
void dct(bitmap *b, rect area, stream str, u32 *offset);
void idct(stream dct_matrix, u32 *offset, rect area, bitmap *b);
